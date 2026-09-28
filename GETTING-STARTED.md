# Getting this running from scratch

Written for somebody who has never opened this repo. About an hour,
most of it waiting on downloads.

At the end you will have a ground control station on your screen,
talking real MAVLink to a real ArduPilot autopilot, and you will be
able to arm it, fly it to 60 meters, and bring it home.

---

## What this is

A small ground control station for an unmanned aircraft, written in
C++ and Qt 6 with a QML front end.

It talks to an aircraft three different ways, and which one it uses is
picked when you build it:

1. A make believe aircraft flying inside the program. No network.
2. Real MAVLink version 2 frames over a UDP socket, from a separate
   vehicle simulator program in this repo.
3. Real MAVLink to ArduPilot SITL - the actual ArduPilot flight code
   compiled to run on a PC.

Nothing above the radio layer knows which one is in use. That is the
point of the whole thing.

The MAVLink framing, the checksums, and the messages are hand written
in `mavlink/`. Nothing was generated and no MAVLink library is linked
in.

---

## Step 1. Get the code

    git clone https://github.com/ultramanjones/TeachMeAI.git

---

## Step 2. Install Qt

Go to https://www.qt.io/download-qt-installer and get the open source
installer. It makes you sign in with a Qt account, which is free.

In the installer, choose **Custom installation**. You need:

- **Qt 6.11.2** (any Qt 6.5 or newer will work, this is what it was
  built against)
  - **MinGW 11.2.0 64-bit** or whichever MinGW it offers under that
    version. On Windows use MinGW, not MSVC - that is the compiler
    this was built with.
  - **Qt Quick** and **Qt Quick Controls** are part of the default Qt 6
    selection, leave them ticked.
- Under **Developer and Designer Tools**:
  - **Qt Creator** (the editor)
  - **CMake**
  - **Ninja**
  - **MinGW 11.2.0 64-bit** (the toolchain itself, a separate tick from
    the Qt build above)

It is a big download. Start it and go do something else.

On Linux or macOS, install Qt the same way and take the default
compiler for your platform instead of MinGW.

---

## Step 3. Install Mission Planner

This is how you get an autopilot without owning an aircraft.

https://ardupilot.org/planner/ - download the Windows installer and
run it. Take the defaults. It installs a driver package along the way;
let it.

If it asks about an **Altitude Angel** plugin on first run, answer No.
It wants an account and you do not need it.

**On Linux or macOS**, Mission Planner is not the route. Install
ArduPilot SITL directly by following
https://ardupilot.org/dev/docs/setting-up-sitl-on-linux.html and start
it with `sim_vehicle.py -v ArduCopter`. That puts MAVLink on UDP port
14550, so in step 5 tick only `GCS_USE_MAVLINK_RADIO` and leave
`GCS_MAVLINK_TCP` off.

---

## Step 4. Start the autopilot

Open Mission Planner. Click the **SIMULATION** tab across the top.
Pick **Multirotor**, and pick **Stable**.

The first time, it downloads the autopilot. Then it starts it and
connects.

You will know it worked when the artificial horizon comes alive, the
map jumps to somewhere in Australia, and Sats reads 10. Australia is
correct - ArduPilot's default home position is an airfield outside
Canberra.

**Leave Mission Planner open.** It is hosting the autopilot. Close it
and the aircraft is gone.

---

## Step 5. Open and configure the project

In Qt Creator: **File**, **Open Project**, and pick `CMakeLists.txt`
at the top of the repo folder.

Pick the **Desktop Qt 6.11.2 MinGW 64-bit** kit when it asks. Let it
configure.

Now turn the radio on. Click **Projects** in the left sidebar, then
**Build** under your kit. Scroll down to the CMake settings list.
There is a **Filter** box - type `GCS_` in it.

Tick both:

    GCS_USE_MAVLINK_RADIO
    GCS_MAVLINK_TCP

Click **Run CMake**, or **Apply Configuration Changes**, whichever
button you see.

You may see a warning that the build path contains a space. Ignore it.

---

## Step 6. Build and run

Build with **Ctrl+B**.

This repo builds several programs, so pick the one you want before
running. Bottom left of Qt Creator, click the computer icon above the
green arrow. A panel opens with a **Run** list. Choose **mini_gcs**.

Run it.

Within a second or two the status bar fills in: MODE Stabilize, GPS
RTK Fixed, 10 sats, BATTERY 100 percent. The message list at the
bottom says **Vehicle 1 is on the air**.

RADIO reads `--` and that is correct. Signal strength is reported by
the telemetry radio, and there is no radio in a network socket, so
nothing reported one and the program does not invent a number.

---

## Step 7. Fly it

In the COMMANDS panel:

**Arm**. It should come back accepted and ARMED lights up.

**Launch**. It climbs to 60 meters and holds.

**Return**. It flies back over home.

**Land**. It comes down.

Under the hood, Launch is two orders, not one. ArduCopter only obeys a
takeoff in Guided mode, so the link sends a mode change first, waits
for the answer, and only then sends the takeoff. You press one button
and get one answer.

---

## Step 8. The thing worth seeing

With it flying, close Mission Planner.

The status bar goes gray. The numbers stay on screen but they stop
being current, and the program says so rather than letting a stale
battery reading look live. An operator acting on a minute old number
has no way to know it is old unless the screen tells them.

Start the simulator again and contact comes back.

---

## What to read

If you only have twenty minutes:

- `VVMMVM-REFERENCE.md` - the architecture, and what the radio swap
  proved about it
- `mini-gcs/main.cpp` - every object in the program is built and wired
  here and nowhere else. Read this one file and you know the system.

If you want the MAVLink details:

- `MAVLINK-NOTES.md` - frame layout, why the CRC extra byte is really a
  version check, field ordering, and an honest list of what is missing
  compared to a real ground station
- `mavlink/mavlinkframe.cpp` - bytes in, frames out
- `mavlink/mavlinkradiolink.cpp` - the socket, the per vehicle state,
  the watchdog, and the command round trip

Other docs: `SITL-SETUP.md` covers the simulator in more depth.

---

## If it does not work

**Nothing arrives.** Check Mission Planner is still running and still
shows a live vehicle. Then check the port: 5762 is the default; if
something else has it, try 5763 by changing the number in
`mini-gcs/main.cpp` in the `GCS_MAVLINK_TCP` branch.

**Firewall prompt.** Windows sometimes asks on first run and the
prompt hides behind another window.

**It builds but the window is dead.** Look at the Application Output
pane in Qt Creator. The link prints a warning naming the host and port
if it could not connect.
