#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabBar>
#include <QStackedWidget>
#include <QToolButton>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include "logpanel.h"
#include "layerpanel.h"
#include "propertypanel.h"
#include "flowpanel.h"
#include "datapanel.h"
#include "backstagedialog.h"
#include "ribbonbar.h"
#include "paramdialog.h"
#include "mock/mockalgo.h"
#include "mock/SimCallbackBridge.h"
#include "QtUItoAlgoInterface.h"







class QDockWidget;
class QTreeWidget;
class QListWidget;
class QTableWidget;
class QPlainTextEdit;
class QAction;
class QToolBar;
class VtkViewHost;
class VtkAdapter;


class MainWindow : public QMainWindow
{
    Q_OBJECT
public:

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onNewProject();
    void onOpenProject();
    void onLayerChanged();


private:
    // 顶部：工具栏/按钮
    QToolBar* ribbon = nullptr;
    QAction* actNewProject = nullptr;
    QAction* actOpenProject = nullptr;

    RibbonBar* ribbonBar = nullptr;

    // 左 Dock：流程管理
    QDockWidget* dockFlow = nullptr;
    FlowPanel* flowPanel = nullptr;


    // 左 Dock：数据/数模
    QDockWidget* dockData = nullptr;
    DataPanel* dataPanel = nullptr;


    // 右 Dock：图层 + 属性
    QDockWidget* dockLayers = nullptr;
    LayerPanel* layerPanel = nullptr;


    QDockWidget* dockProps = nullptr;
    PropertyPanel* propertyPanel = nullptr;


    // 底 Dock：消息输出
    QDockWidget* dockLog = nullptr;
    LogPanel* logPanel = nullptr;

    // Ribbon 顶部
    QMenu* fileMenu = nullptr;


    // ===== File Backstage（文件后台）=====
    BackstageDialog* backstageDlg = nullptr;

    // 新建文件
    QString lastProjectDir;


    //仿真模拟
    MockAlgo* mockAlgo_ = nullptr;
    SimCallbackBridge* callbackBridge_ = nullptr;
    QtUItoAlgo::ISimulatorController* simulator_ = nullptr;
    QtUItoAlgo::IDataTransfer* dataTransfer_ = nullptr;

    enum class SimState { Idle, Running, Paused, Stopped, Completed, Failed };
    SimState simState_ = SimState::Idle;

    //进度条
    QProgressBar* simProgressBar_ = nullptr;
    QLabel* simTimeLabel_ = nullptr;

    VtkViewHost* vtkHost_ = nullptr;
    VtkAdapter*  vtkAdapter_ = nullptr;








    void onStartSimulation();
    void onPauseSimulation();
    void onStopSimulation();
    void onResetSimulation();
    // ===== 顶部“文件快捷栏”（打开文件时显示）=====
    void showFileBackstage();
    void hideFileBackstage();

    // 点击导入弹窗
    void onImportParams();
    // 点击新建和打开
    void onNewWorkArea();
    void onOpenWorkArea();

    void updateSimUi();

    void onGridParams();
    void onFluidParams();
    void onSimParams();







    // 初始化分模块写
    void initRibbon();
    void initCenter();
    void initLeftDock();
    void initRightDock();
    void initBottomDock();
    void initConnections();
    void log(const QString& text);
};

#endif
