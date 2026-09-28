cmake_minimum_required(VERSION 3.21)
project(GcsTrainer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)

# qt_standard_project_setup turns on the Qt build steps this project
# needs (moc, resources) and puts QML modules under the standard
# ":/qt/qml/<module>" resource path. Without it, the engine cannot
# find a module by name and loadFromModule() fails at startup.
qt_standard_project_setup(REQUIRES 6.5)

# The MAVLink link. A small static library, shared by the ground
# station and the vehicle simulator so both agree about the bytes.
add_subdirectory(mavlink)

# The vehicle. A separate program on purpose - the only thing it
# shares with the ground station is bytes on a socket.
add_subdirectory(mavlink-vehicle-sim)

# A check that the MAVLink link handles two paths to one vehicle
# without races. Run mavlink_link_check after changing anything in
# mavlink/. It prints PASS or FAIL for each check.
add_subdirectory(mavlink-link-check)

# The ground control station itself.
add_subdirectory(mini-gcs)
