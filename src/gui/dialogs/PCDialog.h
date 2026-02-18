#pragma once
#include <QDialog>
#include "models/Device.h"

class QLineEdit;

class PCDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PCDialog(PC *pc, QWidget *parent = nullptr);
private slots:
    void accept() override;
private:
    PC        *m_pc;
    QLineEdit *m_nameEdit    = nullptr;
    QLineEdit *m_ipEdit      = nullptr;
    QLineEdit *m_maskEdit    = nullptr;
    QLineEdit *m_gatewayEdit = nullptr;
};
