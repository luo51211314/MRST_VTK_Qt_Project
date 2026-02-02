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
    tabs->addTab(tr("远程渲染"));
    tabs->addTab(tr("工具/帮助"));

    topLay->addWidget(btnFile);
    topLay->addWidget(tabs, 1);

    // ===== 第二行：pages =====
    pages = new QStackedWidget(this);
    pages->setObjectName("ribbonPages");
    pages->setFixedHeight(100);

    pages->addWidget(buildRibbonPage("DATA"));
    pages->addWidget(buildRibbonPage("GEO"));
    pages->addWidget(buildRibbonPage("FRAC"));
    pages->addWidget(buildRibbonPage("NUM"));
    pages->addWidget(buildRibbonPage("TRAJ"));
    pages->addWidget(buildRibbonPage("CURVE"));
    pages->addWidget(buildRibbonPage("2D"));
    pages->addWidget(buildRibbonPage("3D"));
    pages->addWidget(buildRibbonPage("RR"));
    pages->addWidget(buildRibbonPage("TOOLS"));

    // 文件快捷栏（默认隐藏）
    buildFileQuickBar();
    fileQuickBar->setVisible(false);

    rootLay->addWidget(topBar);
    rootLay->addWidget(fileQuickBar);
    rootLay->addWidget(pages);

    // tab 切换 -> page 切换
    // 文件按钮点击 -> 切换文件模式
    connect(btnFile, &QToolButton::clicked, this, [this](){
        setFileMode(!btnFile->isChecked());
        emit fileClicked();
    });

    // tab 切换 -> page 切换 + 退出文件模式
    connect(tabs, &QTabBar::currentChanged, this, [this](int idx){
        setFileMode(false);
        pages->setCurrentIndex(idx);
        emit tabChanged(idx);
    });


    tabs->setCurrentIndex(0);
    pages->setCurrentIndex(0);
    setFileMode(false);


    // 文件按钮点击 -> 发信号给 MainWindow
    //connect(btnFile, &QToolButton::clicked, this, &RibbonBar::fileClicked);

    // 样式（基本沿用你原来的）
    setStyleSheet(R"(
        QWidget#ribbonRoot { background: #f3f3f3; }
        QWidget#topBar { background: #f3f3f3; }

        /* ✅ 文件按钮：默认普通 */
        QToolButton#btnFile {
            height: 30px;                 /* 和 tab 高度一致 */
            padding: 0px 14px;            /* 和 tab 一致 */
            margin: 0px 6px;              /* 和 tab 一致 */
            background: transparent;
            border: none;
            color: #222;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
        }

        /* hover：轻微底色（不要像按钮那样一整块） */
        QToolButton#btnFile:hover {
            background: rgba(255,255,255,0.55);
        }

        /* 选中：完全复刻 QTabBar::tab:selected */
        QToolButton#btnFile:checked {
            background: #1f5fbf;
            color: white;
            border: 1px solid #1f5fbf;
            border-bottom: 0px;
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
    b->setFixedSize(60, 56);         // 统一按钮大小
    b->setAutoRaise(true);
    b->setFocusPolicy(Qt::NoFocus);


    b->setStyleSheet(R"(
    QToolButton{
        padding: 2px 4px;
        margin: 0px;
        border: 1px solid transparent;
        border-radius: 5px;
    }
    QToolButton:hover{
        background:#F3F6FB;
        border-color:#C9D6EA;
    }
    QToolButton:pressed{
        background:#E7EEF9;
    }
    )");
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
    pageLay->setContentsMargins(6, 6, 6, 6);
    pageLay->setSpacing(6);

    auto makeVLine = [&](QWidget* parent)->QWidget*
    {
        QWidget* box = new QWidget(parent);
        auto* lay = new QVBoxLayout(box);
        lay->setContentsMargins(0,0,0,0);
        lay->addStretch();

        auto* line = new QFrame(box);
        line->setFrameShape(QFrame::VLine);
        line->setLineWidth(1);
        line->setStyleSheet("color:#D6DCE6;");
        line->setFixedHeight(86);

        lay->addWidget(line);
        lay->addStretch();
        return box;
    };


    auto makeGroup = [&](const QString& title, const QStringList& buttons)->QWidget*


    {
        QWidget* group = new QWidget(page);
        auto* v = new QVBoxLayout(group);
        v->setContentsMargins(6, 4, 6, 4);
        v->setSpacing(2);

        QWidget* btnArea = new QWidget(group);
        auto* h = new QHBoxLayout(btnArea);
        h->setContentsMargins(0,0,0,0);
        h->setSpacing(3);

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

            // ===== 数值模拟：仿真控制 =====
            if (groupTitle == tr("仿真控制")) {
                if (text == tr("开始")) return st->standardIcon(QStyle::SP_MediaPlay);
                if (text == tr("暂停")) return st->standardIcon(QStyle::SP_MediaPause);
                if (text == tr("停止")) return st->standardIcon(QStyle::SP_MediaStop);
                if (text == tr("重置")) return st->standardIcon(QStyle::SP_BrowserReload);
            }

            // ===== 数值模拟：仿真设置 =====
            if (groupTitle == tr("仿真设置")) {
                if (text == tr("参数设置")) return st->standardIcon(QStyle::SP_FileDialogDetailedView);
                if (text == tr("网格设置")) return st->standardIcon(QStyle::SP_DirIcon);
                if (text == tr("物性参数")) return st->standardIcon(QStyle::SP_ComputerIcon);
                if (text == tr("裂缝参数")) return st->standardIcon(QStyle::SP_MessageBoxWarning);
            }

            // ===== 数值模拟：结果与状态 =====
            if (groupTitle == tr("结果与状态")) {
                if (text == tr("进度"))   return st->standardIcon(QStyle::SP_ArrowRight);
                if (text == tr("日志"))   return st->standardIcon(QStyle::SP_FileDialogInfoView);
                if (text == tr("导出结果")) return st->standardIcon(QStyle::SP_DialogSaveButton);
            }

            // ===== 远程渲染 =====
            if (groupTitle == tr("远程渲染")) {
                if (text == tr("开始模拟")) return st->standardIcon(QStyle::SP_MediaPlay);
                if (text == tr("停止模拟")) return st->standardIcon(QStyle::SP_MediaStop);
                if (text == tr("连接"))     return st->standardIcon(QStyle::SP_DialogYesButton);
                if (text == tr("断开"))     return st->standardIcon(QStyle::SP_DialogNoButton);
                if (text == tr("截图"))     return st->standardIcon(QStyle::SP_DialogOpenButton);
                if (text == tr("录屏"))     return st->standardIcon(QStyle::SP_DialogApplyButton);
                if (text == tr("发送参数"))  return st->standardIcon(QStyle::SP_BrowserReload);

                // FPS 我建议不放图标，做成“纯文字小按钮”，返回空图标
                if (text.startsWith("FPS")) return QIcon();
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
                b->setAutoRaise(true);
                b->setFocusPolicy(Qt::NoFocus);

                // ✅ FPS：做成小文字按钮，更像“档位选择”
                if (title == tr("远程渲染") && text.startsWith("FPS")) {
                    b->setToolButtonStyle(Qt::ToolButtonTextOnly);
                    b->setFixedSize(54, 28);
                    b->setStyleSheet(R"(
                        QToolButton{
                            border: 1px solid #D6DCE6;
                            border-radius: 6px;
                            background: transparent;
                            padding: 0px 8px;
                        }
                        QToolButton:hover{ background:#F3F6FB; border-color:#C9D6EA; }
                        QToolButton:pressed{ background:#E7EEF9; }
                    )");
                } else {
                    // 其它没图标的保持原样
                    b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
                    b->setIconSize(QSize(24,24));
                    b->setFixedSize(68, 60);
                }
            }


            // ✅ 关键：如果它就是“工区组”的“导入”按钮，就发信号给外部
            if (title == tr("工区") && text == tr("导入")) {
                connect(b, &QToolButton::clicked, this, &RibbonBar::importParamsRequested);
            }
            if (title == tr("工区") && text == tr("新建")) {
                connect(b, &QToolButton::clicked, this, &RibbonBar::newProjectRequested);
            }
            if (title == tr("工区") && text == tr("打开")) {
                connect(b, &QToolButton::clicked, this, &RibbonBar::openProjectRequested);
            }


            // ===== 数值模拟：仿真控制 =====
            if (title == tr("仿真控制")) {
                if (text == tr("开始")) {
                    connect(b, &QToolButton::clicked, this, &RibbonBar::startSimulationRequested);
                }
                else if (text == tr("暂停")) {
                    connect(b, &QToolButton::clicked, this, &RibbonBar::pauseSimulationRequested);
                }
                else if (text == tr("停止")) {
                    connect(b, &QToolButton::clicked, this, &RibbonBar::stopSimulationRequested);
                }
                else if (text == tr("重置")) {
                    connect(b, &QToolButton::clicked, this, &RibbonBar::resetSimulationRequested);
                }
            }

            // ===== 数值模拟：仿真设置 =====
            if (title == tr("仿真设置")) {
                if (text == tr("网格设置")) {
                    connect(b, &QToolButton::clicked, this, [this](){
                        emit gridParamsRequested();
                    });
                } else if (text == tr("物性参数")) {
                    connect(b, &QToolButton::clicked, this, [this](){
                        emit fluidParamsRequested();
                    });
                } else if (text == tr("参数设置")) {
                    connect(b, &QToolButton::clicked, this, [this](){
                        emit simParamsRequested();
                    });
                } else if (text == tr("裂缝参数")) {
                    connect(b, &QToolButton::clicked, this, [this](){
                        emit fractureParamsRequested();
                    });
                }
            }

            // ===== 三维分析：远程渲染（统一成组）=====
            if (title == tr("远程渲染")) {
                if (text == tr("开始模拟")) {
                    connect(b, &QToolButton::clicked, this, &RibbonBar::startRemoteRenderMockRequested);
                }
                else if (text == tr("停止模拟")) {
                    connect(b, &QToolButton::clicked, this, &RibbonBar::stopRemoteRenderMockRequested);
                }
                else if (text == tr("连接")) {
                    connect(b, &QToolButton::clicked, this, &RibbonBar::connectRemoteRenderRequested);
                }
                else if (text == tr("断开")) {
                    connect(b, &QToolButton::clicked, this, &RibbonBar::disconnectRemoteRenderRequested);
                }
                else if (text == tr("截图")) {
                    connect(b, &QToolButton::clicked, this, &RibbonBar::screenshotRemoteRenderRequested);
                }
                else if (text == tr("录屏")) {
                    connect(b, &QToolButton::clicked, this, &RibbonBar::recordRemoteRenderRequested);
                }
                else if (text == tr("发送参数")) {
                    connect(b, &QToolButton::clicked, this, &RibbonBar::syncParamsToVtkRequested);
                }

                // FPS：先用三个按钮占位（15/30/60），后续也可以换成下拉框
                else if (text == tr("FPS15")) {
                    connect(b, &QToolButton::clicked, this, [this](){ emit setRemoteRenderFpsRequested(15); });
                }
                else if (text == tr("FPS30")) {
                    connect(b, &QToolButton::clicked, this, [this](){ emit setRemoteRenderFpsRequested(30); });
                }
                else if (text == tr("FPS60")) {
                    connect(b, &QToolButton::clicked, this, [this](){ emit setRemoteRenderFpsRequested(60); });
                }
            }






            h->addWidget(b);
        }

        h->addStretch();

        auto* lbl = new QLabel(title, group);
        lbl->setAlignment(Qt::AlignHCenter);
        lbl->setFixedHeight(16);
        lbl->setStyleSheet("color:#4A5568; font-size:12px; padding:0px; margin:0px;");


        v->addWidget(btnArea);
        v->addWidget(lbl);
        return group;
    };

    if (key == "DATA") {
        pageLay->addWidget(makeGroup(tr("工区"),
                                     {tr("新建"), tr("打开"), tr("保存"),
                                      tr("关闭"), tr("导入"), tr("导出")}));
        pageLay->addWidget(makeVLine(page));

        pageLay->addWidget(makeGroup(tr("数据导入"),
                                     {tr("点数据"), tr("线数据"), tr("场数据"),
                                      tr("图文件"), tr("井位"), tr("曲线")}));
        pageLay->addWidget(makeVLine(page));

        pageLay->addWidget(makeGroup(tr("数据导出"),
                                     {tr("点数据"), tr("线数据"), tr("场数据"),
                                      tr("图文件"), tr("井位"), tr("曲线")}));

        pageLay->addStretch(1);
    }  else if (key == "NUM") {

        // ===== 仿真控制 =====
        pageLay->addWidget(makeGroup(tr("仿真控制"),
                                     {tr("开始"), tr("暂停"), tr("停止"), tr("重置")}));
        pageLay->addWidget(makeVLine(page));

        // ===== 仿真设置 =====
        pageLay->addWidget(makeGroup(tr("仿真设置"),
                                     {tr("参数设置"), tr("网格设置"), tr("物性参数"),tr("裂缝参数")}));
        pageLay->addWidget(makeVLine(page));

        // ===== 结果与状态 =====
        pageLay->addWidget(makeGroup(tr("结果与状态"),
                                     {tr("进度"), tr("日志"), tr("导出结果")}));

        pageLay->addStretch(1);

        } else if (key == "RR") {

        // ✅ 远程渲染：独立成组（后续都往这组加）
        pageLay->addWidget(makeGroup(tr("远程渲染"),
                                     {tr("开始模拟"), tr("停止模拟"),
                                      tr("连接"), tr("断开"),
                                      tr("截图"), tr("录屏"),
                                      tr("发送参数"),
                                      tr("FPS15"), tr("FPS30"), tr("FPS60")}));
        pageLay->addWidget(makeVLine(page));


        pageLay->addStretch(1);

        }else {
            pageLay->addWidget(makeGroup(key,
                                         {tr("功能1"), tr("功能2"), tr("功能3"),
                                          tr("功能4"), tr("功能5"), tr("功能6")}));
            pageLay->addStretch(1);
        }





    // 给 page 里的按钮一个统一 hover（你原来只给 fileQuickBar 写了 hover）
    page->setStyleSheet(R"(
        QWidget[ribbonGroup="true"]{
            border: 1px solid #E3E8F0;
            border-radius: 6px;
            background: rgba(255,255,255,0.65);
        }
        QToolButton{
            padding: 2px 4px;
            margin: 0px;
            border: 1px solid transparent;
            border-radius: 5px;
        }
        QToolButton:hover{
            background:#F3F6FB;
            border-color:#C9D6EA;
        }
        QToolButton:pressed{
            background:#E7EEF9;
        }
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


