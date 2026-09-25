# Wiring the MAVLink link into mini-gcs

Three edits. Nothing above the link changes.

## 1. Top level CMakeLists.txt

Add these two lines beside the existing `add_subdirectory` calls:

```cmake
add_subdirectory(mavlink)
add_subdirectory(mavlink-vehicle-sim)
```

## 2. mini-gcs/CMakeLists.txt

Add `mavlink_link` to what mini_gcs links against:

```cmake
target_link_libraries(mini_gcs PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    mavlink_link          # <- add this
)
```

## 3. mini-gcs/main.cpp, the composition root

Swap one line. Keep the old one commented beside it so the difference
is visible.

```cpp
#include "mavlink/mavlinkradiolink.h"

// The fake radio, everything in memory:
// auto *radioLink = new SimulatedRadioLink(&simulatedAirspace);

// The real one, listening for MAVLink frames on UDP 14550:
auto *radioLink = new MavlinkRadioLink(14550);
```

Everything after that line stays exactly as it is - the same
moveToThread, the same connects, the same viewmodels. That is the whole
point of the seam.

## Running

Two terminals.

```
./mavlink_vehicle_sim
./mini_gcs
```

Arm. Launch. Watch it climb to 60 meters and orbit at 150 meters
radius. Return. Watch it fly straight home and land.

Then kill the vehicle with Ctrl+C while it is flying, and watch the
ground station notice.
