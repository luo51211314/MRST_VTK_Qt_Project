#pragma once
#include <vtkCommand.h>
#include <vtkSmartPointer.h>

class vtkCellPicker;
class vtkRenderer;
class vtkTextActor;

class MouseMoveCoordCallback : public vtkCommand {
public:
  static MouseMoveCoordCallback* New();
  vtkTypeMacro(MouseMoveCoordCallback, vtkCommand);

  vtkCellPicker* Picker = nullptr;
  vtkRenderer* Renderer = nullptr;
  vtkTextActor* CoordText = nullptr;

  void Execute(vtkObject* caller, unsigned long eventId, void* callData) override;

private:
  int LastX = -999999;
  int LastY = -999999;
};
