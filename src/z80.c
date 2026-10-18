/* The Z80's program ($666B4, loaded at Z80 $0000 by SoundInit): a sample player, ported to C an
 * instruction at a time. The 68k asks for a sample by writing its number ($81 on) to $1FFF; the
 * player takes it (bit 7 cleared), plays it to the YM2612's DAC ($2A) and waits for the next.
 *
 *   $0000 the bank register from $1FFA/$1FFB (a bit a write to $6000), then $002E
 *   $002E wait for $1FFF bit 7; the DAC on (YM $2B = $80); the sample's 12-byte entry: samples $81-
 *         $88 from the table at $016B (their bytes in Z80 RAM), the others from the one at ($1FF8)
 *         (in the bank window, their offsets from ($1FF8) too); $1FF6 / $1FF7 the number
 *   $00A2 a byte a pass, its high nibble then its low one: a delta from the table at $001E added
 *         to the level (C of the other register set), the level to the DAC once the chip is not
 *         busy, then a wait of (entry + 11) DJNZ turns; a new request (unless entry + 5 bit 7)
 *         starts it at once
 *   $00FF the loop count at $1FFE (entry + 4; bit 7: keep the level at $1FFC): the sample again
 *         from its loop point, the last time to (entry + 8) bytes; then the DAC off (YM $2B = 0)
 *
 * The program's two register sets trade places with each sample (EXX), so both are kept. $1FFD is
 * $80 while the program touches the YM2612 (the 68k waits for it to clear before it writes the
 * chip itself, the bus held).
 *
 * Time, as ninety-four's relay port has it (and as Genesis Plus GX runs a Z80): master clocks, 15
 * a Z80 cycle, the cycles the Zilog manual gives, 3 more for a read through the bank window; the
 * YM2612 busy for 32 of its cycles (42 clocks each, on a grid from the frame's start) after a
 * data write, its status read at the end of the instruction. The Z80 runs whole instructions while
 * it is behind the client's time, and not while the 68k holds its bus or its reset. */
#include <string.h>
#include "mem.h"
#include "z80.h"

uint8_t z80_mem[0x2000];
struct Z80Player zp;
void (*z80_player_ym)(int port, uint8_t value);

#define CYCLE 15
#define BANK_READ 3
#define YM_GRID 42
#define YM_BUSY 32

enum { FC = 0x01, FPV = 0x04, FZ = 0x40, FS = 0x80 };

/* ---- memory ---- */

static uint8_t rd(uint16_t a, unsigned *wait)
{
    if (a < 0x2000) return z80_mem[a];
    if (a >= 0x8000) {
        *wait += BANK_READ;
        return rom8(zp.bank | (a & 0x7FFF));
    }
    return 0xFF;
}

static void ym_port(int port, uint8_t value)
{
    if (port & 1) z80_player_ym_busy(zp.zc);
    if (z80_player_ym) z80_player_ym(port, value);
}

/* a write at the end of the instruction (zp.zc moved on already) */
static void wr(uint16_t a, uint8_t v)
{
    if (a < 0x2000) z80_mem[a] = v;
    else if (a >= 0x4000 && a < 0x6000) ym_port(a & 3, v);
    else if (a >= 0x6000 && a < 0x6100) zp.bank = ((zp.bank >> 1) | (uint32_t)(v & 1) << 23) & 0xFF8000;
}

/* the YM2612's status (at the end of the instruction): bit 7 busy */
static uint8_t ym_status(void)
{
    return zp.zc < zp.ym_free ? 0x80 : 0;
}

void z80_player_ym_busy(uint64_t t)
{
    uint64_t at = t - zp.frame;
    zp.ym_free = zp.frame + ((at + YM_GRID - 1) / YM_GRID + YM_BUSY) * YM_GRID;
}

/* ---- registers ---- */

#define BC ((uint16_t)(zp.b << 8 | zp.c))
#define DE ((uint16_t)(zp.d << 8 | zp.e))
#define HL ((uint16_t)(zp.h << 8 | zp.l))
static void set_bc(uint16_t v) { zp.b = (uint8_t)(v >> 8); zp.c = (uint8_t)v; }
static void set_de(uint16_t v) { zp.d = (uint8_t)(v >> 8); zp.e = (uint8_t)v; }
static void set_hl(uint16_t v) { zp.h = (uint8_t)(v >> 8); zp.l = (uint8_t)v; }

