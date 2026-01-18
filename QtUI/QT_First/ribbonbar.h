#ifndef RIBBONBAR_H
#define RIBBONBAR_H

#pragma once
#include <QWidget>

class QToolButton;
class QTabBar;
class QStackedWidget;

class RibbonBar : public QWidget
{
    Q_OBJECT
public:
    explicit RibbonBar(QWidget* parent = nullptr);

    // 给 MainWindow 用：拿到“文件按钮”，用于定位 Backstage 弹窗
    QToolButton* fileButton() const;

    // 给 MainWindow 用：切换“文件模式”（显示快捷栏、隐藏功能区）
    void setFileMode(bool on);

signals:
    // Ribbon 上的按钮点击（先用信号占位，后面逐步加）
    void fileClicked();                 // 点击左上角“文件”
    void tabChanged(int index);         // Tab 切换（如果你后续要联动）
    void importParamsRequested();   // 点击“导入”时发出

private:
    void buildUi();
    QWidget* buildRibbonPage(const QString& key);
    void buildFileQuickBar();

private:
    QWidget* root = nullptr;            // 相当于你原来的 ribbonRoot
    QWidget* topBar = nullptr;          // 顶部一行（文件 + tabs）
    QWidget* fileQuickBar = nullptr;    // 文件快捷栏（文件模式显示）
    QToolButton* btnFile = nullptr;     // 文件按钮
    QTabBar* tabs = nullptr;            // Tab
    QStackedWidget* pages = nullptr;    // 功能区页面
};

#endif // RIBBONBAR_H
