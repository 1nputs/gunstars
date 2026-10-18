#!/usr/bin/env python3
"""A Z80 decoder (the documented instructions, by the x/y/z/p/q fields of the opcode) and a
listing of the Z80 program the 68k loads (sound.c: Z80_PROGRAM, $666B4, $1F28 bytes, at Z80 $0000),
following flow from its entry and the addresses given.

usage: z80.py ROM [--base 0x666B4] [--size 0x1F28] [--entry 0,...]
"""
import argparse

R = ['b', 'c', 'd', 'e', 'h', 'l', '(hl)', 'a']
RP = ['bc', 'de', 'hl', 'sp']
RP2 = ['bc', 'de', 'hl', 'af']
CC = ['nz', 'z', 'nc', 'c', 'po', 'pe', 'p', 'm']
ALU = ['add a,', 'adc a,', 'sub ', 'sbc a,', 'and ', 'xor ', 'or ', 'cp ']
ROT = ['rlc', 'rrc', 'rl', 'rr', 'sla', 'sra', 'sll', 'srl']
IM = ['0', '0', '1', '2', '0', '0', '1', '2']
BLI = {(4, 0): 'ldi', (4, 1): 'cpi', (4, 2): 'ini', (4, 3): 'outi',
       (5, 0): 'ldd', (5, 1): 'cpd', (5, 2): 'ind', (5, 3): 'outd',
       (6, 0): 'ldir', (6, 1): 'cpir', (6, 2): 'inir', (6, 3): 'otir',
       (7, 0): 'lddr', (7, 1): 'cpdr', (7, 2): 'indr', (7, 3): 'otdr'}

SEQ, JUMP, COND, CALL, RET, CRET, STOP = 'seq', 'jump', 'cond', 'call', 'ret', 'cret', 'stop'


class Insn:
    def __init__(self, addr, size, text, flow=SEQ, target=None):
        self.addr, self.size, self.text, self.flow, self.target = addr, size, text, flow, target


