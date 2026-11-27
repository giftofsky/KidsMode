#include "launcherwindow.h"

#include "appicon.h"

#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

// The friendliest face the operating system already has: Windows 11 ships Segoe UI Rounded,
// Comic Sans MS is on every Windows since 95. Nothing to ship, nothing to license.
// It reaches the kid pages only - SettingsDialog is its own window and stays on the plain font.
const QString kKidFontFamily =
    QStringLiteral("'Segoe UI Rounded','Comic Sans MS','Segoe UI'");

// Segoe UI Emoji, reached through Qt's font fallback. These glyphs are decoration: each one sits
// in its own label with an explicit accessible name, so no heading or button reads out a glyph.
const QString kBear = QStringLiteral("\U0001F9F8");
const QString kLock = QStringLiteral("\U0001F510");

// One card colour for every tile. A hash-picked pastel read as an accident - two pinks side by
// side, then two mints - and the icons are already the colour on this screen.
QString tileStyle()
{
    return QStringLiteral("QToolButton{background:#fffaf0;border:2px solid #e6dccb;border-radius:18px;"
                          "padding:12px;font-size:16px;font-weight:600;color:#4a3f35;}"
                          "QToolButton:hover{border-color:#c9a227;}"
                          "QToolButton:pressed{background:#f3ead9;}"
                          "QToolButton:focus{border:3px solid #c9a227;}"
                          "QToolButton:disabled{color:#a99f92;background:#f3eee4;}");
}

// QToolButton cannot word-wrap: it elides mid-word ("Coloring G..."). Break a long name onto a
// second line at the space nearest the middle, so a child reads two short words instead.
// shortcut: a name whose halves are still too long, or that has no space at all, still elides.
QString wrapName(const QString &name)
{
    if (name.size() <= 20)
        return name;

    int best = -1;
    const int middle = name.size() / 2;
    for (int i = name.indexOf(QLatin1Char(' ')); i != -1; i = name.indexOf(QLatin1Char(' '), i + 1)) {
        if (best < 0 || qAbs(i - middle) < qAbs(best - middle))
            best = i;
    }
    if (best < 0)
        return name;
    return name.left(best) + QLatin1Char('\n') + name.mid(best + 1);
}

QString timeBarStyle(bool low)
{
    return QStringLiteral("QProgressBar{border:0;border-radius:13px;background:#efe6d5;"
                          "text-align:center;font-size:16px;font-weight:600;color:#4a3f35;}"
                          "QProgressBar::chunk{border-radius:13px;background:%1;}")
        .arg(low ? QStringLiteral("#e09a2b") : QStringLiteral("#6fae7c"));
}

QPushButton *keypadButton(const QString &text, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setFixedSize(92, 68);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(QStringLiteral(
        "QPushButton{background:#fffaf0;border:2px solid #e6dccb;border-radius:16px;"
        "font-size:24px;color:#4a3f35;}"
        "QPushButton:pressed{background:#f3ead9;}"
        "QPushButton:focus{border-color:#c9a227;}"));
    return button;
}

// The parent gesture: press and hold the remaining-time bar for two seconds.
class TimeBar : public QProgressBar
{
public:
    explicit TimeBar(QWidget *parent = nullptr)
        : QProgressBar(parent)
    {
        setTextVisible(true);
        setFixedHeight(26);
        m_hold.setSingleShot(true);
        m_hold.setInterval(2000);
        connect(&m_hold, &QTimer::timeout, this, [this] {
            if (onLongPress)
                onLongPress();
        });
    }

    std::function<void()> onLongPress;

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
            m_hold.start();
        QProgressBar::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        m_hold.stop();
        QProgressBar::mouseReleaseEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        m_hold.stop();
        QProgressBar::leaveEvent(event);
    }

private:
    QTimer m_hold;
};

} // namespace

LauncherWindow::LauncherWindow(QWidget *parent)
    : QWidget(parent)
{
    // Qt's own flags stay as they are for the whole session: WS_EX_TOPMOST is toggled in place
    // by setTopmost(), because setWindowFlag() on a visible widget hides and re-creates the
    // window and the desktop shows through for a frame (backlog 004).
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral("LauncherWindow{background:#fdf6e3;font-family:%1;}")
                      .arg(kKidFontFamily));
    setFocusPolicy(Qt::StrongFocus);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    m_pages = new QStackedWidget(this);
    outer->addWidget(m_pages);

    buildTilesPage();
    buildLockPage();
    buildShieldPage();
    m_pages->setCurrentIndex(0);
}

