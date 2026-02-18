#include "gui/MainWindow.h"
#include "gui/NetworkCanvas.h"
#include "models/Network.h"
#include "routing/RoutingEngine.h"
#include "validation/Validator.h"
#include <QUuid>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QAction>
#include <QActionGroup>
#include <QDockWidget>
#include <QTextEdit>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QCloseEvent>
#include <QApplication>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    m_network = new Network(this);
    connect(m_network, &Network::modified, this, [this]() {
        m_modified = true;
        updateTitle();
    });

    m_canvas = new NetworkCanvas(m_network, this);
    setCentralWidget(m_canvas);
    connect(m_canvas, &NetworkCanvas::statusMessage, this, &MainWindow::onStatusMessage);

    setupMenuBar();
    setupToolBar();
    setupDockWidgets();

    m_statusLabel = new QLabel("Ready");
    statusBar()->addWidget(m_statusLabel);

    setMinimumSize(1024, 700);
    updateTitle();
}

MainWindow::~MainWindow() {}

// ---------------------------------------------------------------------------
void MainWindow::setupMenuBar()
{
    // File
    QMenu *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&New",              QKeySequence::New,    this, &MainWindow::newNetwork);
    fileMenu->addAction("&Open...",          QKeySequence::Open,   this, &MainWindow::openNetwork);
    fileMenu->addAction("Load &Sample Network", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N),
                        this, &MainWindow::createSampleNetwork);
    fileMenu->addSeparator();
    fileMenu->addAction("&Save",             QKeySequence::Save,   this, &MainWindow::saveNetwork);
    fileMenu->addAction("Save &As...",       QKeySequence::SaveAs, this, &MainWindow::saveNetworkAs);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit",             QKeySequence::Quit,   qApp, &QApplication::quit);

    // Simulate
    QMenu *simMenu = menuBar()->addMenu("&Simulate");
    simMenu->addAction("&Run Simulation",         this, &MainWindow::runSimulation);
    simMenu->addAction("Run with &PIM-DM Tree...",this, &MainWindow::runSimulationWithPim);

    // Validate
    QMenu *valMenu = menuBar()->addMenu("&Validate");
    valMenu->addAction("&Validate Network", QKeySequence(Qt::Key_F5), this, &MainWindow::validateNetwork);

    // Help
    QMenu *helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&About", this, &MainWindow::showAbout);
}

void MainWindow::setupToolBar()
{
    m_modeGroup = new QActionGroup(this);

    QToolBar *tb = addToolBar("Main Toolbar");
    tb->setIconSize(QSize(28, 28));

    auto makeAction = [&](const QString &text, const QString &tip,
                          bool checkable = false) -> QAction * {
        auto *a = new QAction(text, this);
        a->setToolTip(tip);
        a->setCheckable(checkable);
        if (checkable) m_modeGroup->addAction(a);
        tb->addAction(a);
        return a;
    };

    // Mode buttons
    auto *selAction = makeAction("↖ Select",  "Select / move devices (S)", true);
    selAction->setChecked(true);
    connect(selAction, &QAction::triggered, this, &MainWindow::setModeSelect);

    auto *conAction = makeAction("⛓ Connect", "Connect two devices (C)", true);
    connect(conAction, &QAction::triggered, this, &MainWindow::setModeConnect);

    auto *delAction = makeAction("✕ Delete",  "Delete device or link (Del)", true);
    connect(delAction, &QAction::triggered, this, &MainWindow::setModeDelete);

    tb->addSeparator();

    // Device placement buttons
    auto *rAction  = makeAction("⊙ Router", "Place Router (R)", true);
    connect(rAction,  &QAction::triggered, this, &MainWindow::setModePlaceRouter);

    auto *swAction = makeAction("▣ Switch", "Place Switch (W)", true);
    connect(swAction, &QAction::triggered, this, &MainWindow::setModePlaceSwitch);

    auto *hAction  = makeAction("◇ Hub",   "Place Hub (H)", true);
    connect(hAction,  &QAction::triggered, this, &MainWindow::setModePlaceHub);

    auto *pcAction = makeAction("▭ PC",    "Place PC (P)", true);
    connect(pcAction, &QAction::triggered, this, &MainWindow::setModePlacePC);

    tb->addSeparator();

    // Simulation / validation
    auto *simAction = makeAction("▶ Simulate", "Run routing simulation (F6)");
    simAction->setShortcut(Qt::Key_F6);
    connect(simAction, &QAction::triggered, this, &MainWindow::runSimulation);

    auto *valAction = makeAction("✔ Validate", "Validate network (F5)");
    valAction->setShortcut(Qt::Key_F5);
    connect(valAction, &QAction::triggered, this, &MainWindow::validateNetwork);
}

