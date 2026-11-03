"""Luke Steuber. Drive the installed PBW through native buttons and QMP touch.

Run with pebble-tool's Python after `pebble build`. Each run owns a fresh profile.
No AppMessage test backdoor is compiled into the game.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import tempfile
import time
from types import SimpleNamespace
from uuid import UUID

SDK_VERSION = '4.33.1'
SDK = Path.home() / 'Library/Application Support/Pebble SDK/SDKs' / SDK_VERSION
os.environ['PEBBLE_QEMU_PATH'] = str(SDK / 'toolchain/bin/qemu-pebble')
os.environ['PATH'] = str(SDK / 'toolchain/bin') + os.pathsep + os.environ['PATH']
import png
from pebble_tool.commands.screenshot import ScreenshotCommand
from pebble_tool.commands.emucontrol import send_data_to_qemu
import pebble_tool.sdk.emulator as emulator
from libpebble2.services.install import AppInstaller
from libpebble2.protocol.apps import AppRunState, AppRunStateStart, AppRunStateStop
from libpebble2.protocol.logs import AppLogMessage, AppLogShippingControl
from libpebble2.communication.transports.qemu.protocol import QemuButton

ROOT = Path(__file__).resolve().parent.parent
SIZES = {'basalt': (144, 168), 'chalk': (180, 180), 'diorite': (144, 168),
         'emery': (200, 228), 'flint': (144, 168), 'gabbro': (260, 260)}
TOUCH = {'emery', 'gabbro'}
BW = {'diorite', 'flint'}
# About portrait: (top, size) on each display, matching src/c/layout.c.
PORTRAIT = {'emery': (47, 96), 'gabbro': (47, 96), 'chalk': (34, 72)}
PBW = ROOT / 'build' / (ROOT.name + '.pbw')
APP = UUID(json.loads((ROOT / 'package.json').read_text())['pebble']['uuid'])


def run(platform):
    installed_sha = hashlib.sha256(PBW.read_bytes()).hexdigest()
    out = ROOT / 'build/combined-evidence'
    out.mkdir(exist_ok=True)
    state = Path(tempfile.mkdtemp(prefix='emu-', dir=ROOT / 'build'))
    # macOS Unix sockets have short paths; a loopback QMP port avoids that limit.
    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1', 0))
        qmp_port = reservation.getsockname()[1]
    def persist(target, version=None):
        p = state / target
        p.mkdir(exist_ok=True)
        return str(p)
    emulator.get_sdk_persist_dir = persist
    emulator.get_emulator_info_path = lambda: str(state / 'emulators.json')
    emulator.get_default_account = lambda: SimpleNamespace(is_logged_in=False)
    bridge = (out / f'{platform}-bridge.log').open('w')
    emulator.ManagedEmulatorTransport._get_output = lambda self: bridge
    cmd = ScreenshotCommand()
    cmd._set_debugging(0)
    def shutdown():
        info = emulator.get_emulator_info(platform, SDK_VERSION)
        if info:
            for key in ('qemu', 'pypkjs', 'websockify'):
                pid = info.get(key, {}).get('pid')
                if pid:
                    ps = subprocess.run(['ps', '-p', str(pid), '-o', 'command='], capture_output=True, text=True)
                    if ps.returncode == 0 and str(state / platform) not in ps.stdout:
                        raise RuntimeError(f'PID {pid} belongs to a different profile')
        cmd._shutdown_platform_emulator(platform, SDK_VERSION)
    original_popen = subprocess.Popen
    def popen(argv, *args, **kwargs):
        if isinstance(argv, list) and Path(argv[0]).name == 'qemu-pebble':
            argv = argv + ['-qmp', f'tcp:127.0.0.1:{qmp_port},server=on,wait=off']
        return original_popen(argv, *args, **kwargs)
    try:
        subprocess.Popen = popen
        watch = cmd._connect_emulator(platform, SDK_VERSION)
        cmd.pebble = watch
    except BaseException:
        shutdown()
        bridge.close()
        raise
    finally:
        subprocess.Popen = original_popen
    qmp_socket = socket.create_connection(('127.0.0.1', qmp_port), timeout=5)
    qmp = qmp_socket.makefile('rwb')
    assert 'QMP' in json.loads(qmp.readline())
    def execute(command, arguments=None):
        qmp.write((json.dumps({'execute': command, 'arguments': arguments or {}}) + '\n').encode())
        qmp.flush()
        while True:
            response = json.loads(qmp.readline())
            if 'error' in response:
                raise RuntimeError(response)
            if 'return' in response:
                return response['return']
    execute('qmp_capabilities')
    logs, frames, retries = [], [], []
    def log(packet):
        line = str(packet.message)
        logs.append(line)
        if any(prefix in line for prefix in ['HS ', 'HF ', 'HX ']):
            print(line, flush=True)
    handle = watch.register_endpoint(AppLogMessage, log)
    watch.send_packet(AppLogShippingControl(enable=True))
    def fields(line):
        return {k:int(v) for k,v in re.findall(r'(\w+)=(-?\d+)',line)}
    def current():
        latest=next(line for line in reversed(logs) if 'HX app=' in line)
        result=fields(latest)
        prefix='HS mode=' if result['app']==2 else 'HF kind='
        line=next((line for line in reversed(logs) if prefix in line), '')
        result.update(fields(line))
        extra='HS seed=' if result['app']==2 else 'HF storage '
        line=next((line for line in reversed(logs) if extra in line), '')
        result.update(fields(line))
        return result
    def count():
        return sum('HX app=' in line for line in logs)
    def responded(previous):
        # Generous under a loaded host; a hung or crashed app still fails.
        deadline = time.monotonic() + 10
        while count() == previous and time.monotonic() < deadline:
            time.sleep(.02)
        time.sleep(.1)
        return count() > previous
    def response(previous):
        assert responded(previous), 'Native input did not respond'
    def press(name, hold):
        send_data_to_qemu(watch.transport, QemuButton(state=getattr(QemuButton.Button, name)))
        time.sleep(hold)
        send_data_to_qemu(watch.transport, QemuButton(state=0))
    def button(name, hold=.07):
        previous = count()
        press(name, hold)
        if not previous:
            time.sleep(.2)
        elif not responded(previous):
            # A loaded emulator can drop an injected press. Record what the
            # watch showed, resend once, and fail if the app still is silent.
            retries.append(f'{name} after {previous} states')
            grab(f'no-response-{len(retries)}')
            press(name, hold)
            response(previous)
    def expect(**values):
        actual = current()
        assert all(actual[k] == v for k, v in values.items()), (values, actual)
    def grab(name):
        time.sleep(.15)
        pixels = cmd._grab_processed_image(argparse.Namespace(no_correction=True, scale=1, no_open=True, v=0), show_progress=False)
        if name.endswith('-about'):
            # Screenshot transport can sample during a native draw. Require a
            # naturally settled portrait in two consecutive complete captures.
            # Color portraits are counted by saturated pixels; the
            # black-and-white line art by white pixels.
            previous = None
            deadline = time.monotonic() + 3
            top, size = PORTRAIT.get(platform, (28, 72))
            while True:
                picture = tuple(tuple(row) for row in pixels)
                colored = sum((min(row[i:i+3]) > 200) if platform in BW else
                              (max(row[i:i+3])-min(row[i:i+3]) > 60)
                              for row in picture[top:top+size]
                              for i in range((width//2-size//2)*4,(width//2+size//2)*4,4))
                if colored > 300 and picture == previous:
                    break
                assert time.monotonic() < deadline, ('Portrait did not settle', name, colored)
                previous = picture
                time.sleep(.15)
                pixels = cmd._grab_processed_image(argparse.Namespace(no_correction=True, scale=1, no_open=True, v=0), show_progress=False)
        file = out / f'{platform}-{name}.png'
        png.from_array(pixels, mode='RGBA;8').save(str(file))
        frames.append(str(file.relative_to(ROOT)))
        return pixels
    width, height = SIZES[platform]
    touchscreen = platform in TOUCH
    x, y = width // 2, 105 if platform == 'emery' else 126
    def pointer(px, py, down):
        events = [{'type': 'abs', 'data': {'axis': axis, 'value': round(value * 32767 / (size - 1))}}
                  for axis, value, size in [('x', px, width), ('y', py, height)]]
        events.append({'type': 'btn', 'data': {'button': 'left', 'down': down}})
        execute('input-send-event', {'events': events})
    def touch(points, interval=.05):
        assert touchscreen, 'no touchscreen on ' + platform
        previous = count()
        for px, py in points:
            pointer(px, py, True)
            time.sleep(interval)
        pointer(*points[-1], False)
        response(previous)
    def modes():
        button('Back')
        # The 3D pause menu's touch row exists only on touch watches.
        for _ in range(4 if current()['app']==2 and touchscreen else 3): button('Down')
        button('Select');expect(picker=1)
    def choose(mode):
        while current()['choice']!=mode:button('Down')
        button('Select');expect(app=mode,picker=0)
    def settle():
        # Infinite flood fill pumps on a timer; read totals only once it stops.
        deadline=time.monotonic()+90
        while current().get('busy') and time.monotonic()<deadline:
            time.sleep(.1)
        assert not current().get('busy'), 'flood fill did not settle'
    def check_about(label):
        settle()
        before=current()
        app=before['app']
        index=5 if app==2 else 4
        button('Back')
        for _ in range(index-(app==2 and not touchscreen)):button('Down')
        expect(menu=index,about=0)
        grab(label+'-about-menu')
        button('Select');expect(about=1,menu=index)
        grab(label+'-about')
        button('Down');expect(about=1,menu=index)
        button('Select');expect(about=0,menu=index)
        button('Select');expect(about=1)
        grab(label+'-reopened-about')
        button('Back');expect(about=0,menu=index)
        if touchscreen:
            # Open the visible fourth row, then return using a separate tap.
            touch([(width//2,48+3*38+19)]);expect(about=1)
            touch([(width//2,100)]);expect(about=0,menu=index)
            time.sleep(.3);expect(about=0,menu=index)
            # A held contact must not pass through a button dismissal.
            button('Select');expect(about=1)
            pointer(width//2,100,True);time.sleep(.05)
            button('Back');expect(about=0,menu=index)
            pointer(width//2,100,False);time.sleep(.3)
            expect(about=0,menu=index)
        button('Back')
        keys=('selected','flags','status','opened') if app==2 else ('selected','flags','status','open') if app==0 else ('q','r','flags','status','open')
        expect(**{k:before[k] for k in keys})
        expect(**({'mode':0} if app==2 else {'ui':0}))
    try:
        time.sleep(4);button('Back')
        AppInstaller(watch,str(PBW)).install()
        watch.send_packet(AppRunState(data=AppRunStateStart(uuid=APP)))
        time.sleep(1);expect(picker=1);grab('modes')
        choose(0);expect(ui=3);grab('classic-help')
        button('Select');expect(ui=0,open=0)
        button('Select',hold=.65);expect(flags=1)
        grab('classic-flag')
        button('Select',hold=.65);expect(flags=0)
        button('Select');assert current()['open']>=7
        classic=current();grab('classic')
        check_about('classic')
        if touchscreen:
            pitch=26 if platform=='emery' else 32
            rise=22 if platform=='emery' else 27
            coords=[(q,r) for r in range(-3,4) for q in range(-3,4) if abs(q+r)<=3]
            for i,(q,r) in enumerate(coords):
                touch([(width//2+pitch*q+pitch//2*r,height//2-2+rise*r)])
                expect(selected=i,open=classic['open'],flags=classic['flags'])
            touch([(width//2,height//2)])
            expect(selected=18)
        else:
            # Every one of the 37 cells takes focus with Down alone.
            start=current()['selected']
            for i in range(1,38):
                button('Down')
                expect(selected=(start+i)%37,open=classic['open'],flags=classic['flags'])
        button('Back')
        for _ in range(5):button('Down')
        grab('classic-menu-last')
        if touchscreen:
            # The last selected row scrolls into view; Game modes is now row two.
            touch([(width//2,48+1*38+19)])
        else:
            button('Up');button('Up');expect(menu=3);button('Select')
        expect(picker=1)
        choose(0);expect(ui=0)
        if touchscreen:
            button('Back');touch([(width//2,48+19)]);expect(ui=0)
        else:
            button('Back');button('Select');expect(ui=0)
        modes();choose(1);expect(ui=3);grab('infinite-help')
        button('Select');button('Select')
        time.sleep(2)
        button('Up');expect(ui=1)
        assert current()['open']>=7
        for _ in range(12):button('Select')
        button('Back');expect(q=12,r=0,ui=0)
        while current().get('cell')==1:
            button('Up');button('Select');button('Back')
        button('Select',hold=.65);expect(flags=1)
        settle();before=current();grab('infinite-distant-flag')
        check_about('infinite')
        if touchscreen:
            cy=height//2
            pitch=26 if platform=='emery' else 32
            touch([(width//2+2*pitch,cy),(width//2-2*pitch,cy)])
            expect(q=before['q']+4,r=before['r'],flags=1)
            touch([(width//2-2*pitch,cy),(width//2+2*pitch,cy)])
            expect(q=before['q'],r=before['r'],cell=2)
        modes();choose(2);expect(mode=3)
        button('Select');button('Select',hold=.65);expect(flags=1)
        grab('sphere-flag')
        check_about('sphere')
        modes();choose(1)
        expect(q=before['q'],r=before['r'],flags=1,open=before['open'],ui=0)
        watch.send_packet(AppRunState(data=AppRunStateStop(uuid=APP)))
        time.sleep(.5)
        watch.send_packet(AppRunState(data=AppRunStateStart(uuid=APP)))
        time.sleep(.8)
        expect(app=1,picker=1,choice=1)
        grab('launch-mode-choice')
        choose(1)
        expect(app=1,picker=0,q=before['q'],r=before['r'],flags=1,open=before['open'])
        grab('infinite-resumed')
        modes();choose(0);expect(open=classic['open'],flags=classic['flags'],ui=0)
        grab('classic-resumed')
        modes();choose(2);expect(flags=1)
        button('Back')
        for _ in range(4 if touchscreen else 3):button('Down')
        expect(menu=4)
        grab('sphere-menu-last')
        if touchscreen:
            touch([(width//2,48+3*38+19)])
        else:
            button('Select')
        expect(picker=1)
        choose(2);expect(flags=1)
        assert hashlib.sha256(PBW.read_bytes()).hexdigest()==installed_sha
        assert not any('fault' in x.lower() or 'crash' in x.lower() for x in logs)
        if touchscreen:
            checks=['three-mode picker','classic first dig and flag','all 37 enlarged Classic tap targets','enlarged scrolling menu taps','infinite safe opening',
                    'button exploration beyond viewport','distant flag','drag pan and return',
                    'About in all modes: two consecutive settled portrait captures per opening/reopening, Select/Back/tap return, held-contact isolation, unchanged game state','independent sphere flag','per-mode saves','launch picker highlights last mode and preserves exact save']
        else:
            checks=['three-mode picker','classic first dig and flag','all 37 Classic cells focused with Down','scrolling menus with buttons','infinite safe opening',
                    'button exploration beyond viewport','distant flag',
                    'About in all modes: two consecutive settled portrait captures per opening/reopening, Select/Back return, unchanged game state','independent sphere flag','per-mode saves','launch picker highlights last mode and preserves exact save']
        report={'platform':platform,'sdk':SDK_VERSION,'pbwSHA256':installed_sha,'passed':True,
                'touchscreen':touchscreen,'checks':checks,
                'frames':frames,'logs':logs,'inputRetries':retries}
        (out/f'{platform}-report.json').write_text(json.dumps(report,indent=2))
        print('PASS combined',platform)
    finally:
        (out/f'{platform}-app.log').write_text('\n'.join(logs))
        watch.unregister_endpoint(handle)
        cmd._close_pebble_connection(watch);cmd.pebble=None
        qmp.close();qmp_socket.close();shutdown();bridge.close()
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('platform',choices=sorted(SIZES))
    run(parser.parse_args().platform)
