//
// Headless integration test for the routing simulation and validation engines.
// Prints PASS / FAIL for each assertion and exits non-zero on any failure.
//
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <iostream>
#include <functional>

#include "models/Network.h"
#include "routing/RoutingEngine.h"
#include "validation/Validator.h"

// ---------------------------------------------------------------------------
// Tiny test harness
// ---------------------------------------------------------------------------
static int g_passed = 0;
static int g_failed = 0;

static void check(bool condition, const char *desc)
{
    if (condition) {
        std::cout << "  PASS  " << desc << "\n";
        ++g_passed;
    } else {
        std::cout << "  FAIL  " << desc << "\n";
        ++g_failed;
    }
}

static void section(const char *title)
{
    std::cout << "\n=== " << title << " ===\n";
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static bool hasRoute(const QList<RoutingEntry> &table,
                     const QString &dest, const QString &mask,
                     const QString &proto = {})
{
    for (const auto &e : table)
        if (e.destination == dest && e.mask == mask &&
            (proto.isEmpty() || e.protocol == proto))
            return true;
    return false;
}

static bool hasIssue(const QList<ValidationIssue> &issues,
                     ValidationIssue::Severity sev,
                     const QString &fragment)
{
    for (const auto &i : issues)
        if (i.severity == sev && i.message.contains(fragment))
            return true;
    return false;
}

// ---------------------------------------------------------------------------
// Build a simple two-router RIPv2 topology and return the network.
//
//   PC1 --- R1 (Gi0/1: 192.168.1.1/24) --- (Gi0/0: 10.0.0.1/30)
//                                            |
//   PC2 --- R2 (Gi0/1: 172.16.0.1/24)  --- (Gi0/0: 10.0.0.2/30)
// ---------------------------------------------------------------------------
static Network *buildRipNetwork(QObject *parent)
{
    auto *net = new Network(parent);

    auto *r1 = new Router("R1", net);
    r1->setRoutingProtocol(Router::RoutingProtocol::RIPv2);
    r1->interfaces()[0].ipAddress  = "10.0.0.1";
    r1->interfaces()[0].subnetMask = "255.255.255.252";
    r1->interfaces()[1].ipAddress  = "192.168.1.1";
    r1->interfaces()[1].subnetMask = "255.255.255.0";
    r1->setPosition(100, 200);
    net->addDevice(r1);

    auto *r2 = new Router("R2", net);
    r2->setRoutingProtocol(Router::RoutingProtocol::RIPv2);
    r2->interfaces()[0].ipAddress  = "10.0.0.2";
    r2->interfaces()[0].subnetMask = "255.255.255.252";
    r2->interfaces()[1].ipAddress  = "172.16.0.1";
    r2->interfaces()[1].subnetMask = "255.255.255.0";
    r2->setPosition(300, 200);
    net->addDevice(r2);

    auto *pc1 = new PC("PC1", net);
    pc1->interfaces()[0].ipAddress  = "192.168.1.10";
    pc1->interfaces()[0].subnetMask = "255.255.255.0";
    pc1->setDefaultGateway("192.168.1.1");
    net->addDevice(pc1);

    auto *pc2 = new PC("PC2", net);
    pc2->interfaces()[0].ipAddress  = "172.16.0.10";
    pc2->interfaces()[0].subnetMask = "255.255.255.0";
    pc2->setDefaultGateway("172.16.0.1");
    net->addDevice(pc2);

    // R1 Gi0/0 <-> R2 Gi0/0
    net->addLink({"link-r1r2", r1->id(), "Gi0/0", r2->id(), "Gi0/0"});
    // R1 Gi0/1 <-> PC1 eth0
    net->addLink({"link-r1pc1", r1->id(), "Gi0/1", pc1->id(), "eth0"});
    // R2 Gi0/1 <-> PC2 eth0
    net->addLink({"link-r2pc2", r2->id(), "Gi0/1", pc2->id(), "eth0"});

    return net;
}

// ---------------------------------------------------------------------------
// Build a two-router OSPF topology
// ---------------------------------------------------------------------------
static Network *buildOspfNetwork(QObject *parent)
{
    auto *net = new Network(parent);

    auto *r1 = new Router("OR1", net);
    r1->setRoutingProtocol(Router::RoutingProtocol::OSPF);
    r1->ospfConfig().routerId = "1.1.1.1";
    r1->interfaces()[0].ipAddress  = "10.1.0.1";
    r1->interfaces()[0].subnetMask = "255.255.255.252";
    r1->interfaces()[0].ospfCost   = 10;
    r1->interfaces()[1].ipAddress  = "192.168.10.1";
    r1->interfaces()[1].subnetMask = "255.255.255.0";
    net->addDevice(r1);

    auto *r2 = new Router("OR2", net);
    r2->setRoutingProtocol(Router::RoutingProtocol::OSPF);
    r2->ospfConfig().routerId = "2.2.2.2";
    r2->interfaces()[0].ipAddress  = "10.1.0.2";
    r2->interfaces()[0].subnetMask = "255.255.255.252";
    r2->interfaces()[0].ospfCost   = 10;
    r2->interfaces()[1].ipAddress  = "172.16.10.1";
    r2->interfaces()[1].subnetMask = "255.255.255.0";
    net->addDevice(r2);

    net->addLink({"link-or1or2", r1->id(), "Gi0/0", r2->id(), "Gi0/0"});

    return net;
}

// ---------------------------------------------------------------------------
// Build a static-routing topology with three routers in a chain
// ---------------------------------------------------------------------------
static Network *buildStaticNetwork(QObject *parent)
{
    auto *net = new Network(parent);

    auto *r1 = new Router("SR1", net);
    r1->setRoutingProtocol(Router::RoutingProtocol::Static);
    r1->interfaces()[0].ipAddress  = "10.0.0.1";
    r1->interfaces()[0].subnetMask = "255.255.255.252";
    r1->interfaces()[1].ipAddress  = "192.168.20.1";
    r1->interfaces()[1].subnetMask = "255.255.255.0";
    r1->staticRoutes().append({"172.16.20.0", "255.255.255.0", "10.0.0.2", 1});
    net->addDevice(r1);

    auto *r2 = new Router("SR2", net);
    r2->setRoutingProtocol(Router::RoutingProtocol::Static);
    r2->interfaces()[0].ipAddress  = "10.0.0.2";
    r2->interfaces()[0].subnetMask = "255.255.255.252";
    r2->interfaces()[1].ipAddress  = "172.16.20.1";
    r2->interfaces()[1].subnetMask = "255.255.255.0";
    r2->staticRoutes().append({"192.168.20.0", "255.255.255.0", "10.0.0.1", 1});
    net->addDevice(r2);

    net->addLink({"link-sr1sr2", r1->id(), "Gi0/0", r2->id(), "Gi0/0"});

    return net;
}

// ---------------------------------------------------------------------------
// Build a network with intentional validation errors
// ---------------------------------------------------------------------------
static Network *buildBrokenNetwork(QObject *parent)
{
    auto *net = new Network(parent);

    // Two routers with the same IP on connected interfaces (subnet mismatch + conflict)
    auto *r1 = new Router("BR1", net);
    r1->setRoutingProtocol(Router::RoutingProtocol::OSPF);
    r1->ospfConfig().routerId = "3.3.3.3";
    r1->interfaces()[0].ipAddress  = "10.0.5.1";
    r1->interfaces()[0].subnetMask = "255.255.255.0"; // /24
    net->addDevice(r1);

    auto *r2 = new Router("BR2", net);
    r2->setRoutingProtocol(Router::RoutingProtocol::OSPF);
    r2->ospfConfig().routerId = "3.3.3.3"; // duplicate router-id!
    r2->interfaces()[0].ipAddress  = "10.0.5.2";
    r2->interfaces()[0].subnetMask = "255.255.255.252"; // /30 — mismatch with BR1
    net->addDevice(r2);

    net->addLink({"link-br1br2", r1->id(), "Gi0/0", r2->id(), "Gi0/0"});

    // PC with no gateway
    auto *pc = new PC("BPC", net);
    pc->interfaces()[0].ipAddress  = "192.168.99.5";
    pc->interfaces()[0].subnetMask = "255.255.255.0";
    // No default gateway set
    net->addDevice(pc);  // isolated — not connected

    return net;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
static void testRipv2()
{
    section("RIPv2 Simulation");
    QObject owner;
    Network *net = buildRipNetwork(&owner);
    RoutingEngine::run(net);

    Router *r1 = nullptr, *r2 = nullptr;
    for (auto *d : net->devices()) {
        if (auto *r = qobject_cast<Router *>(d)) {
            if (r->name() == "R1") r1 = r;
            if (r->name() == "R2") r2 = r;
        }
    }

    // R1 should have its own connected routes
    check(hasRoute(r1->computedRoutingTable(), "10.0.0.0",   "255.255.255.252", "Connected"),
          "R1 has connected route 10.0.0.0/30");
    check(hasRoute(r1->computedRoutingTable(), "192.168.1.0","255.255.255.0",   "Connected"),
          "R1 has connected route 192.168.1.0/24");

    // R1 should have learned R2's LAN via RIPv2
    check(hasRoute(r1->computedRoutingTable(), "172.16.0.0", "255.255.255.0",   "RIPv2"),
          "R1 learned 172.16.0.0/24 via RIPv2");

    // R2 should have learned R1's LAN
    check(hasRoute(r2->computedRoutingTable(), "192.168.1.0","255.255.255.0",   "RIPv2"),
          "R2 learned 192.168.1.0/24 via RIPv2");

    // R2 should have its own connected routes
    check(hasRoute(r2->computedRoutingTable(), "172.16.0.0", "255.255.255.0",   "Connected"),
          "R2 has connected route 172.16.0.0/24");

    // Next-hop on R1's learned route should be R2's interface IP
    bool correctNextHop = false;
    for (const auto &e : r1->computedRoutingTable())
        if (e.destination == "172.16.0.0" && e.nextHop == "10.0.0.2")
            correctNextHop = true;
    check(correctNextHop, "R1 next-hop for 172.16.0.0/24 is 10.0.0.2");
}

static void testOspf()
{
    section("OSPF Simulation");
    QObject owner;
    Network *net = buildOspfNetwork(&owner);
    RoutingEngine::run(net);

    Router *r1 = nullptr, *r2 = nullptr;
    for (auto *d : net->devices()) {
        if (auto *r = qobject_cast<Router *>(d)) {
            if (r->name() == "OR1") r1 = r;
            if (r->name() == "OR2") r2 = r;
        }
    }

    check(hasRoute(r1->computedRoutingTable(), "10.1.0.0",    "255.255.255.252", "Connected"),
          "OR1 has connected route 10.1.0.0/30");
    check(hasRoute(r1->computedRoutingTable(), "192.168.10.0","255.255.255.0",   "Connected"),
          "OR1 has connected route 192.168.10.0/24");
    check(hasRoute(r1->computedRoutingTable(), "172.16.10.0", "255.255.255.0",   "OSPF"),
          "OR1 learned 172.16.10.0/24 via OSPF");
    check(hasRoute(r2->computedRoutingTable(), "192.168.10.0","255.255.255.0",   "OSPF"),
          "OR2 learned 192.168.10.0/24 via OSPF");

    // OSPF metric should equal the link cost (10)
    bool correctMetric = false;
    for (const auto &e : r1->computedRoutingTable())
        if (e.destination == "172.16.10.0" && e.metric == 10)
            correctMetric = true;
    check(correctMetric, "OR1 OSPF metric for 172.16.10.0/24 is 10 (link cost)");
}

static void testStatic()
{
    section("Static Routing Simulation");
    QObject owner;
    Network *net = buildStaticNetwork(&owner);
    RoutingEngine::run(net);

    Router *r1 = nullptr, *r2 = nullptr;
    for (auto *d : net->devices()) {
        if (auto *r = qobject_cast<Router *>(d)) {
            if (r->name() == "SR1") r1 = r;
            if (r->name() == "SR2") r2 = r;
        }
    }

    check(hasRoute(r1->computedRoutingTable(), "192.168.20.0","255.255.255.0",   "Connected"),
          "SR1 has connected route 192.168.20.0/24");
    check(hasRoute(r1->computedRoutingTable(), "172.16.20.0", "255.255.255.0",   "Static"),
          "SR1 has static route to 172.16.20.0/24");
    check(hasRoute(r2->computedRoutingTable(), "192.168.20.0","255.255.255.0",   "Static"),
          "SR2 has static route to 192.168.20.0/24");

    bool correctNextHop = false;
    for (const auto &e : r1->computedRoutingTable())
        if (e.destination == "172.16.20.0" && e.nextHop == "10.0.0.2")
            correctNextHop = true;
    check(correctNextHop, "SR1 static route next-hop is 10.0.0.2");
}

static void testValidationClean()
{
    section("Validation — Clean Network");
    QObject owner;
    Network *net = buildRipNetwork(&owner);
    const auto issues = Validator::validate(net);

    const int errors = std::count_if(issues.begin(), issues.end(),
        [](const ValidationIssue &i){ return i.severity == ValidationIssue::Severity::Error; });
    check(errors == 0, "No errors on a correctly configured RIPv2 network");

    for (const auto &i : issues)
        std::cout << "    [" << i.severityString().toStdString() << "] "
                  << i.message.toStdString() << "\n";
}

static void testValidationErrors()
{
    section("Validation — Broken Network");
    QObject owner;
    Network *net = buildBrokenNetwork(&owner);
    const auto issues = Validator::validate(net);

    std::cout << "  Issues found:\n";
    for (const auto &i : issues)
        std::cout << "    [" << i.severityString().toStdString() << "] "
                  << i.message.toStdString() << "\n";

    check(hasIssue(issues, ValidationIssue::Severity::Error,   "Subnet mismatch"),
          "Detected subnet mismatch between BR1 (/24) and BR2 (/30)");
    check(hasIssue(issues, ValidationIssue::Severity::Error,   "router-id"),
          "Detected duplicate OSPF router-id 3.3.3.3");
    check(hasIssue(issues, ValidationIssue::Severity::Warning, "gateway"),
          "Detected PC with no default gateway");
    check(hasIssue(issues, ValidationIssue::Severity::Warning, "not connected"),
          "Detected isolated device (BPC)");
}

static void testSaveLoad()
{
    section("Save / Load");
    QObject owner;
    Network *original = buildRipNetwork(&owner);
    const QString path = QCoreApplication::applicationDirPath() + "/test_network.net";

    QString err;
    check(original->save(path, &err), "Network saves without error");

    auto *loaded = new Network(&owner);
    check(loaded->load(path, &err), "Network loads without error");
    check(loaded->devices().size() == original->devices().size(),
          "Loaded device count matches original");
    check(loaded->links().size() == original->links().size(),
          "Loaded link count matches original");

    // Re-run simulation on loaded network to confirm data integrity
    RoutingEngine::run(loaded);
    Router *r1 = nullptr;
    for (auto *d : loaded->devices())
        if (auto *r = qobject_cast<Router *>(d))
            if (r->name() == "R1") { r1 = r; break; }

    check(r1 != nullptr, "R1 found after load");
    if (r1)
        check(hasRoute(r1->computedRoutingTable(), "172.16.0.0","255.255.255.0","RIPv2"),
              "R1 still learns 172.16.0.0/24 via RIPv2 after save/load");

    QFile::remove(path);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "\nNetworkEmulator — Simulation & Validation Tests\n";
    std::cout << "================================================\n";

    testRipv2();
    testOspf();
    testStatic();
    testValidationClean();
    testValidationErrors();
    testSaveLoad();

    std::cout << "\n------------------------------------------------\n";
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed.\n";

    return (g_failed > 0) ? 1 : 0;
}
