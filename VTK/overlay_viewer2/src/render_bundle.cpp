#include "../combined_visualization/render_bundle.h"

void SetPropsVisible(const std::vector<vtkProp*>& props, bool vis) {
    for (auto* p : props) {
        if (!p) continue;
        p->SetVisibility(vis ? 1 : 0);
    }
}
