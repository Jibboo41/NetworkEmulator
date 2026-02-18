#pragma once
#include <QMainWindow>
#include <QString>

class Network;
class NetworkCanvas;
class QTextEdit;
class QDockWidget;
class QActionGroup;
class QAction;
class QLabel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void newNetwork();
    void openNetwork();
    bool saveNetwork();
    bool saveNetworkAs();
    void createSampleNetwork();
    void runSimulation();
    void runSimulationWithPim();
    void validateNetwork();
    void showAbout();
    void onStatusMessage(const QString &msg);
    void setModeSelect();
    void setModePlaceRouter();
    void setModePlaceSwitch();
    void setModePlaceHub();
    void setModePlacePC();
    void setModeConnect();
    void setModeDelete();

private:
    void setupMenuBar();
    void setupToolBar();
    void setupDockWidgets();
    void updateTitle();
    bool confirmDiscardChanges();

    Network        *m_network     = nullptr;
    NetworkCanvas  *m_canvas      = nullptr;
    QTextEdit      *m_resultsView = nullptr;
    QDockWidget    *m_resultsDock = nullptr;
    QLabel         *m_statusLabel = nullptr;
    QActionGroup   *m_modeGroup   = nullptr;
    QString         m_currentFile;
    bool            m_modified    = false;
};
