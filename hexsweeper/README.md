# Hexsweeper for Pebble

The combined development build offers **Classic, Infinite, and 3D** in one app.
Each mode keeps its own board. The first launch opens the mode picker; later
launches resume the last mode. Choose Game modes from the pause menu to switch,
or Save and exit from the picker to return to the watch.

| Mode | Board and objective |
| --- | --- |
| Classic | 37 cells, six mines. Reveal all 31 safe cells to win. |
| Infinite | Explore a deterministic field and increase the safe-cell count. No final board or win screen. |
| 3D | 42 cells, seven mines. Reveal all 35 safe cells to win. |

All modes protect the first dig and its neighbors and support chording. With Dig
active, select an open clue: matching neighboring flags open the other neighbors.
Incorrect flags can lose the game. Flags have an amber fill and black pennant.

Classic uses Up/Down to move focus, Select to dig or chord, and hold Select to flag.
Touch aims without digging. Infinite adds drag panning, Up for six-direction
button exploration, and Down to switch Dig/Flag. Back opens the pause menu.

Infinite retains up to 64 explored 8-by-8 chunks. Discovered cells are never
recycled. At the history limit, a message explains that new areas cannot be
saved; existing areas remain playable. Removing the last flag from an otherwise
untouched chunk frees its slot. Two checksummed snapshots retain position, flags,
revealed cells and pending flood expansion across interrupted saves.

The combined app keeps the existing 3D UUID and save format. The old standalone
Classic app has separate storage that this app cannot import. Archived releases
and historical validation below describe the earlier standalone 3D app; the
combined development build has not yet been packaged or uploaded.

A small sphere of 42 cells, with seven hidden mines. Drag it to turn it in your
hand, or travel around it using the watch buttons. Built by Luke Steuber.

This native, offline watchapp targets **Pebble Time 2** (`emery`, 200 × 228)
and **Pebble Round 2** (`gabbro`, 260 × 260). It does not target the older
Pebble 2 or Time Round. It uses no phone JavaScript, network, account, microphone,
or health permissions.

Version **0.4.0** gives flagged cells a solid amber fill and a larger black flag. The fill stays visible as a cell rotates toward the rim. A cyan outline over a white border marks focus independently of flags. Saved boards and game rules are unchanged.

Version **0.3.0** adds inset outlines and sparse dots to covered front-facing cells, with flat white open cells and black numbers. Fills no longer change with depth, so rotation does not change their state cue. Flags and the selected-cell outline remain distinct.

Testing build **0.2.0** advances the watch-visible major/minor version and removes general-purpose math-library calls from projection and rendering. The UUID and saved-board format stay unchanged. The reported phone installation failure remains unverified on hardware.

Touch requires firmware **4.33.2 or later**. Version 0.1.1 checks the firmware
before subscribing to touch and removes the unconditional touch-bridge call
from startup. On older firmware the app uses button controls and explains the
touch requirement in Controls. The UUID and save format are unchanged from 0.1.0.
This addresses a launch compatibility risk; the reported physical Time 2 crash
has not yet been reproduced or confirmed fixed on that watch.

## Playing

The 30 hexagons and 12 pentagons form one connected sphere. Numbers count mines
in adjacent cells; pentagons have five neighbors, hexagons six. The first dig
and its neighbors are safe. Clear all 35 safe cells to win. Flags are optional
and protect a cell from digging and flood opening.

With Dig selected, activate an open clue to chord: if its neighboring flag count
matches the clue, its unflagged neighbors open. Incorrect flags can detonate a
mine. This works on both pentagons and hexagons.

| Control | During play |
| --- | --- |
| Drag | Rotate the sphere; never dig |
| Tap | Center a cell; Select then applies the current tool |
| Select | Dig, or flag when the Flag tool is selected |
| Hold Select | Toggle a flag without digging |
| Down | Switch between Dig and Flag |
| Up | Enter Explore |
| Back | Open the pause menu |

Explore supports a complete game with buttons alone. Up and Down cycle through
the centered cell's neighbors in clockwise order. Select moves to the highlighted
neighbor and centers it. Back returns to play, where Select applies the tool.
The cyan and white outline marks the target; numbers and flag symbols convey cell
state without relying on color.

Choose **Touch: direct** in the pause menu to make taps apply the current tool.
Rim taps still only center a cell so a narrow edge cannot accidentally reveal it.
A tap lasts at most 350 ms. Movement beyond eight pixels becomes a drag, even
when the finger returns to its starting point. There is no inertia or pinch zoom.

