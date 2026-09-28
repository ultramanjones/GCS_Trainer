# mini-gcs

A small ground control station for an unmanned aircraft, written in
C++ and Qt 6 with a QML front end.

It shows a live map with the aircraft's track, an artificial horizon,
a telemetry bar, command buttons with slide-to-confirm, and an alert
list. It has been flown against ArduPilot's own flight software,
running in Mission Planner's simulator.

## What is in it

- **A hand-written MAVLink 2 link.** Framing, checksums and messages
  are written out in `mavlink/`. No MAVLink library is linked in.
- **Three radios behind one interface.** A simulated aircraft inside
  the program, real MAVLink over UDP, or real MAVLink over TCP.
  `RadioLinkInterface` is the only thing the ground station sees, so
  nothing above the radio changes when the radio does.
- **The network on its own thread.** The socket, decoding and link
  timers run on a worker thread. Data reaches the screen only as
  copies, through queued signals. There are no locks.
- **Several paths to one aircraft.** Two radios and a cell modem can
  all reach the same aircraft. One path is the main path for commands,
  and it moves when that path goes quiet. Copies and late arrivals are
  dropped using the aircraft's own clock. The aircraft counts as lost
  only when every path is quiet.
- **Every command gets an answer.** Each order has a request number
  and a timeout. The operator always sees accepted, refused with a
  reason, or no answer.
- **Honest link loss.** When the link drops, the screen says so. The
  last position stays on the map, marked as old, and nothing is
  invented for the time the link was down.

## Layout

```
mini-gcs/             the ground station
  main.cpp            builds and connects every object; read this first
  model/              plain value types: position, attitude, battery, sitrep
  modelview/          the radio interface, the ground station, the vehicles
  viewmodel/          organizes data for the screens
  view/               QML
mavlink/              the MAVLink link, shared with the vehicle simulator
mavlink-vehicle-sim/  a separate program that plays the aircraft over UDP
mavlink-link-check/   a test that plays an aircraft on two paths at once
```

## Building

Qt 6.5 or newer, with Qt Quick and Qt Quick Controls. Open the top
`CMakeLists.txt` in Qt Creator and build.

Two CMake options pick the radio:

| GCS_USE_MAVLINK_RADIO | GCS_MAVLINK_TCP | Radio |
|---|---|---|
| OFF | - | simulated aircraft, no network |
| ON | OFF | MAVLink over UDP, listening on port 14550 |
| ON | ON | MAVLink over TCP to 127.0.0.1:5762 (Mission Planner's simulator) |

Run `mavlink_link_check` after any change in `mavlink/`. It prints PASS
or FAIL for each check.

## More reading

- `GETTING-STARTED.md` - from nothing to flying it against ArduPilot
- `SITL-SETUP.md` - the simulator setup in more detail
- `VVMMVM-REFERENCE.md` - the four-layer design and why it is split that way
- `MAVLINK-NOTES.md` - the protocol, and what this does not do yet
