#include "ui_helpers.h"
#include <vtkTextProperty.h>

void SetupTextActor(vtkTextActor* t, int fontSize,
                    double r,double g,double b,
                    double br,double bg,double bb,double bop,
                    bool bold) {
  auto* p = t->GetTextProperty();
  p->SetFontSize(fontSize);
  p->SetColor(r,g,b);
  p->SetBackgroundColor(br,bg,bb);
  p->SetBackgroundOpacity(bop);
  p->SetBold(bold ? 1 : 0);
}
