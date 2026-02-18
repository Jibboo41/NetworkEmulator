cmake_minimum_required(VERSION 3.20)
project(NetworkEmulator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(SOURCES
    src/main.cpp
    src/models/Device.cpp
    src/models/Link.cpp
    src/models/Network.cpp
    src/routing/RoutingEngine.cpp
    src/routing/RIPv2.cpp
    src/routing/OSPF.cpp
    src/routing/StaticRouting.cpp
    src/routing/PIMDenseMode.cpp
    src/validation/Validator.cpp
    src/gui/MainWindow.cpp
    src/gui/NetworkCanvas.cpp
    src/gui/DeviceItem.cpp
    src/gui/LinkItem.cpp
    src/gui/dialogs/RouterDialog.cpp
    src/gui/dialogs/SwitchDialog.cpp
    src/gui/dialogs/PCDialog.cpp
    src/gui/dialogs/HubDialog.cpp
)

set(HEADERS
    src/utils/IpUtils.h
    src/models/Device.h
    src/models/Link.h
    src/models/Network.h
    src/routing/RoutingEngine.h
    src/routing/RIPv2.h
    src/routing/OSPF.h
    src/routing/StaticRouting.h
    src/routing/PIMDenseMode.h
    src/validation/Validator.h
    src/gui/MainWindow.h
    src/gui/NetworkCanvas.h
    src/gui/DeviceItem.h
    src/gui/LinkItem.h
    src/gui/dialogs/RouterDialog.h
    src/gui/dialogs/SwitchDialog.h
    src/gui/dialogs/PCDialog.h
    src/gui/dialogs/HubDialog.h
)

# ---------------------------------------------------------------------------
# Shared library sources (everything except the GUI entry point)
# ---------------------------------------------------------------------------
set(CORE_SOURCES
    src/models/Device.cpp
    src/models/Link.cpp
    src/models/Network.cpp
    src/routing/RoutingEngine.cpp
    src/routing/RIPv2.cpp
    src/routing/OSPF.cpp
    src/routing/StaticRouting.cpp
    src/routing/PIMDenseMode.cpp
    src/validation/Validator.cpp
    src/gui/MainWindow.cpp
    src/gui/NetworkCanvas.cpp
    src/gui/DeviceItem.cpp
    src/gui/LinkItem.cpp
    src/gui/dialogs/RouterDialog.cpp
    src/gui/dialogs/SwitchDialog.cpp
    src/gui/dialogs/PCDialog.cpp
    src/gui/dialogs/HubDialog.cpp
)

# ---------------------------------------------------------------------------
# Main application
# ---------------------------------------------------------------------------
add_executable(NetworkEmulator WIN32 src/main.cpp ${CORE_SOURCES} ${HEADERS})

target_include_directories(NetworkEmulator PRIVATE src)

target_link_libraries(NetworkEmulator PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# ---------------------------------------------------------------------------
# Headless test executable
# ---------------------------------------------------------------------------
add_executable(NetworkEmulatorTests src/test_main.cpp ${CORE_SOURCES} ${HEADERS})

target_include_directories(NetworkEmulatorTests PRIVATE src)

target_link_libraries(NetworkEmulatorTests PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

if(WIN32)
    # Copy Qt runtime DLLs next to the executable
    add_custom_command(TARGET NetworkEmulator POST_BUILD
        COMMAND "${Qt6_DIR}/../../../bin/windeployqt6.exe"
                --no-translations
                $<TARGET_FILE:NetworkEmulator>
        COMMENT "Deploying Qt runtime..."
        VERBATIM
    )
endif()
