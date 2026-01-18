#ifndef BACKSTAGEDIALOG_H
#define BACKSTAGEDIALOG_H
#pragma once
#include <QDialog>

class QListWidget;
class QStackedWidget;

class BackstageDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BackstageDialog(QWidget* parent = nullptr);

signals:
    void requestOpen();     // 点击“打开...”
    void requestExit();     // 点击“退出”
    void requestNew();      // 以后可以加“新建”

private:
    void buildUi();

    QListWidget* nav = nullptr;
    QStackedWidget* stack = nullptr;
};

#endif // BACKSTAGEDIALOG_H
