// fracture_select_effect.h
#pragma once
#include <set>
#include <string>
#include <vtkSmartPointer.h>

class vtkActor;
class vtkPolyData;
class vtkRenderer;
class vtkRenderWindowInteractor;
class vtkTextActor;

class FractureSelectEffect {
public:
  vtkRenderer* Renderer = nullptr;
  vtkPolyData* FracturePoly = nullptr;
  int CellsPerFracture = 1;

  // 左上角：选择信息（Pick/Box 都写这里）
  vtkTextActor* HudText = nullptr;

  // 浮窗：鼠标悬停在“已高亮裂缝”上时显示
  vtkTextActor* TooltipText = nullptr;

  // Pick：单选
  void ApplyPick(vtkRenderWindowInteractor* iren, int fractureIndex, long long cellId);

  // Box：多选
  void ApplyBox(vtkRenderWindowInteractor* iren,
                const std::set<int>& fractureIndices,
                int xmin, int ymin, int xmax, int ymax);

  // 清空高亮 + 清空HUD + 清空Tooltip
  void Clear(vtkRenderWindowInteractor* iren);

  // MouseMove：更新悬停浮窗（只有 hover 到“已选中(高亮)”的裂缝上才显示）
  void UpdateHoverTooltip(vtkRenderWindowInteractor* iren,
                          vtkActor* fractureActor,
                          int mouseX, int mouseY);
                          
  void HideTooltip();
  
  // （可选）给外部读当前选中集合
  const std::set<int>& GetSelected() const { return SelectedFractureIds; }

private:
  vtkSmartPointer<vtkPolyData> HighlightPoly;
  vtkSmartPointer<vtkActor> HighlightActor;

  std::set<int> SelectedFractureIds; // 当前被高亮的裂缝集合

  void EnsureHighlightObjects();

  void BuildHighlightSingle(int fractureIndex);
  void BuildHighlightMulti(const std::set<int>& fractureIndices);

  void ShowHudTopLeft(vtkRenderWindowInteractor* iren, const std::string& text);
  void HideHudTopLeft();

  void ShowTooltip(vtkRenderWindowInteractor* iren, int x, int y, const std::string& text);

  bool GetFractureBounds(int fractureIndex, double bounds6[6]) const;
  std::string MakePickInfoText(int fractureIndex, long long cellId) const;
  std::string MakeBoxInfoText(const std::set<int>& fractureIndices,
                              int xmin, int ymin, int xmax, int ymax) const;
  std::string MakeHoverInfoText(int fractureIndex) const;
};