The watch saves the board, orientation, selected cell, tool, and touch preference.
New Game asks whether to replace the current board and defaults to keeping it.
Save & Exit returns to the watch. A failed write leaves the app open with a
message; an invalid or incompatible save starts a fresh board and shows controls.

## Build and verify

Validated toolchain: Pebble SDK **4.33.1**, pebble-tool **5.0.39**. Install the SDK
through the rePebble tools, select that SDK, and run from this directory:

```sh
sh test/core.sh
pebble build
# Use the Python interpreter belonging to the installed pebble-tool environment:
python test/emulator.py emery
python test/emulator.py gabbro
python test/combined.py emery
python test/combined.py gabbro
python3 tools/package.py
```

The generated `src/c/geometry.h` is checked in, so `pebble build` has no Node or
Python art dependency beyond the SDK itself. Geometry verification needs Node;
`tools/geometry.cjs` exports the same 42-cell topology as the web game's
`sphere/hexasphere.js`. Regenerate it with `node tools/geometry.cjs` if that
canonical topology changes. The optional launcher-mark generator uses Pillow.

`test/core.sh` runs the production rules, projection and controller with address
and undefined-behavior sanitizers. It checks 42,000 seeded boards, first-dig
safety, flood fill, flags, wins/losses, every cell's button reachability, gesture
arbitration, horizon clipping and damaged saves.

`test/emulator.py` installs the actual PBW into a fresh isolated emulator profile,
injects physical-button events and QMP absolute-pointer touch events, plays a
button-only win, checks a loss, verifies persistence and both touch styles, and
captures native screenshots and logs. It only stops the processes it owns.
It uses SDK emulator internals and is pinned to the toolchain above.

`tools/package.py` requires passing reports for both targets that match the exact
PBW hash. It stages the PBW, portable source ZIP, screenshots, reports and checksums
under `release/<version>/`. The ZIP retains the repository-relative geometry input;
unzip it and enter `hexsweeper-source/appstores/pebble/hexsweeper` to build.

## Implementation

- `game.c`: bounded rules over the generated adjacency graph.
- `sphere.c`: normalized quaternion rotation, orthographic projection, front
  hemisphere clipping and polygon hit testing.
- `controller.c`: shared button/touch state machine and versioned checksummed save.
- `main.c`: native GPath rendering, fonts, touch subscription, physical clicks,
  focus cancellation and local persistence.

Rendering is event driven with drag redraw requests capped at 20 Hz. There is
no idle animation. Cell labels remain upright and disappear on the narrow rim.
The frontmost cell becomes the selection after a drag. The app retains the
firmware's default opt-out from system touch navigation, so the firmware does
not also turn a drag into a button action. It does not call the new bridge API
to retain that default.

## Validation boundary

Version 0.4.0 retains the 0.2 rules and saved-board format. Its graphics changes are checked with the same native interaction suite and captures on both display shapes. Physical discrimination of tile states remains a watch test.

Version 0.2.0 passed the sanitizer suite and all 16 native interaction checks on both targets using firmware 4.33.2. These checks install and launch the exact packaged PBW in isolated emulators; they do not establish why installation failed through the physical phone/watch.

Version 0.1.1 passed the sanitizer suite, firmware-boundary checks and all 16
native interaction checks on both SDK 4.33.1 emulators (firmware 4.33.2) on
September 9, 2026. RAM footprint is 19,516 bytes on Time 2 and 19,520 on Round 2.
The older-firmware guard is unit tested; launch on an older physical firmware
has not been verified. The older SDK emulator probes did not finish booting,
so they provide no app compatibility evidence.

Version 0.1.0 passed the sanitizer suite and all 16 native interaction checks on
both emulators on September 9, 2026. The final RAM footprint is 19,048 bytes on
Time 2 and 19,052 bytes on Round 2, leaving more than 112 KB free on each target.
The SDK linker emits its RWX load-segment warning; application C compilation
has no warnings. Exact PBW hashes and native captures accompany the staged bundle.

The native emulator proves software behavior and screenshots at both display
sizes. It does **not** prove physical touch accuracy, finger occlusion, outdoor
readability, tactile controls, battery life or the target of 15 fps while dragging.
Those checks remain open on a physical Time 2 and a physical Round 2. This is a
locally staged testing build, not a published Pebble store release.
