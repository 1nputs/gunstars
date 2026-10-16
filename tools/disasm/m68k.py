"""A 68000 decoder whose output vasm (Motorola syntax, -no-opt -m68000) assembles back to the same
bytes: every encoding gets the mnemonic and operand form that selects it (addi versus add #,
(xxx).w versus .l, bra.s versus bra.w ...). Encodings an assembler cannot reproduce (nonzero
reserved bits, a byte immediate with a high byte) are flagged `raw`, and emitted as dc.w.

decode(rom, addr) -> Insn, or raises Bad for bytes that are not a 68000 instruction.
"""

CC = ('t', 'f', 'hi', 'ls', 'cc', 'cs', 'ne', 'eq', 'vc', 'vs', 'pl', 'mi', 'ge', 'lt', 'gt', 'le')
SZ = ('b', 'w', 'l')


class Bad(Exception):
    pass


# operands: tuples, first element the kind
#   ('d', n) ('a', n) ('ind', n) ('inc', n) ('dec', n) ('disp', n, d16) ('idx', n, d8, xreg, xlong)
#   ('absw', addr) ('absl', addr) ('pcd', target) ('pcx', target, xreg, xlong) ('imm', value, size)
#   ('q', n) quick or bit number, ('list', mask, predec), ('sr',) ('ccr',) ('usp',), ('lab', target)
#   xreg: 0-7 data, 8-15 address registers

# flow kinds
SEQ, BRANCH, COND, CALL, JUMP, RET, STOP = 'seq', 'branch', 'cond', 'call', 'jump', 'ret', 'stop'


class Insn:
    __slots__ = ('addr', 'size', 'mnem', 'ops', 'flow', 'target', 'raw', 'words')

    def __init__(self, addr, size, mnem, ops, flow=SEQ, target=None, raw=False):
        self.addr, self.size, self.mnem, self.ops = addr, size, mnem, ops
        self.flow, self.target, self.raw = flow, target, raw
        self.words = None

    def refs(self):
        """ROM or RAM addresses this instruction names: (address, how) with how in
        'pc' (pc-relative), 'abs' (absolute), 'imm' (a long immediate), 'target' (branch)"""
        out = []
        if self.target is not None:
            out.append((self.target, 'target'))
        for o in self.ops:
            k = o[0]
            if k in ('pcd', 'pcx'):
                out.append((o[1], 'pc'))
            elif k in ('absw', 'absl'):
                out.append((o[1], 'abs'))
            elif k == 'imm' and o[2] == 'l':
                out.append((o[1], 'imm'))
        return out


class Reader:
    __slots__ = ('rom', 'pc', 'raw')

    def __init__(self, rom, pc):
        self.rom, self.pc, self.raw = rom, pc, False

    def w(self):
        if self.pc + 2 > len(self.rom):
            raise Bad('past the end')
        v = self.rom[self.pc] << 8 | self.rom[self.pc + 1]
        self.pc += 2
        return v

    def l(self):
        return self.w() << 16 | self.w()


def s8(v):
    return v - 0x100 if v & 0x80 else v


def s16(v):
    return v - 0x10000 if v & 0x8000 else v


def sx16(v):
    """a 16-bit absolute address as the 68000 sign-extends it"""
    return v | 0xFFFF0000 if v & 0x8000 else v


# effective address classes
ALL, DATA, MEM, ALT, DATA_ALT, MEM_ALT, CTRL, CTRL_ALT, ALT_ANY = range(9)


