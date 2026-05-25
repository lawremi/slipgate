cmake_minimum_required(VERSION 3.24)

project(slipgate VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_standard_project_setup()

qt_add_executable(slipgate
    src/CatalogService.cpp
    src/CatalogService.h
    src/EnvironmentDetector.cpp
    src/EnvironmentDetector.h
    src/InstallManager.cpp
    src/InstallManager.h
    src/Launcher.cpp
    src/Launcher.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/ModEntry.cpp
    src/ModEntry.h
    src/ModInstallService.cpp
    src/ModInstallService.h
    src/QuakeData.cpp
    src/QuakeData.h
    src/SearchFilter.cpp
    src/SearchFilter.h
    src/main.cpp
)

target_link_libraries(slipgate
    PRIVATE
        Qt6::Network
        Qt6::Widgets
)

install(TARGETS slipgate
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