static int parity(uint8_t v) { v ^= v >> 4; v ^= v >> 2; v ^= v >> 1; return !(v & 1); }

/* OR, XOR, AND (AND sets H, bit 4) */
static void logic(uint8_t v, int and)
{
    zp.a = v;
    zp.f = (uint8_t)((v & 0xA8) | (v ? 0 : FZ) | (parity(v) ? FPV : 0) | (and ? 0x10 : 0));
}

/* SUB, CP (flags S, Z, C as the result; the result to A for SUB) */
static uint8_t sub8(uint8_t n)
{
    uint8_t r = (uint8_t)(zp.a - n);
    zp.f = (uint8_t)((r & FS) | (r ? 0 : FZ) | (zp.a < n ? FC : 0) | 0x02);
    return r;
}

static void add8(uint8_t n)
{
    unsigned s = (unsigned)zp.a + n;
    zp.a = (uint8_t)s;
    zp.f = (uint8_t)((zp.a & FS) | (zp.a ? 0 : FZ) | (s > 0xFF ? FC : 0));
}

static void bit7(uint8_t v)
{
    zp.f = (uint8_t)((zp.f & FC) | 0x10 | (v & 0x80 ? 0 : FZ | FPV) | (v & 0x80));
}

static uint16_t add16(uint16_t a, uint16_t b)
{
    unsigned s = (unsigned)a + b;
    zp.f = (uint8_t)((zp.f & (FS | FZ | FPV)) | (s > 0xFFFF ? FC : 0));
    return (uint16_t)s;
}

static void exx(void)
{
    uint8_t t;
    t = zp.b; zp.b = zp.b2; zp.b2 = t;
    t = zp.c; zp.c = zp.c2; zp.c2 = t;
    t = zp.d; zp.d = zp.d2; zp.d2 = t;
    t = zp.e; zp.e = zp.e2; zp.e2 = t;
    t = zp.h; zp.h = zp.h2; zp.h2 = t;
    t = zp.l; zp.l = zp.l2; zp.l2 = t;
}

static void push(uint8_t hi, uint8_t lo)
{
    zp.sp = (uint16_t)(zp.sp - 1); wr(zp.sp, hi);
    zp.sp = (uint16_t)(zp.sp - 1); wr(zp.sp, lo);
}

static uint16_t pop(void)
{
    unsigned w = 0;
    uint16_t v = (uint16_t)(rd(zp.sp, &w) | rd((uint16_t)(zp.sp + 1), &w) << 8);
    zp.sp = (uint16_t)(zp.sp + 2);
    return v;
}

static uint16_t word_at(uint16_t a, unsigned *w) { return (uint16_t)(rd(a, w) | rd((uint16_t)(a + 1), w) << 8); }

/* ---- the program ---- */

#define NEXT(n) zp.pc = (n)
#define JUMP_IF(c, to, next) zp.pc = (c) ? (to) : (next)

