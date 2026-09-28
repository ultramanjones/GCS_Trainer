# MAVLink, in plain words

Background notes on the protocol. This is the reading, not the code.
The code is in `mavlink/` and `mavlink-vehicle-sim/`.

---

## What MAVLink actually is

A way of packing small messages into small frames so a vehicle and a
ground station can talk over a radio that drops bytes. That is the
whole idea. It is not a protocol stack, not a transport, not a
framework. It is a framing rule and a list of message layouts.

It rides on anything that carries bytes. Serial radio, UDP, TCP. This
project uses UDP because it is what everyone develops against.

---

## The frame

Every message goes out inside a frame. Version 2 frames look like this:

```
byte 0       0xFD        start marker
byte 1       length      how many payload bytes, 0 to 255
byte 2       incompat    flags a receiver MUST understand or drop the frame
byte 3       compat      flags a receiver may ignore
byte 4       sequence    counts up, wraps at 255
byte 5       system id   which vehicle
byte 6       component   which part of that vehicle
bytes 7-9    message id  three bytes, low byte first
bytes 10+    payload
last 2       checksum    low byte first
```

Ten bytes of header, then payload, then two bytes of check. Version 1
starts with 0xFE and has a six byte header. Anything current is
version 2.

Three details that matter and that people get wrong:

**Trailing zeros are dropped.** Version 2 trims zero bytes off the end
of the payload before sending, and the receiver fills the rest of the
struct back in with zeros. It saves radio time on messages where most
fields are empty, which is most of them. It also means the length byte
does not tell you the message's real size.

**The sequence number is per sender, not per message type.** A gap in
it means frames were lost between you and that vehicle. That is how a
ground station knows the link is lossy rather than the vehicle being
quiet.

**System id and component id are how one radio carries several
vehicles.** The ground station is itself a system id, usually 255.

---

## The check value, and why it is clever

The checksum is CRC-16/MCRF4XX, run over the frame from the length byte
through the end of the payload. Then one more byte is fed in, and that
byte depends on which message this is.

That extra byte is computed from the message's field list. Change a
field, and the byte changes. Which means two programs built against
different versions of the same message cannot accidentally talk to each
other and misread the data — every frame fails its check instead.

It is a version check disguised as a checksum. If you ever point a
ground station at a real autopilot and every single frame fails, that
table is the first place to look.

---

## How a payload is laid out

Two rules, and getting either wrong gives you a frame that passes its
check and decodes to garbage:

1. **Fields are ordered largest first.** All the eight byte fields,
   then the four byte, then the two byte, then the one byte. Not the
   order they are listed in the documentation.
2. **Little endian, no padding.**

A real project generates all of this from the XML message definitions
that ship with MAVLink and ends up with several hundred messages. The
eight in this project are written by hand because they are the ones a
ground station actually needs, and because a generated header teaches
nothing.

---

## The messages this project speaks

| id  | name                  | rate      | carries |
|-----|-----------------------|-----------|---------|
| 0   | HEARTBEAT             | 1 Hz      | alive, flight mode, armed or not |
| 1   | SYS_STATUS            | 2 Hz      | battery |
| 24  | GPS_RAW_INT           | 2 Hz      | fix quality, satellite count |
| 30  | ATTITUDE              | 20 Hz     | roll, pitch, yaw in radians |
| 33  | GLOBAL_POSITION_INT   | 20 Hz     | position, altitude, velocity, heading |
| 74  | VFR_HUD               | 20 Hz     | airspeed, groundspeed, climb rate |
| 76  | COMMAND_LONG          | as needed | an order going out |
| 77  | COMMAND_ACK           | as needed | the answer to that order |
| 253 | STATUSTEXT            | as needed | the vehicle talking in words |

Heartbeat at one per second is the convention. A vehicle that stops
sending it is considered gone.

Position comes as degrees times ten million in a signed 32-bit integer.
About a centimeter of resolution, and no floating point drift on the
wire. Angles come in radians. People read degrees, so the link converts
once on the way in and no layer above ever does trigonometry.

---

## Why a sitrep is built from several messages

MAVLink does not send one message with everything in it. Position is in
one message, attitude in another, battery in a third, armed state in a
fourth, and they all arrive at their own rates.

So `MavlinkRadioLink` keeps one running picture per vehicle, updates
whatever piece just arrived, and sends the whole picture upward on a
50 millisecond timer.

That timer is the coalescing, and it is the answer to "how do you keep
the UI responsive under high rate telemetry." Frames arrive in the
hundreds per second across several vehicles. The screen gets twenty
updates a second carrying the newest of everything. The thread that
draws the window never sees the difference between a quiet link and a
loud one.

---

## Commands, and why every one gets an answer

An order goes out as COMMAND_LONG with a MAV_CMD number and up to seven
float parameters:

| number | name                    | what it means |
|--------|-------------------------|---------------|
| 400    | COMPONENT_ARM_DISARM    | param1 is 1 to arm, 0 to disarm |
| 22     | NAV_TAKEOFF             | param7 is target height above home |
| 20     | NAV_RETURN_TO_LAUNCH    | come home |
| 21     | NAV_LAND                | land here |
| 185    | DO_FLIGHT_TERMINATION   | stop, now |

Every one comes back as COMMAND_ACK with a MAV_RESULT: accepted,
temporarily rejected, denied, unsupported, or failed.

The ground station must match an answer to the order that caused it. An
answer that arrives late looks exactly like an answer to whatever was
sent next, and an operator who sees "accepted" against the wrong button
is being lied to. `VehicleCommandRequest` carries a request identifier
for exactly this.

---

## Where this sits in the architecture

The seam was already there. `RadioLinkInterface` is the line, and
everything above it — viewmodel, view — only ever sees a
`VehicleSitRep` arrive and a `VehicleCommandRequest` go out.

```
view/            QML. Knows nothing about radios.
viewmodel/       Qt objects the QML binds to.
modelview/       SimulatedRadioLink  <- fake, all in memory
                 MavlinkRadioLink    <- real frames over UDP
model/           plain value types. No Qt objects.
```

Swapping one link for the other is one line at the composition root:

```cpp
// was
auto *radioLink = new SimulatedRadioLink(&airspace);

// now
auto *radioLink = new MavlinkRadioLink(14550);
```

Nothing else changes. That is what the seam was for. The parser could
be swapped in without touching a line above it.

---

## What is missing, compared to real QGroundControl

The known gaps, stated plainly:

- **No signing.** MAVLink 2 can sign frames so a receiver knows they
  came from who they claim. Not implemented here.
- **No parameter protocol.** Reading and writing the hundreds of tuning
  parameters on an autopilot is its own message set and its own state
  machine, with retries.
- **No mission protocol.** Uploading and downloading waypoints is
  another protocol on top, and it is the fiddly one — item by item,
  each acknowledged, with sequence handling.
- **No log download, no file transfer, no camera protocol.**
- **Home position is guessed.** The first fix becomes home. A real
  ground station takes it from HOME_POSITION and updates when it moves.
- **The message set is eight, not several hundred.**
- **The flight model in the simulator is trigonometry**, not
  aerodynamics. It flies a circle. It does not fly.

---

## Running it

Two programs, two terminals.

```
./mavlink_vehicle_sim      # the vehicle, sends on UDP 14550, listens on 14551
./mini_gcs                 # the ground station, with a MavlinkRadioLink
```

Then Arm, then Launch, then watch it climb and orbit, then Return and
watch it fly a straight line home and come down.

Kill the vehicle with Ctrl+C mid-flight. The watchdog in the link
notices at 1.5 seconds and reports contact lost.
