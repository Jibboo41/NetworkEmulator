#include "gui/dialogs/RouterDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QPushButton>
#include <QStackedWidget>
#include <QListWidget>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QScrollArea>
#include <QNetworkInterface>
#include "utils/IpUtils.h"

RouterDialog::RouterDialog(Router *router, QWidget *parent)
    : QDialog(parent), m_router(router)
{
    setWindowTitle(QString("Configure Router – %1").arg(router->name()));
    setMinimumWidth(560);
    setMinimumHeight(480);

    auto *mainLayout = new QVBoxLayout(this);

    // Name field
    auto *nameForm = new QFormLayout;
    m_nameEdit = new QLineEdit(router->name());
    nameForm->addRow("Name:", m_nameEdit);
    mainLayout->addLayout(nameForm);

    auto *tabs = new QTabWidget;
    buildInterfacesTab(tabs);
    buildRoutingTab(tabs);
    buildHostPCTab(tabs);
    mainLayout->addWidget(tabs);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &RouterDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    populateFields();
}

void RouterDialog::buildInterfacesTab(QTabWidget *tabs)
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);

    auto *grid = new QGridLayout;
    grid->addWidget(new QLabel("<b>Interface</b>"), 0, 0);
    grid->addWidget(new QLabel("<b>IP Address</b>"), 0, 1);
    grid->addWidget(new QLabel("<b>Subnet Mask</b>"), 0, 2);
    grid->addWidget(new QLabel("<b>OSPF Cost</b>"), 0, 3);

    const auto &ifaces = m_router->interfaces();
    for (int i = 0; i < ifaces.size(); ++i) {
        grid->addWidget(new QLabel(ifaces[i].name), i + 1, 0);

        auto *ipEdit   = new QLineEdit;
        auto *maskEdit = new QLineEdit;
        auto *costEdit = new QLineEdit;
        costEdit->setFixedWidth(60);
        ipEdit->setPlaceholderText("e.g. 192.168.1.1");
        maskEdit->setPlaceholderText("e.g. 255.255.255.0");
        costEdit->setPlaceholderText("1");

        grid->addWidget(ipEdit,   i + 1, 1);
        grid->addWidget(maskEdit, i + 1, 2);
        grid->addWidget(costEdit, i + 1, 3);

        m_ifIpEdits.append(ipEdit);
        m_ifMaskEdits.append(maskEdit);
        m_ifCostEdits.append(costEdit);
    }

    layout->addLayout(grid);
    layout->addStretch();
    tabs->addTab(widget, "Interfaces");
}

