cmake_minimum_required(VERSION 3.28)

project(brownian-motor)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(
    app_icon_resource_windows
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/brownian-motor.rc"
)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Matplot++ CONFIG REQUIRED)

find_path(
        EXPRTK_INCLUDE_DIR
        NAMES exprtk.hpp
        REQUIRED
)

add_executable(brownian-motor
        main.cpp

        include/core/BurnInValidator.h
        src/core/BurnInValidator.cpp

        include/gui/MainWindow.h
        src/gui/MainWindow.cpp

        include/gui/SimulationWorker.h
        src/gui/SimulationWorker.cpp

        include/gui/CompactDoubleSpinBox.h

        include/gui/TrajectoryPlotWidget.h
        src/gui/TrajectoryPlotWidget.cpp

        include/core/ExperimentLogger.h
        src/core/ExperimentLogger.cpp

        include/core/PotentialDefinition.h

        include/core/ExpressionEvaluator.h
        src/core/ExpressionEvaluator.cpp

        include/core/PotentialConfigParser.h
        src/core/PotentialConfigParser.cpp

        resources/icons.qrc
        ${app_icon_resource_windows}
)

if (MINGW)
    set_source_files_properties(
            src/core/ExpressionEvaluator.cpp
            PROPERTIES
            COMPILE_OPTIONS "-Wa,-mbig-obj"
    )
endif()

target_include_directories(brownian-motor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${EXPRTK_INCLUDE_DIR}
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

include(CTest)

if (BUILD_TESTING)
    add_executable(
            exprtk-smoke-test
            tests/ExprTkSmokeTest.cpp

            include/core/PotentialDefinition.h

            include/core/ExpressionEvaluator.h
            src/core/ExpressionEvaluator.cpp
    )

    target_include_directories(
            exprtk-smoke-test
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${EXPRTK_INCLUDE_DIR}
    )

    add_test(
            NAME exprtk-smoke-test
            COMMAND exprtk-smoke-test
    )

    add_executable(
            potential-config-parser-smoke-test
            tests/PotentialConfigParserSmokeTest.cpp

            include/core/PotentialDefinition.h

            include/core/ExpressionEvaluator.h
            src/core/ExpressionEvaluator.cpp

            include/core/PotentialConfigParser.h
            src/core/PotentialConfigParser.cpp
    )

    target_include_directories(
            potential-config-parser-smoke-test
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${EXPRTK_INCLUDE_DIR}
    )

    target_link_libraries(
            potential-config-parser-smoke-test
            PRIVATE
            Qt6::Core
    )

    target_compile_definitions(
            potential-config-parser-smoke-test
            PRIVATE
            "BROWNIAN_MOTOR_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\""
    )

    add_test(
            NAME potential-config-parser-smoke-test
            COMMAND potential-config-parser-smoke-test
    )
endif()