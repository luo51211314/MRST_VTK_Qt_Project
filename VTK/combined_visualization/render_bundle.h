#pragma once

#include "shared.h"

#include <vector>
#include <string>

#include <vtkProp.h>
#include <vtkPropCollection.h>
#include <vtkRenderer.h>

// 一个模式“渲染出来的东西”的打包结果：
// props：这一模式新增的所有可显示对象（Actor / Actor2D / ScalarBar 等都算 vtkProp）
// isStacked：相机是否要当作“Z拉伸过”的模式（给你后面设置相机用）
// zScale：该模式使用的Z缩放（例如 mode2 用 10.0）
struct RenderBundle {
    std::string name;
    std::vector<vtkProp*> props;
    bool isStacked = false;
    double zScale = 1.0;
};

// 收集“从 beforeCount 之后新增的 props”
// beforeCount 的来源：你在渲染前先记录 renderer->GetViewProps()->GetNumberOfItems()
inline std::vector<vtkProp*> CollectNewProps(vtkRenderer* renderer, vtkIdType beforeCount) {
    std::vector<vtkProp*> out;
    if (!renderer) return out;

    auto* props = renderer->GetViewProps();
    vtkIdType total = props->GetNumberOfItems();

    props->InitTraversal();
    for (vtkIdType i = 0; i < total; ++i) {
        vtkProp* p = props->GetNextProp();
        if (i >= beforeCount && p) out.push_back(p);
    }
    return out;
}

// 批量设置显隐（vis=true 显示；vis=false 隐藏）
inline void SetPropsVisible(const std::vector<vtkProp*>& props, bool vis) {
    for (auto* p : props) {
        if (!p) continue;
        p->SetVisibility(vis ? 1 : 0);
    }
}
