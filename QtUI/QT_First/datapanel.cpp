#include "datapanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QStackedWidget>
#include <QToolButton>
#include <QButtonGroup>
#include <QLabel>
#include <QFrame>
#include <QStyle>

static QWidget* makeHeaderBar(const QString& title, QWidget* parent)
{
    auto* bar = new QWidget(parent);
    bar->setObjectName("panelHeader");
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(6, 4, 6, 4);
    h->setSpacing(6);

    auto* lbl = new QLabel(title, bar);
    lbl->setObjectName("panelTitle");

    auto* tb = new QToolBar(bar);
    tb->setIconSize(QSize(16,16));
    tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
    tb->setMovable(false);
    tb->setFloatable(false);

    QStyle* st = bar->style();
    tb->addAction(st->standardIcon(QStyle::SP_ArrowUp),        QObject::tr("上移"));
    tb->addAction(st->standardIcon(QStyle::SP_ArrowDown),      QObject::tr("下移"));
    tb->addAction(st->standardIcon(QStyle::SP_DialogCloseButton), QObject::tr("删除"));
    tb->addAction(st->standardIcon(QStyle::SP_FileDialogNewFolder), QObject::tr("新建"));

    h->addWidget(lbl);
    h->addStretch(1);
    h->addWidget(tb);

    // header + toolbar 的 hover 风格
    bar->setStyleSheet(R"(
        QWidget#panelHeader{ background:#f3f3f3; border-bottom:1px solid #d0d0d0; }
        QLabel#panelTitle{ color:#1f5fbf; font-weight:600; }
        QToolButton{ border:none; padding:2px; margin:0px 2px; }
        QToolButton:hover{ background:#eef4ff; border-radius:4px; }
        QToolButton:pressed{ background:#dbe9ff; }
    )");

    return bar;
}

DataPanel::DataPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0,0,0,0);
    lay->setSpacing(0);

    // ===== 1) 内容区：stack =====
    m_stack = new QStackedWidget(this);
    m_stack->setObjectName("dataStack");

    m_stack->addWidget(buildPage(tr("输入")));  // index 0
    m_stack->addWidget(buildPage(tr("建模")));  // index 1
    m_stack->addWidget(buildPage(tr("数模")));  // index 2
    m_stack->addWidget(buildPage(tr("压裂")));  // index 3
    m_stack->addWidget(buildPage(tr("图形")));  // index 4

    // ===== 2) 底部导航条 =====
    QWidget* nav = buildNavBar();

    lay->addWidget(m_stack, 1);
    lay->addWidget(nav, 0);

    // 默认页
    m_stack->setCurrentIndex(0);
    if (auto* b0 = m_group->button(0)) b0->setChecked(true);

    connect(m_group, &QButtonGroup::idClicked, this, [=](int id){
        m_stack->setCurrentIndex(id);
        emit pageChanged(id); // 可选
    });

    // 全局风格：去边框、底部导航条像 Compass
    setStyleSheet(R"(
        QStackedWidget#dataStack{ background:white; border:none; }
        QWidget#navBar{
            background:#f3f3f3;
            border-top:1px solid #d0d0d0;
        }

        /* ✅ 底部按钮：图标贴左 + 文字紧跟 */
        QToolButton#navBtn{
            padding: 2px 4px;              /* 左右 padding 小一点 */
            border: 1px solid transparent;
            border-radius: 6px;
            background: transparent;

            /* ✅ 关键：左对齐 */
            qproperty-toolButtonStyle: ToolButtonTextBesideIcon;
            text-align: left;
        }

        /* ✅ 关键：把图标和文字的间距压小（Qt 支持 menu-indicator，icon spacing 用 margin 控） */
        QToolButton#navBtn::menu-indicator { image: none; }

        QToolButton#navBtn:checked{
            background: white;
            border-color:#cfd6e4;
        }
    )");


    // 允许面板被压缩到更窄（关键）
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    setMinimumWidth(60);                 // 你想多窄都行，先给个很小的下限
    if (m_stack) m_stack->setMinimumWidth(0);

    updateNavCompact();


}