void MainWindow::setupDockWidgets()
{
    m_resultsDock = new QDockWidget("Simulation / Validation Results", this);
    m_resultsDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);

    m_resultsView = new QTextEdit;
    m_resultsView->setReadOnly(true);
    m_resultsView->setFont(QFont("Courier New", 9));
    m_resultsView->setPlaceholderText(
        "Routing tables and validation results will appear here.\n"
        "Use Simulate (F6) or Validate (F5).");
    m_resultsDock->setWidget(m_resultsView);
    addDockWidget(Qt::BottomDockWidgetArea, m_resultsDock);
    m_resultsDock->hide();
}

// ---------------------------------------------------------------------------
void MainWindow::updateTitle()
{
    const QString base = m_currentFile.isEmpty() ? "Untitled" : m_currentFile;
    setWindowTitle(QString("Network Emulator — %1%2").arg(base, m_modified ? " *" : ""));
}

void MainWindow::onStatusMessage(const QString &msg)
{
    m_statusLabel->setText(msg);
}

// ---------------------------------------------------------------------------
// Mode slots
// ---------------------------------------------------------------------------
void MainWindow::setModeSelect()      { m_canvas->setMode(NetworkCanvas::Mode::Select); }
void MainWindow::setModePlaceRouter() { m_canvas->setMode(NetworkCanvas::Mode::PlaceRouter); }
void MainWindow::setModePlaceSwitch() { m_canvas->setMode(NetworkCanvas::Mode::PlaceSwitch); }
void MainWindow::setModePlaceHub()    { m_canvas->setMode(NetworkCanvas::Mode::PlaceHub); }
void MainWindow::setModePlacePC()     { m_canvas->setMode(NetworkCanvas::Mode::PlacePC); }
void MainWindow::setModeConnect()     { m_canvas->setMode(NetworkCanvas::Mode::Connect); }
void MainWindow::setModeDelete()      { m_canvas->setMode(NetworkCanvas::Mode::Delete); }

// ---------------------------------------------------------------------------
// File operations
// ---------------------------------------------------------------------------
bool MainWindow::confirmDiscardChanges()
{
    if (!m_modified) return true;
    const auto btn = QMessageBox::question(
        this, "Unsaved Changes",
        "You have unsaved changes. Discard them?",
        QMessageBox::Discard | QMessageBox::Cancel);
    return btn == QMessageBox::Discard;
}

void MainWindow::newNetwork()
{
    if (!confirmDiscardChanges()) return;
    m_canvas->clear();
    m_network->clear();
    m_currentFile.clear();
    m_modified = false;
    updateTitle();
    onStatusMessage("New network created.");
}

void MainWindow::openNetwork()
{
    if (!confirmDiscardChanges()) return;
    const QString path = QFileDialog::getOpenFileName(
        this, "Open Network", {}, "Network Files (*.net);;All Files (*)");
    if (path.isEmpty()) return;

    QString error;
    if (!m_network->load(path, &error)) {
        QMessageBox::critical(this, "Open Failed", error);
        return;
    }
    m_canvas->rebuildFromNetwork();
    m_currentFile = path;
    m_modified    = false;
    updateTitle();
    onStatusMessage("Network loaded.");
}

bool MainWindow::saveNetwork()
{
    if (m_currentFile.isEmpty()) return saveNetworkAs();
    QString error;
    if (!m_network->save(m_currentFile, &error)) {
        QMessageBox::critical(this, "Save Failed", error);
        return false;
    }
    m_modified = false;
    updateTitle();
    return true;
}

