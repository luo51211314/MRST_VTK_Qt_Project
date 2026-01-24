#include "fluidparamdialog.h"
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QVBoxLayout>

FluidParamDialog::FluidParamDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("物性参数");
    resize(360, 260);

    auto make = [&](double v, double min, double max, int dec){
        auto* b = new QDoubleSpinBox(this);
        b->setRange(min, max);
        b->setDecimals(dec);
        b->setValue(v);
        return b;
    };

    mu_w_ = make(1.0, 1e-6, 1e6, 6);
    mu_o_ = make(5.0, 1e-6, 1e6, 6);
    mu_g_ = make(0.2, 1e-6, 1e6, 6);
    p_ref_ = make(100.0, 0.0, 1e9, 3);

    swi_ = make(0.2, 0.0, 1.0, 3);
    sor_ = make(0.2, 0.0, 1.0, 3);
    sgc_ = make(0.05,0.0, 1.0, 3);

    auto* form = new QFormLayout;
    form->addRow("mu_w (cP)", mu_w_);
    form->addRow("mu_o (cP)", mu_o_);
    form->addRow("mu_g (cP)", mu_g_);
    form->addRow("P_ref (bar)", p_ref_);
    form->addRow("Swi", swi_);
    form->addRow("Sor", sor_);
    form->addRow("Sgc", sgc_);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addWidget(bb);
}

QtUItoAlgo::FluidProperties FluidParamDialog::props() const
{
    QtUItoAlgo::FluidProperties p;
    p.mu_w = mu_w_->value();
    p.mu_o = mu_o_->value();
    p.mu_g = mu_g_->value();
    p.P_ref = p_ref_->value();
    p.Swi = swi_->value();
    p.Sor = sor_->value();
    p.Sgc = sgc_->value();
    return p;
}
