#include "cb_mouse_move_coord.h"

#include <vtkObjectFactory.h>          // ★必须有
#include <vtkCellPicker.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkTextActor.h>

#include <sstream>
#include <ios>

vtkStandardNewMacro(MouseMoveCoordCallback);


void MouseMoveCoordCallback::Execute(vtkObject* caller, unsigned long eventId, void* callData) {
  if (eventId != vtkCommand::MouseMoveEvent) return;

  auto* iren = vtkRenderWindowInteractor::SafeDownCast(caller);
  if (!iren || !Renderer || !Picker || !CoordText) return;

  int* pos = iren->GetEventPosition();
  int x = pos[0], y = pos[1];

  if (x == LastX && y == LastY) return;
  LastX = x; LastY = y;

  Picker->PickFromListOff();
  Picker->Pick(x, y, 0, Renderer);

  vtkIdType cellId = Picker->GetCellId();
  double wp[3]{0, 0, 0};

  std::ostringstream ss;
  ss.setf(std::ios::fixed);
  ss.precision(3);

  if (cellId >= 0) {
    Picker->GetPickPosition(wp);
    ss << "Mouse World XYZ:\n"
       << "X = " << wp[0] << "\n"
       << "Y = " << wp[1] << "\n"
       << "Z = " << wp[2];
  } else {
    ss << "Mouse World XYZ:\n(no hit)";
  }

  CoordText->SetInput(ss.str().c_str());
  CoordText->SetVisibility(1);
  iren->GetRenderWindow()->Render();
}
