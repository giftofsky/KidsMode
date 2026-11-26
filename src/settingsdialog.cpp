#include "settingsdialog.h"

#include "appicon.h"
#include "pin.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>

SettingsDialog::SettingsDialog(const Policy &policy, bool setupMode, QWidget *parent)
    : QDialog(parent)
    , m_policy(policy)
    , m_setupMode(setupMode)
{
    setWindowTitle(setupMode ? tr("Set up Kids Mode") : tr("Kids Mode settings"));
    setMinimumWidth(520);

    auto *layout = new QVBoxLayout(this);

    auto *intro = new QLabel(setupMode
                                 ? tr("Choose a PIN, a daily limit and the apps your child may open.\n"
                                      "Run this while logged in as the child.")
                                 : tr("Changes take effect immediately."),
                             this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_budget = new QSpinBox(this);
    m_budget->setRange(5, 480);
    m_budget->setSingleStep(5);
    m_budget->setSuffix(tr(" min"));
    m_budget->setValue(policy.budgetSeconds > 0 ? policy.budgetSeconds / 60 : 60);
    auto *budgetRow = new QFormLayout;
    budgetRow->addRow(tr("Time per day:"), m_budget);
    layout->addLayout(budgetRow);

    auto *appsGroup = new QGroupBox(tr("Apps"), this);
    auto *appsLayout = new QVBoxLayout(appsGroup);
    m_apps = new QListWidget(appsGroup);
    m_apps->setIconSize(QSize(28, 28));
    m_apps->setMinimumHeight(150);
    appsLayout->addWidget(m_apps);
    auto *appButtons = new QHBoxLayout;
    auto *addInstalled = new QPushButton(tr("Add from Start Menu..."), appsGroup);
    auto *add = new QPushButton(tr("Add app..."), appsGroup);
    auto *rename = new QPushButton(tr("Rename..."), appsGroup);
    auto *remove = new QPushButton(tr("Remove"), appsGroup);
    appButtons->addWidget(addInstalled);
    appButtons->addWidget(add);
    appButtons->addWidget(rename);
    appButtons->addWidget(remove);
    appButtons->addStretch();
    appsLayout->addLayout(appButtons);
    layout->addWidget(appsGroup, 1);
    connect(addInstalled, &QPushButton::clicked, this, &SettingsDialog::addFromStartMenu);
    connect(add, &QPushButton::clicked, this, &SettingsDialog::addApp);
    connect(rename, &QPushButton::clicked, this, &SettingsDialog::renameApp);
    connect(remove, &QPushButton::clicked, this, &SettingsDialog::removeApp);

    auto *pinGroup = new QGroupBox(tr("Parent PIN"), this);
    auto *pinLayout = new QFormLayout(pinGroup);
    m_pin = new QLineEdit(pinGroup);
    m_pinConfirm = new QLineEdit(pinGroup);
    const QRegularExpressionValidator *validator =
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^[0-9]{0,4}$")), pinGroup);
    for (QLineEdit *field : {m_pin, m_pinConfirm}) {
        field->setValidator(validator);
        field->setMaxLength(4);
        field->setEchoMode(QLineEdit::Password);
        field->setMaximumWidth(120);
    }
    // a saved PIN is only stored hashed, so show dots as a hint instead of the real digits
    m_pin->setPlaceholderText(!setupMode && policy.hasPin() ? QStringLiteral("\u2022\u2022\u2022\u2022")
                                                            : tr("4 digits"));
    if (!setupMode && policy.hasPin())
        m_pin->setToolTip(tr("Leave blank to keep the current PIN"));
    m_pinConfirm->setPlaceholderText(tr("Repeat"));
    pinLayout->addRow(tr("PIN:"), m_pin);
    pinLayout->addRow(tr("Repeat:"), m_pinConfirm);
    layout->addWidget(pinGroup);

    auto *honesty = new QLabel(tr("Kids Mode is a boundary, not a lock: an administrator can "
                                  "always remove it."),
                               this);
    honesty->setWordWrap(true);
    honesty->setStyleSheet(QStringLiteral("color:#7a6f63;"));
    layout->addWidget(honesty);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    layout->addWidget(buttons);

    syncAppsToList();
}

void SettingsDialog::syncAppsToList()
{
    m_apps->clear();
    for (const AppTile &tile : m_policy.apps) {
        auto *item = new QListWidgetItem(appIcon(tile), tile.label(), m_apps);
        const QString target = tile.args.isEmpty()
            ? tile.exePath
            : tile.exePath + QLatin1Char(' ') + tile.args;
        // A shortened label hides the Windows name, so keep it reachable in the tooltip.
        item->setToolTip(tile.label() == tile.name ? target
                                                   : tr("%1\n%2").arg(tile.name, target));
    }
}

void SettingsDialog::addApp()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Choose an app"),
                                                      QStringLiteral("C:/Program Files"),
                                                      tr("Programs (*.exe)"));
    if (path.isEmpty())
        return;

    const QFileInfo info(path);
    m_policy.apps.append(AppTile{info.completeBaseName(), QDir::toNativeSeparators(path), {}});
    syncAppsToList();
}

