cmake_minimum_required(VERSION 3.21)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)

# One program. The folders are the four layers from the house rules:
# view, viewmodel, modelview, model. main.cpp is the composition root
# and sits beside them, inside none of them.
qt_add_executable(mini_gcs
    main.cpp

    # Model — value types and messages. Plain copyable C++, no Qt
    # objects, testable with nothing running.
    model/vehicleposition.h
    model/vehicleposition.cpp
    model/vehicleattitude.h
    model/vehicleattitude.cpp
    model/vehiclebattery.h
    model/vehiclebattery.cpp
    model/vehiclesitrep.h
    model/vehiclecommand.h
    model/vehiclealert.h

    # ModelView — the far end of the radio
    modelview/flightpattern.h
    modelview/racetrackflightpattern.h
    modelview/racetrackflightpattern.cpp
    modelview/simulatedvehicle.h
    modelview/simulatedvehicle.cpp
    modelview/simulatedairspace.h
    modelview/simulatedairspace.cpp

    # ModelView — the radio itself
    modelview/radiolinkinterface.h
    modelview/simulatedradiolink.h
    modelview/simulatedradiolink.cpp

    # ModelView — the ground
    modelview/mapvehicle.h
    modelview/mapvehicle.cpp
    modelview/groundcontrolstation.h
    modelview/groundcontrolstation.cpp

    # ViewModel — organizes for the screen, calculates nothing
    viewmodel/vehiclestatusviewmodel.h
    viewmodel/vehiclestatusviewmodel.cpp
    viewmodel/vehiclegroundtrackviewmodel.h
    viewmodel/vehiclegroundtrackviewmodel.cpp
    viewmodel/vehiclecommandviewmodel.h
    viewmodel/vehiclecommandviewmodel.cpp
    viewmodel/alertlistviewmodel.h
    viewmodel/alertlistviewmodel.cpp
)

set(miniGcsQmlFiles
    view/GcsTheme.qml
    view/Main.qml
    view/TelemetryStatusBar.qml
    view/AttitudeIndicator.qml
    view/FlightMapPanel.qml
    view/CommandPanel.qml
    view/SlideToConfirm.qml
    view/AlertListPanel.qml
)

# The QML files live in the view folder on disk, because the house
# rules say the view layer gets its own folder. Inside the built
# program they are moved to the top of the QML module instead. That way
# one view file can use another by plain name, the way QML expects,
# without a folder name in the middle.
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
