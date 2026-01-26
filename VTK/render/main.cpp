#include <iostream>
#include <string>
#include <cstdlib>

int main(int argc, char* argv[]) {
    std::string visualizationType = "both";
    if (argc > 1) {
        visualizationType = argv[1];
    }

    std::cout << "Main controller started." << std::endl;

    if (visualizationType == "stacked" || visualizationType == "both") {
        std::cout << "Running stacked layers visualization..." << std::endl;
        int stackedResult = system("./stacked_visualizer");
        if (stackedResult != 0) {
            std::cerr << "Error running stacked layers visualization." << std::endl;
        }
    }

    if (visualizationType == "cube" || visualizationType == "both") {
        std::cout << "Running cube visualization..." << std::endl;
        int cubeResult = system("./cube_visualizer");
        if (cubeResult != 0) {
            std::cerr << "Error running cube visualization." << std::endl;
        }
    }

    std::cout << "Main controller completed." << std::endl;
    return 0;
}
