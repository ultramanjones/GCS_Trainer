# Drill 1 — Telemetry Bar

**Timer: 20 minutes.**

## The task

From a blank `.cpp` file (no starter code, no notes):

1. Write a `QObject` with two `Q_PROPERTY` values and a `NOTIFY`
   signal for each.
2. Write a second `QObject` — a worker — that gets moved to a
   `QThread` and emits a new value every 100 ms.
3. Connect the worker's signal to a slot on the first object, with a
   queued connection.
4. Print the values as they arrive (`qDebug()` is fine).

No QML. No window. Just the two classes, the thread, and the
connection, compiling and printing in a console app.

## Say out loud while you work

- Why the worker needs to be moved, not subclassed.
- Why the connection has to be queued.
- What you'd change if this were real telemetry instead of a test
  value.

## Rules

- Blank file. No Claude. No browser. No looking at Lesson 1's code.
- The compiler is allowed. Compiler errors are not cheating — reading
  the fix in a search engine is.
- If you blank on something, say so out loud and reason toward it
  anyway. That recovery is what the real round is grading.

## When the timer ends

Stop, whether it compiles or not. Show Claude the code as it stands.
Claude names what's missing or wrong — and only after the timer,
never during.

If it builds and runs, commit it — plain-English message, per the
house git law (build-and-run before every commit, no exceptions,
even for a drill). If it doesn't build, don't commit it; just keep it
somewhere you'll see it again (a `drills/` scratch folder outside the
repo is fine) so tomorrow's drill can target the same gap.
