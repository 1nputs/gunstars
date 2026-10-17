/* Helpers for the drafts tools/disasm/draft.py writes (build/draft/): the 68000's exact semantics
 * where plain C needs more than a cast. Drafts are never committed: the hand rewrite of a routine
 * removes every one of these (PLAN.md, how routines are written). */
#pragma once
#include <stdint.h>
#include "game.h"

/* callers a routine that popped return addresses skips on its way out (an rts with the stack
 * +4 returns past its caller): each call site of such a routine returns while it is set */
__attribute__((weak)) int draft_unwind;

/* part of a data register written, the rest kept */
#define SETB(r, v) ((r) = ((r) & 0xFFFFFF00u) | (uint8_t)(v))
#define SETW(r, v) ((r) = ((r) & 0xFFFF0000u) | (uint16_t)(v))

/* the condition codes as the 68000 keeps them: X N Z V C in bits 4-0 */
enum { CF = 1, VF = 2, ZF = 4, NF = 8, XF = 16 };
static inline uint32_t msb(int bits) { return 1u << (bits - 1); }
static inline uint32_t mask(int bits) { return bits == 32 ? 0xFFFFFFFFu : (1u << bits) - 1; }
static inline uint8_t ccr_nz(uint32_t r, int bits)
{
    r &= mask(bits);
    return (uint8_t)((r & msb(bits) ? NF : 0) | (r == 0 ? ZF : 0));
}
/* move, tst, and, or, eor, not, clr, ext, swap, mul: V and C clear */
static inline uint8_t ccr_logic(uint8_t x, uint32_t r, int bits) { return (uint8_t)((x & XF) | ccr_nz(r, bits)); }
/* cmp d - s (X kept) */
static inline uint8_t ccr_cmp(uint8_t x, uint32_t d, uint32_t s, int bits)
{
    uint32_t r = (d - s) & mask(bits);
    d &= mask(bits); s &= mask(bits);
    uint8_t c = (uint8_t)(ccr_nz(r, bits) | (s > d ? CF : 0) | (((d ^ s) & (d ^ r)) & msb(bits) ? VF : 0));
    return (uint8_t)((x & XF) | c);
}
/* sub d - s: X = C */
static inline uint8_t ccr_sub(uint32_t d, uint32_t s, int bits)
{
    uint8_t c = ccr_cmp(0, d, s, bits);
    return (uint8_t)(c | (c & CF ? XF : 0));
}
/* add d + s: X = C */
static inline uint8_t ccr_add(uint32_t d, uint32_t s, int bits)
{
    uint32_t r = (d + s) & mask(bits);
    d &= mask(bits); s &= mask(bits);
    int carry = bits == 32 ? r < d : ((d + s) >> bits) & 1;
    uint8_t c = (uint8_t)(ccr_nz(r, bits) | (carry ? CF | XF : 0) | ((~(d ^ s) & (d ^ r)) & msb(bits) ? VF : 0));
    return c;
}
#define CC_T(c)  1
#define CC_F(c)  0
#define CC_HI(c) (!((c) & (CF | ZF)))
#define CC_LS(c) (((c) & (CF | ZF)) != 0)
#define CC_CC(c) (!((c) & CF))
#define CC_CS(c) (((c) & CF) != 0)
#define CC_NE(c) (!((c) & ZF))
#define CC_EQ(c) (((c) & ZF) != 0)
#define CC_VC(c) (!((c) & VF))
#define CC_VS(c) (((c) & VF) != 0)
#define CC_PL(c) (!((c) & NF))
#define CC_MI(c) (((c) & NF) != 0)
#define CC_GE(c) (!!((c) & NF) == !!((c) & VF))
#define CC_LT(c) (!!((c) & NF) != !!((c) & VF))
#define CC_GT(c) (!((c) & ZF) && CC_GE(c))
#define CC_LE(c) (((c) & ZF) || CC_LT(c))

