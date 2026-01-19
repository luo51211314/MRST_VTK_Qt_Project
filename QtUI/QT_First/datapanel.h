#ifndef DATAPANEL_H
#define DATAPANEL_H
#pragma once
#include <QWidget>
#include <QVector>

class QStackedWidget;
class QButtonGroup;
class QToolButton;

class DataPanel : public QWidget
{
    Q_OBJECT
public:
    explicit DataPanel(QWidget* parent=nullptr);

protected:
    void resizeEvent(QResizeEvent* e) override;


signals:
    // 如果你以后希望 MainWindow 知道当前页（可选）
    void pageChanged(int index);


private:
    QWidget* buildPage(const QString& title);   // 先用占位页
    QWidget* buildNavBar();
    QToolButton* addNavBtn(const QString& text, int idx);
    QVector<QToolButton*> m_navBtns;
    void updateNavCompact();
private:
    QStackedWidget* m_stack = nullptr;
    QButtonGroup*   m_group = nullptr;
};


#endif // DATAPANEL_H
