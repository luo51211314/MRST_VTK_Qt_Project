cmake_minimum_required(VERSION 3.16)

project(QT_First VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Svg Concurrent)


find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Svg Concurrent)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        resources.qrc
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(QT_First
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        flowpanel.h flowpanel.cpp
        datapanel.h datapanel.cpp
        layerpanel.h layerpanel.cpp
        propertypanel.h propertypanel.cpp
        logpanel.h logpanel.cpp
        backstagedialog.h backstagedialog.cpp
        ribbonbar.h ribbonbar.cpp
        paramdialog.h paramdialog.cpp
        resources.qrc
        mock/mockalgo.cpp mock/mockalgo.h
        mock/SimCallbackBridge.cpp mock/SimCallbackBridge.h
        gridparamdialog.h gridparamdialog.cpp
        fluidparamdialog.h fluidparamdialog.cpp
        simparamdialog.h simparamdialog.cpp
        gridparamdialog.h gridparamdialog.cpp
        fluidparamdialog.h fluidparamdialog.cpp
        mock/algo_adapter.cpp mock/algo_adapter.h
        mock/algo_adapter.h
        mock/algo_adapter.cpp
        vtk/VtkAdapter.cpp vtk/VtkAdapter.h vtk/VtkViewHost.h
        fractureparamdialog.h fractureparamdialog.cpp

        # ===== Algo sources (from ../../Algo) =====
        ../../Algo/edfm_3d_blackoil_integrated_simulator.cpp
        fractureparamdialog.h fractureparamdialog.cpp
        #../../Algo/edfm_3d_blackoil.cpp
        #../../Algo/edfm_3d_blackoil_lgr.cpp






    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET QT_First APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(QT_First SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(QT_First
            ${PROJECT_SOURCES}
        )
    endif()
endif()


# ===== 引入团队的 Common 接口头文件目录 =====
target_include_directories(QT_First PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Algo
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/mock
    ${CMAKE_CURRENT_SOURCE_DIR}/vtk
    ${CMAKE_CURRENT_SOURCE_DIR}/icons
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/eigen/eigen
)

target_link_libraries(QT_First PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Svg Qt${QT_VERSION_MAJOR}::Concurrent)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.QT_First)
endif()
set_target_properties(QT_First PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS QT_First
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(QT_First)
endif()
