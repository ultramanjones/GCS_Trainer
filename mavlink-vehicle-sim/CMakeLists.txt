# The vehicle. A separate program on purpose - the only thing it shares
# with the ground station is bytes on a socket.
find_package(Qt6 REQUIRED COMPONENTS Core Network)

qt_add_executable(mavlink_vehicle_sim
    main.cpp
)

target_link_libraries(mavlink_vehicle_sim PRIVATE
    Qt6::Core
    Qt6::Network
    mavlink_link
)
