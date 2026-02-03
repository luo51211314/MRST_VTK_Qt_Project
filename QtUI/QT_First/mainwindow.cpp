#include "mainwindow.h"

#include <QToolBar>
#include <QAction>
#include <QDockWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QTableWidget>
#include <QPlainTextEdit>
#include <QHeaderView>
#include <QToolButton>
#include <QMenu>
#include <QWidgetAction>
#include <QTabBar>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QSizePolicy>
#include <QDialog>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QStatusBar>
#include <QtConcurrent>
#include "gridparamdialog.h"
#include "fluidparamdialog.h"
#include "simparamdialog.h"
#include "fractureparamdialog.h"
#include "algo_adapter.h"
#include "vtk/VtkViewHost.h"
#include "vtk/VtkAdapter.h"
#include <QFileDialog>
#include <QMessageBox>
#include "qttovtkcontroladapter.h"
#include <cmath>
#include <algorithm>
#include "vtk/VtkSmokeView.h"











MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)

{
    qDebug() << "[TRACE] MainWindow ctor entered, qApp =" << qApp;

    resize(1200, 800);  // 主窗口初始大小（Ribbon + Dock + 中央区比较舒适）

    // ===== 初始化顺序 =====
    initRibbon();   // 顶部 Ribbon（menuWidget）
    initCenter();   // 中央工作区
    initLeftDock();     // 左侧流程管理
    initRightDock();    // 右侧图层 + 属性
    initBottomDock();    // 底部日志
    initConnections();  // 所有信号-槽集中在这里

    // ===== VTK 适配器（Qt 侧接入点）=====
    vtkAdapter_ = new VtkAdapter(vtkHost_, this);
    connect(vtkAdapter_, &VtkAdapter::sigLog, this, &MainWindow::log);
    vtkAdapter_->initRemoteStream();
    vtkControl_ = new QtToVtkControlAdapter(vtkAdapter_, vtkHost_);


    resizeDocks({dockLayers, dockProps}, {190, 170}, Qt::Horizontal);

    log("Ready.");

    // ===== 创建回调桥（必须先 new）=====
    callbackBridge_ = new SimCallbackBridge(this);

    // ===== Algo 初始化（当前是 Mock 引擎，未来可替换真实引擎）=====
    auto* algo = new AlgoAdapter();

    // 关键：传入 ISimulationCallback*
    algo->setCallback(static_cast<QtUItoAlgo::ISimulationCallback*>(callbackBridge_));

    // 用接口指针持有（后续换真实算法无需改 UI）
    simulator_    = static_cast<QtUItoAlgo::ISimulatorController*>(algo);
    dataTransfer_ = static_cast<QtUItoAlgo::IDataTransfer*>(algo);


    // ===== 回调：写到消息输出（logPanel）=====
    connect(callbackBridge_, &SimCallbackBridge::sigProgress,
            this, [=](double p, double t){
                // 1) 更新进度条
                if (simProgressBar_) {
                    int v = qBound(0, int(p * 100.0 + 0.5), 100);
                    simProgressBar_->setValue(v);
                }

                // 2) 更新时间显示（你现在的 t 是“模拟时间”，单位你可按需要写 d/秒）
                if (simTimeLabel_) {
                    simTimeLabel_->setText(QString("Time: %1").arg(t, 0, 'f', 2));
                }
            });


    connect(callbackBridge_, &SimCallbackBridge::sigCompleted,
            this, [=](){
                log("Simulation completed.");
                simState_ = SimState::Completed;
                if (simProgressBar_) simProgressBar_->setValue(100);
                updateSimUi();
                // ✅ A方案：完成后自动导出
                exportCsvAfterSimulation();
            });
    connect(callbackBridge_, &SimCallbackBridge::sigFailed,
            this, [=](const QString& msg){
                log("Simulation failed: " + msg);
                simState_ = SimState::Failed;
                if (simProgressBar_) simProgressBar_->setValue(0);
                updateSimUi();
            });
    connect(callbackBridge_, &SimCallbackBridge::sigTimeStep,
            this, [=](double dt){
                log(QString("TimeStep changed: %1").arg(dt, 0, 'f', 6));
            });


    // ===== 状态栏：进度条 + 时间显示 =====
    simProgressBar_ = new QProgressBar(this);
    simProgressBar_->setRange(0, 100);
    simProgressBar_->setValue(0);
    simProgressBar_->setTextVisible(true);
    simProgressBar_->setFixedWidth(220);

    simTimeLabel_ = new QLabel("Time: 0.00", this);
    simTimeLabel_->setMinimumWidth(120);

    // 注意：QMainWindow 自带 statusBar()
    statusBar()->addPermanentWidget(simTimeLabel_);
    statusBar()->addPermanentWidget(simProgressBar_);










}

