// style_lockable.cpp
#include "style_lockable.h"
#include <vtkObjectFactory.h>

vtkStandardNewMacro(LockableTrackballStyle);

void LockableTrackballStyle::OnLeftButtonDown() {
  if (CameraLocked) return;
  vtkInteractorStyleTrackballCamera::OnLeftButtonDown();
}
void LockableTrackballStyle::OnLeftButtonUp() {
  if (CameraLocked) return;
  vtkInteractorStyleTrackballCamera::OnLeftButtonUp();
}
void LockableTrackballStyle::OnMouseMove() {
  if (CameraLocked) return;
  vtkInteractorStyleTrackballCamera::OnMouseMove();
}