def ea(r, mode, reg, size, cls):
    """read the operand of an effective address field; size 'b' 'w' 'l' for immediates"""
    if mode == 0:
        if cls in (MEM, MEM_ALT, CTRL, CTRL_ALT):
            raise Bad('Dn not allowed')
        return ('d', reg)
    if mode == 1:
        if cls in (DATA, MEM, DATA_ALT, MEM_ALT, CTRL, CTRL_ALT) or size == 'b':
            raise Bad('An not allowed')
        return ('a', reg)
    if mode == 2:
        return ('ind', reg)
    if mode in (3, 4):
        if cls in (CTRL, CTRL_ALT):
            raise Bad('(An)+ / -(An) not allowed')
        return ('inc' if mode == 3 else 'dec', reg)
    if mode == 5:
        return ('disp', reg, s16(r.w()))
    if mode == 6:
        return index(r, ('idx', reg))
    # mode 7
    if reg == 0:
        return ('absw', sx16(r.w()))
    if reg == 1:
        return ('absl', r.l())
    if reg in (2, 3):
        if cls in (ALT, DATA_ALT, MEM_ALT, CTRL_ALT, ALT_ANY):
            raise Bad('pc-relative not alterable')
        base = r.pc
        if reg == 2:
            return ('pcd', (base + s16(r.w())) & 0xFFFFFFFF)
        o = index(r, ('pcx',))
        return ('pcx', (base + o[1]) & 0xFFFFFFFF, o[2], o[3])
    if reg == 4:
        if cls not in (ALL, DATA):
            raise Bad('immediate not allowed')
        if size == 'l':
            return ('imm', r.l(), 'l')
        v = r.w()
        if size == 'b':
            if v & 0xFF00:
                r.raw = True
            return ('imm', v & 0xFF, 'b')
        return ('imm', v, 'w')
    raise Bad('mode 7 register %d' % reg)


def index(r, head):
    e = r.w()
    if e & 0x0700:                      # scale and full-format bits: not an 68000 encoding an assembler writes
        r.raw = True
    xreg = (e >> 12) & 15
    xlong = bool(e & 0x0800)
    if head[0] == 'idx':
        return ('idx', head[1], s8(e & 0xFF), xreg, xlong)
    return ('pcx', s8(e & 0xFF), xreg, xlong)


def decode(rom, addr):
    if addr & 1:
        raise Bad('odd address')
    r = Reader(rom, addr)
    op = r.w()
    line = op >> 12
    fn = LINES[line]
    i = fn(r, op, addr)
    i.size = r.pc - addr
    i.raw = i.raw or r.raw
    return i


def I(addr, mnem, *ops, flow=SEQ, target=None):
    return Insn(addr, 0, mnem, list(ops), flow, target)


# ---- line 0: immediates, bit operations, movep -------------------------------------------------
IMM_NAMES = {0: 'ori', 1: 'andi', 2: 'subi', 3: 'addi', 5: 'eori', 6: 'cmpi'}
BIT_NAMES = ('btst', 'bchg', 'bclr', 'bset')


def line0(r, op, a):
    mode, reg = (op >> 3) & 7, op & 7
    if op & 0x0100:
        if mode == 1:                                            # movep
            opm = (op >> 6) & 7
            d = s16(r.w())
            dn = (op >> 9) & 7
            sz = 'w' if opm in (4, 6) else 'l'
            if opm in (4, 5):
                return I(a, 'movep.' + sz, ('disp', reg, d), ('d', dn))
            return I(a, 'movep.' + sz, ('d', dn), ('disp', reg, d))
        t = (op >> 6) & 3
        dst = ea(r, mode, reg, 'b', DATA if t == 0 else DATA_ALT)
        return I(a, BIT_NAMES[t], ('d', (op >> 9) & 7), dst)
    top = (op >> 9) & 7
    if top == 4:                                                 # static bit operation
        t = (op >> 6) & 3
        n = r.w()
        if n & 0xFF00:
            r.raw = True
        if mode == 7 and reg == 4:
            raise Bad('bit op on an immediate')
        dst = ea(r, mode, reg, 'b', DATA if t == 0 else DATA_ALT)
        return I(a, BIT_NAMES[t], ('q', n & 0xFF), dst)
    if op in (0x003C, 0x023C, 0x0A3C):
        v = r.w()
        if v & 0xFF00:
            r.raw = True
        return I(a, IMM_NAMES[top] + '.b', ('imm', v & 0xFF, 'b'), ('ccr',))
    if op in (0x007C, 0x027C, 0x0A7C):
        return I(a, IMM_NAMES[top] + '.w', ('imm', r.w(), 'w'), ('sr',))
    if top not in IMM_NAMES:
        raise Bad('line 0')
    s = (op >> 6) & 3
    if s == 3:
        raise Bad('size 3')
    sz = SZ[s]
    imm = ea(r, 7, 4, sz, ALL)
    dst = ea(r, mode, reg, sz, DATA_ALT)
    return I(a, IMM_NAMES[top] + '.' + sz, imm, dst)