MainWindow::~MainWindow() {}

void MainWindow::initRibbon()
{
    ribbonBar = new RibbonBar(this);
    setMenuWidget(ribbonBar);

    connect(ribbonBar, &RibbonBar::fileClicked, this, &MainWindow::showFileBackstage);
}


void MainWindow::initCenter()
{
    QWidget* workspaceBg = new QWidget(this);
    workspaceBg->setObjectName("workspaceBg");

    auto* lay = new QVBoxLayout(workspaceBg);
    lay->setContentsMargins(20, 20, 20, 20);
    lay->setSpacing(0);

    // ✅ 用封装好的 VtkViewHost（替代 renderHost_/layout/placeholder）
    vtkHost_ = new VtkViewHost(workspaceBg);
    lay->addWidget(vtkHost_, 1);

    setCentralWidget(workspaceBg);

    workspaceBg->setStyleSheet(R"(
        QWidget#workspaceBg { background: #f2f2f2; }
    )");
}







void MainWindow::initLeftDock()
{
    // ========== A. 左侧第 1 栏：数据/数模 ==========
    dockData = new QDockWidget(this);
    dockData->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);



    dataPanel = new DataPanel(dockData);
    dockData->setWidget(dataPanel);

    addDockWidget(Qt::LeftDockWidgetArea, dockData);


    // ========== B. 左侧第 2 栏：流程管理 ==========
    dockFlow = new QDockWidget(tr("流程管理"), this);
    dockFlow->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    flowPanel = new FlowPanel(dockFlow);
    dockFlow->setWidget(flowPanel);

    addDockWidget(Qt::LeftDockWidgetArea, dockFlow);


    // ========== C. 关键：左右并排 ==========
    splitDockWidget(dockData, dockFlow, Qt::Horizontal);

    // ========== D. 调整两栏宽度比例（数模:流程 = 2:3，可按你喜好改） ==========
    resizeDocks({dockData, dockFlow}, {200, 150}, Qt::Horizontal);
}


void MainWindow::initRightDock()
{
    // --- 图层 ---
    dockLayers = new QDockWidget(tr("图层"), this);
    dockLayers->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    layerPanel = new LayerPanel(dockLayers);
    dockLayers->setWidget(layerPanel);

    addDockWidget(Qt::RightDockWidgetArea, dockLayers);


    // --- 属性 ---
    dockProps = new QDockWidget(tr("属性"), this);
    dockProps->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    propertyPanel = new PropertyPanel(dockProps);
    dockProps->setWidget(propertyPanel);

    addDockWidget(Qt::RightDockWidgetArea, dockProps);
    splitDockWidget(dockLayers, dockProps, Qt::Vertical);

}

void MainWindow::initBottomDock()
{
    dockLog = new QDockWidget(tr("消息输出"), this);
    dockLog->setAllowedAreas(Qt::BottomDockWidgetArea);

    logPanel = new LogPanel(dockLog);
    dockLog->setWidget(logPanel);

    addDockWidget(Qt::BottomDockWidgetArea, dockLog);
    resizeDocks({dockLog}, {120}, Qt::Vertical);

}