void LauncherWindow::buildTilesPage()
{
    auto *page = new QWidget(m_pages);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(64, 40, 64, 32);
    layout->setSpacing(20);

    // The bear is the app's own mascot, in its own label so "What shall we play?" keeps a clean
    // accessible name; the glyph would otherwise be read out as garbage.
    auto *header = new QHBoxLayout;
    header->setSpacing(16);
    auto *bear = new QLabel(kBear, page);
    bear->setAccessibleName(tr("Kids Mode"));
    bear->setStyleSheet(QStringLiteral("font-size:42px;"));
    auto *title = new QLabel(tr("What shall we play?"), page);
    title->setStyleSheet(QStringLiteral("font-size:34px;font-weight:700;color:#8a5a3c;"));
    header->addStretch();
    header->addWidget(bear, 0, Qt::AlignVCenter);
    header->addWidget(title);
    header->addStretch();
    layout->addLayout(header);

    auto *host = new QWidget;
    m_tileGrid = new QGridLayout(host);
    m_tileGrid->setSpacing(24);
    // Room at the bottom so the last row can scroll clear of the footer instead of stopping
    // half-cut above it (backlog 003).
    m_tileGrid->setContentsMargins(0, 8, 0, 28);
    m_tileGrid->setAlignment(Qt::AlignCenter);

    auto *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(host);
    scroll->setStyleSheet(QStringLiteral(
        "QScrollArea{background:transparent;}"
        "QScrollArea > QWidget > QWidget{background:transparent;}"
        "QScrollBar:vertical{background:transparent;width:16px;margin:4px;}"
        "QScrollBar::handle:vertical{background:#e6dccb;border-radius:6px;min-height:60px;}"
        "QScrollBar::handle:vertical:hover{background:#c9a227;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
        "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:transparent;}"));
    layout->addWidget(scroll, 1);

    auto *bar = new TimeBar(page);
    bar->setStyleSheet(timeBarStyle(false));
    bar->onLongPress = [this] { emit parentRequested(); };
    m_timeBar = bar;

    // A visible way out for the parent: the PIN prompt follows the click. The padlock says
    // "grown-up territory" to a child who cannot read the word yet.
    auto *parent = new QPushButton(kLock + QStringLiteral(" ") + tr("Parent"), page);
    parent->setAccessibleName(tr("Parent"));
    parent->setFixedSize(132, 40);
    parent->setCursor(Qt::PointingHandCursor);
    parent->setStyleSheet(QStringLiteral(
        "QPushButton{background:#f3ead9;color:#7a6f63;border:1px solid #e6dccb;"
        "border-radius:12px;font-size:15px;}"
        "QPushButton:pressed{background:#e6dccb;}"));
    connect(parent, &QPushButton::clicked, this, &LauncherWindow::parentRequested);

    auto *bottom = new QHBoxLayout;
    bottom->setSpacing(12);
    bottom->addWidget(parent);
    bottom->addSpacing(10);
    bottom->addWidget(m_timeBar, 1);
    layout->addLayout(bottom);

    m_pages->addWidget(page);
}

