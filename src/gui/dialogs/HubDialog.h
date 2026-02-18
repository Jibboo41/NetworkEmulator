#pragma once
#include <QDialog>
#include "models/Device.h"

class QLineEdit;

class HubDialog : public QDialog
{
    Q_OBJECT
public:
    explicit HubDialog(Hub *hub, QWidget *parent = nullptr);
private slots:
    void accept() override;
private:
    Hub       *m_hub;
    QLineEdit *m_nameEdit = nullptr;
};