void MainWindow::initConnections()
{
    connect(flowPanel, &FlowPanel::newProjectRequested, this, &MainWindow::onNewProject);
    connect(flowPanel, &FlowPanel::openProjectRequested, this, &MainWindow::onOpenProject);

    connect(layerPanel, &LayerPanel::layerChanged, this, &MainWindow::onLayerChanged);

    // Ribbon “导入” -> 弹参数窗并保存 CSV
    connect(ribbonBar, &RibbonBar::importParamsRequested,
            this, &MainWindow::onImportParams);
    connect(ribbonBar, &RibbonBar::newProjectRequested,
            this, &MainWindow::onNewProject);

    connect(ribbonBar, &RibbonBar::openProjectRequested,
            this, &MainWindow::onOpenProject);
    connect(ribbonBar, &RibbonBar::fractureParamsRequested,
            this, &MainWindow::onFractureParams);


    // ===== 数值模拟：仿真控制按钮 =====
    connect(ribbonBar, &RibbonBar::startSimulationRequested,
            this, &MainWindow::onStartSimulation);

    connect(ribbonBar, &RibbonBar::pauseSimulationRequested,
            this, &MainWindow::onPauseSimulation);

    connect(ribbonBar, &RibbonBar::stopSimulationRequested,
            this, &MainWindow::onStopSimulation);

    connect(ribbonBar, &RibbonBar::resetSimulationRequested,
            this, &MainWindow::onResetSimulation);

    connect(ribbonBar, &RibbonBar::gridParamsRequested,
            this, &MainWindow::onGridParams);

    connect(ribbonBar, &RibbonBar::fluidParamsRequested,
            this, &MainWindow::onFluidParams);

    connect(ribbonBar, &RibbonBar::simParamsRequested,
            this, &MainWindow::onSimParams);

    // ===== 三维分析：远程渲染模拟 =====
    connect(ribbonBar, &RibbonBar::startRemoteRenderMockRequested, this, [this](){
        if (!vtkAdapter_) return;
        vtkAdapter_->startMockStream(30);
        log("[UI] 开始远程渲染模拟");
    });

    connect(ribbonBar, &RibbonBar::stopRemoteRenderMockRequested, this, [this](){
        if (!vtkAdapter_) return;
        vtkAdapter_->stopMockStream();
        log("[UI] 停止远程渲染模拟");
    });
    connect(ribbonBar, &RibbonBar::syncParamsToVtkRequested,
            this, &MainWindow::onSyncToVtk);



    // ===== 预留：后续真实远程渲染接入 =====
    connect(ribbonBar, &RibbonBar::setRemoteRenderFpsRequested, this, [this](int fps){
        if (!vtkAdapter_) return;
        vtkAdapter_->startMockStream(fps);   // 现在先用 mock 复用
        log(QString("[UI] 设置远程渲染 FPS = %1").arg(fps));
    });

    connect(ribbonBar, &RibbonBar::connectRemoteRenderRequested, this, [this](){
        qDebug() << "[UI] connectRemoteRenderRequested triggered"
                 << "qApp=" << qApp
                 << "thread=" << QThread::currentThread();


        log("[UI] 连接远程渲染（预留）");

        // ✅ 核心：无论信号来自哪里，都把“创建 QWidget”的动作丢回 GUI 线程
        QMetaObject::invokeMethod(this, [this]() {
            QWidget* host = centralWidget();     // 你中间区域的容器（如果你有更具体的 centerPanel 就换成它）
            if (!host) return;

            if (!smokeView_) {
                smokeView_ = new VtkSmokeView(host);
                smokeView_->setMinimumSize(200, 200);

                // 如果 centralWidget 没 layout，就给它补一个
                if (!host->layout()) {
                    auto* lay = new QVBoxLayout(host);
                    lay->setContentsMargins(0,0,0,0);
                    lay->setSpacing(0);
                }
                host->layout()->addWidget(smokeView_);
            }

            smokeView_->show();
            smokeView_->raise();
        }, Qt::QueuedConnection);
    });

    connect(ribbonBar, &RibbonBar::disconnectRemoteRenderRequested, this, [this](){
        log("[UI] 断开远程渲染（预留）");
    });

    connect(ribbonBar, &RibbonBar::screenshotRemoteRenderRequested, this, [this](){
        log("[UI] 截图（预留）");
    });

    connect(ribbonBar, &RibbonBar::recordRemoteRenderRequested, this, [this](){
        log("[UI] 录屏（预留）");
    });






}

void MainWindow::onNewProject()
{
    // 默认目录：上次目录 > Documents
    QString baseDir = lastProjectDir.isEmpty()
                          ? (QDir::homePath() + "/Documents")
                          : lastProjectDir;

    QString filter = tr("工区文件 (*.dvs);;所有文件 (*.*)");

    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("新建工区"),
        baseDir + "/工区1.dvs",
        filter
        );

    if (filePath.isEmpty()) {
        log("新建工区：取消");
        return;
    }

    // 自动补 .dvs
    if (QFileInfo(filePath).suffix().isEmpty())
        filePath += ".dvs";
    else if (QFileInfo(filePath).suffix().toLower() != "dvs") {
        // 如果用户写了别的扩展名，你可以选择强行改 or 提示；这里强行改成 dvs
        filePath = QFileInfo(filePath).path() + "/" + QFileInfo(filePath).completeBaseName() + ".dvs";
    }

    lastProjectDir = QFileInfo(filePath).absolutePath();

    log(QString("新建工区：%1").arg(filePath));

    // TODO：这里后续真正创建 dvs 文件（写入默认内容）
}


