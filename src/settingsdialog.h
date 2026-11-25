#pragma once

#include <QDialog>

#include "policy.h"

class QLineEdit;
class QListWidget;
class QSpinBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    SettingsDialog(const Policy &policy, bool setupMode, QWidget *parent = nullptr);

    Policy policy() const { return m_policy; }

protected:
    void accept() override;

private:
    void addApp();
    void addFromStartMenu();
    void removeApp();
    void syncAppsToList();

    Policy m_policy;
    bool m_setupMode;
    QSpinBox *m_budget = nullptr;
    QListWidget *m_apps = nullptr;
    QLineEdit *m_pin = nullptr;
    QLineEdit *m_pinConfirm = nullptr;
};
