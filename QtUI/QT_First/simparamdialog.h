#ifndef SIMPARAMDIALOG_H
#define SIMPARAMDIALOG_H

#pragma once
#include <QDialog>
#include <QDoubleSpinBox>
#include "QtUItoAlgoInterface.h"

class SimParamDialog : public QDialog {
    Q_OBJECT
public:
    explicit SimParamDialog(QWidget* parent=nullptr);
    QtUItoAlgo::SimulationParameters params() const;

private:
    QDoubleSpinBox *total_, *dt0_, *dtMin_, *dtMax_;
};

#endif // SIMPARAMDIALOG_H
