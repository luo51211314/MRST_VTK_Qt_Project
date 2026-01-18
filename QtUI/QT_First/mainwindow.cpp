#include "mainwindow.h"

#include <QToolBar>
#include <QAction>
#include <QDockWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QTableWidget>
#include <QPlainTextEdit>
#include <QHeaderView>
#include <QLabel>
#include <QToolButton>
#include <QMenu>
#include <QWidgetAction>
#include <QTabBar>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QFrame>
#include <QSizePolicy>
#include <QDialog>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)

{
    resize(1200, 800);  // 主窗口初始大小（Ribbon + Dock + 中央区比较舒适）

    // ===== 初始化顺序 =====
    initRibbon();   // 顶部 Ribbon（menuWidget）
    initCenter();   // 中央工作区
    initLeftDock();     // 左侧流程管理
    initRightDock();    // 右侧图层 + 属性
    initBottomDock();    // 底部日志
    initConnections();  // 所有信号-槽集中在这里

    log("Ready.");
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
    // 中间先放一个占位，后面你再换成绘图区/表格/图形视图都行
    auto *center = new QLabel(tr("中央工作区"));
    center->setAlignment(Qt::AlignCenter);
    center->setStyleSheet("background:#ffffff; border:1px solid #ddd; border-radius:8px;");
    setCentralWidget(center);
}

void MainWindow::initLeftDock()
{
    // ========== A. 左侧第 1 栏：数据/数模 ==========
    dockData = new QDockWidget(tr("数模"), this);
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
    resizeDocks({dockData, dockFlow}, {150, 150}, Qt::Horizontal);
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
}

void MainWindow::initConnections()
{
    connect(flowPanel, &FlowPanel::newProjectRequested, this, &MainWindow::onNewProject);
    connect(flowPanel, &FlowPanel::openProjectRequested, this, &MainWindow::onOpenProject);

    connect(layerPanel, &LayerPanel::layerChanged, this, &MainWindow::onLayerChanged);

    // Ribbon “导入” -> 弹参数窗并保存 CSV
    connect(ribbonBar, &RibbonBar::importParamsRequested,
            this, &MainWindow::onImportParams);

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









