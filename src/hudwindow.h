#pragma once

#include <QTimer>
#include <QWidget>

class QLabel;

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
    void enterEvent(QEnterEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void setDim(bool dim);

    QLabel *m_label = nullptr;
    QTimer m_idle; // dim again a few seconds after the last touch
    bool m_low = false;
};
