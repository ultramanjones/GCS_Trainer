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

# Each lesson is its own program, in its own subfolder, with its own
# CMakeLists.txt. Add a new lesson by adding one line here.
add_subdirectory(lesson-01-telemetry-bar)

# The practice ground control station. One whole working program,
# not a lesson. This is the thing to read and to show people.
add_subdirectory(mini-gcs)
