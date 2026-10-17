#!/usr/bin/env python3
"""The options screen's input script (mode $C), for capture_all.sh's game_options session.

usage: options_script.py OUT

The title (Start at 291 and 350 as the TAS), the menu at 530, Up to OPTIONS and Start. Then each
line of the menu: the LEVEL both ways round, the SOUND TEST and the S.E. TEST both ways round and
played and stopped, the cursor both ways round; the CONFIG by A, both pads through every line and
button (pressed and stepped), back by Start, the CONFIG again by Right; random presses on both pads
(no Start); EXIT by A to the title; the options again, left by Start.
"""
import random
import sys

AGAIN = 190   # from EXIT to the title's Start


class Script:
    def __init__(self):
        self.lines, self.t = [], 0

    def at(self, t, p1='', p2=None):
        self.t = t
        self.lines.append(f'{t}:{p1}' + (f'|{p2}' if p2 is not None else ''))

    def press(self, p1='', p2=None, gap=4, n=1):
        for _ in range(n):
            self.at(self.t, p1, p2)
            self.at(self.t + 1)
            self.t += gap - 1

    def wait(self, frames):
        self.t += frames


def enter(s, start):
    """from the title's start (Start at `start` shows the menu): Up to OPTIONS, Start"""
    s.at(start, 'START')
    s.at(start + 1)
    s.at(start + 10, 'UP')
    s.at(start + 11)
    s.at(start + 20, 'START')
    s.at(start + 21)
    s.wait(90)


def main():
    s = Script()
    s.at(0)
    s.at(291, 'START')
    s.at(292)
    s.at(350, 'START')
    s.at(351)
    enter(s, 530)
    # LEVEL
    s.press('RIGHT', n=5)
    s.press('LEFT', n=5)
    s.press('RIGHT', p2='LEFT')
    # SOUND TEST
    s.press('DOWN', n=2)
    s.press('RIGHT', n=3)
    s.press('A')
    s.wait(30)
    s.press('B')
    s.press('LEFT', n=30)
    s.press('C')
    s.wait(60)
    s.press('B')
    # S.E. TEST
    s.press('DOWN')
    s.press('RIGHT', n=5)
    s.press('A')
    s.wait(20)
    s.press('C')
    s.wait(20)
    s.press('LEFT', n=90)
    s.press('B')
    s.press('', p2='A')
    s.wait(10)
    # the cursor round both ways
    s.press('DOWN', n=2)
    s.press('UP', n=2)
    s.press('', p2='UP', n=3)
    # the CONFIG (line 4) by A
    s.press('A')
    s.wait(50)
    for pad in (0, 1):
        def p(b):
            s.press(*((b, None) if pad == 0 else ('', b)))
        for line in range(3):
            for b in ('A', 'B', 'C', 'RIGHT', 'RIGHT', 'RIGHT', 'LEFT', 'LEFT', 'LEFT', 'B', 'A'):
                p(b)
            p('DOWN')
        p('UP')
        p('UP')
        p('DOWN')
    s.press('A', p2='C')
    s.press('DOWN', p2='UP')
    s.press('START')
    s.wait(50)
    # the CONFIG again by Right, back by pad 2's Start
    s.press('RIGHT')
    s.wait(50)
    s.press('', p2='START')
    s.wait(50)
    # random presses, no Start
    rng = random.Random(400)
    buttons = ('UP', 'DOWN', 'LEFT', 'RIGHT', 'A', 'B', 'C', '')
    for _ in range(200):
        s.press(rng.choice(buttons), p2=rng.choice(buttons), gap=rng.randint(2, 6))
    # out of the CONFIG if there, the cursor to EXIT, A
    s.press('START')
    s.wait(50)
    s.press('UP', n=2)
    s.wait(10)
    s.press('A')
    print(f'exit by A at {s.t}', file=sys.stderr)
    enter(s, s.t + AGAIN)
    s.press('START')
    s.wait(100)
    open(sys.argv[1], 'w').write('\n'.join(s.lines) + '\n')


if __name__ == '__main__':
    main()
