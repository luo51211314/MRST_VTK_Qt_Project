#ifndef FLOWPANEL_H
#define FLOWPANEL_H

#pragma once
#include <QWidget>

class QTreeWidget;

class FlowPanel : public QWidget
{
    Q_OBJECT
public:
    explicit FlowPanel(QWidget* parent = nullptr);

signals:
    void newProjectRequested();
    void openProjectRequested();

private:
    QTreeWidget* tree = nullptr;
};

#endif // FLOWPANEL_H
