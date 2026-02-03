#pragma once
#include <QWidget>

class QVTKOpenGLNativeWidget;

class VtkSmokeView : public QWidget
{
    Q_OBJECT
public:
    explicit VtkSmokeView(QWidget* parent = nullptr);

private:
    QVTKOpenGLNativeWidget* vtkWidget_ = nullptr;
};
