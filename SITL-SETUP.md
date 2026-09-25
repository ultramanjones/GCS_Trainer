# Flying your ground station against a real autopilot

SITL means software in the loop. It is the actual ArduPilot flight
code, compiled to run on a PC instead of on a flight board. It does the
real math, runs the real state machine, and speaks real MAVLink. To
your ground station it is indistinguishable from an aircraft.

This costs nothing and needs no drone.

## The short version

1. Install Mission Planner.
2. Simulation tab, pick Multirotor, pick Stable. It downloads and runs
   the autopilot for you.
3. Build mini_gcs with both GCS_USE_MAVLINK_RADIO and GCS_MAVLINK_TCP
   turned on.
4. Run mini_gcs. It dials 127.0.0.1 port 5762 and starts receiving.

## Why TCP and not UDP here

Mission Planner runs the autopilot as a child process and keeps the
first connection for itself on port 5760. It leaves ports 5762 and
5763 open for other ground stations, and those are TCP. Our own vehicle
simulator uses UDP on 14550, which is the more common arrangement for
real radios and wifi telemetry bridges.

The link now speaks both. Same decoder, same frames, same everything
above it. The only difference is which socket class gets built, and
that is decided by one CMake option.

## Step by step

### 1. Mission Planner

Download from https://ardupilot.org/planner/ and install it. It is the
standard Windows ground station for ArduPilot, free and open source.

### 2. Start the autopilot

Open Mission Planner. Click the SIMULATION tab across the top. Pick
Multirotor in the bottom centre, and pick Stable. Mission Planner
downloads the autopilot the first time, then starts it and connects to
it. You should see an artificial horizon come alive and a vehicle on
the map.

Leave Mission Planner running. It is hosting the autopilot.

### 3. Build our ground station for TCP

In Qt Creator: Projects, then Build, then the CMake section. Find and
tick both of these:

    GCS_USE_MAVLINK_RADIO
    GCS_MAVLINK_TCP

Apply Configuration Changes, then build.

### 4. Run it

Run mini_gcs. It dials 127.0.0.1:5762. Within a second or two the
telemetry bar should fill in with real numbers coming out of real
ArduPilot code.

## What to do once it connects

Arm it, then take off, from Mission Planner's own controls at first.
Watch our panels follow. That alone is the thing worth showing someone:
our ground station reading a live autopilot it was never written
against.

Then try our command panel. Our link sends COMMAND_LONG and waits for
COMMAND_ACK, which is exactly what ArduPilot expects, so arm and
takeoff should work from our side too. If a command comes back refused,
read the reason. ArduPilot refuses for good reasons, and the refusal
arriving correctly is itself proof the command path works end to end.

## If nothing arrives

Check in this order.

Mission Planner is still running and still shows a live vehicle. If it
lost the autopilot, we have nothing to listen to.

The port. 5762 first. If that one is busy, try 5763 - change the number
in mini-gcs/main.cpp, in the GCS_MAVLINK_TCP branch.

Windows Firewall. It sometimes prompts on first run and the prompt can
hide behind another window.

The console. The link prints a warning naming the host and port if the
dial fails.

## Going back to our own vehicle simulator

Untick GCS_MAVLINK_TCP and rebuild. It goes back to binding UDP 14550,
which is where mavlink_vehicle_sim sends. Untick GCS_USE_MAVLINK_RADIO
as well and it goes back to the in-memory simulation with no network at
all.

Three configurations, one seam, nothing above it changed.