# ---- lines 1-3: move ----------------------------------------------------------------------------
def line_move(r, op, a):
    sz = {1: 'b', 3: 'w', 2: 'l'}[op >> 12]
    src = ea(r, (op >> 3) & 7, op & 7, sz, ALL)
    dmode, dreg = (op >> 6) & 7, (op >> 9) & 7
    if dmode == 1:
        if sz == 'b':
            raise Bad('movea.b')
        return I(a, 'movea.' + sz, src, ('a', dreg))
    dst = ea(r, dmode, dreg, sz, DATA_ALT)
    return I(a, 'move.' + sz, src, dst)


# ---- line 4: miscellaneous ----------------------------------------------------------------------
FIXED4 = {0x4E70: ('reset', SEQ), 0x4E71: ('nop', SEQ), 0x4E73: ('rte', RET), 0x4E75: ('rts', RET),
          0x4E76: ('trapv', SEQ), 0x4E77: ('rtr', RET), 0x4AFC: ('illegal', STOP)}


def line4(r, op, a):
    mode, reg = (op >> 3) & 7, op & 7
    if op in FIXED4:
        m, f = FIXED4[op]
        return I(a, m, flow=f)
    if op == 0x4E72:
        return I(a, 'stop', ('imm', r.w(), 'w'))
    if (op & 0xFFF0) == 0x4E40:
        return I(a, 'trap', ('q', op & 15))
    if (op & 0xFFF8) == 0x4E50:
        return I(a, 'link', ('a', reg), ('imm', r.w(), 'w'))
    if (op & 0xFFF8) == 0x4E58:
        return I(a, 'unlk', ('a', reg))
    if (op & 0xFFF0) == 0x4E60:
        if op & 8:
            return I(a, 'move.l', ('usp',), ('a', reg))
        return I(a, 'move.l', ('a', reg), ('usp',))
    if (op & 0xFFC0) == 0x4E80:
        t = ea(r, mode, reg, 'l', CTRL)
        return I(a, 'jsr', t, flow=CALL, target=static_target(t))
    if (op & 0xFFC0) == 0x4EC0:
        t = ea(r, mode, reg, 'l', CTRL)
        return I(a, 'jmp', t, flow=JUMP, target=static_target(t))
    if (op & 0xF1C0) == 0x41C0:
        return I(a, 'lea', ea(r, mode, reg, 'l', CTRL), ('a', (op >> 9) & 7))
    if (op & 0xF1C0) == 0x4180:
        return I(a, 'chk.w', ea(r, mode, reg, 'w', DATA), ('d', (op >> 9) & 7))
    if (op & 0xFFC0) == 0x40C0:
        return I(a, 'move.w', ('sr',), ea(r, mode, reg, 'w', DATA_ALT))
    if (op & 0xFFC0) == 0x44C0:
        return I(a, 'move.w', ea(r, mode, reg, 'w', DATA), ('ccr',))
    if (op & 0xFFC0) == 0x46C0:
        return I(a, 'move.w', ea(r, mode, reg, 'w', DATA), ('sr',))
    if (op & 0xFFB8) == 0x4880:
        return I(a, 'ext.' + ('l' if op & 0x40 else 'w'), ('d', reg))
    if (op & 0xFFF8) == 0x4840:
        return I(a, 'swap', ('d', reg))
    if (op & 0xFFC0) == 0x4800:
        return I(a, 'nbcd', ea(r, mode, reg, 'b', DATA_ALT))
    if (op & 0xFFC0) == 0x4840:
        return I(a, 'pea', ea(r, mode, reg, 'l', CTRL))
    if (op & 0xFB80) == 0x4880:
        sz = 'l' if op & 0x40 else 'w'
        mask = r.w()
        if op & 0x0400:                                          # memory to registers
            if mode == 4:
                raise Bad('movem -(An) to registers')
            src = ea(r, mode, reg, sz, CTRL) if mode != 3 else ('inc', reg)
            return I(a, 'movem.' + sz, src, ('list', mask, False))
        if mode == 3:
            raise Bad('movem registers to (An)+')
        dst = ('dec', reg) if mode == 4 else ea(r, mode, reg, sz, CTRL_ALT)
        return I(a, 'movem.' + sz, ('list', mask, mode == 4), dst)
    if (op & 0xFFC0) == 0x4AC0:
        if op == 0x4AFC:
            return I(a, 'illegal', flow=STOP)
        return I(a, 'tas', ea(r, mode, reg, 'b', DATA_ALT))
    s = (op >> 6) & 3
    names = {0x4000: 'negx', 0x4200: 'clr', 0x4400: 'neg', 0x4600: 'not', 0x4A00: 'tst'}
    n = names.get(op & 0xFF00)
    if n and s != 3:
        sz = SZ[s]
        return I(a, n + '.' + sz, ea(r, mode, reg, sz, DATA_ALT))
    raise Bad('line 4')


