#include "ribbonbar.h"

#include <QToolButton>
#include <QTabBar>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QStyle>
#include <QMessageBox>

RibbonBar::RibbonBar(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

QToolButton* RibbonBar::fileButton() const
{
    return btnFile;
}

void RibbonBar::setFileMode(bool on)
{
    if (fileQuickBar) fileQuickBar->setVisible(on);
    if (pages)        pages->setVisible(!on);
}

static QIcon rIcon(const char* name)
{
    // 你的 qrc: prefix=/icons, file=icons/xxx.svg
    // 所以路径是 :/icons/icons/xxx.svg
    return QIcon(QString(":/icons/icons/%1").arg(name));
}


void RibbonBar::buildUi()
{
    setObjectName("ribbonRoot");
    setAttribute(Qt::WA_StyledBackground, true);

    auto* rootLay = new QVBoxLayout(this);
    rootLay->setContentsMargins(0,0,0,0);
    rootLay->setSpacing(0);

    // ===== 顶部一行：文件按钮 + tabs =====
    topBar = new QWidget(this);
    topBar->setObjectName("topBar");
    topBar->setAttribute(Qt::WA_StyledBackground, true);

    auto* topLay = new QHBoxLayout(topBar);
    topLay->setContentsMargins(8, 4, 8, 0);
    topLay->setSpacing(6);

    btnFile = new QToolButton(topBar);
    btnFile->setObjectName("btnFile");
    btnFile->setText(tr("文件"));
    btnFile->setPopupMode(QToolButton::InstantPopup);
    btnFile->setMenu(nullptr);

    tabs = new QTabBar(topBar);
    tabs->setObjectName("tabsBar");
    tabs->setExpanding(false);
    tabs->setMovable(false);
    tabs->setUsesScrollButtons(true);
    tabs->setDrawBase(false);

    tabs->addTab(tr("数据管理"));
    tabs->addTab(tr("地质建模"));
    tabs->addTab(tr("压裂模拟"));
    tabs->addTab(tr("数值模拟"));
    tabs->addTab(tr("井轨迹设计"));
    tabs->addTab(tr("曲线分析"));
    tabs->addTab(tr("二维分析"));
    tabs->addTab(tr("三维分析"));
    tabs->addTab(tr("工具/帮助"));

    topLay->addWidget(btnFile);
    topLay->addWidget(tabs, 1);

    // ===== 第二行：pages =====
    pages = new QStackedWidget(this);
    pages->setObjectName("ribbonPages");
    pages->setFixedHeight(110);

    pages->addWidget(buildRibbonPage("DATA"));
    pages->addWidget(buildRibbonPage("GEO"));
    pages->addWidget(buildRibbonPage("FRAC"));
    pages->addWidget(buildRibbonPage("NUM"));
    pages->addWidget(buildRibbonPage("TRAJ"));
    pages->addWidget(buildRibbonPage("CURVE"));
    pages->addWidget(buildRibbonPage("2D"));
    pages->addWidget(buildRibbonPage("3D"));
    pages->addWidget(buildRibbonPage("TOOLS"));

    // 文件快捷栏（默认隐藏）
    buildFileQuickBar();
    fileQuickBar->setVisible(false);

    rootLay->addWidget(topBar);
    rootLay->addWidget(fileQuickBar);
    rootLay->addWidget(pages);

    // tab 切换 -> page 切换
    connect(tabs, &QTabBar::currentChanged, pages, &QStackedWidget::setCurrentIndex);
    connect(tabs, &QTabBar::currentChanged, this, &RibbonBar::tabChanged);

    tabs->setCurrentIndex(0);
    pages->setCurrentIndex(0);

    // 文件按钮点击 -> 发信号给 MainWindow
    connect(btnFile, &QToolButton::clicked, this, &RibbonBar::fileClicked);

    // 样式（基本沿用你原来的）
    setStyleSheet(R"(
        QWidget#ribbonRoot { background: #f3f3f3; }
        QWidget#topBar { background: #f3f3f3; }

        QToolButton#btnFile {
            background: #1f5fbf;
            color: white;
            border: 0px;
            padding: 6px 10px;
            border-radius: 4px;
        }
        QToolButton#btnFile::menu-indicator { image: none; }

        QTabBar::tab {
            height: 30px;
            padding: 0px 14px;
            margin: 0px 6px;
            background: transparent;
            border: none;
            color: #222;
        }
        QTabBar::tab:selected {
            background: white;
            color: #1f5fbf;
            border: 1px solid #d0d0d0;
            border-bottom: 0px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
        }

        QStackedWidget#ribbonPages {
            background: white;
            border-top: 1px solid #d0d0d0;
        }
    )");

}


