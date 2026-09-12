cmake_minimum_required(VERSION 3.21)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)

# One program. The folders under here are the four layers from the
# house rules: view, viewmodel, modelview, model. main.cpp is the
# composition root and sits beside them, inside none of them.
qt_add_executable(mini_gcs
    main.cpp

    model/telemetrysnapshot.h
    model/vehiclealert.h

    modelview/linkinterface.h
    modelview/vtolflightsimulator.h
    modelview/vtolflightsimulator.cpp
    modelview/mocklink.h
    modelview/mocklink.cpp

    viewmodel/vehiclestatusviewmodel.h
    viewmodel/vehiclestatusviewmodel.cpp
    viewmodel/flightmapviewmodel.h
    viewmodel/flightmapviewmodel.cpp
    viewmodel/alertlistviewmodel.h
    viewmodel/alertlistviewmodel.cpp
    viewmodel/commandpanelviewmodel.h
    viewmodel/commandpanelviewmodel.cpp
)

set(miniGcsQmlFiles
    view/GcsTheme.qml
    view/Main.qml
    view/TelemetryStatusBar.qml
    view/AttitudeIndicator.qml
    view/FlightMapPanel.qml
    view/CommandPanel.qml
    view/AlertListPanel.qml
)

# The QML files live in the view folder on disk, because the house
# rules say the view layer gets its own folder. Inside the built
# program they are moved to the top of the QML module instead. That
# way one view file can use another by plain name, the way QML
# expects, without a folder name in the middle.
foreach(oneQmlFilePath IN LISTS miniGcsQmlFiles)
    get_filename_component(oneQmlFileName ${oneQmlFilePath} NAME)
    set_source_files_properties(${oneQmlFilePath} PROPERTIES
        QT_RESOURCE_ALIAS ${oneQmlFileName})
endforeach()

# GcsTheme is a singleton: one shared object holding the colors, so
# every panel reads the same palette and a color change lands in one
# place.
set_source_files_properties(view/GcsTheme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(mini_gcs
    URI MiniGcs
    VERSION 1.0
    QML_FILES ${miniGcsQmlFiles}
)

target_include_directories(mini_gcs PRIVATE .)

target_link_libraries(mini_gcs PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
)
