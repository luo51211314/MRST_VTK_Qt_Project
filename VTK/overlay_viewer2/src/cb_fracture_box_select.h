// cb_fracture_box_select.h
#pragma once
#include <vtkCommand.h>
#include <vtkSmartPointer.h>

class vtkActor2D;
class vtkPlanes;
class vtkPoints;
class vtkCellArray;
class vtkPolyData;
class vtkRenderer;
class vtkTextActor;

class FractureSelectEffect;  // ★新增

class FractureBoxSelectCallback : public vtkCommand {
public:
  static FractureBoxSelectCallback* New();
  vtkTypeMacro(FractureBoxSelectCallback, vtkCommand);

  vtkRenderer* Renderer = nullptr;

  vtkPolyData* FracturePoly = nullptr;
  int CellsPerFracture = 1;

  vtkTextActor* HudText = nullptr;        // 左上角HUD（交给Effect写）
  vtkTextActor* LockHintText = nullptr;

  FractureSelectEffect* Effect = nullptr; // ★新增：统一高亮+信息

  // 原状态
  bool CamLocked = true;
  bool bDown = false;
  bool dragging = false;
  int x0=0,y0=0,x1=0,y1=0;
  bool boxLatched = false;

  void Execute(vtkObject* caller, unsigned long eventId, void* callData) override;

private:
  // 画绿框 2D actor
  vtkSmartPointer<vtkPoints> boxPts;
  vtkSmartPointer<vtkCellArray> boxLines;
  vtkSmartPointer<vtkPolyData> boxPoly;
  vtkSmartPointer<vtkActor2D> boxActor;

  void EnsureBox();
  void UpdateBox(int ax0,int ay0,int ax1,int ay1);
  void ShowBox(bool on);

  void BuildSelectionFromRect(int ax0,int ay0,int ax1,int ay1);
};

