cmake_minimum_required(VERSION 3.16)

project(Knapsack_CLI VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(knapsack_cli
    main.cpp
    algorithm.cpp
    bag.cpp
    package.cpp
    dependency.cpp
)

target_include_directories(knapsack_cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS knapsack_cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)