cmake_minimum_required(VERSION 3.28)

project(brownian-motor)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Matplot++ CONFIG REQUIRED)

add_executable(brownian-motor
        main.cpp

        include/gui/MainWindow.h
        src/gui/MainWindow.cpp

        include/gui/SimulationWorker.h
        src/gui/SimulationWorker.cpp
)

target_include_directories(brownian-motor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(brownian-motor PRIVATE
        Qt6::Widgets

        Matplot++::cimg
        Matplot++::matplot
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(brownian-motor PRIVATE
            $<$<CONFIG:Release>:-O3>
            $<$<CONFIG:Release>:-march=native>
    )
endif()