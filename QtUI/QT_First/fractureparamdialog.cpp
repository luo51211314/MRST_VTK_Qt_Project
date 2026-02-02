#include "fractureparamdialog.h"
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>

#include <random>
#include <cmath>
#include <algorithm>

static QDoubleSpinBox* makeD(QWidget* parent, double v, double mn, double mx, int dec)
{
    auto* b = new QDoubleSpinBox(parent);
    b->setRange(mn, mx);
    b->setDecimals(dec);
    b->setValue(v);
    b->setSingleStep((mx - mn) / 1000.0);
    return b;
}

static QSpinBox* makeI(QWidget* parent, int v, int mn, int mx)
{
    auto* b = new QSpinBox(parent);
    b->setRange(mn, mx);
    b->setValue(v);
    return b;
}

FractureParamDialog::FractureParamDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("裂缝参数");
    resize(520, 520);

    // ===================== 模式 =====================
    mode_ = new QComboBox(this);
    mode_->addItem("单条裂缝（手动）");
    mode_->addItem("随机生成裂缝（批量）");
    mode_->setCurrentIndex(1); // 默认随机生成（更符合你的需求）

    // ===================== 随机生成参数 =====================
    count_ = makeI(this, 100, 1, 100000);
    seed_  = makeI(this, 42, 0, 1000000000);

    xMin_ = makeD(this, 0.0, -1e9, 1e9, 3);
    xMax_ = makeD(this, 1000.0, -1e9, 1e9, 3);
    yMin_ = makeD(this, 0.0, -1e9, 1e9, 3);
    yMax_ = makeD(this, 500.0, -1e9, 1e9, 3);
    zMin_ = makeD(this, 0.0, -1e9, 1e9, 3);
    zMax_ = makeD(this, 20.0, -1e9, 1e9, 3);

    constexpr double PI = 3.14159265358979323846;
    strikeMin_ = makeD(this, 0.0, -1000, 1000, 6);
    strikeMax_ = makeD(this, PI,  -1000, 1000, 6);
    dipMin_    = makeD(this, 0.0, -1000, 1000, 6);
    dipMax_    = makeD(this, PI/3.0, -1000, 1000, 6);

    lenMin_ = makeD(this, 30.0, 1e-6, 1e9, 3);
    lenMax_ = makeD(this, 80.0, 1e-6, 1e9, 3);
    heightRatio_ = makeD(this, 0.5, 1e-6, 1000.0, 3);

    apertureRand_ = makeD(this, 0.001, 1e-9, 1e3, 6);
    permRand_     = makeD(this, 10000.0, 1e-9, 1e12, 6);

    // ===================== 水力裂缝（附加 3 条）=====================
    addHydraulic_ = new QComboBox(this);
    addHydraulic_->addItem("生成 3 条水力裂缝");
    addHydraulic_->addItem("不生成水力裂缝");
    addHydraulic_->setCurrentIndex(0);

    hydAperture_ = makeD(this, 0.01, 1e-9, 1e3, 6);
    hydPerm_     = makeD(this, 100000.0, 1e-9, 1e12, 6);

    hydHalfY_ = makeD(this, 100.0, 1e-6, 1e9, 3);  // yc±100
    hydHalfZ_ = makeD(this, 50.0,  1e-6, 1e9, 3);  // zc±50
    hydOffset_ = makeD(this, 100.0, 0.0,  1e9, 3); // offsets[-100,0,100]

    // 默认中心点：会在 setGridBounds 自动填充为网格中心
    hydXc_ = makeD(this, 500.0, -1e9, 1e9, 3);
    hydYc_ = makeD(this, 250.0, -1e9, 1e9, 3);
    hydZc_ = makeD(this, 10.0,  -1e9, 1e9, 3);

    // ===================== 单条裂缝（你原来的控件保留）=====================
    id_ = new QSpinBox(this);
    id_->setRange(1, 1000000);
    id_->setValue(1);

    aperture_ = makeD(this, 0.01, 1e-6, 1e3, 6);
    perm_     = makeD(this, 100.0, 1e-9, 1e9, 6);
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

    // ===================== 布局：分组 =====================
    auto* formTop = new QFormLayout;
    formTop->addRow("模式", mode_);

    auto* genBox = new QGroupBox("随机生成参数（与算法 demo 对齐）", this);
    auto* genForm = new QFormLayout(genBox);
    genForm->addRow("条数 (count)", count_);
    genForm->addRow("随机种子 (seed)", seed_);

    genForm->addRow("X 范围", new QLabel("", this)); // 占位
    auto* xRow = new QWidget(this);
    auto* xLay = new QHBoxLayout(xRow); xLay->setContentsMargins(0,0,0,0);
    xLay->addWidget(xMin_); xLay->addWidget(new QLabel("~")); xLay->addWidget(xMax_);
    genForm->addRow("x_min ~ x_max", xRow);

    auto* yRow = new QWidget(this);
    auto* yLay = new QHBoxLayout(yRow); yLay->setContentsMargins(0,0,0,0);
    yLay->addWidget(yMin_); yLay->addWidget(new QLabel("~")); yLay->addWidget(yMax_);
    genForm->addRow("y_min ~ y_max", yRow);

    auto* zRow = new QWidget(this);
    auto* zLay = new QHBoxLayout(zRow); zLay->setContentsMargins(0,0,0,0);
    zLay->addWidget(zMin_); zLay->addWidget(new QLabel("~")); zLay->addWidget(zMax_);
    genForm->addRow("z_min ~ z_max", zRow);

    auto* sRow = new QWidget(this);
    auto* sLay = new QHBoxLayout(sRow); sLay->setContentsMargins(0,0,0,0);
    sLay->addWidget(strikeMin_); sLay->addWidget(new QLabel("~")); sLay->addWidget(strikeMax_);
    genForm->addRow("走向 strike (rad)", sRow);

    auto* dRow = new QWidget(this);
    auto* dLay = new QHBoxLayout(dRow); dLay->setContentsMargins(0,0,0,0);
    dLay->addWidget(dipMin_); dLay->addWidget(new QLabel("~")); dLay->addWidget(dipMax_);
    genForm->addRow("倾角 dip (rad)", dRow);

    auto* lRow = new QWidget(this);
    auto* lLay = new QHBoxLayout(lRow); lLay->setContentsMargins(0,0,0,0);
    lLay->addWidget(lenMin_); lLay->addWidget(new QLabel("~")); lLay->addWidget(lenMax_);
    genForm->addRow("长度 len", lRow);

    genForm->addRow("高度比例 height=len*ratio", heightRatio_);
    genForm->addRow("开度 aperture", apertureRand_);
    genForm->addRow("渗透率 perm", permRand_);

    auto* hydBox = new QGroupBox("附加水力裂缝（3 条）", this);
    auto* hydForm = new QFormLayout(hydBox);
    hydForm->addRow("是否生成", addHydraulic_);
    hydForm->addRow("aperture", hydAperture_);
    hydForm->addRow("perm", hydPerm_);
    hydForm->addRow("中心 xc", hydXc_);
    hydForm->addRow("中心 yc", hydYc_);
    hydForm->addRow("中心 zc", hydZc_);
    hydForm->addRow("y 半长 (100)", hydHalfY_);
    hydForm->addRow("z 半长 (50)", hydHalfZ_);
    hydForm->addRow("x 偏移间距 (100)", hydOffset_);

    auto* singleBox = new QGroupBox("单条裂缝（旧模式：顶点由平面+中心+宽高生成）", this);
    auto* singleForm = new QFormLayout(singleBox);
    singleForm->addRow("id", id_);
    singleForm->addRow("aperture", aperture_);
    singleForm->addRow("perm", perm_);
    singleForm->addRow("center_x", cx_);
    singleForm->addRow("center_y", cy_);
    singleForm->addRow("center_z", cz_);
    singleForm->addRow("width (w)", w_);
    singleForm->addRow("height (h)", h_);
    singleForm->addRow("plane", plane_);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* root = new QVBoxLayout(this);
    root->addLayout(formTop);
    root->addWidget(genBox);
    root->addWidget(hydBox);
    root->addWidget(singleBox);
    root->addWidget(bb);

    connect(mode_, &QComboBox::currentIndexChanged, this, [this](){ applyModeUi(); });
    applyModeUi();
}

