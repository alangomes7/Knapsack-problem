# Set the minimum required CMake version. Qt 6.7+ often requires 3.25 or newer.
cmake_minimum_required(VERSION 3.25)

project(KnapsackProblem VERSION 0.1 LANGUAGES CXX)

# Enable Qt's automatic tools
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt 6.10.0 specifically
find_package(Qt6 6.10.0 REQUIRED COMPONENTS Widgets LinguistTools Concurrent)

# --- Define Project Files ---

set(TS_FILES
    KnapsackProblem_en_US.ts
)

# Generate translation (.qm) files from the .ts files
qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})

set(PROJECT_SOURCES
    main.cpp
    knapsackwindow.cpp
    data_model.cpp
    file_processor.cpp
    package.cpp
    dependency.cpp
    bag.cpp
    algorithm.cpp
    solution_repair.cpp
    random_provider.cpp
    constructive_solutions.cpp
    search_engine.cpp
    vnd.cpp
    vns.cpp
    grasp.cpp
    grasp_vns.cpp
    grasp_helper.cpp
    vns_helper.cpp
)

set(PROJECT_HEADERS
    knapsackwindow.h
    data_model.h
    file_processor.h
    package.h
    dependency.h
    bag.h
    algorithm.h
    solution_repair.h
    random_provider.h
    constructive_solutions.h
    search_engine.h
    vnd.h
    vns.h
    grasp.h
    grasp_vns.h
    grasp_helper.h
    vns_helper.h
)

set(PROJECT_UIS
    knapsackwindow.ui
)

# --- Create Executable ---

# Create the executable using all defined sources, headers, UIs, and translations
qt_add_executable(KnapsackProblem
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
    ${PROJECT_UIS}
    ${TS_FILES} # Include .ts files in the project
)

# --- Link Libraries ---

# Link the required Qt modules. Qt6::Widgets automatically links Core and Gui.
target_link_libraries(KnapsackProblem PRIVATE
    Qt6::Widgets
    Qt6::Concurrent
)

# --- Set Target Properties ---

# Set properties for macOS and Windows bundles
set_target_properties(KnapsackProblem PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Set a local install path inside the build folder
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install")

# --- Install Target ---

include(GNUInstallDirs)
install(TARGETS KnapsackProblem
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Finalize the executable (e.g., handles deployment of dependencies)
qt_finalize_executable(KnapsackProblem)