/* shifts by a count (a register's low six bits): counts past the width as the 68000 does */
static inline uint32_t lsl_n(uint32_t v, int n, int bits) { return n >= bits ? 0 : (v << n) & mask(bits); }
static inline uint32_t lsr_n(uint32_t v, int n, int bits) { return n >= bits ? 0 : (v & mask(bits)) >> n; }
static inline uint32_t asr_n(uint32_t v, int n, int bits)
{
    int32_t s = (int32_t)((v & mask(bits)) << (32 - bits)) >> (32 - bits);
    return (uint32_t)(n >= bits ? (s < 0 ? -1 : 0) : s >> n) & mask(bits);
}
static inline uint32_t rol_n(uint32_t v, int n, int bits)
{
    v &= mask(bits); n %= bits;
    return n ? ((v << n) | (v >> (bits - n))) & mask(bits) : v;
}
static inline uint32_t ror_n(uint32_t v, int n, int bits) { n %= bits; return rol_n(v, n ? bits - n : 0, bits); }
/* the last bit a shift left or right by n (1 to bits) moves out: C and X */
static inline int out_left(uint32_t v, int n, int bits) { return n > bits ? 0 : (v >> (bits - n)) & 1; }
static inline int out_right(uint32_t v, int n, int bits) { return n > bits ? 0 : (v >> (n - 1)) & 1; }

/* divu.w / divs.w: quotient in the low word, remainder in the high; on overflow d is unchanged.
 * Division by zero traps on the 68000; the game never divides by zero */
static inline uint32_t divu_w(uint32_t d, uint16_t s)
{
    uint32_t q = d / s, r = d % s;
    return q > 0xFFFF ? d : (r << 16) | q;
}
static inline uint32_t divs_w(uint32_t d, int16_t s)
{
    int32_t q = (int32_t)d / s, r = (int32_t)d % s;
    return q > 32767 || q < -32768 ? d : ((uint32_t)(uint16_t)r << 16) | (uint16_t)q;
}

/* abcd / sbcd: packed decimal with X; Z cleared by a nonzero result, kept otherwise */
static inline uint8_t abcd(uint8_t d, uint8_t s, uint8_t *ccr)
{
    int x = (*ccr & XF) != 0;
    unsigned lo = (d & 15) + (s & 15) + x, r = (d & 0xF0) + (s & 0xF0) + lo;
    if (lo > 9) r += 6;
    int c = r > 0x99;
    if (c) r += 0x60;
    *ccr = (uint8_t)((*ccr & ZF & ((r & 0xFF) ? 0 : ZF)) | (c ? CF | XF : 0) | (r & 0x80 ? NF : 0));
    return (uint8_t)r;
}
static inline uint8_t sbcd(uint8_t d, uint8_t s, uint8_t *ccr)
{
    int x = (*ccr & XF) != 0;
    int lo = (d & 15) - (s & 15) - x, r = (d & 0xF0) - (s & 0xF0) + lo;
    if (lo < 0) r -= 6;
    int c = r < 0;
    if (c) r -= 0x60;
    *ccr = (uint8_t)((*ccr & ZF & ((r & 0xFF) ? 0 : ZF)) | (c ? CF | XF : 0) | (r & 0x80 ? NF : 0));
    return (uint8_t)r;
}

/* roxl / roxr by n through X: C and X the last bit out, N and Z of the result, V clear */
static inline uint32_t rox_n(uint32_t v, int n, int bits, int left, uint8_t *ccr)
{
    uint32_t x = (*ccr & XF) != 0;
    v &= mask(bits);
    for (int i = 0; i < n; i++) {
        uint32_t out = left ? (v >> (bits - 1)) & 1 : v & 1;
        v = left ? ((v << 1) | x) & mask(bits) : (v >> 1) | (x << (bits - 1));
        x = out;
    }
    *ccr = (uint8_t)((n ? (x ? CF | XF : 0) : (*ccr & XF) | (x ? CF : 0)) | ccr_nz(v, bits));
    return v;
}