bool MainWindow::saveNetworkAs()
{
    const QString path = QFileDialog::getSaveFileName(
        this, "Save Network", {}, "Network Files (*.net);;All Files (*)");
    if (path.isEmpty()) return false;
    m_currentFile = path;
    return saveNetwork();
}

// ---------------------------------------------------------------------------
// Simulation
// ---------------------------------------------------------------------------
void MainWindow::runSimulation()
{
    const SimulationResult result = RoutingEngine::run(m_network);

    QString html;
    html += "<html><body style='font-family:Courier New;font-size:9pt'>";
    html += "<h3>Routing Simulation Results</h3>";

    for (const RouterSimResult &rr : result.routerResults) {
        html += QString("<b>%1</b>  [%2]<br>").arg(rr.routerName.toHtmlEscaped(),
                                                    rr.protocol.toHtmlEscaped());
        html += "<table border='1' cellspacing='0' cellpadding='3' style='border-collapse:collapse'>";
        html += "<tr style='background:#dde'>"
                "<th>Destination</th><th>Mask</th><th>Next Hop</th>"
                "<th>Interface</th><th>Metric</th><th>Protocol</th></tr>";

        if (rr.routingTable.isEmpty()) {
            html += "<tr><td colspan='6'><i>No routes</i></td></tr>";
        } else {
            for (const RoutingEntry &e : rr.routingTable) {
                html += QString("<tr><td>%1</td><td>%2</td><td>%3</td>"
                                "<td>%4</td><td>%5</td><td>%6</td></tr>")
                            .arg(e.destination, e.mask, e.nextHop,
                                 e.exitInterface, QString::number(e.metric), e.protocol);
            }
        }
        html += "</table><br>";
    }

    html += "</body></html>";
    m_resultsView->setHtml(html);
    m_resultsDock->show();
    onStatusMessage("Simulation complete.");
}

void MainWindow::runSimulationWithPim()
{
    bool ok;
    const QString src = QInputDialog::getText(
        this, "PIM-DM Source", "Multicast source IP address:", QLineEdit::Normal, {}, &ok);
    if (!ok || src.isEmpty()) return;

    const QString grp = QInputDialog::getText(
        this, "PIM-DM Group", "Multicast group address (e.g. 239.1.1.1):",
        QLineEdit::Normal, "239.1.1.1", &ok);
    if (!ok || grp.isEmpty()) return;

    SimulationResult result = RoutingEngine::run(m_network, src, grp);

    // Show unicast tables first
    runSimulation(); // re-populates view

    QString html = m_resultsView->toHtml();
    html.replace("</body>", ""); // we'll append

    html += "<h3>PIM Dense Mode — Multicast Distribution Tree</h3>";
    for (const MulticastTree &tree : result.multicastTrees) {
        html += QString("<b>Source:</b> %1 &nbsp; <b>Group:</b> %2<br>")
                    .arg(tree.sourceIp, tree.groupAddress);
        html += "<table border='1' cellspacing='0' cellpadding='3' style='border-collapse:collapse'>";
        html += "<tr style='background:#ded'>"
                "<th>Router</th><th>RPF (Incoming)</th><th>OIL (Outgoing)</th></tr>";
        for (const MulticastTreeEntry &e : tree.entries) {
            html += QString("<tr><td>%1</td><td>%2</td><td>%3</td></tr>")
                        .arg(e.routerName,
                             e.incomingInterface.isEmpty() ? "<i>source</i>" : e.incomingInterface,
                             e.outgoingInterfaces.join(", "));
        }
        html += "</table>";
        if (!tree.pruned.isEmpty())
            html += QString("<br><b>Pruned:</b> %1<br>").arg(tree.pruned.join(", "));
        html += "<br>";
    }
    html += "</body></html>";
    m_resultsView->setHtml(html);
    m_resultsDock->show();
}

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------
void MainWindow::validateNetwork()
{
    const QList<ValidationIssue> issues = Validator::validate(m_network);

    QString html;
    html += "<html><body style='font-family:Courier New;font-size:9pt'>";
    html += "<h3>Validation Report</h3>";

    if (issues.isEmpty()) {
        html += "<p style='color:green'><b>✔ No issues found. Network configuration appears valid.</b></p>";
    } else {
        int errors = 0, warnings = 0;
        for (const auto &issue : issues) {
            if (issue.severity == ValidationIssue::Severity::Error)   ++errors;
            if (issue.severity == ValidationIssue::Severity::Warning) ++warnings;
        }
        html += QString("<p>Found <b>%1 error(s)</b>, <b>%2 warning(s)</b>.</p>")
                    .arg(errors).arg(warnings);

        for (const auto &issue : issues) {
            QString color;
            switch (issue.severity) {
                case ValidationIssue::Severity::Error:   color = "#c00"; break;
                case ValidationIssue::Severity::Warning: color = "#a60"; break;
                case ValidationIssue::Severity::Info:    color = "#006"; break;
            }
            html += QString("<p style='color:%1'><b>[%2]</b> %3</p>")
                        .arg(color, issue.severityString(),
                             issue.message.toHtmlEscaped());
        }
    }
    html += "</body></html>";
    m_resultsView->setHtml(html);
    m_resultsDock->show();
    onStatusMessage(issues.isEmpty() ? "Validation passed." :
                    QString("Validation: %1 issue(s) found.").arg(issues.size()));
}

