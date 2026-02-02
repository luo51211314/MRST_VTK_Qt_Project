#include "gridparamdialog.h"
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QVBoxLayout>

GridParamDialog::GridParamDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("网格设置");
    resize(360, 220);

    nx_ = new QSpinBox(this); ny_ = new QSpinBox(this); nz_ = new QSpinBox(this);
    nx_->setRange(1, 2000); ny_->setRange(1, 2000); nz_->setRange(1, 2000);
    nx_->setValue(20); ny_->setValue(10); nz_->setValue(2);

    lx_ = new QDoubleSpinBox(this); ly_ = new QDoubleSpinBox(this); lz_ = new QDoubleSpinBox(this);
    for (auto* b : {lx_, ly_, lz_}) { b->setRange(0.0001, 1e9); b->setDecimals(4); }
    lx_->setValue(1000); ly_->setValue(500); lz_->setValue(20);

    auto* form = new QFormLayout;
    form->addRow("Nx", nx_);
    form->addRow("Ny", ny_);
    form->addRow("Nz", nz_);
    form->addRow("Lx", lx_);
    form->addRow("Ly", ly_);
    form->addRow("Lz", lz_);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addWidget(bb);
}

QtUItoAlgo::GridParameters GridParamDialog::params() const
{
    QtUItoAlgo::GridParameters p;
    p.Nx = nx_->value(); p.Ny = ny_->value(); p.Nz = nz_->value();
    p.Lx = lx_->value(); p.Ly = ly_->value(); p.Lz = lz_->value();
    return p;
}