void RouterDialog::buildRoutingTab(QTabWidget *tabs)
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);

    auto *protoForm = new QFormLayout;
    m_protocolBox   = new QComboBox;
    m_protocolBox->addItem("Static",        (int)Router::RoutingProtocol::Static);
    m_protocolBox->addItem("RIPv2",         (int)Router::RoutingProtocol::RIPv2);
    m_protocolBox->addItem("OSPF",          (int)Router::RoutingProtocol::OSPF);
    m_protocolBox->addItem("PIM Dense Mode",(int)Router::RoutingProtocol::PIM_DM);
    protoForm->addRow("Routing Protocol:", m_protocolBox);
    layout->addLayout(protoForm);
    connect(m_protocolBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RouterDialog::onProtocolChanged);

    m_protoStack = new QStackedWidget;

    // --- Page 0: Static ---
    auto *staticPage  = new QWidget;
    auto *staticLayout = new QVBoxLayout(staticPage);
    staticLayout->addWidget(new QLabel("Static Routes:"));
    m_staticTable = new QTableWidget(0, 4);
    m_staticTable->setHorizontalHeaderLabels({"Destination", "Mask", "Next Hop", "Metric"});
    m_staticTable->horizontalHeader()->setStretchLastSection(false);
    m_staticTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_staticTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    staticLayout->addWidget(m_staticTable);
    auto *staticBtnLayout = new QHBoxLayout;
    auto *addBtn    = new QPushButton("Add Route");
    auto *removeBtn = new QPushButton("Remove Route");
    staticBtnLayout->addWidget(addBtn);
    staticBtnLayout->addWidget(removeBtn);
    staticBtnLayout->addStretch();
    staticLayout->addLayout(staticBtnLayout);
    connect(addBtn,    &QPushButton::clicked, this, &RouterDialog::addStaticRoute);
    connect(removeBtn, &QPushButton::clicked, this, &RouterDialog::removeStaticRoute);
    m_protoStack->addWidget(staticPage);  // index 0

    // --- Page 1: RIPv2 ---
    auto *ripPage   = new QWidget;
    auto *ripLayout = new QVBoxLayout(ripPage);
    ripLayout->addWidget(new QLabel("Networks to advertise (one per line, e.g. 192.168.1.0):"));
    m_ripNetList = new QListWidget;
    m_ripNetList->setAlternatingRowColors(true);
    ripLayout->addWidget(m_ripNetList);
    auto *ripBtnLayout = new QHBoxLayout;
    auto *ripAddBtn = new QPushButton("Add");
    auto *ripDelBtn = new QPushButton("Remove");
    ripBtnLayout->addWidget(ripAddBtn);
    ripBtnLayout->addWidget(ripDelBtn);
    ripBtnLayout->addStretch();
    ripLayout->addLayout(ripBtnLayout);
    connect(ripAddBtn, &QPushButton::clicked, this, [this]() {
        auto *item = new QListWidgetItem("0.0.0.0");
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        m_ripNetList->addItem(item);
        m_ripNetList->editItem(item);
    });
    connect(ripDelBtn, &QPushButton::clicked, this, [this]() {
        qDeleteAll(m_ripNetList->selectedItems());
    });
    m_protoStack->addWidget(ripPage); // index 1

    // --- Page 2: OSPF ---
    auto *ospfPage   = new QWidget;
    auto *ospfLayout = new QFormLayout(ospfPage);
    m_ospfRidEdit  = new QLineEdit;  m_ospfRidEdit->setPlaceholderText("e.g. 1.1.1.1");
    m_ospfAreaEdit = new QLineEdit;  m_ospfAreaEdit->setPlaceholderText("0");
    m_ospfPidEdit  = new QLineEdit;  m_ospfPidEdit->setPlaceholderText("1");
    ospfLayout->addRow("Router ID:",  m_ospfRidEdit);
    ospfLayout->addRow("Area:",       m_ospfAreaEdit);
    ospfLayout->addRow("Process ID:", m_ospfPidEdit);
    m_protoStack->addWidget(ospfPage); // index 2

    // --- Page 3: PIM-DM ---
    auto *pimPage   = new QWidget;
    auto *pimLayout = new QVBoxLayout(pimPage);
    pimLayout->addWidget(new QLabel("Enable PIM-DM on interfaces:"));
    m_pimIfaceList = new QListWidget;
    for (const auto &iface : m_router->interfaces()) {
        auto *item = new QListWidgetItem(iface.name, m_pimIfaceList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(
            m_router->pimdmConfig().enabledInterfaces.contains(iface.name)
            ? Qt::Checked : Qt::Unchecked);
    }
    pimLayout->addWidget(m_pimIfaceList);
    m_protoStack->addWidget(pimPage); // index 3

    layout->addWidget(m_protoStack);
    tabs->addTab(widget, "Routing");
}

