#include "gui/dialogs/PCDialog.h"
#include "utils/IpUtils.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QMessageBox>

PCDialog::PCDialog(PC *pc, QWidget *parent)
    : QDialog(parent), m_pc(pc)
{
    setWindowTitle(QString("Configure PC – %1").arg(pc->name()));
    auto *layout = new QVBoxLayout(this);

    auto *form = new QFormLayout;
    m_nameEdit    = new QLineEdit(pc->name());
    m_ipEdit      = new QLineEdit(pc->ipAddress());
    m_maskEdit    = new QLineEdit(pc->subnetMask());
    m_gatewayEdit = new QLineEdit(pc->defaultGateway());

    m_ipEdit->setPlaceholderText("e.g. 192.168.1.10");
    m_maskEdit->setPlaceholderText("e.g. 255.255.255.0");
    m_gatewayEdit->setPlaceholderText("e.g. 192.168.1.1");

    form->addRow("Name:",            m_nameEdit);
    form->addRow("IP Address:",      m_ipEdit);
    form->addRow("Subnet Mask:",     m_maskEdit);
    form->addRow("Default Gateway:", m_gatewayEdit);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &PCDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void PCDialog::accept()
{
    const QString ip      = m_ipEdit->text().trimmed();
    const QString mask    = m_maskEdit->text().trimmed();
    const QString gateway = m_gatewayEdit->text().trimmed();

    if (!ip.isEmpty() && !IpUtils::isValidIp(ip)) {
        QMessageBox::warning(this, "Invalid Input", "Invalid IP address: " + ip);
        return;
    }
    if (!mask.isEmpty() && !IpUtils::isValidMask(mask)) {
        QMessageBox::warning(this, "Invalid Input", "Invalid subnet mask: " + mask);
        return;
    }
    if (!gateway.isEmpty() && !IpUtils::isValidIp(gateway)) {
        QMessageBox::warning(this, "Invalid Input", "Invalid gateway address: " + gateway);
        return;
    }

    m_pc->setName(m_nameEdit->text().trimmed());
    if (!m_pc->interfaces().isEmpty()) {
        m_pc->interfaces()[0].ipAddress  = ip;
        m_pc->interfaces()[0].subnetMask = mask;
    }
    m_pc->setDefaultGateway(gateway);
    QDialog::accept();
}
