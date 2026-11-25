#include "hudwindow.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>

namespace {

QString barStyle(bool low)
{
    return QStringLiteral("QProgressBar{border:0;border-radius:5px;background:#4a423a;}"
                          "QProgressBar::chunk{border-radius:5px;background:%1;}")
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
    setFixedSize(460, 64);
    setStyleSheet(QStringLiteral(
        "HudWindow{background:#2f2a24;}"
        "QLabel{color:#fdf6e3;font-size:16px;font-weight:600;}"
        "QPushButton{background:#4a423a;color:#fdf6e3;border:0;border-radius:10px;"
        "font-size:15px;padding:0 18px;}"
        "QPushButton:pressed{background:#5c5349;}"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(18, 10, 12, 10);

    m_label = new QLabel(this);
    m_label->setMinimumWidth(80);

    m_bar = new QProgressBar(this);
    m_bar->setTextVisible(false);
    m_bar->setFixedHeight(10);
    m_bar->setStyleSheet(barStyle(false));

    auto *home = new QPushButton(tr("Home"), this);
    home->setFixedHeight(40);
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

void HudWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect area = screen->availableGeometry();
        move(area.center().x() - width() / 2, area.bottom() - height() - 16);
    }
}
