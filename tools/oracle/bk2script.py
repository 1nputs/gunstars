#!/usr/bin/env python3
"""Turn a BizHawk movie (.bk2, Genesis Plus GX core, two pads) into an input script
(tools/oracle/script.py): line n of the input log is frame n from power on.

usage: bk2script.py MOVIE.bk2 OUT.txt
"""
import sys
import zipfile

sys.path.insert(0, __import__('os').path.dirname(__file__))
from script import write  # noqa: E402


def main():
    movie, out = sys.argv[1], sys.argv[2]
    with zipfile.ZipFile(movie) as z:
        header = z.read('Header.txt').decode()
        log = z.read('Input Log.txt').decode().splitlines()
    key = next(l for l in log if l.startswith('LogKey:'))
    groups = [g.split('|') for g in key[len('LogKey:'):].split('#') if g]
    groups = [[b for b in g if b] for g in groups]
    names = []
    for g in groups[1:]:                                   # the first group is Power / Reset
        names.append([b.split(' ', 1)[1].upper() for b in g])
    entries = []
    frames = [l for l in log if l.startswith('|')]
    for n, line in enumerate(frames):
        cols = line.split('|')[1:-1]
        if cols[0].strip('.'):
            sys.exit(f'frame {n}: a power or reset press ({cols[0]}), which scripts cannot say')
        pads = []
        for g, c in zip(names, cols[1:]):
            pads.append(tuple(nm for nm, ch in zip(g, c) if ch != '.'))
        while len(pads) < 2:
            pads.append(())
        entries.append((n, pads[0], pads[1]))
    write(out, entries)
    core = next((l.split(' ', 1)[1] for l in header.splitlines() if l.startswith('Core ')), '?')
    print(f'{out}: {len(frames)} frames ({core})')


if __name__ == '__main__':
    main()
