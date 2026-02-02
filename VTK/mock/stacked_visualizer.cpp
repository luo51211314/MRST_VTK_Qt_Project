#include "shared.h"
#include "mock_data.h"

int main(int argc, char* argv[]) {
    try {
        std::cout << "Generating MOCK data for stacked layers visualization..." << std::endl;

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

        createStackedLayersScene(renderer, fieldData, lut, 10.0);
        drawFractures(renderer, fractures, 10.0);
        drawWells(renderer, wells, fractures, 10.0);

        setupCamera(renderer, cx, cy, cz * 10.0, maxDim, true);

        renderWindow->Render();
        saveImage(renderWindow, "pressure_layers_stacked.png");

        std::cout << "Stacked layers visualization completed." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error in stacked visualizer: " << e.what() << std::endl;
        return 1;
    }
}

