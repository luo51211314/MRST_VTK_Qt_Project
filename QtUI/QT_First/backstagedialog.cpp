#include "backstagedialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QFrame>
#include <QToolButton>
#include <QFont>

BackstageDialog::BackstageDialog(QWidget* parent)
    : QDialog(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setObjectName("backstageDlg");
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedSize(720, 420);

    buildUi();
}

void BackstageDialog::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    auto* body = new QWidget(this);
    auto* bodyLay = new QHBoxLayout(body);
    bodyLay->setContentsMargins(0,0,0,0);
    bodyLay->setSpacing(0);

    // 左侧导航
    nav = new QListWidget(body);
    nav->setObjectName("backstageNav");
    nav->setFixedWidth(160);
    nav->setIconSize(QSize(32,32));
    nav->setSpacing(6);

    auto addNav = [&](const QString& text){
        auto* it = new QListWidgetItem(text, nav);
        it->setSizeHint(QSize(150, 52));
        return it;
    };

    auto* itNew   = addNav(tr("新建"));
    auto* itOpen  = addNav(tr("打开..."));
    addNav(tr("保存"));
    addNav(tr("打印"));
    addNav(tr("关闭"));

    // 右侧页面堆栈（先只做“最近使用的工区”页）
    stack = new QStackedWidget(body);
    stack->setObjectName("backstageStack");

    auto* recentPage = new QWidget(stack);
    auto* pLay = new QVBoxLayout(recentPage);
    pLay->setContentsMargins(18, 18, 18, 18);
    pLay->setSpacing(10);

    auto* title = new QLabel(tr("最近使用的工区"), recentPage);
    QFont f = title->font(); f.setPointSize(14); f.setBold(true);
    title->setFont(f);

    auto* line = new QFrame(recentPage);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);

    auto* hint = new QLabel(tr("（后续这里可以放工程列表）"), recentPage);
    hint->setStyleSheet("color:#666;");

    pLay->addWidget(title);
    pLay->addWidget(line);
    pLay->addWidget(hint);
    pLay->addStretch();

    stack->addWidget(recentPage);

    bodyLay->addWidget(nav);
    bodyLay->addWidget(stack, 1);

    // 底部栏
    auto* bottom = new QWidget(this);
    bottom->setObjectName("backstageBottom");
    auto* bLay = new QHBoxLayout(bottom);
    bLay->setContentsMargins(10, 6, 10, 6);

    bLay->addStretch();

    auto* btnOpen = new QToolButton(bottom);
    btnOpen->setText(tr("打开..."));
    btnOpen->setAutoRaise(true);

    auto* btnExit = new QToolButton(bottom);
    btnExit->setText(tr("退出"));
    btnExit->setAutoRaise(true);

    bLay->addWidget(btnOpen);
    bLay->addWidget(btnExit);

    root->addWidget(body, 1);
    root->addWidget(bottom);

    // 导航切换（先固定显示最近页）
    connect(nav, &QListWidget::currentRowChanged, this, [this](int){
        stack->setCurrentIndex(0);
    });
    nav->setCurrentRow(0);

    // 信号：打开/退出/新建
    connect(btnOpen, &QToolButton::clicked, this, &BackstageDialog::requestOpen);
    connect(btnExit, &QToolButton::clicked, this, &BackstageDialog::requestExit);
    connect(nav, &QListWidget::itemClicked, this, [=](QListWidgetItem* it){
        if (it == itNew)  emit requestNew();
        if (it == itOpen) emit requestOpen();
    });

    // 样式
    setStyleSheet(R"(
        QDialog#backstageDlg {
            background: white;
            border: 1px solid #cfcfcf;
            border-radius: 6px;
        }

        QListWidget#backstageNav {
            background: #f5f5f5;
            border: none;
            outline: 0;
            padding: 8px;
        }
        QListWidget#backstageNav::item {
            border-radius: 6px;
            padding-left: 10px;
        }
        QListWidget#backstageNav::item:selected {
            background: #1f5fbf;
            color: white;
            border: 2px solid #0f3f8f;
        }
        QListWidget#backstageNav::item:hover {
            background: #e9eef8;
        }

        QWidget#backstageBottom {
            background: #f5f5f5;
            border-top: 1px solid #e0e0e0;
        }
        QWidget#backstageBottom QToolButton {
            padding: 4px 10px;
        }
        QWidget#backstageBottom QToolButton:hover {
            background: #e9eef8;
            border-radius: 6px;
        }
    )");
}
