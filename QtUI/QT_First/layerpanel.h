#ifndef LAYERPANEL_H
#define LAYERPANEL_H

#pragma once
#include <QWidget>

class QListWidget;

class LayerPanel : public QWidget
{
    Q_OBJECT
public:
    explicit LayerPanel(QWidget* parent = nullptr);

    QString currentLayer() const;

signals:
    void layerChanged(const QString& name);

private:
    QListWidget* list = nullptr;
};
#endif // LAYERPANEL_H
