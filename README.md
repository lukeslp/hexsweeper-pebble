# Hexsweeper for Pebble

Native Pebble watchapps of Hexsweeper, the hex-grid Minesweeper. Both are
written in C against the Pebble SDK and run offline, with no phone JavaScript,
network access or account.

The web and phone game lives at
[lukeslp/hexsweeper](https://github.com/lukeslp/hexsweeper).

## Apps

| Folder | What it is | Watches | Status |
| --- | --- | --- | --- |
| [`hexsweeper/`](hexsweeper/) | Classic, Infinite and 3D in one app, each with its own saved board | Time 2, Round 2, Time and Time Steel, Time Round, Pebble 2, Pebble 2 Duo | 0.8.0 on the Rebble store |
| [`hexsweeper-flat/`](hexsweeper-flat/) | A single flat 37-cell board | Time 2, Round 2 | Testing builds only |

The combined app is on the Rebble store as
[Minesweeper Hex](https://apps.repebble.com/94e4d9abfb8e44488f419862).

Each app folder keeps its released builds under `release/<version>/` (the
`.pbw`, a source ZIP, emulator screenshots, test reports and checksums) and its
store media under `store/<version>/`. Older listing records (in earlier
commits) and the release source ZIPs contain absolute paths from the original
checkout; the current listing records use paths relative to this repository.

## Building

You need Pebble SDK 4.33.1 and pebble-tool 5.0.39. Run `pebble build` inside
an app folder. Each app's README lists its tests and packaging steps.

The 3D sphere's cell layout comes from `sphere/hexasphere.js` in the main
Hexsweeper repository. The generated `hexsweeper/src/c/geometry.h` is checked
in, so `pebble build` needs nothing else. The geometry check in
`test/core.sh`, the emulator suites and `tools/package.py` look for that file
at `../../sphere/hexasphere.js` from this repository's root, which is where it
sits when this repository is checked out as `appstores/pebble/` inside a
Hexsweeper checkout. Each release's source ZIP already has that layout.

## License

MIT; see [LICENSE](LICENSE).
