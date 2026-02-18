#pragma once
#include <QDialog>
#include "models/Device.h"

class QLineEdit;

class SwitchDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SwitchDialog(Switch *sw, QWidget *parent = nullptr);
private slots:
    void accept() override;
private:
    Switch    *m_switch;
    QLineEdit *m_nameEdit = nullptr;
};
