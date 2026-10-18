/* The Z80's sample player ported to C (z80.c). */
#pragma once
#include <stdint.h>

struct Z80Player {
    /* time, in master clocks (15 a Z80 cycle) since the player's clock started */
    uint64_t now;           /* the client's time: how far z80_player_run has been told */
    uint64_t zc;            /* the Z80's: the end of the instruction it last ran */
    uint64_t frame;         /* the start of the frame (the YM2612 busy flag's 42-clock grid) */
    uint64_t ym_free;       /* the end of the YM2612's busy flag */
    uint8_t running;        /* out of reset */
    uint8_t bus_taken;      /* the 68k holds the bus: the Z80 waits */
    uint16_t pc, sp, ix, iy;
    uint8_t a, f, b, c, d, e, h, l;
    uint8_t b2, c2, d2, e2, h2, l2;     /* the other set (EXX) */
    uint32_t bank;          /* the bank window's 68k address (bits 15-23) */
};
extern uint8_t z80_mem[0x2000];
extern struct Z80Player zp;

/* the platform's YM2612: a port write (0 / 1 part I's address / data, 2 / 3 part II's) at zp.zc */
extern void (*z80_player_ym)(int port, uint8_t value);

void z80_player_run(uint32_t mclk);         /* time went by (master clocks) */
void z80_player_reset(uint16_t v);          /* the 68k wrote $A11200: $100 runs the program from 0 */
void z80_player_busreq(uint16_t v);         /* the 68k wrote $A11100: $100 takes the bus */
void z80_player_frame(void);                /* a frame starts (now) */
void z80_player_ym_busy(uint64_t t);        /* a data write to the YM2612 at t (the 68k's too) */
void z80_player_power_on(void);