QWidget* DataPanel::buildPage(const QString& title)
{
    // 每一页长这样：
    // [标题+工具条]
    // [白色内容区占位]
    auto* page = new QWidget(this);
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(0,0,0,0);
    v->setSpacing(0);

    v->addWidget(makeHeaderBar(title, page));

    auto* body = new QFrame(page);
    body->setFrameShape(QFrame::NoFrame);
    body->setStyleSheet("background:white;"); // 你以后把 tree/列表等放这里
    v->addWidget(body, 1);

    return page;
}

QWidget* DataPanel::buildNavBar()
{
    auto* nav = new QWidget(this);
    nav->setObjectName("navBar");
    auto* h = new QHBoxLayout(nav);
    h->setContentsMargins(6, 4, 6, 4);
    h->setSpacing(6);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);

    addNavBtn(tr("输入"), 0);
    addNavBtn(tr("建模"), 1);
    addNavBtn(tr("数模"), 2);
    addNavBtn(tr("压裂"), 3);
    addNavBtn(tr("图形"), 4);

    //h->addStretch(1);
    return nav;
}

QToolButton* DataPanel::addNavBtn(const QString& text, int idx)
{
    auto* b = new QToolButton(this);
    b->setObjectName("navBtn");
    b->setText(text);
    // 保存全称（宽时用）
    b->setProperty("fullText", text);
    // 保存默认至少显示 1 个字（窄时用）
    b->setProperty("shortText", text.left(1));

    b->setCheckable(true);
    b->setAutoRaise(true);
    b->setFocusPolicy(Qt::NoFocus);
    b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    b->setLayoutDirection(Qt::LeftToRight);
    b->setStyleSheet("QToolButton { text-align:left; }");


    // ✅ 给图标（先用标准图标占位，后面换你的 SVG）
    QStyle* st = style();
    switch (idx) {
    case 0: b->setIcon(st->standardIcon(QStyle::SP_FileDialogContentsView)); break; // 输入
    case 1: b->setIcon(st->standardIcon(QStyle::SP_DirIcon)); break;               // 建模
    case 2: b->setIcon(st->standardIcon(QStyle::SP_ComputerIcon)); break;          // 数模
    case 3: b->setIcon(st->standardIcon(QStyle::SP_BrowserReload)); break;         // 压裂
    case 4: b->setIcon(st->standardIcon(QStyle::SP_DesktopIcon)); break;           // 图形
    default: break;
    }
    b->setIconSize(QSize(10,10));

    // ✅ 关键：先默认“宽模式”
    b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    // 让按钮不要把最小宽度卡死（否则拖不动）
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    b->setMinimumWidth(28);     // 窄到极限也至少放下“图标+1字”
    b->setFixedHeight(26);


    // 加入布局
    auto* nav = findChild<QWidget*>("navBar");
    if (nav && nav->layout())
        static_cast<QHBoxLayout*>(nav->layout())->addWidget(b);

    m_group->addButton(b, idx);
    m_navBtns.push_back(b);   // ✅ 记录下来，resize 时统一切换

    return b;
}
void DataPanel::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    updateNavCompact();
}

void DataPanel::updateNavCompact()
{
    const int w = width();

    // ✅ 阈值：你可以调，越大代表“更早显示全称”
    const bool showFull = (w >= 220);

    for (auto* b : m_navBtns) {
        if (!b) continue;

        const QString full  = b->property("fullText").toString();
        const QString short1 = b->property("shortText").toString();

        b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

        if (showFull) {
            b->setText(full);                // 宽：输入/建模/...
            b->setIconSize(QSize(18,18));
            b->setMinimumWidth(58);
        } else {
            b->setText(short1);              // ✅ 默认：输/建/数/压/图（保证有字）
            b->setIconSize(QSize(16,16));
            b->setMinimumWidth(30);
        }
    }
}



