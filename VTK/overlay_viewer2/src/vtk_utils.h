// vtk_utils.h
#pragma once
#include <vtkSmartPointer.h>

class vtkPolyData;

void CopyCellsAsLines(
  vtkPolyData* src,
  long long cellBegin,
  long long cellEnd,
  vtkPolyData* dst);