void MainWindow::onOpenProject()
{
    QString baseDir = lastProjectDir.isEmpty()
    ? (QDir::homePath() + "/Documents")
    : lastProjectDir;

    QString filter = tr("工区文件 (*.dvs);;所有文件 (*.*)");

    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("打开工区"),
        baseDir,
        filter
        );

    if (filePath.isEmpty()) {
        log("打开工区：取消");
        return;
    }

    // 可选：只允许 dvs
    if (QFileInfo(filePath).suffix().toLower() != "dvs") {
        log("打开工区：请选择 .dvs 工区文件");
        return;
    }

    lastProjectDir = QFileInfo(filePath).absolutePath();

    log(QString("打开工区：%1").arg(filePath));

    // TODO：这里后续真正加载 dvs（解析/刷新左侧树、中央工作区等）
}


void MainWindow::onLayerChanged()
{
    if (!propertyPanel || !layerPanel) return;
    propertyPanel->setPropertiesForLayer(layerPanel->currentLayer());
}


void MainWindow::log(const QString &text)
{
    if (logPanel) logPanel->append(text);
}


void MainWindow::showFileBackstage()
{
    if (!backstageDlg) {
        backstageDlg = new BackstageDialog(this);

        // Backstage 里的按钮行为 -> MainWindow 业务
        connect(backstageDlg, &BackstageDialog::requestOpen, this, [this](){
            log("Backstage: 打开...");
            hideFileBackstage();
            onOpenProject();
        });
        connect(backstageDlg, &BackstageDialog::requestNew, this, [this](){
            log("Backstage: 新建...");
            hideFileBackstage();
            onNewProject();
        });
        connect(backstageDlg, &BackstageDialog::requestExit, this, [this](){
            close();
        });
    }

    // 显示文件快捷栏，隐藏 ribbonPages（你之前已有）
    ribbonBar->setFileMode(true);


    // 弹窗位置：贴文件按钮下方
    QPoint p = ribbonBar->fileButton()->mapToGlobal(QPoint(0, ribbonBar->fileButton()->height()));
    backstageDlg->move(p);
    backstageDlg->show();
    backstageDlg->raise();
    backstageDlg->activateWindow();
}

void MainWindow::hideFileBackstage()
{
    // 恢复正常 ribbon
    if (ribbonBar) ribbonBar->setFileMode(false);

    if (backstageDlg && backstageDlg->isVisible())
        backstageDlg->hide();
}


//点击导入的弹窗
void MainWindow::onImportParams()
{
    // 1) 弹出参数输入对话框
    ParamDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    // 2) 让用户选择保存路径
    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("保存参数 CSV"),
        (lastProjectDir.isEmpty() ? (QDir::homePath() + "/Documents") : lastProjectDir) + "/params.csv",
        tr("CSV 文件 (*.csv)")
        );
    if (path.isEmpty())
        return;

    // 3) 写入 CSV
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("保存失败"), tr("无法写入文件：%1").arg(path));
        return;
    }

    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true); // 关键！给 Excel 用
    out << dlg.toCsvText();
    f.close();

    log(QString("参数已保存为 CSV：%1").arg(path));
}

void MainWindow::onStartSimulation()
{
    if (!simulator_) return;

    // 继续：只在暂停状态下允许 resume
    if (simState_ == SimState::Paused) {
        log("继续仿真");
        simulator_->resumeSimulation();
        simState_ = SimState::Running;
        updateSimUi();
        return;
    }

    // ===== 真实算法必需的前置参数检查 =====
    if (!hasGrid_) {
        QMessageBox::warning(this, "缺少参数", "请先设置网格参数（Grid Params）");
        return;
    }
    if (!hasFluid_) {
        QMessageBox::warning(this, "缺少参数", "请先设置流体参数（Fluid Params）");
        return;
    }
    if (!hasSim_) {
        QMessageBox::warning(this, "缺少参数", "请先设置模拟参数（Sim Params）");
        return;
    }
    if (!hasFrac_) {
        QMessageBox::warning(this, "缺少参数", "请先设置裂缝参数（仿真设置 → 裂缝参数）");
        return;
    }


    // ✅ 如果你们真实算法要求必须有 fracture/well，也在这里加检查
    // if (!hasFracture_) ...
    // if (!hasWell_) ...

    log("开始仿真（后台线程）");
    simState_ = SimState::Running;
    updateSimUi();

    // ===== 关键：后台线程跑，避免 UI 线程 abort/卡死 =====
    QtConcurrent::run([this]{
        bool ok = simulator_->runSimulation();
        QMetaObject::invokeMethod(this, [this, ok]{
            log(QString("runSimulation returned: %1").arg(ok));
        }, Qt::QueuedConnection);
    });


}

