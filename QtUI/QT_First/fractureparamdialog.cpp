#include "fractureparamdialog.h"
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QVBoxLayout>

static QDoubleSpinBox* makeD(QWidget* parent, double v, double mn, double mx, int dec)
{
    auto* b = new QDoubleSpinBox(parent);
    b->setRange(mn, mx);
    b->setDecimals(dec);
    b->setValue(v);
    b->setSingleStep((mx - mn) / 1000.0);
    return b;
}

FractureParamDialog::FractureParamDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("裂缝参数");
    resize(420, 260);

    id_ = new QSpinBox(this);
    id_->setRange(1, 1000000);
    id_->setValue(1);

    aperture_ = makeD(this, 0.01, 1e-6, 1e3, 6);
    perm_     = makeD(this, 100.0, 1e-9, 1e9, 6);

    // 默认中心点放在 (500,500,50) 这种中间位置（后面你可以用网格参数来覆盖）
    cx_ = makeD(this, 500.0, -1e9, 1e9, 3);
    cy_ = makeD(this, 500.0, -1e9, 1e9, 3);
    cz_ = makeD(this, 50.0,  -1e9, 1e9, 3);

    w_ = makeD(this, 100.0, 1e-6, 1e9, 3);
    h_ = makeD(this, 100.0, 1e-6, 1e9, 3);

    plane_ = new QComboBox(this);
    plane_->addItem("XY 平面（水平裂缝）");
    plane_->addItem("XZ 平面");
    plane_->addItem("YZ 平面");
    plane_->setCurrentIndex(0);

    auto* form = new QFormLayout;
    form->addRow("id", id_);
    form->addRow("aperture", aperture_);
    form->addRow("perm", perm_);
    form->addRow("center_x", cx_);
    form->addRow("center_y", cy_);
    form->addRow("center_z", cz_);
    form->addRow("width (w)", w_);
    form->addRow("height (h)", h_);
    form->addRow("plane", plane_);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addWidget(bb);
}

QtUItoAlgo::FractureInput FractureParamDialog::fracture() const
{
    QtUItoAlgo::FractureInput f{};
    f.id = id_->value();
    f.aperture = aperture_->value();
    f.perm = perm_->value();

    const double cx = cx_->value();
    const double cy = cy_->value();
    const double cz = cz_->value();
    const double hw = w_->value() * 0.5;
    const double hh = h_->value() * 0.5;

    // 四个顶点：按选择的平面生成一个矩形
    const int plane = plane_->currentIndex();
    if (plane == 0) { // XY
        f.vertices[0] = {cx - hw, cy - hh, cz};
        f.vertices[1] = {cx + hw, cy - hh, cz};
        f.vertices[2] = {cx + hw, cy + hh, cz};
        f.vertices[3] = {cx - hw, cy + hh, cz};
    } else if (plane == 1) { // XZ
        f.vertices[0] = {cx - hw, cy, cz - hh};
        f.vertices[1] = {cx + hw, cy, cz - hh};
        f.vertices[2] = {cx + hw, cy, cz + hh};
        f.vertices[3] = {cx - hw, cy, cz + hh};
    } else { // YZ
        f.vertices[0] = {cx, cy - hw, cz - hh};
        f.vertices[1] = {cx, cy + hw, cz - hh};
        f.vertices[2] = {cx, cy + hw, cz + hh};
        f.vertices[3] = {cx, cy - hw, cz + hh};
    }

    return f;
}
