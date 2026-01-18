#include "propertypanel.h"

#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>

PropertyPanel::PropertyPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0,0,0,0);
    lay->setSpacing(0);

    table = new QTableWidget(0, 2, this);
    table->setHorizontalHeaderLabels({ tr("属性名"), tr("值") });
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::DoubleClicked
                           | QAbstractItemView::EditKeyPressed
                           | QAbstractItemView::SelectedClicked);

    lay->addWidget(table, 1);
}

void PropertyPanel::setPropertiesForLayer(const QString& layerName)
{
    table->setRowCount(0);

    auto addRow = [&](const QString& k, const QString& v){
        int r = table->rowCount();
        table->insertRow(r);
        table->setItem(r, 0, new QTableWidgetItem(k));
        table->setItem(r, 1, new QTableWidgetItem(v));
    };

    addRow("Name", layerName);
    addRow("Visible", "true");
    addRow("Opacity", "1.0");
    addRow("Color", "#FFFFFF");
}
