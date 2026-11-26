#include "hudwindow.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>

namespace {

// The strip is a way out, not the point of the screen, so it spends its life faded.
constexpr qreal kDimmed = 0.55;
constexpr int kAwakeMs = 4000;

QString labelStyle(bool low)
{
    return QStringLiteral("color:%1;font-size:17px;font-weight:700;")
        .arg(low ? QStringLiteral("#e09a2b") : QStringLiteral("#fdf6e3"));
}

} // namespace

HudWindow::HudWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                   | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedSize(230, 44);
    setStyleSheet(QStringLiteral(
        "HudWindow{background:#2f2a24;}"
        "QPushButton{background:#4a423a;color:#fdf6e3;border:0;border-radius:10px;"
        "font-size:15px;padding:0 16px;}"
        "QPushButton:pressed{background:#5c5349;}"));

    // Faded, never hidden: a touchscreen has no hover, and this is the only way home. The mouse
    // entering or a press brings it to full strength; it fades back once things have been idle.
    m_idle.setSingleShot(true);
    m_idle.setInterval(kAwakeMs);
    connect(&m_idle, &QTimer::timeout, this, [this] { setDim(true); });

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 6, 8, 6);

    m_label = new QLabel(this);
    m_label->setMinimumWidth(64);

    auto *home = new QPushButton(tr("Home"), this);
    home->setFixedHeight(32);
    home->setCursor(Qt::PointingHandCursor);
    connect(home, &QPushButton::clicked, this, &HudWindow::homeRequested);

    layout->addWidget(m_label);
    layout->addStretch(1);
    layout->addWidget(home);

    setRemaining(0, 1);
}

void HudWindow::setRemaining(int remainingSeconds, int totalSeconds)
{
    const int total = qMax(totalSeconds, 1);
    m_label->setText(tr("%1 min").arg((remainingSeconds + 59) / 60));

    const bool low = remainingSeconds <= 300;
    if (low != m_low) {
        m_low = low;
        m_label->setStyleSheet(labelStyle(low));
        setDim(!low);
    }
}

void HudWindow::setDim(bool dim)
{
    setWindowOpacity(dim && !m_low ? kDimmed : 1.0); // a warning is never faded
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
