#ifndef PARAMDIALOG_H
#define PARAMDIALOG_H

#pragma once
#include <QDialog>
#include <QVector>
#include <QString>

class QTableWidget;

class ParamDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ParamDialog(QWidget* parent = nullptr);

    // 返回用户输入的参数（name,value）
    QVector<QPair<QString, QString>> params() const;

    // 也可以直接导出为CSV文本
    QString toCsvText() const;

private:
    QTableWidget* table = nullptr;

    void addRow(const QString& name,
                const QString& value,
                bool isFixed = false);

};


#endif // PARAMDIALOG_H