// ---------------------------------------------------------------------------
void MainWindow::showAbout()
{
    QMessageBox::about(this, "About Network Emulator",
        "<h3>Network Emulator v1.0</h3>"
        "<p>A desktop tool for designing and validating network topologies.</p>"
        "<p>Supported protocols: Static routing, RIPv2, OSPF, PIM Dense Mode.</p>"
        "<p>Built with Qt6 and C++17.</p>");
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (confirmDiscardChanges())
        event->accept();
    else
        event->ignore();
}

// ---------------------------------------------------------------------------
// Sample Network
// ---------------------------------------------------------------------------
void MainWindow::createSampleNetwork()
{
    if (!confirmDiscardChanges()) return;
    m_canvas->clear();
    m_network->clear();

    // ---- Routers ----------------------------------------------------------
    auto *r1 = new Router("R1");
    r1->setPosition(-350, 0);
    r1->setRoutingProtocol(Router::RoutingProtocol::RIPv2);
    r1->getInterface("Gi0/0")->ipAddress  = "10.0.0.1";
    r1->getInterface("Gi0/0")->subnetMask = "255.255.255.252";
    r1->getInterface("Gi0/1")->ipAddress  = "192.168.1.1";
    r1->getInterface("Gi0/1")->subnetMask = "255.255.255.0";
    r1->ripv2Config().networks            = {"192.168.1.0", "10.0.0.0"};

    auto *r2 = new Router("R2");
    r2->setPosition(0, 0);
    r2->setRoutingProtocol(Router::RoutingProtocol::RIPv2);
    r2->getInterface("Gi0/0")->ipAddress  = "10.0.0.2";
    r2->getInterface("Gi0/0")->subnetMask = "255.255.255.252";
    r2->getInterface("Gi0/1")->ipAddress  = "172.16.0.1";
    r2->getInterface("Gi0/1")->subnetMask = "255.255.0.0";
    r2->getInterface("Gi0/2")->ipAddress  = "10.0.1.1";
    r2->getInterface("Gi0/2")->subnetMask = "255.255.255.252";
    r2->ripv2Config().networks            = {"172.16.0.0", "10.0.0.0", "10.0.1.0"};

    auto *r3 = new Router("R3");
    r3->setPosition(350, 0);
    r3->setRoutingProtocol(Router::RoutingProtocol::PIM_DM);
    r3->getInterface("Gi0/0")->ipAddress  = "10.0.1.2";
    r3->getInterface("Gi0/0")->subnetMask = "255.255.255.252";
    r3->getInterface("Gi0/1")->ipAddress  = "192.168.2.1";
    r3->getInterface("Gi0/1")->subnetMask = "255.255.255.0";
    r3->pimdmConfig().enabledInterfaces   = {"Gi0/0", "Gi0/1"};

    // ---- Switches ---------------------------------------------------------
    auto *sw1 = new Switch("SW1");
    sw1->setPosition(-350, -220);

    auto *sw2 = new Switch("SW2");
    sw2->setPosition(0, -220);

    // ---- Hub --------------------------------------------------------------
    auto *hub1 = new Hub("Hub1");
    hub1->setPosition(350, -220);

    // ---- PCs --------------------------------------------------------------
    auto makePc = [](const QString &name, const QString &ip,
                     const QString &mask, const QString &gw,
                     qreal x, qreal y) -> PC * {
        auto *pc = new PC(name);
        pc->setPosition(x, y);
        pc->getInterface("eth0")->ipAddress  = ip;
        pc->getInterface("eth0")->subnetMask = mask;
        pc->setDefaultGateway(gw);
        return pc;
    };

    auto *pc1 = makePc("PC1", "192.168.1.10", "255.255.255.0", "192.168.1.1", -470, -400);
    auto *pc2 = makePc("PC2", "192.168.1.20", "255.255.255.0", "192.168.1.1", -230, -400);
    auto *pc3 = makePc("PC3", "172.16.0.10",  "255.255.0.0",   "172.16.0.1",  -120, -400);
    auto *pc4 = makePc("PC4", "172.16.0.20",  "255.255.0.0",   "172.16.0.1",   120, -400);
    auto *pc5 = makePc("PC5", "192.168.2.10", "255.255.255.0", "192.168.2.1",  230, -400);
    auto *pc6 = makePc("PC6", "192.168.2.20", "255.255.255.0", "192.168.2.1",  470, -400);

    // ---- Add devices to network (network takes ownership) -----------------
    m_network->addDevice(r1);
    m_network->addDevice(r2);
    m_network->addDevice(r3);
    m_network->addDevice(sw1);
    m_network->addDevice(sw2);
    m_network->addDevice(hub1);
    m_network->addDevice(pc1);
    m_network->addDevice(pc2);
    m_network->addDevice(pc3);
    m_network->addDevice(pc4);
    m_network->addDevice(pc5);
    m_network->addDevice(pc6);

    // ---- Links ------------------------------------------------------------
    auto makeLink = [](const QString &d1, const QString &i1,
                       const QString &d2, const QString &i2) -> Link {
        Link l;
        l.id         = QUuid::createUuid().toString(QUuid::WithoutBraces);
        l.device1Id  = d1;
        l.interface1 = i1;
        l.device2Id  = d2;
        l.interface2 = i2;
        return l;
    };

    m_network->addLink(makeLink(r1->id(),   "Gi0/0", r2->id(),   "Gi0/0")); // WAN R1-R2
    m_network->addLink(makeLink(r2->id(),   "Gi0/2", r3->id(),   "Gi0/0")); // WAN R2-R3
    m_network->addLink(makeLink(r1->id(),   "Gi0/1", sw1->id(),  "Fa0/0")); // R1 -> SW1
    m_network->addLink(makeLink(sw1->id(),  "Fa0/1", pc1->id(),  "eth0"));  // SW1 -> PC1
    m_network->addLink(makeLink(sw1->id(),  "Fa0/2", pc2->id(),  "eth0"));  // SW1 -> PC2
    m_network->addLink(makeLink(r2->id(),   "Gi0/1", sw2->id(),  "Fa0/0")); // R2 -> SW2
    m_network->addLink(makeLink(sw2->id(),  "Fa0/1", pc3->id(),  "eth0"));  // SW2 -> PC3
    m_network->addLink(makeLink(sw2->id(),  "Fa0/2", pc4->id(),  "eth0"));  // SW2 -> PC4
    m_network->addLink(makeLink(r3->id(),   "Gi0/1", hub1->id(), "Port0")); // R3 -> Hub1
    m_network->addLink(makeLink(hub1->id(), "Port1", pc5->id(),  "eth0"));  // Hub1 -> PC5
    m_network->addLink(makeLink(hub1->id(), "Port2", pc6->id(),  "eth0"));  // Hub1 -> PC6

    // ---- Sync canvas and state --------------------------------------------
    m_canvas->rebuildFromNetwork();
    m_currentFile.clear();
    m_modified = false;
    updateTitle();
    onStatusMessage("Sample network loaded: R1+R2 (RIPv2), R3 (PIM-DM), SW1, SW2, Hub1, PC1-PC6.");
}
