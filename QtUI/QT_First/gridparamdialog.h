#ifndef GRIDPARAMDIALOG_H
#define GRIDPARAMDIALOG_H
#pragma once
#include <QDialog>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include "QtUItoAlgoInterface.h"

class GridParamDialog : public QDialog {
    Q_OBJECT
public:
    explicit GridParamDialog(QWidget* parent=nullptr);

    QtUItoAlgo::GridParameters params() const;

private:
    QSpinBox *nx_, *ny_, *nz_;
    QDoubleSpinBox *lx_, *ly_, *lz_;
};

#endif // GRIDPARAMDIALOG_H
