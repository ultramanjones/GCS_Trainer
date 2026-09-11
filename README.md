# GCS Trainer

Hands-on Qt6/QML lessons. Each lesson builds one piece of a small
ground control station (GCS) for a made-up VTOL drone. This is
interview practice for a Shield AI "Staff Engineer, Software, GCS –
C++" role, which asks for Qt/QML plus hands-on QGroundControl work.

By the end, the lessons add up to a mini-GCS shaped like
QGroundControl: the same class names (Vehicle, LinkInterface,
FactGroup, MockLink), the same idea of separating the link from the
view from the data.

This is not a quiz app. Every lesson is real Qt code, written by hand.

## The two modes

- **BUILD mode.** Read `LESSON.md`, follow the numbered steps, ask
  Claude anything. This is where the code gets learned.
- **DRILL mode.** Read `DRILL.md`. Set a timer. Blank file. No Claude,
  no browser, compiler allowed. Talk out loud the whole time — the
  live interview round grades the thinking, not just the code. When
  the timer ends, show Claude the code. Claude names the gaps and
  never writes drill code.

## Daily rhythm

1. One drill, timer on, alone, out loud.
2. Show Claude the drill code. Claude names the gaps. Gaps go on
   tomorrow's drill.
3. Then BUILD mode on the next lesson, with Claude's help.
4. Commit at the end, once it builds and runs. Plain-English message.

## Running a lesson

Open the top-level `CMakeLists.txt` in Qt Creator and pick a target:

- `lesson01_telemetry_bar` — the starter version, with `// TODO`
  markers where you fill in the lesson's code.
- `lesson01_telemetry_bar_solution` — the finished, working version.
  Build and run this one if you want to see the target before you
  start, or to compare after your own attempt.

## Layout

```
gcs-trainer/
  CMakeLists.txt
  README.md
  lesson-01-telemetry-bar/
    LESSON.md          <- the teaching text (BUILD mode)
    DRILL.md            <- the timed exercise (DRILL mode)
    CMakeLists.txt
    src/                <- starter code, TODOs marked
    solution/           <- finished reference version
  lesson-02-.../
```

Every file in every lesson follows the house rules in
`Cleere_Programming_Best_Practices.md` (in the planning folder, one
level up, not in this repo): View → ViewModel → ModelView → Model,
one composition root, long clear names, no spinners, error logs that
name the method and the object.
