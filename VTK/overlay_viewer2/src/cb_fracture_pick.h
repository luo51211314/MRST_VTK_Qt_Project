// cb_fracture_pick.h
#pragma once

#include <vtkCommand.h>
#include <vtkSmartPointer.h>
#include <string>

class vtkActor;
class vtkCellPicker;
class vtkPolyData;
class vtkRenderer;
class vtkRenderWindowInteractor;
class vtkTextActor;

// ★新增：统一效果
class FractureSelectEffect;

class FracturePickCallback : public vtkCommand {
public:
  static FracturePickCallback* New();
  vtkTypeMacro(FracturePickCallback, vtkCommand);

  vtkRenderer* Renderer = nullptr;
  vtkCellPicker* Picker = nullptr;

  vtkActor* FractureActor = nullptr;
  vtkPolyData* FracturePoly = nullptr;
  int CellsPerFracture = 1;

  // 兼容字段：main 里要用
  vtkTextActor* HudText = nullptr;

  // 统一效果（Pick/Box共用）
  FractureSelectEffect* Effect = nullptr;

  bool OnlyWhen3D = false;
  bool* Is3DPtr = nullptr;

  void Execute(vtkObject* caller, unsigned long eventId, void* callData) override;

private:
  int LastFractureIndex = -1;

  void ClearSelection(vtkRenderWindowInteractor* iren);
  void ShowHud(vtkRenderWindowInteractor* iren, const std::string& text);
};