def static_target(t):
    if t[0] == 'absw' or t[0] == 'absl' or t[0] == 'pcd':
        return t[1] & 0xFFFFFF
    return None


# ---- line 5: addq, subq, scc, dbcc --------------------------------------------------------------
def line5(r, op, a):
    mode, reg = (op >> 3) & 7, op & 7
    s = (op >> 6) & 3
    cc = (op >> 8) & 15
    if s == 3:
        if mode == 1:
            base = r.pc
            t = (base + s16(r.w())) & 0xFFFFFF
            return I(a, 'db' + CC[cc], ('d', reg), ('lab', t), flow=COND, target=t)
        return I(a, 's' + CC[cc], ea(r, mode, reg, 'b', DATA_ALT))
    sz = SZ[s]
    q = (op >> 9) & 7 or 8
    return I(a, ('subq.' if op & 0x100 else 'addq.') + sz, ('q', q), ea(r, mode, reg, sz, ALT_ANY))


# ---- line 6: branches ---------------------------------------------------------------------------
def line6(r, op, a):
    cc = (op >> 8) & 15
    d = op & 0xFF
    base = r.pc
    if d == 0:
        t = (base + s16(r.w())) & 0xFFFFFF
        sz = '.w'
    elif d == 0xFF:
        raise Bad('68020 long branch')
    else:
        t = (base + s8(d)) & 0xFFFFFF
        sz = '.s'
    if t & 1:
        raise Bad('odd branch target')
    if cc == 0:
        return I(a, 'bra' + sz, ('lab', t), flow=BRANCH, target=t)
    if cc == 1:
        return I(a, 'bsr' + sz, ('lab', t), flow=CALL, target=t)
    return I(a, 'b' + CC[cc] + sz, ('lab', t), flow=COND, target=t)


def line7(r, op, a):
    if op & 0x100:
        raise Bad('moveq bit 8')
    return I(a, 'moveq', ('imm', s8(op & 0xFF), 'q'), ('d', (op >> 9) & 7))


