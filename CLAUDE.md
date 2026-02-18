# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

**Requires:** Qt6, CMake ≥ 3.20, a C++17 compiler (MSVC 2022 or MinGW-w64).

```bash
# Configure (from project root)
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"

# Build
cmake --build build --config Release

# The executable appears at build/Release/NetworkEmulator.exe
# windeployqt6 runs automatically post-build to copy Qt DLLs
```

## Architecture

```
src/
├── utils/IpUtils.h          Header-only IPv4 arithmetic (parse, format, networkAddr, sameSubnet)
├── models/
│   ├── Device.{h,cpp}       Device base class + Router, Switch, Hub, PC subclasses
│   │                        Also defines NetworkInterface, RoutingEntry, and per-protocol configs
│   ├── Link.{h,cpp}         POD struct connecting two (device, interface) pairs
│   └── Network.{h,cpp}      QObject container; owns Device* via Qt parent; save/load JSON
├── routing/
│   ├── RIPv2.{h,cpp}        Bellman-Ford with split horizon; updates Router::computedRoutingTable
│   ├── OSPF.{h,cpp}         Dijkstra SPF; builds per-router routing tables
│   ├── StaticRouting.{h,cpp} Applies user-configured static routes + connected networks
│   ├── PIMDenseMode.{h,cpp} Flood-and-prune multicast tree builder; returns MulticastTree
│   └── RoutingEngine.{h,cpp} Calls all four protocols; returns SimulationResult
├── validation/
│   └── Validator.{h,cpp}    Checks IP conflicts, subnet mismatches, PC gateways,
│                            OSPF router-id uniqueness, unconnected interfaces, reachability
└── gui/
    ├── DeviceItem.{h,cpp}   QGraphicsObject drawn on canvas; double-click opens config dialog
    ├── LinkItem.{h,cpp}     QGraphicsObject drawn as a line with interface labels
    ├── NetworkCanvas.{h,cpp} QGraphicsView; owns DeviceItem/LinkItem maps; handles all mouse modes
    ├── MainWindow.{h,cpp}   QMainWindow; toolbar mode buttons, menus, results dock, file I/O
    └── dialogs/             One QDialog per device type (Router/Switch/Hub/PC)
```

## Key Design Points

- **Memory:** `Network` is the QObject parent for all `Device*`. `QGraphicsScene` owns all `QGraphicsItem*`.  Do not `delete` items managed by these parents.
- **Routing simulation flow:** `RoutingEngine::run()` → each protocol's `compute(network)` → mutates `Router::m_routingTable` in-place → `MainWindow` reads back and renders HTML.
- **Canvas modes:** Enum `NetworkCanvas::Mode` drives `mousePressEvent` behaviour. The toolbar `QActionGroup` keeps mode buttons mutually exclusive.
- **Persistence:** `Network::save/load` use `QJsonDocument`. Each `Device` subclass implements `toJson()` and a static `fromJson()` factory.
- **IP math:** Always go through `IpUtils::` functions rather than string parsing ad-hoc. Addresses are stored as `QString` and converted to `quint32` only for arithmetic.
