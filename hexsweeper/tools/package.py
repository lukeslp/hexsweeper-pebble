"""Luke Steuber. Freeze a PBW only with matching native emulator evidence."""
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

ROOT = Path(__file__).resolve().parent.parent
REPO = ROOT.parents[2]
metadata = json.loads((ROOT / 'package.json').read_text())
version = metadata['version']
pbw = ROOT / 'build' / (ROOT.name + '.pbw')
sha = hashlib.sha256(pbw.read_bytes()).hexdigest()
TARGETS = ('basalt', 'chalk', 'diorite', 'emery', 'flint', 'gabbro')
NAMES = {'basalt': 'Pebble Time · 144 × 168', 'chalk': 'Pebble Time Round · 180 × 180',
         'diorite': 'Pebble 2 · 144 × 168', 'emery': 'Pebble Time 2 · 200 × 228',
         'flint': 'Pebble 2 Duo · 144 × 168', 'gabbro': 'Pebble Round 2 · 260 × 260'}
ROUND = {'chalk', 'gabbro'}
evidence = ROOT / 'build/evidence'
reports = []
for suite in ('evidence', 'combined-evidence'):
    for target in TARGETS:
        report_path = ROOT / 'build' / suite / f'{target}-report.json'
        report = json.loads(report_path.read_text())
        assert report['passed'] and report['platform'] == target and report['pbwSHA256'] == sha
        reports.append((suite, report_path, report))
with zipfile.ZipFile(pbw) as bundle:
    names = set(bundle.namelist())
    assert all(f'{t}/pebble-app.bin' in names for t in TARGETS)
    assert not any(name.endswith('.js') for name in names)
    info = json.loads(bundle.read('appinfo.json'))
    assert set(info['targetPlatforms']) == set(TARGETS)
    assert not info.get('capabilities')
    assert not info['watchapp']['watchface']

out = ROOT / 'release' / version
if out.exists():
    raise SystemExit('This release directory already exists; increment the version.')
out.mkdir(parents=True, exist_ok=True)
filename = f'hexsweeper-{version}.pbw'
existing = out / filename
if existing.exists() and hashlib.sha256(existing.read_bytes()).hexdigest() != sha:
    raise SystemExit('This version is already frozen with different bytes; increment the version.')
shutil.copy2(pbw, existing)
(out / 'evidence').mkdir(exist_ok=True)
for suite, report_path, report in reports:
    shutil.copy2(report_path, out / 'evidence' / f'{suite}-{report_path.name}')
    for frame in report['frames']:
        shutil.copy2(ROOT / frame, out / 'evidence')
shutil.copy2(ROOT / 'README.md', out / 'README.md')
source = out / f'hexsweeper-{version}-source.zip'
with zipfile.ZipFile(source, 'w', zipfile.ZIP_DEFLATED) as archive:
    for file in sorted(ROOT.rglob('*')):
        relative = file.relative_to(ROOT)
        if not file.is_file() or any(p in {'build', 'release', '__pycache__'} or p.startswith(('.lock-waf', '.waf')) for p in relative.parts):
            continue
        archive.write(file, Path('hexsweeper-source') / file.relative_to(REPO))
    for relative in ('sphere/hexasphere.js', 'LICENSE'):
        archive.write(REPO / relative, Path('hexsweeper-source') / relative)
(out / 'manifest.json').write_text(json.dumps({'name': 'Hexsweeper', 'version': version,
    'author': 'Luke Steuber', 'uuid': metadata['pebble']['uuid'], 'pbwSHA256': sha,
    'targets': list(TARGETS), 'evidence': 'Native emulator; physical-watch checks remain open.'}, indent=2) + '\n')
def figure(target):
    frame = 'touch-dig' if target in ('emery', 'gabbro') else 'dig'
    w, h = {'chalk': (270, 270), 'gabbro': (300, 300), 'emery': (250, 285)}.get(target, (216, 252))
    css = ' class="round"' if target in ROUND else ''
    return (f'<figure><img{css} width="{w}" height="{h}" src="evidence/{target}-{frame}.png" '
            f'alt="{NAMES[target].split(" ·")[0]} emulator showing the sphere after a safe dig">'
            f'<figcaption>{NAMES[target]}</figcaption></figure>')
figures = '\n'.join(figure(t) for t in TARGETS)
(out / 'preview.html').write_text('''<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Hexsweeper — Pebble preview</title>
<style>
body{margin:0;background:#151719;color:#f5f4ef;font:17px/1.5 system-ui,sans-serif}
main{max-width:880px;margin:auto;padding:48px 24px}h1{font-size:34px;line-height:1.15}
p{max-width:650px;color:#c5c9cc}a{color:#ffc12b}.watches{display:flex;gap:40px;align-items:center;flex-wrap:wrap}
figure{margin:20px 0}img{image-rendering:pixelated;max-width:100%;height:auto}figcaption{margin-top:12px}
.round{border-radius:50%}details{margin-top:30px}small{color:#c5c9cc}
</style><main><small>Luke Steuber · Native Pebble watchapp</small>
<h1>Hexsweeper</h1>
<p>Classic, Infinite, and 3D on every current Pebble. Each mode keeps its own
board and resumes when you return. Select digs and holding Select flags; Time 2
and Round 2 also aim by touch.</p>
<div class="watches">''' + figures + '''</div>
<p>These are native emulator captures. Physical readability, buttons, touch accuracy and performance remain untested.</p>
<details><summary>Controls and completed games</summary>
<div class="watches"><figure><img width="260" src="evidence/gabbro-pause-direct.png" alt="Pause menu with direct touch selected"><figcaption>Optional direct touch</figcaption></figure>
<figure><img width="200" src="evidence/emery-won.png" alt="Time 2 win screen showing all 35 safe cells cleared"><figcaption>Complete button-only win</figcaption></figure></div></details>
</main></html>''')
checksums = []
for file in sorted(out.rglob('*')):
    if file.is_file() and file.name != 'SHA256SUMS.txt':
        checksums.append(f'{hashlib.sha256(file.read_bytes()).hexdigest()}  {file.relative_to(out)}')
(out / 'SHA256SUMS.txt').write_text('\n'.join(checksums) + '\n')
print(out)
