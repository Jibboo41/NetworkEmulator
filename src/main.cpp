#include <QApplication>
#include "gui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Network Emulator");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("NetworkEmulator");
    app.setStyle("Fusion");

    MainWindow window;
    window.show();

    return app.exec();
}
