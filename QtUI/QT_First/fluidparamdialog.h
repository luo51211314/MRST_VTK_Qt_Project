#ifndef FLUIDPARAMDIALOG_H
#define FLUIDPARAMDIALOG_H

#pragma once
#include <QDialog>
#include <QDoubleSpinBox>
#include "QtUItoAlgoInterface.h"

class FluidParamDialog : public QDialog {
    Q_OBJECT
public:
    explicit FluidParamDialog(QWidget* parent=nullptr);
    QtUItoAlgo::FluidProperties props() const;

private:
    QDoubleSpinBox *mu_w_, *mu_o_, *mu_g_, *p_ref_;
    QDoubleSpinBox *swi_, *sor_, *sgc_;
};

#endif // FLUIDPARAMDIALOG_H