void RouterDialog::buildHostPCTab(QTabWidget *tabs)
{
    auto *page   = new QWidget;
    auto *layout = new QVBoxLayout(page);

    m_hostPCCheck = new QCheckBox(
        "Act as Host PC (bridge this router's interfaces to physical network adapters)");
    layout->addWidget(m_hostPCCheck);

    // Description label
    auto *descLabel = new QLabel(
        "When enabled, each virtual interface below can be mapped to a real network adapter\n"
        "on this machine. Traffic sent to that interface will travel over the physical adapter,\n"
        "allowing the emulated network to communicate with real routers, switches, and PCs.");
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("color: #555; font-size: 11px;");
    layout->addWidget(descLabel);

    // Mapping panel (shown/hidden based on checkbox)
    m_hostPCMappingWidget = new QWidget;
    auto *mappingLayout   = new QGridLayout(m_hostPCMappingWidget);
    mappingLayout->addWidget(new QLabel("<b>Virtual Interface</b>"), 0, 0);
    mappingLayout->addWidget(new QLabel("<b>Physical Adapter</b>"),  0, 1);

    // Collect all real host interfaces once
    QStringList hostIfaceNames;
    hostIfaceNames << "(none)";
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        // Skip loopback and interfaces that are definitely not useful
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack))
            continue;
        // Build a friendly display string: "eth0 (192.168.1.10)"
        QString display = iface.name();
        const auto addrs = iface.addressEntries();
        for (const auto &entry : addrs) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                display += QString(" (%1)").arg(entry.ip().toString());
                break;
            }
        }
        hostIfaceNames << display;
    }

    const auto &ifaces = m_router->interfaces();
    for (int i = 0; i < ifaces.size(); ++i) {
        mappingLayout->addWidget(new QLabel(ifaces[i].name), i + 1, 0);

        auto *combo = new QComboBox;
        combo->addItems(hostIfaceNames);
        // Pre-select if a mapping already exists
        const QString &existing = ifaces[i].hostInterfaceName;
        if (!existing.isEmpty()) {
            // Match by interface name prefix (the part before " (")
            for (int j = 1; j < combo->count(); ++j) {
                if (combo->itemText(j).startsWith(existing)) {
                    combo->setCurrentIndex(j);
                    break;
                }
            }
        }
        mappingLayout->addWidget(combo, i + 1, 1);
        m_ifHostComboBoxes.append(combo);
    }

    layout->addWidget(m_hostPCMappingWidget);
    layout->addStretch();

    // Initialize visibility
    m_hostPCMappingWidget->setEnabled(m_router->isHostPC());
    m_hostPCCheck->setChecked(m_router->isHostPC());

    connect(m_hostPCCheck, &QCheckBox::toggled, this, &RouterDialog::onHostPCToggled);

    tabs->addTab(page, "Host PC");
}

void RouterDialog::onHostPCToggled(bool checked)
{
    m_hostPCMappingWidget->setEnabled(checked);
}

void RouterDialog::populateFields()
{
    const auto &ifaces = m_router->interfaces();
    for (int i = 0; i < ifaces.size() && i < m_ifIpEdits.size(); ++i) {
        m_ifIpEdits[i]->setText(ifaces[i].ipAddress);
        m_ifMaskEdits[i]->setText(ifaces[i].subnetMask);
        m_ifCostEdits[i]->setText(ifaces[i].ospfCost > 0
                                  ? QString::number(ifaces[i].ospfCost) : QString());
    }

    // Protocol selector
    const int protoIdx = static_cast<int>(m_router->routingProtocol());
    m_protocolBox->setCurrentIndex(protoIdx);
    m_protoStack->setCurrentIndex(protoIdx);

    // Static routes
    for (const auto &sr : m_router->staticRoutes()) {
        const int row = m_staticTable->rowCount();
        m_staticTable->insertRow(row);
        m_staticTable->setItem(row, 0, new QTableWidgetItem(sr.destination));
        m_staticTable->setItem(row, 1, new QTableWidgetItem(sr.mask));
        m_staticTable->setItem(row, 2, new QTableWidgetItem(sr.nextHop));
        m_staticTable->setItem(row, 3, new QTableWidgetItem(QString::number(sr.metric)));
    }

    // RIPv2 networks
    for (const auto &net : m_router->ripv2Config().networks) {
        auto *item = new QListWidgetItem(net);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        m_ripNetList->addItem(item);
    }

    // OSPF
    m_ospfRidEdit->setText(m_router->ospfConfig().routerId);
    m_ospfAreaEdit->setText(m_router->ospfConfig().area);
    m_ospfPidEdit->setText(QString::number(m_router->ospfConfig().processId));
}

void RouterDialog::onProtocolChanged(int index)
{
    m_protoStack->setCurrentIndex(index);
}

void RouterDialog::addStaticRoute()
{
    const int row = m_staticTable->rowCount();
    m_staticTable->insertRow(row);
    m_staticTable->setItem(row, 0, new QTableWidgetItem("0.0.0.0"));
    m_staticTable->setItem(row, 1, new QTableWidgetItem("0.0.0.0"));
    m_staticTable->setItem(row, 2, new QTableWidgetItem(""));
    m_staticTable->setItem(row, 3, new QTableWidgetItem("1"));
    m_staticTable->scrollToBottom();
    m_staticTable->editItem(m_staticTable->item(row, 0));
}

