#include "datapanel.h"

#include <QVBoxLayout>
#include <QToolBar>
#include <QWidget>
#include <QStyle>

DataPanel::DataPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(4,4,4,4);
    lay->setSpacing(4);

    // ===== 顶部工具条 =====
    auto* tb = new QToolBar(this);
    tb->setIconSize(QSize(16,16));
    tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
    tb->setMovable(false);
    tb->setFloatable(false);

    QStyle* st = this->style();

    // 用 Qt 标准图标先占位（后期你可以换成 SVG）
    QAction* actAdd    = tb->addAction(st->standardIcon(QStyle::SP_FileIcon),          tr("新建"));
    QAction* actOpen   = tb->addAction(st->standardIcon(QStyle::SP_DirOpenIcon),       tr("打开"));
    QAction* actSave   = tb->addAction(st->standardIcon(QStyle::SP_DialogSaveButton),  tr("保存"));
    QAction* actDelete = tb->addAction(st->standardIcon(QStyle::SP_TrashIcon),         tr("删除"));

    tb->addSeparator();

    QAction* actMore   = tb->addAction(st->standardIcon(QStyle::SP_TitleBarMenuButton), tr("更多"));

    // hover / pressed 风格（和 Ribbon 保持一致）
    tb->setStyleSheet(R"(
        QToolButton {
            border: none;
            padding: 2px;
            margin: 0px 2px;
        }
        QToolButton:hover {
            background:#eef4ff;
            border-radius:4px;
        }
        QToolButton:pressed {
            background:#dbe9ff;
        }
    )");

    // ===== 内容区 =====
    auto* body = new QWidget(this);
    body->setStyleSheet("background:#fff; border:1px solid #ddd; border-radius:4px;");

    lay->addWidget(tb);
    lay->addWidget(body, 1);
}