void MainWindow::onPauseSimulation()
{
    if (!simulator_) return;

    // 只有 Running 才允许暂停
    if (simState_ == SimState::Running) {
        log("暂停仿真");
        simulator_->pauseSimulation();
        simState_ = SimState::Paused;
        updateSimUi();
    }
}

void MainWindow::onStopSimulation()
{
    if (!simulator_) return;

    log("停止仿真");
    simulator_->stopSimulation();
    if (simProgressBar_) simProgressBar_->setValue(0);
    if (simTimeLabel_) simTimeLabel_->setText("Time: 0.00");
    simState_ = SimState::Stopped;
    updateSimUi();
}

void MainWindow::onResetSimulation()
{
    if (!simulator_) return;

    log("重置仿真");
    simulator_->stopSimulation();
    if (simProgressBar_) simProgressBar_->setValue(0);
    if (simTimeLabel_) simTimeLabel_->setText("Time: 0.00");
    simState_ = SimState::Idle;
    updateSimUi();
}


void MainWindow::updateSimUi()
{
    // 先用日志验证状态切换没问题，后面再去禁用按钮/改按钮文字
    switch (simState_) {
    case SimState::Idle:      log("[UI] SimState=Idle"); break;
    case SimState::Running:   log("[UI] SimState=Running"); break;
    case SimState::Paused:    log("[UI] SimState=Paused"); break;
    case SimState::Stopped:   log("[UI] SimState=Stopped"); break;
    case SimState::Completed: log("[UI] SimState=Completed"); break;
    case SimState::Failed:    log("[UI] SimState=Failed"); break;
    }
}


void MainWindow::onGridParams()
{
    if (!simulator_) return;
    GridParamDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    gridParams_ = dlg.params();
    const bool ok = simulator_->initGrid(gridParams_);
    hasGrid_ = ok;

    log(ok ? "initGrid OK" : "initGrid FAILED");
}

void MainWindow::onFluidParams()
{
    if (!simulator_) return;
    FluidParamDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    fluidProps_ = dlg.props();
    const bool ok = simulator_->setFluidProperties(fluidProps_);
    hasFluid_ = ok;

    log(ok ? "setFluidProperties OK" : "setFluidProperties FAILED");
}

void MainWindow::onSimParams()
{
    if (!simulator_) return;
    SimParamDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    simParams_ = dlg.params();
    const bool ok = simulator_->setSimulationParameters(simParams_);
    hasSim_ = ok;

    log(ok ? "setSimulationParameters OK" : "setSimulationParameters FAILED");
}

void MainWindow::onFractureParams()
{
    if (!simulator_) return;

    FractureParamDialog dlg(this);

    // ✅ 自动填充：用网格 Lx/Ly/Lz（你 gridParams_ 里就是 Lx/Ly/Lz）
    if (hasGrid_) dlg.setGridBounds(gridParams_.Lx, gridParams_.Ly, gridParams_.Lz);

    if (dlg.exec() != QDialog::Accepted) return;

    fracs_ = dlg.fractures();                 // ✅ 一次拿一组
    bool ok = simulator_->addFractures(fracs_);
    hasFrac_ = ok;

    log(ok ? QString("addFractures OK (count=%1)").arg(fracs_.size())
           : "addFractures FAILED");

}


static QtToVTK::Point3 toPoint3(const QtUItoAlgo::Point3& p)
{
    return {p.x, p.y, p.z};
}

static QtToVTK::Point3 average4(const QtUItoAlgo::Point3 v[4])
{
    QtToVTK::Point3 c{0,0,0};
    for (int i=0;i<4;i++) {
        c.x += v[i].x;
        c.y += v[i].y;
        c.z += v[i].z;
    }
    c.x /= 4.0; c.y /= 4.0; c.z /= 4.0;
    return c;
}

