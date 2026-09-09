# Hexsweeper Flat for Pebble

By Luke Steuber. Native testing build 0.2.0.

A flat hexagonal Minesweeper board that fits entirely on the watch: 37 cells, six mines and 31 safe cells. The first dig and its neighbors are safe. Clear every safe cell to win. Numbers count mines among up to six adjacent hexes. Boards are random and are not guaranteed to be solvable without guessing.

- Tap a cell to focus it; Select digs. Touch never digs directly.
- Hold Select for half a second to toggle a flag. A flagged cell cannot be dug.
- Up/Down move focus backward/forward through rows from top to bottom. Every cell is reachable with buttons alone.
- Back opens Resume, How to play, New board, and Save and exit. Replacing a board requires confirmation.
- Drags and long touches are ignored. There is no zoom, scrolling or chording.

Covered cells have a gray fill and sparse dots. Open cells have a black interior and white numbers; blank open cells have no dots. Flagged cells have a solid amber fill and a bold black pennant, pole and base. The focused cell has a cyan and white double outline, distinct from the flag color. Mine symbols appear after a loss. These cues do not require distinguishing hues.

Flat has its own app UUID and save, so it installs alongside Hexsweeper Sphere. The game saves the board and focused cell locally with a checksum. It uses no phone JavaScript, network or accounts.

## Build and verify

Targets: Pebble Time 2 (`emery`, 200 by 228) and Pebble Round 2 (`gabbro`, 260 by 260). Build with Pebble SDK 4.33.1 and pebble-tool 5.0.39:

```sh
pebble build
clang -std=c11 -Wall -Wextra -fsanitize=address,undefined test/core.c src/c/game.c -o build/core-test
build/core-test
# Use pebble-tool's Python environment:
python test/emulator.py emery
python test/emulator.py gabbro
python3 tools/package.py
```

The core suite covers 37,000 boards: symmetric adjacency, safe openings, flag protection, flood reveal, wins and losses. Native tests install the exact PBW, exercise short and held button presses, tap focus and ignored drags, play a button-only win, lose, restore a save and cancel a reset. Screenshots and hash-matched reports accompany `release/0.2.0/`. The source archive builds independently; no geometry generator or web dependencies are required.

Touch is enabled on firmware 4.33.2 or newer; buttons provide every action. Firmware 4.33.2 emulators are the validation target. Physical installation, readability, touch precision, battery use and older firmware compatibility remain unverified. This is a locally staged testing build, not a store release.
