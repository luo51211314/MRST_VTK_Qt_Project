#include "simparamdialog.h"
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QVBoxLayout>

SimParamDialog::SimParamDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("模拟参数");
    resize(360, 220);

    auto make = [&](double v, double min, double max, int dec){
        auto* b = new QDoubleSpinBox(this);
        b->setRange(min, max);
        b->setDecimals(dec);
        b->setValue(v);
        return b;
    };

    total_ = make(100.0, 0.0001, 1e6, 3);
    dt0_   = make(0.001,  1e-6,   1e6, 6);
    dtMin_ = make(0.000001, 1e-6,   1e6, 6);
    dtMax_ = make(10.0,  1e-6,   1e6, 6);

    auto* form = new QFormLayout;
    form->addRow("total_time_days", total_);
    form->addRow("initial_time_step", dt0_);
    form->addRow("min_time_step", dtMin_);
    form->addRow("max_time_step", dtMax_);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addWidget(bb);
}

QtUItoAlgo::SimulationParameters SimParamDialog::params() const
{
    QtUItoAlgo::SimulationParameters p;
    p.total_time_days = total_->value();
    p.initial_time_step = dt0_->value();
    p.min_time_step = dtMin_->value();
    p.max_time_step = dtMax_->value();
    return p;
}
