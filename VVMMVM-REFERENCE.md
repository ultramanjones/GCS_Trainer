# VVMMVM, with a working program to read

**View - ViewModel - ModelView - Model**

The name is a mirror. The two middle words are the same two words
turned around. That is on purpose: each inner layer is the adapter that
faces the layer it is named after. Say it once and the order stays put.

This program, mini-gcs, is the reference implementation. It is a
working ground control station for an unmanned aircraft. It is small
enough to read in an evening and real enough to be honest: it has a
device at the far end, a worker thread, a live screen, and a link that
can go quiet in the middle of a flight.

This document explains the pattern and then points at the code that
does each part.

---

## The four layers

### Model

Plain value types. What a thing IS.

No Qt objects. No signals. No pointers to anything. A model object
copies, and you can test it with no program running and no hardware
present.

In this program:

    model/vehicleposition.h     where a vehicle is
    model/vehicleattitude.h     which way it is pointing
    model/vehiclebattery.h      how much charge is left
    model/vehiclesitrep.h       one report, built from the three above
    model/vehiclecommand.h      an order, and the answer to an order

Look at `VehiclePosition`. It holds meters east and north of the launch
point, and it carries the launch point inside itself, so it can work
out its own latitude and its own distance home without asking anything
else. That is what a model layer is for. The knowledge lives with the
data.

### ModelView

The far end, and the truth kept about it. What the program KNOWS.

This is the layer most people leave out, and leaving it out is where
the spaghetti starts. It holds two different kinds of thing:

The device edge - the radio, the socket, the protocol. Whatever is
actually out there.

    modelview/radiolinkinterface.h    the seam. a pure interface
    modelview/simulatedradiolink.h    a fake radio, all in memory
    mavlink/mavlinkradiolink.h        a real one, MAVLink over the wire

And the program's own running picture, which outlives any window:

    modelview/mapvehicle.h            what we believe about one aircraft
    modelview/groundcontrolstation.h  the station itself, owns the radio

`MapVehicle` is the one to study. It is not the aircraft. It is the
picture of the aircraft, built only out of messages that arrived. And
it knows one thing the aircraft itself can never know: whether it has
gone quiet. The aircraft has no idea it stopped being heard. The link
notices the silence, tells this object, and this object owns that fact
from then on.

That fact has to live here. Not on the link, because one link can carry
several aircraft and you need to know which one went quiet. Not on a
viewmodel, because closing a window must not lose it.

### ViewModel

The shape the screen wants. What gets SHOWN.

It holds no truth of its own. It reaches one layer down for the truth
and arranges it. It formats, it names, it decides what is worth
announcing.

    viewmodel/vehiclestatusviewmodel.h    the status strip and the instrument
    viewmodel/vehiclegroundtrackviewmodel.h   the map
    viewmodel/vehiclecommandviewmodel.h   the buttons and what came back
    viewmodel/alertlistviewmodel.h        the message list

`VehicleStatusViewModel` does the job plainly. Reports arrive twenty
times a second and most values in them did not move. Every property is
compared before it is announced, so QML only redraws what actually
changed.

### View

Pixels. Nothing else.

    view/Main.qml, view/TelemetryStatusBar.qml, and the rest

No arithmetic beyond laying things out. No decisions about what is
true. `TelemetryStatusBar.qml` reads properties and draws them. When
the link is quiet the whole strip turns gray, and even that is not the
view's decision - it reads `isOutOfContact` and colors accordingly.

---

## The rule everybody skips

Everyone agrees on separating concerns. Almost nobody says where the
device's own picture of the world is allowed to sit.

So it ends up on a viewmodel, because that is convenient, and now the
truth about the aircraft dies when a window closes. Or it ends up on
the link class, and now swapping the radio means rewriting what the
program believes.

VVMMVM says: it lives in the ModelView layer, alongside the device
edge, built out of Model value types. That is the whole rule, and it is
the one that keeps the radio from reaching up into the screen.

---

## Reading order

1. `mini-gcs/main.cpp` - the composition root. Every object in this
   program is built here and wired here. Nowhere else. Read this one
   file and you know the whole system.
2. `model/vehiclesitrep.h` - what moves between the layers.
3. `modelview/radiolinkinterface.h` - the seam.
4. `modelview/mapvehicle.h` - the truth that is kept.
5. `viewmodel/vehiclestatusviewmodel.h` - the shape the screen wants.
6. `view/TelemetryStatusBar.qml` - pixels.

---

## The demonstration

