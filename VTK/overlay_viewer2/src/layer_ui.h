#pragma once
#include <memory>
#include <vector>

#include <vtkSmartPointer.h>

class vtkRenderWindowInteractor;
class vtkRenderer;
class vtkActor;
class vtkProp;
class FractureSelectEffect;

struct LayerUIHandle {
  bool showFracture = true;
  bool showOther = true;

  std::vector<vtkProp*> fractureProps;
  std::vector<vtkProp*> otherProps;

  // 只是为了保持 callback/按钮对象生命周期
  vtkSmartPointer<class vtkCommand> clickCb;
};

std::shared_ptr<LayerUIHandle> InstallLayerUI(
  vtkRenderWindowInteractor* iren,
  vtkRenderer* renderer,
  vtkActor* fractureActor,
  FractureSelectEffect* effect
);