# ---- lines 8, 9, b, c, d: arithmetic and logic ---------------------------------------------------
def arith(r, op, a, name, xname, aname, bcd, mul, div):
    reg, opm, mode, ry = (op >> 9) & 7, (op >> 6) & 7, (op >> 3) & 7, op & 7
    if opm in (3, 7):
        if aname:
            sz = 'w' if opm == 3 else 'l'
            return I(a, aname + '.' + sz, ea(r, mode, ry, sz, ALL), ('a', reg))
        if opm == 3 and (mul or div):
            return I(a, (mul or div)[0] + '.w', ea(r, mode, ry, 'w', DATA), ('d', reg))
        if opm == 7 and (mul or div):
            return I(a, (mul or div)[1] + '.w', ea(r, mode, ry, 'w', DATA), ('d', reg))
        raise Bad('opmode')
    if opm >= 4 and mode in (0, 1):
        if xname:                                                # addx / subx
            sz = SZ[opm - 4]
            if mode == 0:
                return I(a, xname + '.' + sz, ('d', ry), ('d', reg))
            return I(a, xname + '.' + sz, ('dec', ry), ('dec', reg))
        if bcd and opm == 4:                                     # abcd / sbcd
            if mode == 0:
                return I(a, bcd, ('d', ry), ('d', reg))
            return I(a, bcd, ('dec', ry), ('dec', reg))
        if name == 'and' and opm == 5:                           # exg
            if mode == 0:
                return I(a, 'exg', ('d', reg), ('d', ry))
            return I(a, 'exg', ('a', reg), ('a', ry))
        if name == 'and' and opm == 6 and mode == 1:
            return I(a, 'exg', ('d', reg), ('a', ry))
        if name == 'cmp' and mode == 1:                          # cmpm
            sz = SZ[opm - 4]
            return I(a, 'cmpm.' + sz, ('inc', ry), ('inc', reg))
        if name in ('or', 'and') or (name == 'cmp' and mode == 0):
            pass                                                 # or / and / eor Dn,Dn falls through
        else:
            raise Bad('opmode')
    if opm < 3:
        sz = SZ[opm]
        if name == 'cmp':
            return I(a, 'cmp.' + sz, ea(r, mode, ry, sz, ALL), ('d', reg))
        cls = DATA if name in ('or', 'and') else ALL
        return I(a, name + '.' + sz, ea(r, mode, ry, sz, cls), ('d', reg))
    sz = SZ[opm - 4]
    if name == 'cmp':
        return I(a, 'eor.' + sz, ('d', reg), ea(r, mode, ry, sz, DATA_ALT))
    if mode == 0 and name in ('or', 'and'):
        raise Bad('or/and Dn,Dn the other way')
    return I(a, name + '.' + sz, ('d', reg), ea(r, mode, ry, sz, MEM_ALT))


def line8(r, op, a):
    if ((op >> 6) & 7) in (5, 6) and ((op >> 3) & 7) in (0, 1):
        raise Bad('pack / unpk')
    return arith(r, op, a, 'or', None, None, 'sbcd', None, ('divu', 'divs'))


def line9(r, op, a):
    return arith(r, op, a, 'sub', 'subx', 'suba', None, None, None)


def lineb(r, op, a):
    return arith(r, op, a, 'cmp', None, 'cmpa', None, None, None)


def linec(r, op, a):
    if ((op >> 6) & 7) == 6 and ((op >> 3) & 7) == 0:
        raise Bad('exg opmode')
    return arith(r, op, a, 'and', None, None, 'abcd', ('mulu', 'muls'), None)


def lined(r, op, a):
    return arith(r, op, a, 'add', 'addx', 'adda', None, None, None)


# ---- line e: shifts and rotates -----------------------------------------------------------------
SHIFTS = ('as', 'ls', 'rox', 'ro')


def linee(r, op, a):
    s = (op >> 6) & 3
    d = 'l' if op & 0x100 else 'r'
    if s == 3:
        t = (op >> 9) & 7
        if t > 3:
            raise Bad('bit field')
        return I(a, SHIFTS[t] + d + '.w', ea(r, (op >> 3) & 7, op & 7, 'w', MEM_ALT))
    sz = SZ[s]
    t = (op >> 3) & 3
    c = (op >> 9) & 7
    if op & 0x20:
        cnt = ('d', c)
    else:
        cnt = ('q', c or 8)
    return I(a, SHIFTS[t] + d + '.' + sz, cnt, ('d', op & 7))


