"""The ported routines' prototypes, as src/game.h declares them (and gen_ports.py's grammar):

    uint8_t PadId(uint32_t port);                 /* $003156 a2 -> d0.b */

load(path) -> {address: Proto}. A Proto has the C name, return type, parameters [(type, name)],
the registers of the value parameters (regs_in) and the results (regs_out: the return value
first when there is one, then the pointer parameters), each ('reg', 'd0', 'b'|'w'|'l') or
('flag', 'Z'|'N'|'C', negated) or ('ccr',). A register kept in a C global, "a6=hint_data",
is ('glob', 'a6', 'hint_data'), before the arrow or after it, and takes no parameter.
"""
import re

PROTO = re.compile(r'^\s*([A-Za-z_][\w ]*?[\w*])\s*\b([A-Za-z_]\w*)\s*\(([^)]*)\)\s*;\s*/\*\s*\$([0-9A-Fa-f]{6})\b([^*]*)\*/')


class Proto:
    __slots__ = ('name', 'ret', 'params', 'regs_in', 'regs_out', 'addr', 'manual', 'globs_in', 'globs_out')

    def __init__(self, name, ret, params, regs_in, regs_out, addr, manual):
        self.name, self.ret, self.params = name, ret, params
        self.regs_in, self.regs_out, self.addr, self.manual = regs_in, regs_out, addr, manual

    def values(self):
        return [p for p in self.params if not p[0].endswith('*')]

    def pointers(self):
        return [p for p in self.params if p[0].endswith('*')]

    def results_after_return(self):
        return self.regs_out[1:] if self.ret != 'void' else self.regs_out


def parse_reg(t):
    t = t.strip()
    m = re.fullmatch(r'([da][0-7])=([A-Za-z_]\w*)', t)
    if m:
        return ('glob', m.group(1), m.group(2))
    m = re.fullmatch(r'([da][0-7])(?:\.([bwl]))?', t)
    if m:
        return ('reg', m.group(1), m.group(2) or 'l')
    m = re.fullmatch(r'(!?)([ZNC])', t)
    if m:
        return ('flag', m.group(2), m.group(1) == '!')
    if t == 'ccr':
        return ('ccr',)
    raise ValueError('register %r' % t)


def params(text):
    text = text.strip()
    if text in ('', 'void'):
        return []
    out = []
    for p in text.split(','):
        p = re.sub(r'/\*.*?\*/', '', p).strip()
        m = re.fullmatch(r'(.*?)([A-Za-z_]\w*)', p)
        out.append((m.group(1).strip(), m.group(2)))
    return out


def parse_line(line):
    m = PROTO.match(line)
    if not m:
        return None
    ret, name, ptext, addr, regs = m.groups()
    manual = 'manual' in regs
    regs = regs.replace('manual', '').strip()
    lhs, _, rhs = regs.partition('->')
    ins = [parse_reg(t) for t in lhs.split(',') if t.strip()]
    outs = [parse_reg(t) for t in rhs.split(',') if t.strip()]
    p = Proto(name, ret.strip(), params(ptext), [r for r in ins if r[0] != 'glob'],
              [r for r in outs if r[0] != 'glob'], int(addr, 16), manual)
    p.globs_in = [r for r in ins if r[0] == 'glob']
    p.globs_out = [r for r in outs if r[0] == 'glob']
    return p


def load(*paths):
    out = {}
    for path in paths:
        for line in open(path):
            p = parse_line(line)
            if p:
                out[p.addr] = p
    return out
