#ifndef PROPERTYPANEL_H
#define PROPERTYPANEL_H
#pragma once
#include <QWidget>

class QTableWidget;

class PropertyPanel : public QWidget
{
    Q_OBJECT
public:
    explicit PropertyPanel(QWidget* parent = nullptr);

    void setPropertiesForLayer(const QString& layerName);

private:
    QTableWidget* table = nullptr;
};

#endif // PROPERTYPANEL_H
