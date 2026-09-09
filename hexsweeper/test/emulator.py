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
PBW = ROOT / 'build' / (ROOT.name + '.pbw')
APP = UUID(json.loads((ROOT / 'package.json').read_text())['pebble']['uuid'])


def run(platform):
    installed_sha = hashlib.sha256(PBW.read_bytes()).hexdigest()
    out = ROOT / 'build/evidence'
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
    logs, frames = [], []
    def log(packet):
        line = str(packet.message)
        logs.append(line)
        if 'HS ' in line:
            print(line, flush=True)
    handle = watch.register_endpoint(AppLogMessage, log)
    watch.send_packet(AppLogShippingControl(enable=True))
    def current():
        for line in reversed(logs):
            if 'HS mode=' in line:
                state = {k: int(v) for k, v in re.findall(r'(\w+)=(\d+)', line)}
                metadata = next(l for l in reversed(logs) if 'HS seed=' in l)
                state.update({k: int(v) for k, v in re.findall(r'(\w+)=(\d+)', metadata)})
                return state
        raise AssertionError('No native state received')
    def count():
        return sum('HS mode=' in line for line in logs)
    def response(previous):
        deadline = time.monotonic() + 4
        while count() == previous and time.monotonic() < deadline:
            time.sleep(.02)
        assert count() > previous, 'Native input did not respond'
        time.sleep(.1)
    def button(name, hold=.07):
        previous = count()
        send_data_to_qemu(watch.transport, QemuButton(state=getattr(QemuButton.Button, name)))
        time.sleep(hold)
        send_data_to_qemu(watch.transport, QemuButton(state=0))
        if previous:
            response(previous)
        else:
            time.sleep(.2)
    def expect(**values):
        actual = current()
        assert all(actual[k] == v for k, v in values.items()), (values, actual)
    def grab(name):
        time.sleep(.15)
        pixels = cmd._grab_processed_image(argparse.Namespace(no_correction=True, scale=1, no_open=True, v=0), show_progress=False)
        file = out / f'{platform}-{name}.png'
        png.from_array(pixels, mode='RGBA;8').save(str(file))
        frames.append(str(file.relative_to(ROOT)))
        return pixels
    width, height = (200, 228) if platform == 'emery' else (260, 260)
    x, y = width // 2, 105 if platform == 'emery' else 126
    def pointer(px, py, down):
        events = [{'type': 'abs', 'data': {'axis': axis, 'value': round(value * 32767 / (size - 1))}}
                  for axis, value, size in [('x', px, width), ('y', py, height)]]
        events.append({'type': 'btn', 'data': {'button': 'left', 'down': down}})
        execute('input-send-event', {'events': events})
    def touch(points, interval=.05):
        previous = count()
        for px, py in points:
            pointer(px, py, True)
            time.sleep(interval)
        pointer(*points[-1], False)
        response(previous)
    def new_game():
        button('Back')
        for _ in range(3):
            button('Down')
        button('Select')
        expect(mode=4, menu=0)
        button('Down')
        button('Select')
        expect(mode=0, status=0, opened=0, flags=0)
    def restart():
        previous = count()
        watch.send_packet(AppRunState(data=AppRunStateStop(uuid=APP)))
        time.sleep(.4)
        watch.send_packet(AppRunState(data=AppRunStateStart(uuid=APP)))
        response(previous)
        button('Select')  # Launch picker keeps 3D highlighted; resume its board.
    mesh = json.loads(subprocess.check_output(['node', '-e',
        "console.log(JSON.stringify(require('./sphere/hexasphere.js').generateHexasphere(1,2).tiles.map(t=>t.neighborIndices)))"], cwd=ROOT.parents[2]))
    def mines_for(seed, first):
        available = [i for i in range(42) if i != first and i not in mesh[first]]
        for i in range(len(available) - 1, 0, -1):
            seed ^= (seed << 13) & 0xffffffff
            seed ^= seed >> 17
            seed ^= (seed << 5) & 0xffffffff
            j = seed % (i + 1)
            available[i], available[j] = available[j], available[i]
        return set(available[:7])
    def move_to(target):
        origin = current()['selected']
        paths = [[origin]]
        seen = {origin}
        while paths:
            path = paths.pop(0)
            if path[-1] == target:
                break
            for n in mesh[path[-1]]:
                if n not in seen:
                    seen.add(n)
                    paths.append(path + [n])
        button('Up')
        for n in path[1:]:
            for _ in range(6):
                if current()['neighbor'] == n:
                    break
                button('Down')
            expect(neighbor=n)
            button('Select')
        button('Back')
        expect(selected=target, mode=0)
    try:
        time.sleep(4)
        button('Back')
        AppInstaller(watch, str(PBW)).install()
        watch.send_packet(AppRunState(data=AppRunStateStart(uuid=APP)))
        time.sleep(1)
        button('Select')  # Fresh mode picker defaults to 3D.
        expect(mode=3, help=0)
        for page in range(4):
            expect(help=page)
            grab(f'help-{page + 1}')
            if page < 3:
                button('Down')
        button('Select')
        expect(mode=0, status=0)
        grab('ready')
        touch([(x, y)])
        expect(status=0, opened=0)
        button('Select', .65)
        expect(flags=1, opened=0, status=0)  # Long click must suppress short dig.
        grab('flag')
        button('Up')
        expect(mode=1, flags=1, opened=0)
        grab('flag-unfocused')
        button('Back')
        restart()
        expect(flags=1, opened=0, status=0)
        button('Select', .65)
        expect(flags=0, opened=0)
        mines = mines_for(current()['seed'], 0)
        button('Select')
        assert current()['opened'] > 0
        grab('first-dig')
        # Every cell must be reachable in the actual native UI with buttons only.
        button('Up')
        visited = {current()['selected']}
        def walk():
            origin = current()['selected']
            if origin not in mines:
                button('Back')
                button('Select')
                button('Up')
            neighbors = []
            initial = current()['neighbor']
            while True:
                neighbors.append(current()['neighbor'])
                button('Down')
                if current()['neighbor'] == initial:
                    break
                assert len(neighbors) <= 6
            for target in neighbors:
                if target in visited:
                    continue
                while current()['neighbor'] != target:
                    button('Down')
                button('Select')
                expect(selected=target, mode=1)
                visited.add(target)
                walk()
                while current()['neighbor'] != origin:
                    button('Down')
                button('Select')
                expect(selected=origin, mode=1)
        walk()
        assert len(visited) == 42
        expect(status=2, opened=35)
        grab('explore')
        button('Back')
        grab('won')
        before = current()
        restart()
        expect(mode=0, selected=before['selected'], opened=before['opened'], seed=before['seed'])
        new_game()
        mines = mines_for(current()['seed'], 0)
        button('Select')
        if current()['status'] == 2:
            new_game()
            mines = mines_for(current()['seed'], 0)
            button('Select')
        assert current()['status'] == 1
        move_to(min(mines))
        button('Select')
        expect(status=3)
        grab('lost')
        new_game()
        button('Back')
        button('Down')
        button('Select')
        expect(direct=1, mode=2)
        grab('pause-direct')
        button('Back')
        touch([(x, y)], interval=.45)
        expect(status=0, opened=0)
        touch([(x, y - (77 if platform == 'emery' else 89))])
        expect(selected=11, status=0, opened=0)
        touch([(x, y), (x + 35, y), (x, y)])
        expect(status=0, opened=0)  # Maximum excursion latches even on return.
        touch([(x, y), (x + 15, y), (x + 30, y), (x + 45, y + 15)])
        expect(status=0, opened=0)
        grab('rotated')
        restart()
        expect(direct=1, opened=0, status=0)
        # Center tap acts in direct mode after a drag.
        touch([(x, y)])
        assert current()['opened'] > 0
        grab('touch-dig')
        before = current()
        button('Back')
        for _ in range(3):
            button('Down')
        button('Select')
        grab('confirm')
        button('Select')
        expect(opened=before['opened'], seed=before['seed'], mode=0)
        hero = grab('resumed')
        time.sleep(.7)
        assert grab('idle') == hero
        assert not any('fault' in line.lower() or 'crash' in line.lower() for line in logs)
        assert hashlib.sha256(PBW.read_bytes()).hexdigest() == installed_sha, 'PBW changed during this test'
        report = {'platform': platform, 'sdk': SDK_VERSION, 'pbwSHA256': installed_sha,
                  'capturedAt': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'passed': True,
                  'checks': ['all help pages', 'aim-first touch', 'long-click suppression', 'first dig', '42 cells reached with buttons', 'button-only win', 'loss',
                             'save/relaunch', 'direct touch preference', 'long touch ignored', 'rim tap centers without digging', 'drag latch', 'drag rotation', 'direct touch dig', 'new-game cancellation', 'static idle'],
                  'frames': frames, 'logs': logs, 'limits': 'Native emulator only; physical touch accuracy, readability and frame rate are untested.'}
        (out / f'{platform}-report.json').write_text(json.dumps(report, indent=2) + '\n')
        print('PASS', platform, flush=True)
    finally:
        (out / f'{platform}-app.log').write_text('\n'.join(logs) + '\n')
        watch.unregister_endpoint(handle)
        cmd._close_pebble_connection(watch)
        cmd.pebble = None
        qmp.close()
        qmp_socket.close()
        shutdown()
        bridge.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('platform', choices=['emery', 'gabbro'])
    run(parser.parse_args().platform)
