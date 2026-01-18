#include "layerpanel.h"

#include <QListWidget>
#include <QVBoxLayout>

LayerPanel::LayerPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0,0,0,0);
    lay->setSpacing(0);

    list = new QListWidget(this);
    list->addItems({ "Layer 1", "Layer 2", "Layer 3" });

    lay->addWidget(list, 1);

    connect(list, &QListWidget::currentTextChanged, this, &LayerPanel::layerChanged);
    list->setCurrentRow(0);
}

QString LayerPanel::currentLayer() const
{
    auto* it = list ? list->currentItem() : nullptr;
    return it ? it->text() : QString();
}
