#!/usr/bin/env python3
"""Boot the ROM in the oracle, drive it with a scripted input stream, dump RAM and screenshots.

usage: run.py ROM [--frames N] [--script "f:BTN+BTN,f:BTN,..."] [--shots DIR --every N]
               [--save STATE] [--load STATE] [--ramdump FILE] [--watch name=addr,...]

The script (tools/oracle/script.py) gives both pads' buttons from a frame on, or @FILE.
Example: "0:,120:START,130:,200:START|START".
"""
import argparse, os, sys, pathlib
sys.path.insert(0, os.path.dirname(__file__))
from retro import Core
from script import Player

HERE = pathlib.Path(__file__).resolve().parents[2]
CORE = HERE / 'ref/gpgx/genesis_plus_gx_libretro.so'


def parse_pokes(spec):
    """'FRAME:ADDR=VAL,ADDR=VAL;FRAME:...' -> {frame: [(addr, val), ...]}"""
    out = {}
    for part in filter(None, spec.split(';')):
        fr, _, kvs = part.partition(':')
        out.setdefault(int(fr), []).extend((int(k, 16), int(v, 16)) for k, v in (kv.split('=') for kv in kvs.split(',') if kv))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('rom')
    ap.add_argument('--core', default=str(CORE))
    ap.add_argument('--frames', type=int, default=600)
    ap.add_argument('--script', default='')
    ap.add_argument('--shots', default='')
    ap.add_argument('--every', type=int, default=60)
    ap.add_argument('--save', default='')
    ap.add_argument('--load', default='')
    ap.add_argument('--ramdump', default='')
    ap.add_argument('--watch', default='')
    ap.add_argument('--poke', default='', help='RAM words to write: "FRAME:ADDR=VAL,ADDR=VAL;FRAME:..." (hex)')
    a = ap.parse_args()

    core = Core(a.core)
    av = core.load(a.rom)
    print(f'loaded: {av.geometry.base_width}x{av.geometry.base_height} @ {av.timing.fps:.3f} fps, pixfmt={core.pixel_format}')
    if a.load:
        core.unserialize(open(a.load, 'rb').read())
    script = Player(a.script)
    pokes = parse_pokes(a.poke)
    watch = [(kv.split('=')[0], int(kv.split('=')[1], 16)) for kv in filter(None, a.watch.split(','))]
    if a.shots:
        os.makedirs(a.shots, exist_ok=True)
    for f in range(a.frames):
        script.at(core, f)
        for ad, v in pokes.get(f, ()):
            core.poke_w(ad, v)
        core.run()
        if a.shots and (f % a.every == 0 or f == a.frames - 1):
            core.screenshot(os.path.join(a.shots, f'f{f:05d}.png'))
        if f % a.every == 0 or f == a.frames - 1:
            vals = ' '.join(f'{n}={core.ram_w(ad):04X}' for n, ad in watch)
            print(f'frame {f:5d}: {vals}')
    if a.save:
        open(a.save, 'wb').write(core.serialize())
    if a.ramdump:
        open(a.ramdump, 'wb').write(core.ram())
    print(f'done. video frames={core.frame_count} audio samples={core.audio_samples}')


if __name__ == '__main__':
    main()
