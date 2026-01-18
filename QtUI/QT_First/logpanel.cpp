#include "logpanel.h"

#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>

LogPanel::LogPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    // 顶部小工具条：清空
    auto* bar = new QWidget(this);
    auto* barLay = new QHBoxLayout(bar);
    barLay->setContentsMargins(6,4,6,4);
    barLay->setSpacing(6);

    auto* btnClear = new QToolButton(bar);
    btnClear->setText(tr("清空"));
    btnClear->setAutoRaise(true);

    barLay->addStretch();
    barLay->addWidget(btnClear);

    edit = new QPlainTextEdit(this);
    edit->setReadOnly(true);

    root->addWidget(bar);
    root->addWidget(edit, 1);

    connect(btnClear, &QToolButton::clicked, this, &LogPanel::clear);
}

void LogPanel::append(const QString& text)
{
    if (edit) edit->appendPlainText(text);
}

void LogPanel::clear()
{
    if (edit) edit->clear();
}
