#pragma once
#include <vtkCommand.h>
#include <vtkSmartPointer.h>
#include <vtkRenderWindowInteractor.h>

// ✅ 加这一行：让 FractureSelectEffect 变成“完整类型”
#include "fracture_select_effect.h"

class vtkActor;
class vtkRenderer;

class FractureHoverTooltipCallback : public vtkCommand {
public:
  static FractureHoverTooltipCallback* New() { return new FractureHoverTooltipCallback; }

  vtkRenderer* Renderer = nullptr;
  vtkActor* FractureActor = nullptr;
  FractureSelectEffect* Effect = nullptr;

  void Execute(vtkObject* caller, unsigned long, void*) override {
    auto* iren = vtkRenderWindowInteractor::SafeDownCast(caller);
    if (!iren || !Effect || !Renderer || !FractureActor) return;

    int x = iren->GetEventPosition()[0];
    int y = iren->GetEventPosition()[1];

    Effect->UpdateHoverTooltip(iren, FractureActor, x, y);
  }
};

