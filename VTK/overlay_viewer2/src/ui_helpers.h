#pragma once
#include <vtkTextActor.h>

void SetupTextActor(vtkTextActor* t,
                    int fontSize,
                    double r,double g,double b,
                    double br,double bg,double bb,double bop,
                    bool bold);
