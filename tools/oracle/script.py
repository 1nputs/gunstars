"""Input scripts for the oracle: both pads, frame by frame.

A script is a list of entries `FRAME:P1` or `FRAME:P1|P2`, separated by commas or new lines; P1
and P2 are button names joined by '+' (UP DOWN LEFT RIGHT A B C START X Y Z MODE), empty for none.
The buttons of an entry are held from that frame until the next entry; an entry without '|'
releases pad 2. '#' starts a comment to the end of the line.

Wherever a tool takes `--script S`, S is the script itself or `@FILE` (a whole game's input is
too long for a command line).
"""
import os

BUTTONS = ('UP', 'DOWN', 'LEFT', 'RIGHT', 'A', 'B', 'C', 'START', 'X', 'Y', 'Z', 'MODE')
# the pad words of pulseman's traces (mdrt/pad.h MDRT_PAD_*)
PAD_BITS = dict(UP=1, DOWN=2, LEFT=4, RIGHT=8, B=16, C=32, A=64, START=128)


def _pad(s):
    names = [b.upper() for b in s.split('+') if b]
    for n in names:
        if n not in BUTTONS:
            raise ValueError(f'unknown button {n!r}')
    return tuple(names)


def parse(text):
    """-> sorted [(frame, p1 names, p2 names)]"""
    if text.startswith('@'):
        text = open(text[1:]).read()
    out = []
    for line in text.splitlines():
        line = line.split('#', 1)[0]
        for item in filter(None, (x.strip() for x in line.split(','))):
            f, _, pads = item.partition(':')
            p1, _, p2 = pads.partition('|')
            out.append((int(f), _pad(p1), _pad(p2)))
    out.sort(key=lambda e: e[0])
    return out


def write(path, entries):
    """entries: [(frame, p1, p2)]; writes only the frames where a pad changes"""
    last = None
    with open(path, 'w') as f:
        for fr, p1, p2 in entries:
            if (p1, p2) == last:
                continue
            last = (p1, p2)
            f.write(f'{fr}:{"+".join(p1)}' + (f'|{"+".join(p2)}' if p2 else '') + '\n')


class Player:
    """Feeds a parsed script to a core frame by frame: call at(core, frame) before each frame."""

    def __init__(self, text):
        self.entries = parse(text) if isinstance(text, str) else list(text)
        self.i = 0
        self.held = ((), ())

    def at(self, core, frame):
        changed = False
        while self.i < len(self.entries) and self.entries[self.i][0] <= frame:
            _, p1, p2 = self.entries[self.i]
            self.held = (p1, p2)
            self.i += 1
            changed = True
        if changed:
            core.set_buttons(0, self.held[0])
            core.set_buttons(1, self.held[1])
        return changed

    def words(self):
        """the held buttons as pulseman pad words (port 1, port 2)"""
        return tuple(sum(PAD_BITS.get(b, 0) for b in p) for p in self.held)


def exists(text):
    return bool(text) and (not text.startswith('@') or os.path.exists(text[1:]))