void LauncherWindow::buildLockPage()
{
    auto *page = new QWidget(m_pages);
    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    m_lockPages = new QStackedWidget(page);
    outer->addWidget(m_lockPages);

    auto *pinPage = new QWidget(m_lockPages);
    auto *layout = new QVBoxLayout(pinPage);
    layout->setContentsMargins(40, 40, 40, 40);
    layout->setSpacing(14);
    layout->addStretch();

    auto *title = new QLabel(tr("Time's up!"), pinPage);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("font-size:46px;font-weight:700;color:#c47a1a;"));
    layout->addWidget(title);

    auto *subtitle = new QLabel(tr("Ask a grown-up to unlock."), pinPage);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet(QStringLiteral("font-size:20px;color:#6b5f52;"));
    layout->addWidget(subtitle);

    m_pinDots = new QLabel(pinPage);
    m_pinDots->setAlignment(Qt::AlignCenter);
    m_pinDots->setStyleSheet(QStringLiteral("font-size:28px;color:#4a3f35;"));
    layout->addWidget(m_pinDots);

    m_pinMessage = new QLabel(pinPage);
    m_pinMessage->setAlignment(Qt::AlignCenter);
    m_pinMessage->setMinimumHeight(24);
    m_pinMessage->setStyleSheet(QStringLiteral("font-size:16px;color:#c47a1a;"));
    layout->addWidget(m_pinMessage);

    auto *keypad = new QGridLayout;
    keypad->setSpacing(10);
    keypad->setAlignment(Qt::AlignCenter);
    for (int digit = 1; digit <= 9; ++digit) {
        const QString text = QString::number(digit);
        auto *button = keypadButton(text, pinPage);
        connect(button, &QPushButton::clicked, this, [this, text] { appendPinDigit(text); });
        keypad->addWidget(button, (digit - 1) / 3, (digit - 1) % 3);
    }

    auto *clear = keypadButton(tr("Clear"), pinPage);
    connect(clear, &QPushButton::clicked, this, [this] {
        m_pin.clear();
        updateDots();
    });
    keypad->addWidget(clear, 3, 0);

    auto *zero = keypadButton(QStringLiteral("0"), pinPage);
    connect(zero, &QPushButton::clicked, this, [this] { appendPinDigit(QStringLiteral("0")); });
    keypad->addWidget(zero, 3, 1);

    auto *back = keypadButton(QStringLiteral("\u232b"), pinPage);
    connect(back, &QPushButton::clicked, this, [this] {
        m_pin.chop(1);
        updateDots();
    });
    keypad->addWidget(back, 3, 2);

    layout->addLayout(keypad);
    layout->addStretch();
    m_lockPages->addWidget(pinPage);

    auto *parentPage = new QWidget(m_lockPages);
    auto *parentLayout = new QVBoxLayout(parentPage);
    parentLayout->setContentsMargins(40, 40, 40, 40);
    parentLayout->setSpacing(18);
    parentLayout->addStretch();

    auto *parentTitle = new QLabel(tr("Parent"), parentPage);
    parentTitle->setAlignment(Qt::AlignCenter);
    parentTitle->setStyleSheet(QStringLiteral("font-size:38px;font-weight:700;color:#4a3f35;"));
    parentLayout->addWidget(parentTitle);

    auto *add = new QPushButton(tr("Add 15 minutes"), parentPage);
    add->setFixedSize(340, 76);
    add->setCursor(Qt::PointingHandCursor);
    add->setStyleSheet(QStringLiteral(
        "QPushButton{background:#6fae7c;color:#ffffff;border:0;border-radius:20px;"
        "font-size:22px;font-weight:600;}"
        "QPushButton:pressed{background:#5f9c6c;}"));
    connect(add, &QPushButton::clicked, this, &LauncherWindow::addTimeRequested);
    parentLayout->addWidget(add, 0, Qt::AlignCenter);

    auto *quit = new QPushButton(tr("Quit Kids Mode"), parentPage);
    quit->setFixedSize(340, 64);
    quit->setCursor(Qt::PointingHandCursor);
    quit->setStyleSheet(QStringLiteral(
        "QPushButton{background:#efe6d5;color:#4a3f35;border:2px solid #e6dccb;"
        "border-radius:20px;font-size:20px;}"
        "QPushButton:pressed{background:#e6dccb;}"));
    connect(quit, &QPushButton::clicked, this, &LauncherWindow::quitRequested);
    parentLayout->addWidget(quit, 0, Qt::AlignCenter);
    parentLayout->addStretch();

    m_lockPages->addWidget(parentPage);
    m_pages->addWidget(page);
    updateDots();
}

void LauncherWindow::buildShieldPage()
{
    auto *page = new QWidget(m_pages);
    page->setStyleSheet(QStringLiteral("background:#fdf6e3;"));
    m_pages->addWidget(page);
}