A pattern is a claim until someone tests it. This one got tested.

The program now runs against three completely different sources of
telemetry:

**One.** `SimulatedRadioLink` with `SimulatedAirspace`. A fake aircraft
flying in memory in this same process. No network at all.

**Two.** `MavlinkRadioLink` over UDP. Real MAVLink version 2 frames,
correct checksums, arriving on a socket from `mavlink-vehicle-sim`,
which is a separate program. The only thing the two share is bytes.

**Three.** `MavlinkRadioLink` over TCP, pointed at ArduPilot SITL. That
is the actual ArduPilot flight code, compiled for a PC. Not a mock of
an autopilot. The autopilot.

### What had to change

Going from one to three:

- A new class in the ModelView layer implementing `RadioLinkInterface`.
- One block in `main.cpp`, the composition root, choosing which one to
  build.

That is all.

### What did not change

- Every file in `model/`. Not one line.
- Every file in `viewmodel/`. Not one line.
- Every file in `view/`. Not one line.
- `MapVehicle` and `GroundControlStation`, the rest of the ModelView
  layer. Not one line.

Roughly three thousand lines above the seam went from a make believe
aircraft to a real autopilot without being touched, because nothing up
there ever knew what was down there. All any of it sees is a
`VehicleSitRep` arriving.

### The exact swap

In `mini-gcs/main.cpp`:

    // in memory, no network
    auto *radioLink = new SimulatedRadioLink(simulatedAirspace);

    // real MAVLink frames on a UDP port
    RadioLinkInterface *radioLink = new MavlinkRadioLink(quint16(14550));

    // real MAVLink over TCP, to ArduPilot SITL
    RadioLinkInterface *radioLink =
        new MavlinkRadioLink(QStringLiteral("127.0.0.1"), 5762);

Everything after that line is identical in all three cases. Same
`moveToThread`. Same connects. Same viewmodels. Same QML.

See `SITL-SETUP.md` for how to run the third one.

---

## What the seam actually is

`modelview/radiolinkinterface.h`. It is small on purpose:

    public slots:
        virtual void startListening() = 0;
        virtual void stopListening() = 0;
        virtual void sendVehicleCommand(VehicleCommandRequest request) = 0;

    signals:
        void vehicleSitRepReceived(VehicleSitRep sitRep);
        void vehicleCommandAcknowledged(VehicleCommandAcknowledgment ack);
        void contactLostWithVehicle(int vehicleIdentifier);
        void contactRegainedWithVehicle(int vehicleIdentifier);

Three orders down, four kinds of news up. Nothing in it mentions UDP,
TCP, MAVLink, or a socket. Nothing in it mentions a window either.

A seam that leaks the protocol is not a seam. If this interface had a
method taking a MAVLink message id, the swap above would have been a
rewrite.

---

## What it costs

Being straight about the price:

**More files.** A value type, an adapter, a viewmodel, and a QML file
to put one number on a screen. For a program that will never change and
never be tested, that is overhead with no return.

**Copying.** Sitreps are copied across the thread boundary rather than
shared by pointer. That is deliberate - shared pointers across threads
is how you get a race - but it is not free.

**Discipline.** The pattern gives you nothing if one viewmodel reaches
straight past the ModelView layer to the socket "just this once." The
first shortcut is the end of it.

It pays off when the hardware changes, when the UI gets rewritten, when
someone needs to test without the hardware present, or when two people
work on the screen and the device at the same time. On this program all
four of those happened.

---

## Where it sits among other patterns

Honestly: the idea is not unprecedented. Ports and adapters, also
called hexagonal architecture, lands in a similar place. MVVM with a
hard boundary at the device edge is a known arrangement. Clean
architecture argues the same direction.

What VVMMVM does is name the fourth layer out loud and say what belongs
in it. Most people nod at MVVM and then let the device talk straight to
the viewmodel, because nothing in the name told them not to. Naming the
layer is the thing that makes the rule stick, and a rule people
remember at the moment they need it beats a better rule they forget.

---

## Applying it

Six questions. If all six have clean answers, the layers are right.

1. Can I test this value type with no program running? It belongs in
   Model.
2. Does this fact have to survive a window closing? It belongs in
   ModelView.
3. Does this class know what a socket is? Then it is the device edge,
   and nothing above it may know.
4. Is this object formatting or naming something for a person to read?
   ViewModel.
5. Is there arithmetic in a QML file that is not about layout? Move it
   down.
6. Is any object built or connected anywhere but the composition root?
   Move it there.