static double dist(const QtUItoAlgo::Point3& a, const QtUItoAlgo::Point3& b)
{
    const double dx=a.x-b.x, dy=a.y-b.y, dz=a.z-b.z;
    return std::sqrt(dx*dx+dy*dy+dz*dz);
}
void MainWindow::onSyncToVtk()
{
    // 1) 基本检查
    if (!vtkControl_) {   // 你得有这个成员（见第④处）
        log("[VTK] syncToVtk FAILED: vtkControl_ is null");
        return;
    }

    if (!hasGrid_) {
        QMessageBox::warning(this, "缺少参数", "请先设置网格参数（Grid Params）");
        return;
    }

    // 2) 同步网格：QtUItoAlgo::GridParameters -> QtToVTK::GridParameters
    QtToVTK::GridParameters g{};
    g.Nx = gridParams_.Nx;
    g.Ny = gridParams_.Ny;
    g.Nz = gridParams_.Nz;
    g.Lx = gridParams_.Lx;
    g.Ly = gridParams_.Ly;
    g.Lz = gridParams_.Lz;

    bool okG = vtkControl_->updateGrid(g);
    log(okG ? "[VTK] updateGrid OK" : "[VTK] updateGrid FAILED");

    // 3) 同步裂缝（如果没有裂缝就跳过）
    if (!hasFrac_ || fracs_.empty()) {
        log("[VTK] no fractures, skip updateFractures");
        vtkControl_->resetView();
        return;
    }

    // ⚠️ 这里需要把 fracs_（QtUItoAlgo 的 fracture 类型）转换成 QtToVTK::FractureParameters
    // 我给你一个“通用模板”，你只需要把字段名对上即可
    std::vector<QtToVTK::FractureParameters> vf;
    vf.reserve(fracs_.size());

    for (const auto& f : fracs_) {
        QtToVTK::FractureParameters fp{};
        fp.id       = f.id;         // 如果你的字段叫 id_
        fp.aperture = f.aperture;   // 如果字段叫 aperture_
        fp.perm     = f.perm;       // 如果字段叫 perm_

        // center：四顶点平均
        fp.center = average4(f.vertices);

        // length：取矩形一条边长度（0-1），另一条边是 1-2
        const double L1 = dist(f.vertices[0], f.vertices[1]);
        const double L2 = dist(f.vertices[1], f.vertices[2]);
        fp.length = std::max(L1, L2);

        // angle/dip：你现在的 FractureInput 没提供角度/倾角
        // 联调阶段先设 0，不影响链路验证
        fp.angle = 0.0;
        fp.dip   = 0.0;

        vf.push_back(fp);
    }

    bool okF = vtkControl_->updateFractures(vf);
    log(okF ? "[VTK] updateFractures OK" : "[VTK] updateFractures FAILED");

    // 可选：让视图归位（方便你看到变化）
    vtkControl_->resetView();
}


    void MainWindow::exportCsvAfterSimulation()
{
        if (!dataTransfer_) {
            log("[Export] dataTransfer_ is null");
            return;
        }

        QString dir = QFileDialog::getExistingDirectory(
            this,
            tr("选择导出目录（CSV）"),
            lastProjectDir.isEmpty() ? (QDir::homePath() + "/Documents") : lastProjectDir
            );

        if (dir.isEmpty()) {
            log("[Export] canceled");
            return;
        }

        // 记住目录，方便下次默认打开
        lastProjectDir = dir;

        const bool ok1 = dataTransfer_->exportResults(dir.toStdString());
        const bool ok2 = dataTransfer_->exportGeometry(dir.toStdString());  // 注意：接口叫 output_path，我们这里传目录
        QString src = QDir::current().absoluteFilePath("output_sim.csv");
        QString dst = dir + "/output_sim.csv";

        if (QFile::exists(src)) {
            QFile::remove(dst);
            if (QFile::copy(src, dst)) {
                log("[Export] copied output_sim.csv");
            } else {
                log("[Export] copy output_sim.csv FAILED: " + src);
            }
        } else {
            log("[Export] output_sim.csv NOT FOUND in: " + src);
        }


        log(QString("[Export] exportResults=%1, exportGeometry=%2, dir=%3")
                .arg(ok1).arg(ok2).arg(dir));

        if (!ok1 || !ok2) {
            QMessageBox::warning(this, tr("导出提示"),
                                 tr("导出可能未完全成功。\nexportResults=%1\nexportGeometry=%2\n目录：%3")
                                     .arg(ok1).arg(ok2).arg(dir));
        } else {
            QMessageBox::information(this, tr("导出成功"),
                                     tr("CSV 已导出到：\n%1").arg(dir));
        }
    }







