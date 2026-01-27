#ifndef FRACTUREPARAMDIALOG_H
#define FRACTUREPARAMDIALOG_H

#pragma once
#include <QDialog>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>

#include "QtUItoAlgoInterface.h"

class FractureParamDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FractureParamDialog(QWidget* parent = nullptr);

    QtUItoAlgo::FractureInput fracture() const;

private:
    QSpinBox*       id_ = nullptr;
    QDoubleSpinBox* aperture_ = nullptr;
    QDoubleSpinBox* perm_ = nullptr;

    QDoubleSpinBox* cx_ = nullptr;
    QDoubleSpinBox* cy_ = nullptr;
    QDoubleSpinBox* cz_ = nullptr;

    QDoubleSpinBox* w_ = nullptr;   // 宽
    QDoubleSpinBox* h_ = nullptr;   // 高

    QComboBox* plane_ = nullptr;    // XY / XZ / YZ
};

#endif // FRACTUREPARAMDIALOG_H
