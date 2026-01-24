// app_context.h
#pragma once
#include <vtkSmartPointer.h>

class vtkRenderer;
class vtkRenderWindowInteractor;
class vtkActor;
class vtkPolyData;
class vtkTextActor;

struct AppContext {
  vtkRenderer* Renderer = nullptr;
  vtkRenderWindowInteractor* Iren = nullptr;

  // 数据/actor
  vtkActor* FractureActor = nullptr;
  vtkPolyData* FracturePoly = nullptr;
  int CellsPerFracture = 1;

  // HUD
  vtkTextActor* HudText = nullptr;       // 你的坐标/提示 HUD
  vtkTextActor* LockHintText = nullptr;  // 你的 CAM lock 提示（可选）

  // 3D 模式状态（给 Pick OnlyWhen3D）
  bool* Is3DPtr = nullptr;
};
