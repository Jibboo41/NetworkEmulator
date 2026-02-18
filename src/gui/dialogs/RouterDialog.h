#pragma once
#include <QDialog>
#include "models/Device.h"

class QLineEdit;
class QComboBox;
class QTabWidget;
class QTableWidget;
class QStackedWidget;
class QListWidget;

class RouterDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RouterDialog(Router *router, QWidget *parent = nullptr);

private slots:
    void onProtocolChanged(int index);
    void addStaticRoute();
    void removeStaticRoute();
    void accept() override;

private:
    void buildInterfacesTab(QTabWidget *tabs);
    void buildRoutingTab(QTabWidget *tabs);
    void populateFields();
    bool validateAndApply();

    Router *m_router;

    // General
    QLineEdit *m_nameEdit = nullptr;

    // Interfaces tab  (one row per interface)
    QList<QLineEdit *> m_ifIpEdits;
    QList<QLineEdit *> m_ifMaskEdits;
    QList<QLineEdit *> m_ifCostEdits;

    // Routing tab
    QComboBox    *m_protocolBox   = nullptr;
    QStackedWidget *m_protoStack  = nullptr;

    // Static sub-panel
    QTableWidget *m_staticTable   = nullptr;

    // RIPv2 sub-panel
    QListWidget  *m_ripNetList    = nullptr;

    // OSPF sub-panel
    QLineEdit    *m_ospfRidEdit   = nullptr;
    QLineEdit    *m_ospfAreaEdit  = nullptr;
    QLineEdit    *m_ospfPidEdit   = nullptr;

    // PIM-DM sub-panel
    QListWidget  *m_pimIfaceList  = nullptr;
};
