#include "flowpanel.h"

#include <QTreeWidget>
#include <QVBoxLayout>
#include <QTreeWidgetItem>

FlowPanel::FlowPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0,0,0,0);
    lay->setSpacing(0);

    tree = new QTreeWidget(this);
    tree->setHeaderHidden(true);

    // 和你原来的结构一致
    auto* root = new QTreeWidgetItem(tree, QStringList() << tr("工程"));
    auto* itNew  = new QTreeWidgetItem(root, QStringList() << tr("新建工区(NEW PROJECT)"));
    auto* itOpen = new QTreeWidgetItem(root, QStringList() << tr("打开工区(OPEN PROJECT)"));
    root->setExpanded(true);

    lay->addWidget(tree, 1);

    // 点击节点触发信号
    connect(tree, &QTreeWidget::itemClicked, this, [this, itNew, itOpen](QTreeWidgetItem* item, int){
        if (item == itNew)  emit newProjectRequested();
        if (item == itOpen) emit openProjectRequested();
    });
}

