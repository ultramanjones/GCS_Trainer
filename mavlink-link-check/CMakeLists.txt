# A check that the MAVLink link handles two paths to one vehicle
# without races. Run it after changing anything in mavlink/. It prints
# PASS or FAIL for each check and takes about ten seconds.
find_package(Qt6 REQUIRED COMPONENTS Core Network)

qt_add_executable(mavlink_link_check
    main.cpp

    # The link is built against the ground station's interface and
    # value types, so the check needs them too.
    ../mini-gcs/modelview/radiolinkinterface.h
    ../mini-gcs/model/vehicleposition.cpp
    ../mini-gcs/model/vehicleattitude.cpp
    ../mini-gcs/model/vehiclebattery.cpp
)

set_target_properties(mavlink_link_check PROPERTIES AUTOMOC ON)

target_link_libraries(mavlink_link_check PRIVATE
    Qt6::Core
    Qt6::Network
    mavlink_link
)