def decode(mem, addr):
    """the instruction at addr (mem: bytes indexed by Z80 address)"""
    def b(o):
        return mem[(addr + o) & 0xFFFF]

    def w(o):
        return b(o) | b(o + 1) << 8

    def s8(v):
        return v - 256 if v & 0x80 else v

    pre, i = None, 0
    op = b(0)
    if op in (0xDD, 0xFD):
        pre, i = ('ix' if op == 0xDD else 'iy'), 1
        op = b(1)
    hl = pre or 'hl'

    def r(k, d_at):
        """register k, with (ix+d) for (hl) under a prefix (d at offset d_at)"""
        if k == 6 and pre:
            return f'({pre}{s8(b(d_at)):+d})'
        if pre and k in (4, 5):
            return pre + ('h' if k == 4 else 'l')
        return R[k]

    if op == 0xCB:
        if pre:
            d, op2, n = s8(b(2)), b(3), 4
            loc = f'({pre}{d:+d})'
        else:
            op2, n = b(1), 2
        x, y, z = op2 >> 6, op2 >> 3 & 7, op2 & 7
        reg = loc if pre else R[z]
        if x == 0:
            return Insn(addr, n, f'{ROT[y]} {reg}')
        return Insn(addr, n, f"{['', 'bit', 'res', 'set'][x]} {y},{reg}")
    if op == 0xED and not pre:
        op2 = b(1)
        x, y, z, p, q = op2 >> 6, op2 >> 3 & 7, op2 & 7, op2 >> 4 & 3, op2 >> 3 & 1
        if x == 1:
            if z == 0:
                return Insn(addr, 2, f'in {R[y]},(c)' if y != 6 else 'in (c)')
            if z == 1:
                return Insn(addr, 2, f'out (c),{R[y]}' if y != 6 else 'out (c),0')
            if z == 2:
                return Insn(addr, 2, f"{'sbc' if q == 0 else 'adc'} hl,{RP[p]}")
            if z == 3:
                return Insn(addr, 4, f'ld (${w(2):04X}),{RP[p]}' if q == 0 else f'ld {RP[p]},(${w(2):04X})')
            if z == 4:
                return Insn(addr, 2, 'neg')
            if z == 5:
                return Insn(addr, 2, 'reti' if y == 1 else 'retn', RET)
            if z == 6:
                return Insn(addr, 2, f'im {IM[y]}')
            return Insn(addr, 2, ['ld i,a', 'ld r,a', 'ld a,i', 'ld a,r', 'rrd', 'rld', 'nop', 'nop'][y])
        if x == 2 and (y, z) in BLI:
            return Insn(addr, 2, BLI[(y, z)])
        return Insn(addr, 2, f'db $ED,${op2:02X}')
    n = i + 1
    x, y, z, p, q = op >> 6, op >> 3 & 7, op & 7, op >> 4 & 3, op >> 3 & 1
    rp = [*RP[:2], hl, 'sp']
    rp2 = [*RP2[:2], hl, 'af']
    if x == 0:
        if z == 0:
            if y == 0:
                return Insn(addr, n, 'nop')
            if y == 1:
                return Insn(addr, n, "ex af,af'")
            t = (addr + n + 1 + s8(b(n))) & 0xFFFF
            if y == 2:
                return Insn(addr, n + 1, f'djnz ${t:04X}', COND, t)
            if y == 3:
                return Insn(addr, n + 1, f'jr ${t:04X}', JUMP, t)
            return Insn(addr, n + 1, f'jr {CC[y - 4]},${t:04X}', COND, t)
        if z == 1:
            if q == 0:
                return Insn(addr, n + 2, f'ld {rp[p]},${w(n):04X}')
            return Insn(addr, n, f'add {hl},{rp[p]}')
        if z == 2:
            if q == 0:
                t = ['ld (bc),a', 'ld (de),a', f'ld (${w(n):04X}),{hl}', f'ld (${w(n):04X}),a'][p]
            else:
                t = ['ld a,(bc)', 'ld a,(de)', f'ld {hl},(${w(n):04X})', f'ld a,(${w(n):04X})'][p]
            return Insn(addr, n + (2 if p >= 2 else 0), t)
        if z == 3:
            return Insn(addr, n, f"{'inc' if q == 0 else 'dec'} {rp[p]}")
        if z in (4, 5):
            k = n + (1 if (pre and y == 6) else 0)
            return Insn(addr, k, f"{'inc' if z == 4 else 'dec'} {r(y, n)}")
        if z == 6:
            k = n + (1 if (pre and y == 6) else 0)
            return Insn(addr, k + 1, f'ld {r(y, n)},${b(k):02X}')
        return Insn(addr, n, ['rlca', 'rrca', 'rla', 'rra', 'daa', 'cpl', 'scf', 'ccf'][y])
    if x == 1:
        if y == 6 and z == 6:
            return Insn(addr, n, 'halt', STOP)
        k = n + (1 if (pre and (y == 6 or z == 6)) else 0)
        # under a prefix, ld (ix+d),h keeps h (only the memory operand is indexed)
        dst = r(y, n) if not (pre and z == 6) else R[y]
        src = r(z, n) if not (pre and y == 6) else R[z]
        if pre and y == 6:
            dst = r(6, n)
        if pre and z == 6:
            src = r(6, n)
        return Insn(addr, k, f'ld {dst},{src}')
    if x == 2:
        k = n + (1 if (pre and z == 6) else 0)
        return Insn(addr, k, f'{ALU[y]}{r(z, n)}')
    # x == 3
    if z == 0:
        return Insn(addr, n, f'ret {CC[y]}', CRET)
    if z == 1:
        if q == 0:
            return Insn(addr, n, f'pop {rp2[p]}')
        t = ['ret', 'exx', f'jp ({hl})', f'ld sp,{hl}'][p]
        return Insn(addr, n, t, RET if p == 0 else (JUMP if p == 2 else SEQ))
    if z == 2:
        return Insn(addr, n + 2, f'jp {CC[y]},${w(n):04X}', COND, w(n))
    if z == 3:
        if y == 0:
            return Insn(addr, n + 2, f'jp ${w(n):04X}', JUMP, w(n))
        if y == 2:
            return Insn(addr, n + 1, f'out (${b(n):02X}),a')
        if y == 3:
            return Insn(addr, n + 1, f'in a,(${b(n):02X})')
        return Insn(addr, n, ['', '', '', '', f'ex (sp),{hl}', 'ex de,hl', 'di', 'ei'][y])
    if z == 4:
        return Insn(addr, n + 2, f'call {CC[y]},${w(n):04X}', CALL, w(n))
    if z == 5:
        if q == 0:
            return Insn(addr, n, f'push {rp2[p]}')
        if p == 0:
            return Insn(addr, n + 2, f'call ${w(n):04X}', CALL, w(n))
        return Insn(addr, n, f'db ${op:02X}')
    if z == 6:
        return Insn(addr, n + 1, f'{ALU[y]}${b(n):02X}')
    return Insn(addr, n, f'rst ${y * 8:02X}', CALL, y * 8)


def follow(mem, entries):
    """every instruction reached from the entries"""
    seen, todo = {}, list(entries)
    while todo:
        a = todo.pop()
        if a in seen or a >= len(mem):
            continue
        i = decode(mem, a)
        seen[a] = i
        nxt = []
        if i.flow in (SEQ, COND, CALL, CRET):
            nxt.append(a + i.size)
        if i.target is not None and i.flow in (JUMP, COND, CALL):
            nxt.append(i.target)
        todo.extend(nxt)
    return seen


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('rom')
    ap.add_argument('--base', type=lambda v: int(v, 0), default=0x666B4)
    ap.add_argument('--size', type=lambda v: int(v, 0), default=0x1F28)
    ap.add_argument('--entry', default='0')
    a = ap.parse_args()
    rom = open(a.rom, 'rb').read()
    mem = rom[a.base:a.base + a.size]
    code = follow(mem, [int(e, 0) for e in a.entry.split(',')])
    labels = {i.target for i in code.values() if i.target is not None}
    addr = 0
    end = max(code) + code[max(code)].size
    while addr < end:
        if addr in code:
            i = code[addr]
            if addr in labels:
                print(f'L{addr:04X}:')
            raw = ' '.join(f'{mem[addr + k]:02X}' for k in range(i.size))
            print(f'    {i.text:24s} ; {addr:04X}  {raw}')
            addr += i.size
        else:
            run = addr
            while run < end and run not in code:
                run += 1
            for k in range(addr, run, 16):
                chunk = mem[k:min(run, k + 16)]
                print(f'    db ' + ','.join(f'${v:02X}' for v in chunk) + f'   ; {k:04X}')
            addr = run
    print(f'; {len(code)} instructions')


if __name__ == '__main__':
    main()