void LauncherWindow::setApps(const QList<AppTile> &apps)
{
    while (QLayoutItem *item = m_tileGrid->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    int index = 0;
    for (const AppTile &tile : apps) {
        const QFileInfo info(tile.exePath);
        const bool exists = info.isFile();

        auto *button = new QToolButton;
        button->setText(wrapName(tile.label()));
        button->setIcon(exists ? appIcon(tile)
                               : style()->standardIcon(QStyle::SP_FileIcon));
        button->setIconSize(QSize(112, 112));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setFixedSize(212, 204);
        button->setCursor(exists ? Qt::PointingHandCursor : Qt::ArrowCursor);
        button->setFocusPolicy(Qt::StrongFocus);
        button->setEnabled(exists);
        button->setToolTip(exists ? tr("%1\n%2").arg(tile.name, tile.exePath)
                                  : tr("%1 is missing").arg(tile.exePath));
        button->setStyleSheet(tileStyle());
        connect(button, &QToolButton::clicked, this, [this, tile] { emit appRequested(tile); });

        m_tileGrid->addWidget(button, index / 4, index % 4, Qt::AlignCenter);
        ++index;
    }

    if (m_tileGrid->count() > 0) {
        if (QWidget *first = m_tileGrid->itemAt(0)->widget())
            QTimer::singleShot(0, first, [first] { first->setFocus(); });
    }
}

void LauncherWindow::setRemaining(int remainingSeconds, int totalSeconds)
{
    if (!m_timeBar)
        return;

    const int total = qMax(totalSeconds, 1);
    const int minutes = (remainingSeconds + 59) / 60;
    m_timeBar->setRange(0, total);
    m_timeBar->setValue(qMin(remainingSeconds, total));
    m_timeBar->setFormat(tr("%1 min left").arg(minutes));
    m_timeBar->setAccessibleName(tr("%1 minutes left").arg(minutes));

    const bool low = remainingSeconds <= 300;
    if (low != m_lowTime) {
        m_lowTime = low;
        m_timeBar->setStyleSheet(timeBarStyle(low));
    }
}

void LauncherWindow::showTiles()
{
    m_pin.clear();
    m_pinMessage->clear();
    updateDots();
    m_pages->setCurrentIndex(0);
}

void LauncherWindow::showLock()
{
    m_pin.clear();
    m_pinMessage->clear();
    updateDots();
    m_lockPages->setCurrentIndex(0);
    m_pages->setCurrentIndex(1);
}

// The launcher never leaves the screen: while an app runs, or a parent dialog is open,
// this page is the desktop instead of the real one.
void LauncherWindow::showShield()
{
    m_pages->setCurrentIndex(2);
}

void LauncherWindow::showParentChoices()
{
    m_lockPages->setCurrentIndex(1);
}

void LauncherWindow::showPinError(const QString &message)
{
    m_pin.clear();
    updateDots();
    m_pinMessage->setText(message);
}

void LauncherWindow::appendPinDigit(const QString &digit)
{
    if (m_pin.size() >= 4)
        return;

    m_pin += digit;
    updateDots();

    if (m_pin.size() == 4) {
        const QString pin = m_pin;
        m_pin.clear();
        updateDots();
        emit pinSubmitted(pin);
    }
}

void LauncherWindow::updateDots()
{
    QString text;
    for (int i = 0; i < 4; ++i)
        text += (i < m_pin.size() ? QStringLiteral("\u25cf") : QStringLiteral("\u25cb"))
            + QLatin1Char(' ');
    m_pinDots->setText(text.trimmed());
}

// Topmost is a state, and Win32 can switch it in place. setWindowFlag() on a visible widget hides
// the window and builds a new one, which flashes the desktop for a frame — the exact thing the
// product promises never to show (backlog 004). So Qt's flags never change after construction and
// only WS_EX_TOPMOST moves.
void LauncherWindow::setTopmost(bool on)
{
    m_topmost = on;
    applyTopmost();
}

void LauncherWindow::applyTopmost()
{
    if (!isVisible())
        return; // showEvent() applies it once the window exists
    ::SetWindowPos(reinterpret_cast<HWND>(winId()), m_topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                   0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void LauncherWindow::setAssertFocus(bool on)
{
    m_assertFocus = on;
}

void LauncherWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    applyTopmost(); // also after a Win+D restore, where Qt may rebuild the window
}

bool LauncherWindow::event(QEvent *event)
{
    if (event->type() == QEvent::WindowDeactivate && m_assertFocus && isVisible()) {
        QTimer::singleShot(200, this, [this] {
            // One of our dialogs is up; raising the launcher would bury it.
            if (!m_assertFocus || !isVisible() || QApplication::activeModalWidget())
                return;
            if (isMinimized())
                showFullScreen(); // Win+D / "Show desktop"
            applyTopmost();
            raise();
            activateWindow();
        });
    }
    return QWidget::event(event);
}

// Alt+F4 is not a way out; a parent leaves with the PIN.
void LauncherWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
}
