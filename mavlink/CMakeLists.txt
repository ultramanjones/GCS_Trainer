# The MAVLink link. A small static library so both the ground station
# and the vehicle simulator can use the same framing and the same
# message definitions. If the two ever disagree about a byte, they
# disagree in one place instead of two.
find_package(Qt6 REQUIRED COMPONENTS Core Network)

add_library(mavlink_link STATIC
    mavlinkframe.h
    mavlinkframe.cpp
    mavlinkmessages.h
    mavlinkmessages.cpp
    mavlinkradiolink.h
    mavlinkradiolink.cpp

    # More than one path to the same vehicle
    mavlinklinkpath.h
    mavlinklinkpath.cpp
    mavlinkvehiclelinkpaths.h
    mavlinkvehiclelinkpaths.cpp
    mavlinkstalemessagefilter.h
    mavlinkstalemessagefilter.cpp
    mavlinkcommandsawaitinganswer.h
    mavlinkcommandsawaitinganswer.cpp
)

set_target_properties(mavlink_link PROPERTIES AUTOMOC ON)

# The link implements RadioLinkInterface and builds a VehicleSitRep, so
# it needs to see mini-gcs headers. Include from the project root so
# the include lines read "model/..." and "modelview/..." the same way
# they do everywhere else.
target_include_directories(mavlink_link PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../mini-gcs
)

target_link_libraries(mavlink_link PUBLIC Qt6::Core Qt6::Network)
