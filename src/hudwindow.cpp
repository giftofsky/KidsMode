#include "hudwindow.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>

namespace {

// The strip is a way out, not the point of the screen, so it spends its life faded.
constexpr qreal kDimmed = 0.55;
constexpr int kAwakeMs = 4000;

QString barStyle(bool low)
{
    return QStringLiteral("QProgressBar{border:0;border-radius:4px;background:#4a423a;}"
                          "QProgressBar::chunk{border-radius:4px;background:%1;}")
        .arg(low ? QStringLiteral("#e09a2b") : QStringLiteral("#6fae7c"));
}

} // namespace

HudWindow::HudWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                   | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedSize(300, 44);
    setStyleSheet(QStringLiteral(
        "HudWindow{background:#2f2a24;}"
        "QLabel{color:#fdf6e3;font-size:17px;font-weight:700;}"
        "QPushButton{background:#4a423a;color:#fdf6e3;border:0;border-radius:10px;"
        "font-size:15px;padding:0 14px;}"
        "QPushButton:pressed{background:#5c5349;}"));

    // Faded, never hidden: a touchscreen has no hover, and this is the only way home. The mouse
    // entering or a press brings it to full strength; leaving fades it at once, and the timer
    // covers a cursor that parks on the strip and never leaves.
    m_idle.setSingleShot(true);
    m_idle.setInterval(kAwakeMs);
    connect(&m_idle, &QTimer::timeout, this, [this] { setDim(true); });
    setDim(true);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 6, 8, 6);
    layout->setSpacing(8);

    m_label = new QLabel(this);
    m_label->setMinimumWidth(58);

    // A child reads how much play is left from a bar running down, not from a number.
    m_bar = new QProgressBar(this);
    m_bar->setTextVisible(false);
    m_bar->setFixedHeight(8);
    m_bar->setStyleSheet(barStyle(false));

    auto *home = new QPushButton(tr("Home"), this);
    home->setFixedHeight(32);
    home->setCursor(Qt::PointingHandCursor);
    connect(home, &QPushButton::clicked, this, &HudWindow::homeRequested);

    layout->addWidget(m_label);
    layout->addWidget(m_bar, 1);
    layout->addWidget(home);

    setRemaining(0, 1);
}

void HudWindow::setRemaining(int remainingSeconds, int totalSeconds)
{
    const int total = qMax(totalSeconds, 1);
    m_bar->setRange(0, total);
    m_bar->setValue(qMin(remainingSeconds, total));
    m_label->setText(tr("%1 min").arg((remainingSeconds + 59) / 60));

    const bool low = remainingSeconds <= 300;
    if (low != m_low) {
        m_low = low;
        m_bar->setStyleSheet(barStyle(low));
    }
}

// One dim level for every state: the amber chunk already carries the warning, and a rule that
// pinned "low" to full opacity left the strip solid for a whole session on a short budget.
void HudWindow::setDim(bool dim)
{
    setWindowOpacity(dim ? kDimmed : 1.0);
    if (dim)
        m_idle.stop();
    else
        m_idle.start();
}

void HudWindow::enterEvent(QEnterEvent *event)
{
    setDim(false);
    QWidget::enterEvent(event);
}

void HudWindow::leaveEvent(QEvent *event)
{
    setDim(true);
    QWidget::leaveEvent(event);
}

void HudWindow::mousePressEvent(QMouseEvent *event)
{
    setDim(false);
    QWidget::mousePressEvent(event);
}

void HudWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect area = screen->availableGeometry();
        move(area.center().x() - width() / 2, area.bottom() - height() - 16);
    }
}
