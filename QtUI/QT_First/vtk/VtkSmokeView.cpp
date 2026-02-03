#include "VtkSmokeView.h"

#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>

// VTK
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkNew.h>
#include <vtkNamedColors.h>
#include <vtkSphereSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>

VtkSmokeView::VtkSmokeView(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0,0,0,0);
    lay->setSpacing(0);

    vtkWidget_ = new QVTKOpenGLNativeWidget(this);
    lay->addWidget(vtkWidget_);

    // 一个最小可视化：球
    vtkNew<vtkGenericOpenGLRenderWindow> rw;
    vtkWidget_->setRenderWindow(rw);

    vtkNew<vtkRenderer> ren;
    rw->AddRenderer(ren);

    vtkNew<vtkNamedColors> colors;
    ren->SetBackground(colors->GetColor3d("SlateGray").GetData());

    vtkNew<vtkSphereSource> sphere;
    sphere->SetRadius(0.5);
    sphere->SetThetaResolution(32);
    sphere->SetPhiResolution(32);

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(sphere->GetOutputPort());

    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(colors->GetColor3d("Cornsilk").GetData());

    ren->AddActor(actor);
    rw->Render();
}