/* the instruction at pc; its cycles (a write it makes is at its end) */
static unsigned instruction(void)
{
    unsigned w = 0;
    uint8_t v;
    switch (zp.pc) {
    case 0x0000: case 0x0001: case 0x0002: NEXT(zp.pc + 1); return 4;          /* DI */
    case 0x0003: zp.sp = 0x1FF4; NEXT(0x0006); return 10;                      /* LD SP,$1FF4 */
    case 0x0006: logic(0, 0); NEXT(0x0007); return 4;                          /* XOR A */
    case 0x0007: zp.zc += 13 * CYCLE; wr(0x1FFD, zp.a); NEXT(0x000A); return 0;   /* LD ($1FFD),A */
    case 0x000A: zp.a = rd(0x1FFB, &w); NEXT(0x000D); return 13;               /* LD A,($1FFB) */
    case 0x000D:                                                               /* RLCA */
        zp.f = (uint8_t)((zp.f & (FS | FZ | FPV)) | (zp.a >> 7));
        zp.a = (uint8_t)(zp.a << 1 | zp.a >> 7);
        NEXT(0x000E); return 4;
    case 0x000E: zp.zc += 13 * CYCLE; wr(0x6000, zp.a); NEXT(0x0011); return 0;   /* LD ($6000),A */
    case 0x0011: zp.b = 8; NEXT(0x0013); return 7;                             /* LD B,8 */
    case 0x0013: zp.a = rd(0x1FFA, &w); NEXT(0x0016); return 13;               /* LD A,($1FFA) */
    case 0x0016: zp.zc += 13 * CYCLE; wr(0x6000, zp.a); NEXT(0x0019); return 0;   /* LD ($6000),A */
    case 0x0019:                                                               /* RRCA */
        zp.f = (uint8_t)((zp.f & (FS | FZ | FPV)) | (zp.a & 1));
        zp.a = (uint8_t)(zp.a >> 1 | zp.a << 7);
        NEXT(0x001A); return 4;
    case 0x001A: zp.b--; JUMP_IF(zp.b, 0x0016, 0x001C); return zp.b ? 13 : 8;   /* DJNZ $0016 */
    case 0x001C: NEXT(0x002E); return 12;                                      /* JR $002E */

    /* wait for a request */
    case 0x002E: set_hl(0x1FFF); NEXT(0x0031); return 10;                      /* LD HL,$1FFF */
    case 0x0031: zp.a = rd(HL, &w); NEXT(0x0032); return 7 + w;                /* LD A,(HL) */
    case 0x0032: logic(zp.a, 0); NEXT(0x0033); return 4;                       /* OR A */
    case 0x0033: JUMP_IF(!(zp.f & FS), 0x0031, 0x0036); return 10;             /* JP P,$0031 */
    case 0x0036: zp.zc += 11 * CYCLE; push(zp.a, zp.f); NEXT(0x0037); return 0;   /* PUSH AF */
    case 0x0037: zp.zc += 11 * CYCLE; push(zp.h, zp.l); NEXT(0x0038); return 0;   /* PUSH HL */
    case 0x0038: zp.a = 0x80; NEXT(0x003A); return 7;                          /* LD A,$80 */
    case 0x003A: zp.zc += 13 * CYCLE; wr(0x1FFD, zp.a); NEXT(0x003D); return 0;   /* LD ($1FFD),A */
    case 0x003D: set_hl(0x4000); NEXT(0x0040); return 10;                      /* LD HL,$4000 */
    case 0x0040: zp.zc += 10 * CYCLE; wr(HL, 0x2B); NEXT(0x0042); return 0;    /* LD (HL),$2B */
    case 0x0042: set_hl((uint16_t)(HL + 1)); NEXT(0x0043); return 6;           /* INC HL */
    case 0x0043: zp.zc += 7 * CYCLE; wr(HL, zp.a); NEXT(0x0044); return 0;     /* LD (HL),A: the DAC on */
    case 0x0044: logic(0, 0); NEXT(0x0045); return 4;                          /* XOR A */
    case 0x0045: zp.zc += 13 * CYCLE; wr(0x1FFD, zp.a); NEXT(0x0048); return 0;   /* LD ($1FFD),A */
    case 0x0048: set_hl((uint16_t)(HL - 1)); NEXT(0x0049); return 6;           /* DEC HL */
    case 0x0049: zp.ix = 0x1FFC; NEXT(0x004D); return 14;                      /* LD IX,$1FFC */
    case 0x004D: zp.d = 0; NEXT(0x004F); return 7;                             /* LD D,0 */
    case 0x004F: exx(); NEXT(0x0050); return 4;                                /* EXX */
    case 0x0050: set_hl(pop()); NEXT(0x0051); return 10;                       /* POP HL */
    case 0x0051: zp.zc += 13 * CYCLE; wr(0x1FF6, zp.a); NEXT(0x0054); return 0;   /* LD ($1FF6),A */
    case 0x0054: { uint16_t af = pop(); zp.a = (uint8_t)(af >> 8); zp.f = (uint8_t)af; } NEXT(0x0055); return 10;   /* POP AF */
    case 0x0055: zp.zc += 13 * CYCLE; wr(0x1FF7, zp.a); NEXT(0x0058); return 0;   /* LD ($1FF7),A */
    case 0x0058: zp.a = sub8(0x81); NEXT(0x005A); return 7;                    /* SUB $81 */
    case 0x005A: zp.zc += 7 * CYCLE; wr(HL, zp.a); NEXT(0x005B); return 0;     /* LD (HL),A: taken */
    case 0x005B: set_de(0); NEXT(0x005E); return 10;                           /* LD DE,0 */
    case 0x005E: zp.iy = 0x016B; NEXT(0x0062); return 14;                      /* LD IY,$016B */
    case 0x0062: sub8(8); NEXT(0x0064); return 7;                              /* CP 8 */
    case 0x0064: JUMP_IF(zp.f & FC, 0x0076, 0x0066); return (zp.f & FC) ? 12 : 7;   /* JR C,$0076 */
    case 0x0066: zp.zc += 13 * CYCLE; wr(0x1FF7, zp.a); NEXT(0x0069); return 0;   /* LD ($1FF7),A */
    case 0x0069: zp.zc += 13 * CYCLE; wr(0x1FF6, zp.a); NEXT(0x006C); return 0;   /* LD ($1FF6),A */
    case 0x006C: zp.iy = word_at(0x1FF8, &w); NEXT(0x0070); return 20 + w;     /* LD IY,($1FF8) */
    case 0x0070: set_de(word_at(0x1FF8, &w)); NEXT(0x0074); return 20 + w;     /* LD DE,($1FF8) */
    case 0x0074: zp.a = sub8(7); NEXT(0x0076); return 7;                       /* SUB 7 */
    case 0x0076: add8(zp.a); NEXT(0x0077); return 4;                           /* ADD A,A */
    case 0x0077: add8(zp.a); NEXT(0x0078); return 4;                           /* ADD A,A */
    case 0x0078: zp.c = zp.a; NEXT(0x0079); return 4;                          /* LD C,A */
    case 0x0079: add8(zp.a); NEXT(0x007A); return 4;                           /* ADD A,A */
    case 0x007A: add8(zp.c); NEXT(0x007B); return 4;                           /* ADD A,C: 12 a sample */
    case 0x007B: zp.c = zp.a; NEXT(0x007C); return 4;                          /* LD C,A */
    case 0x007C: zp.b = 0; NEXT(0x007E); return 7;                             /* LD B,0 */
    case 0x007E: zp.iy = add16(zp.iy, BC); NEXT(0x0080); return 15;            /* ADD IY,BC */
    case 0x0080: zp.e = rd(zp.iy, &w); NEXT(0x0083); return 19 + w;            /* LD E,(IY+0) */
    case 0x0083: zp.d = rd((uint16_t)(zp.iy + 1), &w); NEXT(0x0086); return 19 + w;   /* LD D,(IY+1) */
    case 0x0086: zp.a = rd(0x1FF7, &w); NEXT(0x0089); return 13;               /* LD A,($1FF7) */
    case 0x0089: logic(zp.a, 0); NEXT(0x008A); return 4;                       /* OR A */
    case 0x008A: JUMP_IF(zp.f & FS, 0x0092, 0x008D); return 10;                /* JP M,$0092 */
    case 0x008D: set_hl(word_at(0x1FF8, &w)); NEXT(0x0090); return 16;        /* LD HL,($1FF8) */
    case 0x0090: set_hl(add16(HL, DE)); NEXT(0x0091); return 11;               /* ADD HL,DE */
    case 0x0091: { uint16_t t = DE; set_de(HL); set_hl(t); } NEXT(0x0092); return 4;   /* EX DE,HL */
    case 0x0092: zp.c = rd((uint16_t)(zp.iy + 2), &w); NEXT(0x0095); return 19 + w;   /* LD C,(IY+2) */
    case 0x0095: zp.b = rd((uint16_t)(zp.iy + 3), &w); NEXT(0x0098); return 19 + w;   /* LD B,(IY+3) */
    case 0x0098: zp.a = rd((uint16_t)(zp.iy + 4), &w); NEXT(0x009B); return 19 + w;   /* LD A,(IY+4) */
    case 0x009B: zp.zc += 13 * CYCLE; wr(0x1FFE, zp.a); NEXT(0x009E); return 0;   /* LD ($1FFE),A: loops */
    case 0x009E: exx(); NEXT(0x009F); return 4;                                /* EXX */
    case 0x009F: zp.c = 0x80; NEXT(0x00A1); return 7;                          /* LD C,$80: the level */
    case 0x00A1: exx(); NEXT(0x00A2); return 4;                                /* EXX */

    /* a byte of the sample */
    case 0x00A2: set_hl(0x1FFF); NEXT(0x00A5); return 10;                      /* LD HL,$1FFF */
    case 0x00A5: zp.a = rd(DE, &w); NEXT(0x00A6); return 7 + w;                /* LD A,(DE) */
    case 0x00A6: logic(zp.a & 0xF0, 1); NEXT(0x00A8); return 7;                /* AND $F0 */
    case 0x00A8: case 0x00A9: case 0x00AA: case 0x00AB:                        /* RRCA x 4 */
        zp.f = (uint8_t)((zp.f & (FS | FZ | FPV)) | (zp.a & 1));
        zp.a = (uint8_t)(zp.a >> 1 | zp.a << 7);
        NEXT(zp.pc + 1); return 4;
    case 0x00AC: add8(0x1E); NEXT(0x00AE); return 7;                           /* ADD A,$1E */
    case 0x00AE: exx(); NEXT(0x00AF); return 4;                                /* EXX */
    case 0x00AF: zp.e = zp.a; NEXT(0x00B0); return 4;                          /* LD E,A */
    case 0x00B0: zp.a = rd(DE, &w); NEXT(0x00B1); return 7 + w;                /* LD A,(DE): the delta */
    case 0x00B1: add8(zp.c); NEXT(0x00B2); return 4;                           /* ADD A,C */
    case 0x00B2: zp.c = zp.a; NEXT(0x00B3); return 4;                          /* LD C,A */
    case 0x00B3: zp.a = 0x80; NEXT(0x00B5); return 7;                          /* LD A,$80 */
    case 0x00B5: zp.zc += 13 * CYCLE; wr(0x1FFD, zp.a); NEXT(0x00B8); return 0;   /* LD ($1FFD),A */
    case 0x00B8: zp.b = rd((uint16_t)(zp.iy + 11), &w); NEXT(0x00BB); return 19 + w;   /* LD B,(IY+11) */
    case 0x00BB: zp.zc += 12 * CYCLE; bit7(HL >= 0x4000 && HL < 0x6000 ? ym_status() : rd(HL, &w));   /* BIT 7,(HL) */
        NEXT(0x00BD); return 0;
    case 0x00BD: JUMP_IF(!(zp.f & FZ), 0x00BB, 0x00BF); return (zp.f & FZ) ? 7 : 12;   /* JR NZ,$00BB */
    case 0x00BF: zp.zc += 10 * CYCLE; wr(HL, 0x2A); NEXT(0x00C1); return 0;    /* LD (HL),$2A */
    case 0x00C1: set_hl((uint16_t)(HL + 1)); NEXT(0x00C2); return 6;           /* INC HL */
    case 0x00C2: logic(0, 0); NEXT(0x00C3); return 4;                          /* XOR A */
    case 0x00C3: zp.zc += 7 * CYCLE; wr(HL, zp.c); NEXT(0x00C4); return 0;     /* LD (HL),C: the level */
    case 0x00C4: zp.zc += 13 * CYCLE; wr(0x1FFD, zp.a); NEXT(0x00C7); return 0;   /* LD ($1FFD),A */
    case 0x00C7: set_hl((uint16_t)(HL - 1)); NEXT(0x00C8); return 6;           /* DEC HL */
    case 0x00C8: zp.b--; JUMP_IF(zp.b, 0x00C8, 0x00CA); return zp.b ? 13 : 8;   /* DJNZ $00C8 */
    case 0x00CA: exx(); NEXT(0x00CB); return 4;                                /* EXX */
    case 0x00CB: zp.a = rd(DE, &w); NEXT(0x00CC); return 7 + w;                /* LD A,(DE) */
    case 0x00CC: logic(zp.a & 0x0F, 1); NEXT(0x00CE); return 7;                /* AND $0F */
    case 0x00CE: add8(0x1E); NEXT(0x00D0); return 7;                           /* ADD A,$1E */
    case 0x00D0: exx(); NEXT(0x00D1); return 4;                                /* EXX */
    case 0x00D1: zp.e = zp.a; NEXT(0x00D2); return 4;                          /* LD E,A */
    case 0x00D2: zp.a = rd(DE, &w); NEXT(0x00D3); return 7 + w;                /* LD A,(DE) */
    case 0x00D3: add8(zp.c); NEXT(0x00D4); return 4;                           /* ADD A,C */
    case 0x00D4: zp.c = zp.a; NEXT(0x00D5); return 4;                          /* LD C,A */
    case 0x00D5: zp.a = 0x80; NEXT(0x00D7); return 7;                          /* LD A,$80 */
    case 0x00D7: zp.zc += 13 * CYCLE; wr(0x1FFD, zp.a); NEXT(0x00DA); return 0;   /* LD ($1FFD),A */
    case 0x00DA: zp.b = rd((uint16_t)(zp.iy + 11), &w); NEXT(0x00DD); return 19 + w;   /* LD B,(IY+11) */
    case 0x00DD: zp.zc += 12 * CYCLE; bit7(HL >= 0x4000 && HL < 0x6000 ? ym_status() : rd(HL, &w));   /* BIT 7,(HL) */
        NEXT(0x00DF); return 0;
    case 0x00DF: JUMP_IF(!(zp.f & FZ), 0x00DD, 0x00E1); return (zp.f & FZ) ? 7 : 12;   /* JR NZ,$00DD */
    case 0x00E1: zp.zc += 10 * CYCLE; wr(HL, 0x2A); NEXT(0x00E3); return 0;    /* LD (HL),$2A */
    case 0x00E3: set_hl((uint16_t)(HL + 1)); NEXT(0x00E4); return 6;           /* INC HL */
    case 0x00E4: logic(0, 0); NEXT(0x00E5); return 4;                          /* XOR A */
    case 0x00E5: zp.zc += 7 * CYCLE; wr(HL, zp.c); NEXT(0x00E6); return 0;     /* LD (HL),C */
    case 0x00E6: zp.zc += 13 * CYCLE; wr(0x1FFD, zp.a); NEXT(0x00E9); return 0;   /* LD ($1FFD),A */
    case 0x00E9: set_hl((uint16_t)(HL - 1)); NEXT(0x00EA); return 6;           /* DEC HL */
    case 0x00EA: zp.b--; JUMP_IF(zp.b, 0x00EA, 0x00EC); return zp.b ? 13 : 8;   /* DJNZ $00EA */
    case 0x00EC: exx(); NEXT(0x00ED); return 4;                                /* EXX */
    case 0x00ED: v = rd((uint16_t)(zp.iy + 5), &w); bit7(v); NEXT(0x00F1); return 20 + w;   /* BIT 7,(IY+5) */
    case 0x00F1: JUMP_IF(!(zp.f & FZ), 0x00F8, 0x00F3); return (zp.f & FZ) ? 7 : 12;   /* JR NZ,$00F8 */
    case 0x00F3: zp.zc += 12 * CYCLE; bit7(rd(HL, &w)); NEXT(0x00F5); return 0;   /* BIT 7,(HL): a request */
    case 0x00F5: JUMP_IF(!(zp.f & FZ), 0x0031, 0x00F8); return 10;            /* JP NZ,$0031 */
    case 0x00F8: set_de((uint16_t)(DE + 1)); NEXT(0x00F9); return 6;           /* INC DE */
    case 0x00F9: set_bc((uint16_t)(BC - 1)); NEXT(0x00FA); return 6;           /* DEC BC */
    case 0x00FA: zp.a = zp.c; NEXT(0x00FB); return 4;                          /* LD A,C */
    case 0x00FB: logic(zp.a | zp.b, 0); NEXT(0x00FC); return 4;                /* OR B */
    case 0x00FC: JUMP_IF(!(zp.f & FZ), 0x00A2, 0x00FF); return 10;            /* JP NZ,$00A2 */

    /* the sample's end: again from its loop point, or the DAC off */
    case 0x00FF: zp.a = rd(0x1FFE, &w); NEXT(0x0102); return 13;               /* LD A,($1FFE) */
    case 0x0102: logic(zp.a, 0); NEXT(0x0103); return 4;                       /* OR A */
    case 0x0103: JUMP_IF(zp.f & FZ, 0x0156, 0x0106); return 10;               /* JP Z,$0156 */
    case 0x0106: exx(); NEXT(0x0107); return 4;                                /* EXX */
    case 0x0107: JUMP_IF(!(zp.f & FS), 0x010F, 0x010A); return 10;            /* JP P,$010F */
    case 0x010A: logic(zp.a & 0x7F, 1); NEXT(0x010C); return 7;                /* AND $7F */
    case 0x010C: zp.zc += 19 * CYCLE; wr(zp.ix, zp.c); NEXT(0x010F); return 0;   /* LD (IX+0),C */
    case 0x010F:                                                               /* DEC A */
        zp.a--;
        zp.f = (uint8_t)((zp.f & FC) | (zp.a & FS) | (zp.a ? 0 : FZ) | 0x02);
        NEXT(0x0110); return 4;
    case 0x0110: zp.zc += 13 * CYCLE; wr(0x1FFE, zp.a); NEXT(0x0113); return 0;   /* LD ($1FFE),A */
    case 0x0113: JUMP_IF(zp.f & FZ, 0x0136, 0x0115); return (zp.f & FZ) ? 12 : 7;   /* JR Z,$0136 */
    case 0x0115: zp.c = rd(zp.ix, &w); NEXT(0x0118); return 19;                /* LD C,(IX+0) */
    case 0x0118: exx(); NEXT(0x0119); return 4;                                /* EXX */
    case 0x0119: zp.l = rd((uint16_t)(zp.iy + 6), &w); NEXT(0x011C); return 19 + w;   /* LD L,(IY+6) */
    case 0x011C: zp.h = rd((uint16_t)(zp.iy + 7), &w); NEXT(0x011F); return 19 + w;   /* LD H,(IY+7) */
    case 0x011F: zp.b = zp.h; NEXT(0x0120); return 4;                          /* LD B,H */
    case 0x0120: zp.c = zp.l; NEXT(0x0121); return 4;                          /* LD C,L */
    case 0x0121: zp.e = rd(zp.iy, &w); NEXT(0x0124); return 19 + w;            /* LD E,(IY+0) */
    case 0x0124: zp.d = rd((uint16_t)(zp.iy + 1), &w); NEXT(0x0127); return 19 + w;   /* LD D,(IY+1) */
    case 0x0127: set_hl(word_at(0x1FF8, &w)); NEXT(0x012A); return 16;        /* LD HL,($1FF8) */
    case 0x012A: set_hl(add16(HL, DE)); NEXT(0x012B); return 11;               /* ADD HL,DE */
    case 0x012B: zp.e = rd((uint16_t)(zp.iy + 2), &w); NEXT(0x012E); return 19 + w;   /* LD E,(IY+2) */
    case 0x012E: zp.d = rd((uint16_t)(zp.iy + 3), &w); NEXT(0x0131); return 19 + w;   /* LD D,(IY+3) */
    case 0x0131: set_hl(add16(HL, DE)); NEXT(0x0132); return 11;               /* ADD HL,DE */
    case 0x0132: { uint16_t t = DE; set_de(HL); set_hl(t); } NEXT(0x0133); return 4;   /* EX DE,HL */
    case 0x0133: NEXT(0x00A2); return 10;                                      /* JP $00A2 */
    case 0x0136: zp.c = rd(zp.ix, &w); NEXT(0x0139); return 19;                /* LD C,(IX+0) */
    case 0x0139: exx(); NEXT(0x013A); return 4;                                /* EXX */
    case 0x013A: zp.c = rd((uint16_t)(zp.iy + 8), &w); NEXT(0x013D); return 19 + w;   /* LD C,(IY+8) */
    case 0x013D: zp.b = rd((uint16_t)(zp.iy + 9), &w); NEXT(0x0140); return 19 + w;   /* LD B,(IY+9) */
    case 0x0140: zp.l = rd((uint16_t)(zp.iy + 2), &w); NEXT(0x0143); return 19 + w;   /* LD L,(IY+2) */
    case 0x0143: zp.h = rd((uint16_t)(zp.iy + 3), &w); NEXT(0x0146); return 19 + w;   /* LD H,(IY+3) */
    case 0x0146: zp.e = rd(zp.iy, &w); NEXT(0x0149); return 19 + w;            /* LD E,(IY+0) */
    case 0x0149: zp.d = rd((uint16_t)(zp.iy + 1), &w); NEXT(0x014C); return 19 + w;   /* LD D,(IY+1) */
    case 0x014C: set_hl(add16(HL, DE)); NEXT(0x014D); return 11;               /* ADD HL,DE */
    case 0x014D: set_de(word_at(0x1FF8, &w)); NEXT(0x0151); return 20;         /* LD DE,($1FF8) */
    case 0x0151: set_hl(add16(HL, DE)); NEXT(0x0152); return 11;               /* ADD HL,DE */
    case 0x0152: { uint16_t t = DE; set_de(HL); set_hl(t); } NEXT(0x0153); return 4;   /* EX DE,HL */
    case 0x0153: NEXT(0x00A2); return 10;                                      /* JP $00A2 */
    case 0x0156: set_hl(0x4000); NEXT(0x0159); return 10;                      /* LD HL,$4000 */
    case 0x0159: zp.zc += 10 * CYCLE; wr(HL, 0x2B); NEXT(0x015B); return 0;    /* LD (HL),$2B */
    case 0x015B: set_hl((uint16_t)(HL + 1)); NEXT(0x015C); return 6;           /* INC HL */
    case 0x015C: zp.zc += 10 * CYCLE; wr(HL, 0x00); NEXT(0x015E); return 0;    /* LD (HL),0: the DAC off */
    case 0x015E: set_hl(0x1FF6); NEXT(0x0161); return 10;                      /* LD HL,$1FF6 */
    case 0x0161: zp.a = rd(HL, &w); NEXT(0x0162); return 7;                    /* LD A,(HL) */
    case 0x0162: logic(zp.a, 0); NEXT(0x0163); return 4;                       /* OR A */
    case 0x0163: JUMP_IF(zp.f & FS, 0x002E, 0x0166); return 10;               /* JP M,$002E */
    case 0x0166: logic(0, 0); NEXT(0x0167); return 4;                          /* XOR A */
    case 0x0167: zp.zc += 7 * CYCLE; wr(HL, zp.a); NEXT(0x0168); return 0;     /* LD (HL),A */
    case 0x0168: NEXT(0x002E); return 10;                                      /* JP $002E */
    default:     NEXT(0x0000); return 4;                  /* (no other address runs) */
    }
}

/* run the Z80 until it has caught up with the client's time */
static void catch_up(void)
{
    while (zp.running && !zp.bus_taken && zp.zc < zp.now)
        zp.zc += instruction() * CYCLE;
}

void z80_player_run(uint32_t mclk)
{
    zp.now += mclk;
    catch_up();
}

void z80_player_reset(uint16_t v)
{
    if (v & 0x100) {
        if (zp.running) return;
        zp.running = 1;
        zp.pc = 0;
        zp.ym_free = 0;
        if (!zp.bus_taken) zp.zc = (zp.now + CYCLE - 1) / CYCLE * CYCLE;
        return;
    }
    zp.running = 0;
}

void z80_player_busreq(uint16_t v)
{
    if (v & 0x100) {
        zp.bus_taken = 1;
        return;
    }
    if (zp.bus_taken && zp.running) zp.zc = (zp.now + CYCLE - 1) / CYCLE * CYCLE;
    zp.bus_taken = 0;
}

void z80_player_frame(void) { zp.frame = zp.now; }

void z80_player_power_on(void)
{
    memset(z80_mem, 0, sizeof z80_mem);
    memset(&zp, 0, sizeof zp);
}
