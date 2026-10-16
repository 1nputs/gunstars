#!/usr/bin/env python3
"""Run the oracle over scripted play and add what the CPUs did to the disassembly's evidence in
out/evidence (oracle_shim.c, "disassembly evidence"), merged with what earlier runs found:

  rom_cov.u32    executions of each ROM address as an instruction (u32 a byte of the 1 MB ROM)
  ram_cov.u32    executions in 68k work RAM ($FF0000-$FFFFFF, u32 each)
  z80_cov.u32    executions in Z80 RAM ($0000-$1FFF, u32 each)
  rom_flags.u8   per ROM byte: 1 read as data by the 68k, 2 read by DMA
  rom_reader.u32 per ROM byte: the PC of the last instruction seen reading it (| 0x80000000; 0 none;
                 a checksum's sweep of the whole ROM never replaces another reader)
  edges.txt      control transfers: site target kind count (hex; kind 1 jsr, 2 bsr, 3 jmp,
                 4 interrupt entry with site ffffffff)
  runs.txt       the runs merged so far

usage: evidence.py ROM --name NAME [--state S] [--frames N] [--script S] [--poke P] [--out DIR]
       evidence.py ROM --summary
"""
import argparse, array, ctypes as C, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from oracle import Oracle, ROOT  # noqa: E402
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'oracle'))
from run import parse_pokes  # noqa: E402
from script import Player  # noqa: E402

ROM_SIZE = 0x100000


def load_u32(path, n):
    a = array.array('I', bytes(4 * n))
    if os.path.exists(path):
        a = array.array('I'); a.frombytes(open(path, 'rb').read())
        assert len(a) == n, path
    return a


def load_edges(path):
    e = {}
    if os.path.exists(path):
        for line in open(path):
            s, t, k, c = line.split()
            e[(int(s, 16), int(t, 16))] = [int(k), int(c)]
    return e


def summary(out):
    cov = load_u32(os.path.join(out, 'rom_cov.u32'), ROM_SIZE)
    flags = open(os.path.join(out, 'rom_flags.u8'), 'rb').read() if os.path.exists(os.path.join(out, 'rom_flags.u8')) else bytes(ROM_SIZE)
    ram = load_u32(os.path.join(out, 'ram_cov.u32'), 0x10000)
    z80 = load_u32(os.path.join(out, 'z80_cov.u32'), 0x2000)
    edges = load_edges(os.path.join(out, 'edges.txt'))
    pcs = sum(1 for v in cov if v)
    print(f'68k: {pcs} instructions executed in ROM, {sum(1 for v in ram if v)} in work RAM; '
          f'{sum(1 for f in flags if f & 1)} ROM bytes read as data, {sum(1 for f in flags if f & 2)} by DMA')
    print(f'Z80: {sum(1 for v in z80 if v)} instructions executed')
    kinds = {}
    for (s, t), (k, c) in edges.items():
        kinds[k] = kinds.get(k, 0) + 1
    print('transfers: ' + ', '.join(f'{n} {kinds.get(k, 0)}' for k, n in ((1, 'jsr'), (2, 'bsr'), (3, 'jmp'), (4, 'interrupt'))))
    calls = {t for (s, t), (k, c) in edges.items() if k in (1, 2)}
    print(f'{len(calls)} distinct call targets')
    runs = os.path.join(out, 'runs.txt')
    if os.path.exists(runs):
        print('runs: ' + ', '.join(l.split()[0] for l in open(runs)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('rom')
    ap.add_argument('--name', default='')
    ap.add_argument('--state', default='')
    ap.add_argument('--frames', type=int, default=600)
    ap.add_argument('--script', default='')
    ap.add_argument('--poke', default='')
    ap.add_argument('--out', default=os.path.join(ROOT, 'out', 'evidence'))
    ap.add_argument('--summary', action='store_true')
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    if a.summary:
        summary(a.out)
        return
    if not a.name:
        sys.exit('--name: what this run is, for runs.txt')

    o = Oracle()
    o.load(a.rom)
    if a.state:
        o.unserialize(open(a.state, 'rb').read())
    lib = o.lib
    lib.oracle_evidence_enable.argtypes = [C.c_int]
    lib.oracle_evidence_get.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p, C.c_void_p]
    lib.oracle_evidence_edges.argtypes = [C.c_void_p, C.c_uint32]; lib.oracle_evidence_edges.restype = C.c_uint32
    if not lib.oracle_evidence_enable(1):
        sys.exit('evidence: out of memory')
    o.coverage(True)
    script = Player(a.script)
    pokes = parse_pokes(a.poke)
    for f in range(a.frames):
        script.at(o, f)
        for ad, v in pokes.get(f, ()):
            o.poke_w(ad, v)
        o.run()

    cov = o.coverage_get()
    ram = (C.c_uint32 * 0x10000)(); z80 = (C.c_uint32 * 0x2000)(); flags = (C.c_uint8 * ROM_SIZE)()
    reader = (C.c_uint32 * ROM_SIZE)()
    lib.oracle_evidence_get(ram, z80, flags, reader)
    eb = (C.c_uint32 * (4 * 65536))()
    n = lib.oracle_evidence_edges(eb, 65536)

    for name, new, size in (('rom_cov.u32', cov, ROM_SIZE), ('ram_cov.u32', ram, 0x10000), ('z80_cov.u32', z80, 0x2000)):
        p = os.path.join(a.out, name)
        acc = load_u32(p, size)
        for i, v in enumerate(new):
            if v:
                acc[i] = min(0xFFFFFFFF, acc[i] + v)
        open(p, 'wb').write(acc.tobytes())
    p = os.path.join(a.out, 'rom_flags.u8')
    acc = bytearray(open(p, 'rb').read()) if os.path.exists(p) else bytearray(ROM_SIZE)
    nf = bytes(flags)
    for i in range(ROM_SIZE):
        if nf[i]:
            acc[i] |= nf[i]
    open(p, 'wb').write(acc)
    # a reader that read most of the ROM is a checksum's sweep: it never replaces another reader
    p = os.path.join(a.out, 'rom_reader.u32')
    acc = load_u32(p, ROM_SIZE)
    counts = {}
    for v in reader:
        if v:
            counts[v] = counts.get(v, 0) + 1
    sweep = max(counts, key=counts.get) if counts else 0
    if counts.get(sweep, 0) < ROM_SIZE // 2:
        sweep = 0
    for i, v in enumerate(reader):
        if v and (not acc[i] or v != sweep):
            acc[i] = v
    open(p, 'wb').write(acc.tobytes())
    p = os.path.join(a.out, 'edges.txt')
    edges = load_edges(p)
    for i in range(n):
        s, t, k, c = eb[4 * i:4 * i + 4]
        if (s, t) in edges:
            edges[(s, t)][1] += c
        else:
            edges[(s, t)] = [k, c]
    with open(p, 'w') as f:
        for (s, t), (k, c) in sorted(edges.items()):
            f.write(f'{s:08x} {t:08x} {k} {c}\n')
    with open(os.path.join(a.out, 'runs.txt'), 'a') as f:
        f.write(f'{a.name} frames={a.frames} state={a.state or "-"} script={a.script[:60] or "-"}\n')
    print(f'{a.name}: {a.frames} frames merged into {a.out}')
    summary(a.out)


if __name__ == '__main__':
    main()
