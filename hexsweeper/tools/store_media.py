"""Luke Steuber. Stage store screenshots from a frozen release's native evidence.

Run after tools/package.py. Copies five unmodified emulator captures per target
into store/<version>/ as <platform>_<shot>.png, then writes checksums and an
unpublished listing record. Nothing is uploaded.
"""
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parent.parent
version = json.loads((ROOT / 'package.json').read_text())['version']
release = ROOT / 'release' / version
evidence = release / 'evidence'
out = ROOT / 'store' / version
TARGETS = ('basalt', 'chalk', 'diorite', 'emery', 'flint', 'gabbro')
TOUCH = {'emery', 'gabbro'}
# Shot name -> evidence frame. Time 2 and Round 2 show their touch dig.
SHOTS = {'sphere': lambda t: 'touch-dig' if t in TOUCH else 'first-dig',
         'classic': lambda t: 'classic',
         'infinite': lambda t: 'infinite-distant-flag',
         'won': lambda t: 'won',
         'about': lambda t: 'sphere-about'}
NOTES = ('Minesweeper Hex now runs on Pebble Time, Time Steel, Time Round, Pebble 2 '
         'and Pebble 2 Duo, with boards and menus laid out for each screen and '
         'black-and-white tiles that never rely on color. Time 2 and Round 2 play '
         'exactly as before.')

manifest = json.loads((release / 'manifest.json').read_text())
if out.exists():
    raise SystemExit('Store media for this version already exist.')
out.mkdir(parents=True)
sources = {}
for target in TARGETS:
    for shot, frame in SHOTS.items():
        source = evidence / f'{target}-{frame(target)}.png'
        dest = out / f'{target}_{shot}.png'
        shutil.copy2(source, dest)
        sources[dest.name] = {'from': str(source.relative_to(ROOT)),
                              'sha256': hashlib.sha256(source.read_bytes()).hexdigest()}
(out / 'listing.json').write_text(json.dumps({
    'title': 'Minesweeper Hex', 'version': version, 'author': 'Luke Steuber',
    'uuid': manifest['uuid'], 'pbwSHA256': manifest['pbwSHA256'],
    'targets': list(TARGETS), 'releaseNotes': NOTES,
    'releaseState': 'Staged; not published. Awaiting owner approval.',
    'screenshots': sources,
    'physicalWatchValidated': False}, indent=2) + '\n')
lines = [f'{hashlib.sha256(f.read_bytes()).hexdigest()}  {f.name}'
         for f in sorted(out.iterdir()) if f.name != 'SHA256SUMS.txt']
(out / 'SHA256SUMS.txt').write_text('\n'.join(lines) + '\n')
print(out)
