#include "shared.h"
#include "mock_data.h"

int main(int argc, char* argv[]) {
    try {
        std::cout << "Generating MOCK data for cube visualization..." << std::endl;

        auto ds = MockData::Generate(MockData::GenerateParams{});

        GridInfo gridInfo;
        FieldData fieldData;
        std::vector<Fracture> fractures;
        std::vector<WellInfo> wells;

        BuildSharedInputsFromMock(
            ds.grid, ds.cells, ds.fractures, ds.wells,
            gridInfo, fieldData, fractures, wells
        );

        std::cout << "MOCK dataset ready. p_min=" << fieldData.p_min
                  << " p_max=" << fieldData.p_max << std::endl;

        vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
        renderer->SetBackground(0.0, 0.0, 0.0);

        vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
        renderWindow->AddRenderer(renderer);
        renderWindow->SetSize(1200, 900);
        renderWindow->SetOffScreenRendering(1);

        auto lut = createLookupTable(fieldData.p_min, fieldData.p_max);
        auto scalarBar = createColorBar(lut);
        renderer->AddActor2D(scalarBar);

        double cx = gridInfo.Lx / 2.0;
        double cy = gridInfo.Ly / 2.0;
        double cz = gridInfo.Lz / 2.0;
        double maxDim = std::max({gridInfo.Lx, gridInfo.Ly, gridInfo.Lz});

        renderer->RemoveAllViewProps();
        renderer->AddActor2D(scalarBar);

        createSolidBlockScene(renderer, fieldData, lut);
        drawFractures(renderer, fractures, 1.0);
        drawWells(renderer, wells, fractures, 1.0);

        setupCamera(renderer, cx, cy, cz, maxDim, false);

        renderWindow->Render();
        saveImage(renderWindow, "pressure_solid_block.png");

        std::cout << "Cube visualization completed." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error in cube visualizer: " << e.what() << std::endl;
        return 1;
    }
}