void RouterDialog::removeStaticRoute()
{
    const auto selected = m_staticTable->selectedItems();
    if (!selected.isEmpty())
        m_staticTable->removeRow(m_staticTable->currentRow());
}

void RouterDialog::accept()
{
    if (!validateAndApply()) return;
    QDialog::accept();
}

bool RouterDialog::validateAndApply()
{
    // Validate IP addresses
    const auto &ifaces = m_router->interfaces();
    for (int i = 0; i < ifaces.size() && i < m_ifIpEdits.size(); ++i) {
        const QString ip   = m_ifIpEdits[i]->text().trimmed();
        const QString mask = m_ifMaskEdits[i]->text().trimmed();
        if (!ip.isEmpty() && !IpUtils::isValidIp(ip)) {
            QMessageBox::warning(this, "Invalid Input",
                QString("Invalid IP address on %1: %2").arg(ifaces[i].name, ip));
            return false;
        }
        if (!mask.isEmpty() && !IpUtils::isValidMask(mask)) {
            QMessageBox::warning(this, "Invalid Input",
                QString("Invalid subnet mask on %1: %2").arg(ifaces[i].name, mask));
            return false;
        }
    }

    // Apply
    m_router->setName(m_nameEdit->text().trimmed());

    auto &ifaceList = m_router->interfaces();
    for (int i = 0; i < ifaceList.size() && i < m_ifIpEdits.size(); ++i) {
        ifaceList[i].ipAddress  = m_ifIpEdits[i]->text().trimmed();
        ifaceList[i].subnetMask = m_ifMaskEdits[i]->text().trimmed();
        const QString costStr = m_ifCostEdits[i]->text().trimmed();
        ifaceList[i].ospfCost = costStr.isEmpty() ? 1 : costStr.toInt();
    }

    const int protoIdx = m_protocolBox->currentIndex();
    m_router->setRoutingProtocol(static_cast<Router::RoutingProtocol>(protoIdx));

    // Static routes
    m_router->staticRoutes().clear();
    for (int row = 0; row < m_staticTable->rowCount(); ++row) {
        Router::StaticRoute sr;
        sr.destination = m_staticTable->item(row, 0) ? m_staticTable->item(row, 0)->text() : QString();
        sr.mask        = m_staticTable->item(row, 1) ? m_staticTable->item(row, 1)->text() : QString();
        sr.nextHop     = m_staticTable->item(row, 2) ? m_staticTable->item(row, 2)->text() : QString();
        sr.metric      = m_staticTable->item(row, 3) ? m_staticTable->item(row, 3)->text().toInt() : 1;
        if (!sr.destination.isEmpty())
            m_router->staticRoutes().append(sr);
    }

    // RIPv2 networks
    m_router->ripv2Config().networks.clear();
    for (int i = 0; i < m_ripNetList->count(); ++i)
        m_router->ripv2Config().networks.append(m_ripNetList->item(i)->text());

    // OSPF
    m_router->ospfConfig().routerId  = m_ospfRidEdit->text().trimmed();
    m_router->ospfConfig().area      = m_ospfAreaEdit->text().trimmed().isEmpty()
                                       ? "0" : m_ospfAreaEdit->text().trimmed();
    m_router->ospfConfig().processId = m_ospfPidEdit->text().toInt();

    // PIM-DM enabled interfaces
    m_router->pimdmConfig().enabledInterfaces.clear();
    for (int i = 0; i < m_pimIfaceList->count(); ++i) {
        if (m_pimIfaceList->item(i)->checkState() == Qt::Checked)
            m_router->pimdmConfig().enabledInterfaces.append(m_pimIfaceList->item(i)->text());
    }

    // Host PC
    m_router->setIsHostPC(m_hostPCCheck->isChecked());
    auto &ifaceList2 = m_router->interfaces();
    for (int i = 0; i < ifaceList2.size() && i < m_ifHostComboBoxes.size(); ++i) {
        const QString selected = m_ifHostComboBoxes[i]->currentText();
        if (selected == "(none)") {
            ifaceList2[i].hostInterfaceName.clear();
        } else {
            // Store just the raw interface name (strip the " (IP)" suffix if present)
            const int spaceIdx = selected.indexOf(' ');
            ifaceList2[i].hostInterfaceName = (spaceIdx > 0)
                ? selected.left(spaceIdx) : selected;
        }
    }

    return true;
}
