#include "gui/dialogs/SwitchDialog.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QDialogButtonBox>

SwitchDialog::SwitchDialog(Switch *sw, QWidget *parent)
    : QDialog(parent), m_switch(sw)
{
    setWindowTitle(QString("Configure Switch – %1").arg(sw->name()));
    auto *layout = new QVBoxLayout(this);

    auto *form = new QFormLayout;
    m_nameEdit = new QLineEdit(sw->name());
    form->addRow("Name:", m_nameEdit);
    form->addRow(new QLabel("Ports: 8 × Fa0/0 – Fa0/7  (Layer 2 only, no IP configuration)"));
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &SwitchDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SwitchDialog::accept()
{
    m_switch->setName(m_nameEdit->text().trimmed());
    QDialog::accept();
}
