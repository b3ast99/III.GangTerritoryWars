## Project Context

- Project: `III.GangTerritoryWars`, an ASI plugin mod for GTA III.
- Goal: port selected GTA San Andreas systems and mechanics into GTA III while staying faithful to GTA III's narrative, tone, and world-building.
- Quality bar: stable, well-implemented, portable, and non-invasive.
- Deployment model: loaded entirely through `modloader`; does not edit base game files.
- Persistence model: mod-owned persistence tied to GTA save slots, with sidecar data stored separately from game files.

## Current Development Reality

- The project was initially built through AI-assisted "vibe coding" with the user directing behavior from gameplay testing, logs, and compiler feedback.
- The user is an experienced software developer, but is new to game modding and only lightly familiar with C/C++.
- Main need: move from slow copy/paste iteration toward a tighter, more disciplined engineering loop.

## Working Assumptions

- Logging is a core tool for runtime validation and can be expanded as needed.
- Codex should prefer minimal, targeted inspection rather than broad repo scans to control usage.
- Normal loop:
  1. inspect only the relevant subsystem
  2. make the smallest viable code change
  3. build locally
  4. deploy to the user's GTA III install
  5. use logs plus user gameplay testing to validate behavior

## Local Environment Notes

- Canonical GTA III install: `F:\GTA\GTA III - Remastered`
- Live mod folder: `F:\GTA\GTA III - Remastered\modloader\III.GangTerritoryWars`
- Windows helper scripts:
  - `C:\tmp\build_gtw.bat`
  - `C:\tmp\build_tests.bat`
- The project `.vcxproj` already contains a post-build deploy step that copies the built ASI into the live mod folder when `GTA_III_DIR` is set.