void SettingsDialog::addFromStartMenu()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QList<AppTile> found = installedApps();
    QApplication::restoreOverrideCursor();
    if (found.isEmpty()) {
        QMessageBox::information(this, tr("Add from Start Menu"),
                                 tr("Could not read the Start Menu."));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Add from Start Menu"));
    dlg.resize(560, 500);
    auto *dlgLayout = new QVBoxLayout(&dlg);
    auto *search = new QLineEdit(&dlg);
    search->setPlaceholderText(tr("Search, for example: math"));
    auto *list = new QListWidget(&dlg);
    list->setIconSize(QSize(24, 24));
    for (int i = 0; i < found.size(); ++i) {
        auto *item = new QListWidgetItem(found.at(i).name, list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        item->setData(Qt::UserRole, i);
        item->setToolTip(found.at(i).args.isEmpty() ? found.at(i).exePath
                                                    : found.at(i).exePath + QLatin1Char(' ') + found.at(i).args);
    }
    connect(search, &QLineEdit::textChanged, list, [list](const QString &text) {
        for (int i = 0; i < list->count(); ++i)
            list->item(i)->setHidden(!text.isEmpty()
                                     && !list->item(i)->text().contains(text, Qt::CaseInsensitive));
    });

    // Icons pop in lazily: resolving a Store logo is a shell round-trip, so feed the
    // list a few rows per tick instead of blocking the dialog open. appIcon() caches,
    // so scrolling back or reopening the dialog is instant.
    auto nextRow = std::make_shared<int>(0);
    auto *iconTimer = new QTimer(&dlg);
    connect(iconTimer, &QTimer::timeout, &dlg, [list, found, nextRow, iconTimer]() {
        for (int n = 0; n < 6 && *nextRow < list->count(); ++n, ++*nextRow)
            list->item(*nextRow)
                ->setIcon(appIcon(found.at(list->item(*nextRow)->data(Qt::UserRole).toInt())));
        if (*nextRow >= list->count())
            iconTimer->stop();
    });
    iconTimer->start(20);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    dlgLayout->addWidget(search);
    dlgLayout->addWidget(list, 1);
    dlgLayout->addWidget(box);
    if (dlg.exec() != QDialog::Accepted)
        return;

    int added = 0;
    for (int i = 0; i < list->count(); ++i) {
        const QListWidgetItem *item = list->item(i);
        if (item->checkState() != Qt::Checked)
            continue;
        const AppTile &tile = found.at(item->data(Qt::UserRole).toInt());
        const bool exists = std::any_of(m_policy.apps.cbegin(), m_policy.apps.cend(),
                                        [&](const AppTile &a) { return a.name == tile.name; });
        if (!exists) {
            m_policy.apps.append(tile);
            ++added;
        }
    }
    if (added)
        syncAppsToList();
}

// Accessibility and touch selection can mark an item without making it current.
int SettingsDialog::selectedAppRow() const
{
    QListWidgetItem *item = m_apps->currentItem();
    if (!item && !m_apps->selectedItems().isEmpty())
        item = m_apps->selectedItems().constFirst();
    const int row = item ? m_apps->row(item) : -1;
    return (row >= 0 && row < m_policy.apps.size()) ? row : -1;
}

void SettingsDialog::renameApp()
{
    const int row = selectedAppRow();
    if (row < 0)
        return;

    const AppTile &tile = m_policy.apps.at(row);
    bool ok = false;
    const QString text = QInputDialog::getText(this, tr("Rename app"),
                                              tr("Name your child sees for \"%1\":").arg(tile.name),
                                              QLineEdit::Normal, tile.label(), &ok)
                             .trimmed();
    if (!ok || text.isEmpty())
        return;

    m_policy.apps[row].shownName = text;
    syncAppsToList();
}

void SettingsDialog::removeApp()
{
    const int row = selectedAppRow();
    if (row < 0)
        return;

    const QString name = m_policy.apps.at(row).name;
    const auto answer = QMessageBox::question(this, tr("Remove app"),
                                              tr("Remove \"%1\" from Kids Mode?").arg(name));
    if (answer != QMessageBox::Yes)
        return;

    m_policy.apps.removeAt(row);
    syncAppsToList();
}

void SettingsDialog::accept()
{
    const QString pin = m_pin->text();

    if (pin.isEmpty() && m_setupMode) {
        QMessageBox::warning(this, tr("Kids Mode"), tr("Choose a 4-digit PIN."));
        return;
    }
    if (!pin.isEmpty()) {
        if (pin.size() != 4) {
            QMessageBox::warning(this, tr("Kids Mode"), tr("The PIN must be exactly 4 digits."));
            return;
        }
        if (pin != m_pinConfirm->text()) {
            QMessageBox::warning(this, tr("Kids Mode"), tr("The PINs do not match."));
            return;
        }
        const Pin::Hash hash = Pin::create(pin);
        m_policy.pinSalt = hash.salt;
        m_policy.pinHash = hash.hash;
        m_policy.pinIterations = hash.iterations;
    }
    if (m_policy.apps.isEmpty()) {
        QMessageBox::warning(this, tr("Kids Mode"), tr("Add at least one app."));
        return;
    }

    m_policy.budgetSeconds = m_budget->value() * 60;
    QDialog::accept();
}
