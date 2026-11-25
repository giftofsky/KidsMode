#pragma once

#include <QList>
#include <QWidget>

#include "policy.h"

class QGridLayout;
class QLabel;
class QProgressBar;
class QStackedWidget;

class LauncherWindow : public QWidget
{
    Q_OBJECT
public:
    explicit LauncherWindow(QWidget *parent = nullptr);

    void setApps(const QList<AppTile> &apps);
    void setRemaining(int remainingSeconds, int totalSeconds);

    void showTiles();
    void showLock();
    void showShield();
    void showParentChoices();
    void showPinError(const QString &message);

    void setTopmost(bool on);
    void setAssertFocus(bool on);

signals:
    void appRequested(const AppTile &tile);
    void parentRequested();
    void pinSubmitted(const QString &pin);
    void addTimeRequested();
    void quitRequested();

protected:
    bool event(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void buildTilesPage();
    void buildLockPage();
    void buildShieldPage();
    void appendPinDigit(const QString &digit);
    void updateDots();

    QStackedWidget *m_pages = nullptr;
    QStackedWidget *m_lockPages = nullptr;
    QGridLayout *m_tileGrid = nullptr;
    QProgressBar *m_timeBar = nullptr;
    QLabel *m_pinDots = nullptr;
    QLabel *m_pinMessage = nullptr;
    QString m_pin;
    bool m_assertFocus = false;
    bool m_lowTime = false;
};
