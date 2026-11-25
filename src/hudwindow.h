#pragma once

#include <QWidget>

class QLabel;
class QProgressBar;

class HudWindow : public QWidget
{
    Q_OBJECT
public:
    explicit HudWindow(QWidget *parent = nullptr);

    void setRemaining(int remainingSeconds, int totalSeconds);

signals:
    void homeRequested();

protected:
    void showEvent(QShowEvent *event) override;

private:
    QLabel *m_label = nullptr;
    QProgressBar *m_bar = nullptr;
    bool m_low = false;
};