void FractureParamDialog::setGridBounds(double Lx, double Ly, double Lz)
{
    // 用网格自动填充范围（你要求的“自动填充”）
    xMin_->setValue(0.0);   xMax_->setValue(Lx);
    yMin_->setValue(0.0);   yMax_->setValue(Ly);
    zMin_->setValue(0.0);   zMax_->setValue(Lz);

    // 单条裂缝的默认中心也放网格中心
    cx_->setValue(Lx * 0.5);
    cy_->setValue(Ly * 0.5);
    cz_->setValue(Lz * 0.5);

    // 水力裂缝中心默认在网格中心
    hydXc_->setValue(Lx * 0.5);
    hydYc_->setValue(Ly * 0.5);
    hydZc_->setValue(Lz * 0.5);
}

void FractureParamDialog::applyModeUi()
{
    // 0: 单条  1: 随机
    const bool isRandom = (mode_->currentIndex() == 1);

    // 我这里简单：随机模式下把单条控件 disable，单条模式下反过来
    id_->setEnabled(!isRandom);
    aperture_->setEnabled(!isRandom);
    perm_->setEnabled(!isRandom);
    cx_->setEnabled(!isRandom);
    cy_->setEnabled(!isRandom);
    cz_->setEnabled(!isRandom);
    w_->setEnabled(!isRandom);
    h_->setEnabled(!isRandom);
    plane_->setEnabled(!isRandom);

    count_->setEnabled(isRandom);
    seed_->setEnabled(isRandom);
    xMin_->setEnabled(isRandom); xMax_->setEnabled(isRandom);
    yMin_->setEnabled(isRandom); yMax_->setEnabled(isRandom);
    zMin_->setEnabled(isRandom); zMax_->setEnabled(isRandom);
    strikeMin_->setEnabled(isRandom); strikeMax_->setEnabled(isRandom);
    dipMin_->setEnabled(isRandom); dipMax_->setEnabled(isRandom);
    lenMin_->setEnabled(isRandom); lenMax_->setEnabled(isRandom);
    heightRatio_->setEnabled(isRandom);
    apertureRand_->setEnabled(isRandom);
    permRand_->setEnabled(isRandom);

    addHydraulic_->setEnabled(isRandom);
    hydAperture_->setEnabled(isRandom);
    hydPerm_->setEnabled(isRandom);
    hydXc_->setEnabled(isRandom);
    hydYc_->setEnabled(isRandom);
    hydZc_->setEnabled(isRandom);
    hydHalfY_->setEnabled(isRandom);
    hydHalfZ_->setEnabled(isRandom);
    hydOffset_->setEnabled(isRandom);
}

