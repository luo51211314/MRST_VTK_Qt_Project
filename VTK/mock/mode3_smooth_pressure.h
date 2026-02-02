#pragma once

#include "render_bundle.h"

// Mode3：平滑占满压力（实体块）+ 裂缝 + 井
inline RenderBundle BuildMode3_SmoothPressure(
    vtkRenderer* renderer,
    const GridInfo& gridInfo,
    const FieldData& fieldData,
    const std::vector<Fracture>& fractures,
    const std::vector<WellInfo>& wells
) {
    (void)gridInfo;

    RenderBundle out;
    out.name = "Mode3: SmoothPressure";
    out.isStacked = false;
    out.zScale = 1.0;

    vtkIdType before = renderer->GetViewProps()->GetNumberOfItems();

    auto lut = createLookupTable(fieldData.p_min, fieldData.p_max);
    auto bar = createColorBar(lut);
    renderer->AddActor2D(bar);

    createSolidBlockScene(renderer, fieldData, lut);

    drawFractures(renderer, fractures, out.zScale);
    drawWells(renderer, wells, fractures, out.zScale);

    out.props = CollectNewProps(renderer, before);
    return out;
}
