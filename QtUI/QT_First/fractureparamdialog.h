#ifndef FRACTUREPARAMDIALOG_H
#define FRACTUREPARAMDIALOG_H

#pragma once
#include <QDialog>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <vector>
#include "QtUItoAlgoInterface.h"

class FractureParamDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FractureParamDialog(QWidget* parent = nullptr);

    // ✅ 自动填充：由 MainWindow 把网格 Lx/Ly/Lz 传进来
    void setGridBounds(double Lx, double Ly, double Lz);

    // ✅ 新增：批量生成裂缝（方案B完整版使用这个）
    std::vector<QtUItoAlgo::FractureInput> fractures() const;

    // 兼容旧逻辑：仍保留单条裂缝（可选）
    QtUItoAlgo::FractureInput fracture() const;

private:
    // ========== 生成模式选择 ==========
    QComboBox* mode_ = nullptr;   // 单条 / 随机生成
    QSpinBox*  count_ = nullptr;  // 随机条数
    QSpinBox*  seed_  = nullptr;  // 种子

    // ========== 空间范围 ==========
    QDoubleSpinBox *xMin_=nullptr, *xMax_=nullptr;
    QDoubleSpinBox *yMin_=nullptr, *yMax_=nullptr;
    QDoubleSpinBox *zMin_=nullptr, *zMax_=nullptr;

    // ========== 方向范围（弧度）==========
    QDoubleSpinBox *strikeMin_=nullptr, *strikeMax_=nullptr;
    QDoubleSpinBox *dipMin_=nullptr,    *dipMax_=nullptr;

    // ========== 长度范围 ==========
    QDoubleSpinBox *lenMin_=nullptr, *lenMax_=nullptr;
    QDoubleSpinBox *heightRatio_=nullptr;   // height = len * ratio

    // ========== 物性 ==========
    QDoubleSpinBox *apertureRand_=nullptr, *permRand_=nullptr;

    // ========== 是否附加水力裂缝 ==========
    QComboBox* addHydraulic_ = nullptr; // Yes/No
    QDoubleSpinBox *hydAperture_=nullptr, *hydPerm_=nullptr;
    QDoubleSpinBox *hydHalfY_=nullptr, *hydHalfZ_=nullptr; // 水力裂缝矩形半高（y方向半长、z方向半长）
    QDoubleSpinBox *hydOffset_=nullptr;                 // offsets 间距（100）
    QDoubleSpinBox *hydXc_=nullptr, *hydYc_=nullptr, *hydZc_=nullptr;

    // ========== 单条输入（你原来的控件保留）==========
    QSpinBox* id_ = nullptr;
    QDoubleSpinBox* aperture_ = nullptr;
    QDoubleSpinBox* perm_ = nullptr;
    QDoubleSpinBox* cx_ = nullptr;
    QDoubleSpinBox* cy_ = nullptr;
    QDoubleSpinBox* cz_ = nullptr;
    QDoubleSpinBox* w_ = nullptr;
    QDoubleSpinBox* h_ = nullptr;
    QComboBox* plane_ = nullptr;

private:
    void applyModeUi(); // 根据 mode_ 显示/隐藏区域

};

#endif // FRACTUREPARAMDIALOG_H
