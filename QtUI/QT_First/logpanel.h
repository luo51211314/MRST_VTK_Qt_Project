#ifndef LOGPANEL_H
#define LOGPANEL_H
#pragma once
#include <QWidget>

class QPlainTextEdit;

class LogPanel : public QWidget
{
    Q_OBJECT
public:
    explicit LogPanel(QWidget* parent = nullptr);

    void append(const QString& text);
    void clear();

private:
    QPlainTextEdit* edit = nullptr;
};

#endif // LOGPANEL_H