static QToolButton* makeRibbonBtn(QWidget* parent,
                                  const QIcon& icon,
                                  const QString& text,
                                  const QString& tooltip = QString())
{
    auto* b = new QToolButton(parent);
    b->setIcon(icon);
    b->setText(text);
    b->setToolTip(tooltip.isEmpty() ? text : tooltip);

    b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    b->setIconSize(QSize(24, 24));   // 你 pages 高度 95，这里 24 更协调
    b->setFixedSize(68, 60);         // 统一按钮大小
    b->setAutoRaise(true);
    b->setFocusPolicy(Qt::NoFocus);

    return b;
}


QWidget* RibbonBar::buildRibbonPage(const QString& key)
{
    qDebug() << "[RibbonBar] buildRibbonPage key=" << key;

    // 用 style() 没问题（RibbonBar 继承 QWidget），但我建议取一次避免重复调用
    QStyle* st = this->style();


    // ===== 你的资源 SVG 图标（从 qrc 读取）=====
    const QIcon icoNew    = rIcon("file_new.svg");
    const QIcon icoOpen   = rIcon("file_open.svg");
    const QIcon icoSave   = rIcon("file_save.svg");
    const QIcon icoClose  = rIcon("file_close.svg");
    const QIcon icoImport = rIcon("import.svg");
    const QIcon icoExport = rIcon("export.svg");


    // ===== 给“点/线/场/图/井位/曲线”准备图标 =====
    // 这些是 Qt 自带的标准图标里“相对合适”的占位；后面你想更像工程软件，我们再换成统一 SVG
    const QIcon icoPoint = st->standardIcon(QStyle::SP_FileDialogListView);
    const QIcon icoLine  = st->standardIcon(QStyle::SP_FileDialogDetailedView);
    const QIcon icoField = st->standardIcon(QStyle::SP_FileDialogContentsView);
    const QIcon icoImg   = st->standardIcon(QStyle::SP_FileIcon);
    const QIcon icoWell  = st->standardIcon(QStyle::SP_DirIcon);
    const QIcon icoCurve = st->standardIcon(QStyle::SP_ComputerIcon);


    QWidget* page = new QWidget(this);
    page->setObjectName("page_" + key);

    auto* pageLay = new QHBoxLayout(page);
    pageLay->setContentsMargins(10, 8, 10, 8);
    pageLay->setSpacing(10);

    auto makeGroup = [&](const QString& title, const QStringList& buttons)->QWidget*
    {
        QWidget* group = new QWidget(page);
        auto* v = new QVBoxLayout(group);
        v->setContentsMargins(8, 6, 8, 4);
        v->setSpacing(4);

        QWidget* btnArea = new QWidget(group);
        auto* h = new QHBoxLayout(btnArea);
        h->setContentsMargins(0,0,0,0);
        h->setSpacing(8);

        // ✅ iconFor 放到循环外，只定义一次
        auto iconFor = [&](const QString& groupTitle, const QString& text)->QIcon
        {
            if (groupTitle == tr("工区")) {
                if (text == tr("新建")) return icoNew;
                if (text == tr("打开")) return icoOpen;
                if (text == tr("保存")) return icoSave;
                if (text == tr("关闭")) return icoClose;
                if (text == tr("导入")) return icoImport;
                if (text == tr("导出")) return icoExport;
            }

            if (groupTitle == tr("数据导入") || groupTitle == tr("数据导出")) {
                if (text == tr("点数据")) return icoPoint;
                if (text == tr("线数据")) return icoLine;
                if (text == tr("场数据")) return icoField;
                if (text == tr("图文件")) return icoImg;
                if (text == tr("井位"))   return icoWell;
                if (text == tr("曲线"))   return icoCurve;
            }

            return QIcon();
        };

        // ✅ 只保留这一层循环
        for (const QString& text : buttons)
        {
            QIcon ico = iconFor(title, text);

            QToolButton* b = nullptr;
            if (!ico.isNull()) {
                b = makeRibbonBtn(btnArea, ico, text);
            } else {
                b = new QToolButton(btnArea);
                b->setText(text);
                b->setToolTip(text);
                b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
                b->setIconSize(QSize(24,24));
                b->setFixedSize(68, 60);
                b->setAutoRaise(true);
                b->setFocusPolicy(Qt::NoFocus);
            }

            // ✅ 关键：如果它就是“工区组”的“导入”按钮，就发信号给外部
            if (title == tr("工区") && text == tr("导入")) {
                connect(b, &QToolButton::clicked, this, &RibbonBar::importParamsRequested);
            }

            h->addWidget(b);
        }

        h->addStretch();

        auto* lbl = new QLabel(title, group);
        lbl->setAlignment(Qt::AlignHCenter);
        lbl->setStyleSheet("color:#666; font-size:12px; padding-top:2px;");

        v->addWidget(btnArea);
        v->addWidget(lbl);
        return group;
    };

    if (key == "DATA") {
        pageLay->addWidget(makeGroup(tr("工区"),
                                     {tr("新建"), tr("打开"), tr("保存"),
                                      tr("关闭"), tr("导入"), tr("导出")}));

        pageLay->addWidget(makeGroup(tr("数据导入"),
                                     {tr("点数据"), tr("线数据"), tr("场数据"),
                                      tr("图文件"), tr("井位"), tr("曲线")}));

        pageLay->addWidget(makeGroup(tr("数据导出"),
                                     {tr("点数据"), tr("线数据"), tr("场数据"),
                                      tr("图文件"), tr("井位"), tr("曲线")}));

        pageLay->addStretch(1);
    } else {
        pageLay->addWidget(makeGroup(key, {tr("功能1"), tr("功能2"), tr("功能3"),
                                           tr("功能4"), tr("功能5"), tr("功能6")}));
        pageLay->addStretch(1);
    }

    // 给 page 里的按钮一个统一 hover（你原来只给 fileQuickBar 写了 hover）
    page->setStyleSheet(R"(
        QToolButton:hover { background:#eef4ff; border-radius:6px; }
        QToolButton:pressed { background:#dbe9ff; border-radius:6px; }
    )");

    return page;
}

void RibbonBar::buildFileQuickBar()
{
    fileQuickBar = new QWidget(this);
    fileQuickBar->setObjectName("fileQuickBar");

    auto* lay = new QHBoxLayout(fileQuickBar);
    lay->setContentsMargins(10, 6, 10, 6);
    lay->setSpacing(10);

    auto makeBtn = [&](const QIcon& icon, const QString& text){
        auto* b = new QToolButton(fileQuickBar);
        b->setIcon(icon);
        b->setText(text);
        b->setAutoRaise(true);
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setIconSize(QSize(20,20));
        b->setFixedSize(64, 54);
        return b;
    };


    lay->addWidget(makeBtn(rIcon("file_new.svg"),   tr("新建")));
    lay->addWidget(makeBtn(rIcon("file_open.svg"),  tr("打开")));
    lay->addWidget(makeBtn(rIcon("file_save.svg"),  tr("保存")));
    lay->addWidget(makeBtn(QIcon(),                 tr("打印"))); // 你还没打印图标就先空着
    lay->addWidget(makeBtn(rIcon("file_close.svg"), tr("关闭")));

    lay->addStretch();

    fileQuickBar->setStyleSheet(R"(
        QWidget#fileQuickBar { background: white; border-top: 1px solid #d0d0d0; }
        QToolButton:hover { background:#eef4ff; border-radius:6px; }
    )");
}


