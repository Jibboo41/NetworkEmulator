#include "gui/dialogs/HubDialog.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QDialogButtonBox>

HubDialog::HubDialog(Hub *hub, QWidget *parent)
    : QDialog(parent), m_hub(hub)
{
    setWindowTitle(QString("Configure Hub – %1").arg(hub->name()));
    auto *layout = new QVBoxLayout(this);

    auto *form = new QFormLayout;
    m_nameEdit = new QLineEdit(hub->name());
    form->addRow("Name:", m_nameEdit);
    form->addRow(new QLabel("Ports: 4 × Port0 – Port3  (Layer 1 only, broadcasts all traffic)"));
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &HubDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void HubDialog::accept()
{
    m_hub->setName(m_nameEdit->text().trimmed());
    QDialog::accept();
}
