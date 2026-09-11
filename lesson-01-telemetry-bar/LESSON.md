# Lesson 1 — Telemetry Bar

## 1. What you'll build tonight

A status bar across the top of a window: flight mode, armed state,
GPS fix and satellite count, signal strength, battery, and how many
seconds old the last update is. Behind it, a fake vehicle sends fresh
numbers 50 times a second on its own thread, and the bar updates
smoothly without ever freezing the window.

## 2. Why Shield AI cares

The job posting (R5189, Staff Engineer, Software, GCS – C++) says
the role covers "real-time command & control... and telemetry
visualization," and lists "diagnosing... UI responsiveness
bottlenecks" as a required skill. A telemetry bar fed by a worker
thread is the smallest real example of that problem: data arrives
fast and constantly, the screen must stay smooth, and the two
threads must never touch the same memory at the same time without
Qt's help. Every GCS has a bar like this one. QGroundControl calls
its version of the object underneath it a `Vehicle`, and its Fact
system is the general form of exactly what you're building tonight
by hand.

## 3. The idea

A worker thread makes up fake vehicle data very fast — 50 times a
second — the way a real MAVLink link would decode packets fast. But
the screen doesn't need 50 updates a second. Nobody can see a number
change that fast. So a second timer, slower, packs the latest values
into one bundle and sends **one signal** with all of them. That's
called coalescing: many small changes become one clean update.

The receiving side lives on the main thread — the same thread that
draws the window. It gets the bundle through a **queued connection**,
which is Qt's safe way to hand data from one thread to another. It
only tells QML "something changed" when a value is actually
different from before:

```cpp
if (m_batteryPercent != snapshot.batteryPercent) {
    m_batteryPercent = snapshot.batteryPercent;
    emit batteryPercentChanged();
}
```

That one `if` is most of the lesson. Skip it, and QML repaints
constantly for values that never moved.

## 4. Steps

The files already exist with `// TODO` comments marking exactly
where each step goes. Work through them in order.

1. **Read the data record.** Open `src/model/telemetrysnapshot.h`.
   This is already done — it's just a plain bundle of values, no
   logic. Notice `Q_DECLARE_METATYPE(TelemetrySnapshot)` at the
   bottom. Say out loud why that line has to be there before you move
   on (hint: it's about crossing threads).

2. **Make the worker produce data.** Open
   `src/modelview/telemetrysource.cpp`. Fill in `makeReading()` (the
   50 Hz side — just invent numbers, they don't have to be clever)
   and `publishSnapshot()` (the 20 Hz side — one line, emit the
   signal with the latest reading).

3. **Receive it safely on the main thread.** Open
   `src/viewmodel/vehiclemodel.cpp`. Fill in `applySnapshot()` — the
   if-check-then-emit pattern above, once per field. Finish by
   recording `m_lastSnapshotAtMs` so the link-age clock has something
   to measure against.

4. **Wire it together.** Open `src/main.cpp`. This is the
   composition root — the one place that builds the whole program by
   hand. Connect the worker's signal to the viewmodel's slot with a
   forced `Qt::QueuedConnection`, connect the thread's `started()`
   signal to the worker's `start()` slot, then start the thread.

5. **Show it.** Open `src/view/Main.qml`. Bind each `Text` element's
   `text` to the matching `vehicleModel` property, and bind the
   background `color` to `vehicleModel.linkStale`.

## 5. Run it

Build the `lesson01_telemetry_bar` target and run it. You should see
a dark green bar with numbers moving: RSSI drifting up and down,
battery counting down slowly, "DISARMED" turning to "ARMED" (in red)
after about two seconds. Click "Drop link." The numbers freeze, "Link
age" starts counting up, and after 5 seconds the whole bar fades to
gray. That gray fade is `vehicleModel.linkStale` doing its job.

If it doesn't build, open `solution/` in the same window and compare
your file to the finished one — don't just copy it over, find the
line that's different.

## 6. Interview questions this lesson answers

1. **Why `moveToThread` instead of subclassing `QThread`?**
   Subclassing ties your object's whole design to being a thread.
   `moveToThread` keeps `TelemetrySource` a plain `QObject` that
   happens to run its event loop somewhere else — easier to test,
   easier to reuse, and it's the pattern Qt's own docs recommend.

2. **What happens if you emit 50 property changes a second into
   QML?** QML re-evaluates every binding that reads that property,
   every time it changes. At 50 Hz that's real CPU spent redrawing
   text nobody can perceive changing that fast. Coalescing to a
   slower publish timer, plus the changed-value check, is how you
   avoid it.

3. **What is a queued connection, and when does Qt pick it for
   you?** A queued connection posts the call as an event on the
   receiver's thread instead of calling the function directly. Qt
   auto-picks it (`Qt::AutoConnection`) whenever it can tell sender
   and receiver live on different threads at connect time — but in
   this program we forced it, because that's the honest, readable
   choice when we already know the threads are different for the
   program's whole life.

4. **Why does a `QObject` belong to one thread?** Its timers, event
   handling, and any child `QObject`s all run on whatever thread
   called `moveToThread` last (or the thread that created it, if
   never moved). Calling its slots directly from another thread
   without going through the event system is a race condition
   waiting to happen — which is exactly why `dropLink()` uses
   `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` instead of
   calling `stopLink()` straight.

5. **Why do the two timers inside `TelemetrySource` take `this` as
   their parent?** `moveToThread()` moves an object and its children.
   Nothing else. A `QTimer` member with no parent stays behind on the
   thread that built it. Start it from the worker thread and Qt
   refuses: "Timers cannot be started from another thread." The
   program still opens its window and looks fine. No data ever
   arrives. Giving the timers a parent moves them with the object.

6. **How would you show that a value on screen is stale?** Track
   when the last real update arrived, tick a clock against it (here,
   once a second), and cross a threshold into a visibly different
   state — gray background, dimmed text, a "no signal" label. Never
   just leave the last good number sitting there looking current.