def bad_line(r, op, a):
    raise Bad('line A / F')


LINES = (line0, line_move, line_move, line_move, line4, line5, line6, line7,
         line8, line9, bad_line, lineb, linec, lined, linee, bad_line)


# ---- rendering ----------------------------------------------------------------------------------
def hexs(v, digits=0):
    return '$%0*X' % (digits, v) if digits else '$%X' % v


def sdisp(v):
    return ('-$%X' % -v) if v < 0 else ('$%X' % v)


def xreg_name(x, xlong):
    return ('d%d' % x if x < 8 else ('sp' if x == 15 else 'a%d' % (x - 8))) + ('.l' if xlong else '.w')


def areg(n):
    return 'sp' if n == 7 else 'a%d' % n


def reglist(mask, predec):
    if predec:                                                   # -(An): bit 0 is a7
        mask = int('{:016b}'.format(mask)[::-1], 2)
    regs = [('d%d' % i) for i in range(8) if mask >> i & 1] + [('a%d' % i) for i in range(8) if mask >> (8 + i) & 1]
    # ranges within d and within a
    out = []
    for prefix, base in (('d', 0), ('a', 8)):
        i = 0
        while i < 8:
            if mask >> (base + i) & 1:
                j = i
                while j + 1 < 8 and mask >> (base + j + 1) & 1:
                    j += 1
                out.append(f'{prefix}{i}' + (f'-{prefix}{j}' if j > i else ''))
                i = j + 1
            else:
                i += 1
    return '/'.join(out) if regs else None


def operand(o, name_of, size):
    """name_of(address, how) -> a symbol or None; size: the instruction's size letter"""
    k = o[0]
    if k == 'd':
        return 'd%d' % o[1]
    if k == 'a':
        return areg(o[1])
    if k == 'ind':
        return '(%s)' % areg(o[1])
    if k == 'inc':
        return '(%s)+' % areg(o[1])
    if k == 'dec':
        return '-(%s)' % areg(o[1])
    if k == 'disp':
        return '%s(%s)' % (sdisp(o[2]), areg(o[1]))
    if k == 'idx':
        return '%s(%s,%s)' % (sdisp(o[2]), areg(o[1]), xreg_name(o[3], o[4]))
    if k == 'absw':
        n = name_of(o[1], 'abs')
        return '(%s).w' % (n or hexs(o[1], 8 if o[1] > 0xFFFF else 0))
    if k == 'absl':
        n = name_of(o[1], 'abs')
        return '(%s).l' % (n or hexs(o[1], 8))
    if k == 'pcd':
        n = name_of(o[1], 'pc')
        if not n:
            raise Bad('pc-relative target without a label')
        return '%s(pc)' % n
    if k == 'pcx':
        n = name_of(o[1], 'pc')
        if not n:
            raise Bad('pc-relative target without a label')
        return '%s(pc,%s)' % (n, xreg_name(o[2], o[3]))
    if k == 'imm':
        if o[2] == 'q':
            return '#%d' % o[1]
        if o[2] == 'l':
            n = name_of(o[1], 'imm')
            return '#' + (n or hexs(o[1]))
        return '#' + hexs(o[1])
    if k == 'q':
        return '#%d' % o[1]
    if k == 'list':
        l = reglist(o[1], o[2])
        if l is None:
            raise Bad('empty register list')
        return l
    if k == 'sr':
        return 'sr'
    if k == 'ccr':
        return 'ccr'
    if k == 'usp':
        return 'usp'
    if k == 'lab':
        n = name_of(o[1], 'target')
        if not n:
            raise Bad('branch target without a label')
        return n
    raise ValueError(k)


def render(i, name_of):
    """the instruction as vasm source; Bad when it cannot be written so"""
    size = i.mnem.rsplit('.', 1)[1] if '.' in i.mnem else ''
    ops = ','.join(operand(o, name_of, size) for o in i.ops)
    return i.mnem + (' ' + ops if ops else '')