// 旧接口：单条裂缝（保留）
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

// 新接口：批量裂缝（与算法 demo 逻辑对齐）
std::vector<QtUItoAlgo::FractureInput> FractureParamDialog::fractures() const
{
    // 如果选单条模式：返回 1 条，兼容
    if (mode_->currentIndex() == 0) {
        return { fracture() };
    }

    std::vector<QtUItoAlgo::FractureInput> fracs;
    const int N = count_->value();
    fracs.reserve(N + 3);

    std::mt19937 rng(static_cast<uint32_t>(seed_->value()));

    std::uniform_real_distribution<double> distX(xMin_->value(), xMax_->value());
    std::uniform_real_distribution<double> distY(yMin_->value(), yMax_->value());
    std::uniform_real_distribution<double> distZ(zMin_->value(), zMax_->value());

    std::uniform_real_distribution<double> distAngle(strikeMin_->value(), strikeMax_->value());
    std::uniform_real_distribution<double> distDip(dipMin_->value(), dipMax_->value());
    std::uniform_real_distribution<double> distL(lenMin_->value(), lenMax_->value());

    const double aperture = apertureRand_->value();
    const double perm = permRand_->value();
    const double ratio = heightRatio_->value();

    // --- N 条随机裂缝（完全复刻算法同学逻辑）---
    for (int i = 0; i < N; ++i) {
        QtUItoAlgo::FractureInput f{};
        f.id = i;                 // 自动 id，不用你手填
        f.aperture = aperture;
        f.perm = perm;

        const double cx = distX(rng);
        const double cy = distY(rng);
        const double cz = distZ(rng);

        const double len = distL(rng);
        const double height = distL(rng) * ratio;   // 和算法一致：height = distL * 0.5（ratio默认0.5）
        const double strike = distAngle(rng);
        const double dip = distDip(rng);

        // u = (cos strike, sin strike, 0)
        const double ux = std::cos(strike);
        const double uy = std::sin(strike);

        // n_horiz = (-sin strike, cos strike, 0)
        const double nhx = -std::sin(strike);
        const double nhy =  std::cos(strike);

        // v = (n_horiz * cos(dip), -sin(dip))
        const double vx = nhx * std::cos(dip);
        const double vy = nhy * std::cos(dip);
        const double vz = -std::sin(dip);

        const double hl = len * 0.5;
        const double hh = height * 0.5;

        // v0..v3
        f.vertices[0] = {cx - ux*hl - vx*hh, cy - uy*hl - vy*hh, cz - vz*hh};
        f.vertices[1] = {cx + ux*hl - vx*hh, cy + uy*hl - vy*hh, cz - vz*hh};
        f.vertices[2] = {cx + ux*hl + vx*hh, cy + uy*hl + vy*hh, cz + vz*hh};
        f.vertices[3] = {cx - ux*hl + vx*hh, cy - uy*hl + vy*hh, cz + vz*hh};

        fracs.push_back(f);
    }

    // --- 附加 3 条水力裂缝（可选）---
    const bool addHyd = (addHydraulic_->currentIndex() == 0);
    if (addHyd) {
        const double xc = hydXc_->value();
        const double yc = hydYc_->value();
        const double zc = hydZc_->value();
        const double halfY = hydHalfY_->value();
        const double halfZ = hydHalfZ_->value();
        const double step = hydOffset_->value();

        const double offsets[3] = {-step, 0.0, step};

        for (int k = 0; k < 3; ++k) {
            QtUItoAlgo::FractureInput f{};
            f.id = N + k;               // id 接在随机裂缝后面
            f.aperture = hydAperture_->value();
            f.perm     = hydPerm_->value();

            const double x_curr = xc + offsets[k];
            f.vertices[0] = {x_curr, yc - halfY, zc - halfZ};
            f.vertices[1] = {x_curr, yc + halfY, zc - halfZ};
            f.vertices[2] = {x_curr, yc + halfY, zc + halfZ};
            f.vertices[3] = {x_curr, yc - halfY, zc + halfZ};

            fracs.push_back(f);
        }
    }

    return fracs;
}
