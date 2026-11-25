#include "settingsdialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QVBoxLayout>

#include "pin.h"

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
    auto *add = new QPushButton(tr("Add app..."), appsGroup);
    auto *remove = new QPushButton(tr("Remove"), appsGroup);
    appButtons->addWidget(add);
    appButtons->addWidget(remove);
    appButtons->addStretch();
    appsLayout->addLayout(appButtons);
    layout->addWidget(appsGroup, 1);
    connect(add, &QPushButton::clicked, this, &SettingsDialog::addApp);
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
    m_pin->setPlaceholderText(setupMode ? tr("4 digits") : tr("Leave blank to keep"));
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
    QFileIconProvider provider;
    for (const AppTile &tile : m_policy.apps) {
        auto *item = new QListWidgetItem(provider.icon(QFileInfo(tile.exePath)), tile.name, m_apps);
        item->setToolTip(tile.exePath);
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

void SettingsDialog::removeApp()
{
    // Accessibility and touch selection can mark an item without making it current.
    QListWidgetItem *item = m_apps->currentItem();
    if (!item && !m_apps->selectedItems().isEmpty())
        item = m_apps->selectedItems().constFirst();
    const int row = item ? m_apps->row(item) : -1;
    if (row < 0 || row >= m_policy.apps.size())
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
