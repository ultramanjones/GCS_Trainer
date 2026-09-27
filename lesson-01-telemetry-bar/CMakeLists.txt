cmake_minimum_required(VERSION 3.21)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)

# --- The starter program. This is what Matt edits during the lesson. ---
qt_add_executable(lesson01_telemetry_bar
    src/main.cpp
    src/model/telemetrysnapshot.h
    src/modelview/telemetrysource.h
    src/modelview/telemetrysource.cpp
    src/viewmodel/vehiclemodel.h
    src/viewmodel/vehiclemodel.cpp
)

# The QML file sits in a view folder on disk, but inside the built
# program it goes at the top of the QML module, the same as in
# mini-gcs. Without this, Qt 6.8 and later warn (policy QTP0004) that
# the view folder needs its own qmldir file.
set_source_files_properties(src/view/Main.qml PROPERTIES
    QT_RESOURCE_ALIAS Main.qml)

qt_add_qml_module(lesson01_telemetry_bar
    URI GcsTrainerLesson01
    VERSION 1.0
    QML_FILES
        src/view/Main.qml
)

target_include_directories(lesson01_telemetry_bar PRIVATE src)

target_link_libraries(lesson01_telemetry_bar PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
)

# --- The finished reference version. Claude opens this only after a ---
# --- drill or a BUILD-mode attempt is done, to check the real thing. ---
qt_add_executable(lesson01_telemetry_bar_solution
    solution/main.cpp
    solution/model/telemetrysnapshot.h
    solution/modelview/telemetrysource.h
    solution/modelview/telemetrysource.cpp
    solution/viewmodel/vehiclemodel.h
    solution/viewmodel/vehiclemodel.cpp
)

set_source_files_properties(solution/view/Main.qml PROPERTIES
    QT_RESOURCE_ALIAS Main.qml)

qt_add_qml_module(lesson01_telemetry_bar_solution
    URI GcsTrainerLesson01Solution
    VERSION 1.0
    QML_FILES
        solution/view/Main.qml
)

target_include_directories(lesson01_telemetry_bar_solution PRIVATE solution)

target_link_libraries(lesson01_telemetry_bar_solution PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
)
