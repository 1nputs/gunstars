/* hal.h on pulseman: the VDP, PSG, YM2612 and pads are the runtime's.
 *
 * The 68k's own sound driver writes the YM2612 directly (holding the Z80's bus); the Z80 only
 * plays samples, and its program is not ported yet, so Z80 RAM is kept but nothing plays it.
 * Writes are put at the runtime's position in the frame (mdrt_now): the game's code moves no
 * clock, so they land at the frame's start, where the vertical blank runs the sound driver. */
#include <stdio.h>
#include "game.h"
#include "hal.h"
#include "native.h"

mdrt *native_m;
static uint16_t sr = 0x2700;            /* the 68k's status register: its interrupt mask */
static int bus_taken;                   /* the 68k holds the Z80's bus */
static uint8_t z80_ram[0x2000];
static long frames;

static mdrt_vdp *vdp(void) { return mdrt_get_vdp(native_m); }
static uint32_t now(void) { return mdrt_now(native_m); }

static void hint(void *user) { (void)user; HintHandler(); }

void native_hal_start(mdrt *m)
{
    native_m = m;
    mdrt_set_hint(m, hint, NULL);
    sr = 0x2700;
    bus_taken = 0;
    frames = 0;
    for (int i = 0; i < 0x2000; i++) z80_ram[i] = 0;
}

long native_frames(void) { return frames; }

uint16_t vdp_status(void) { return mdrt_vdp_status(vdp()); }
uint16_t vdp_read(void) { return mdrt_vdp_read16(vdp()); }
void vdp_ctrl(uint32_t v) { mdrt_vdp_ctrl32(vdp(), v); }
void vdp_ctrl16(uint16_t v) { mdrt_vdp_ctrl16(vdp(), v); }
void vdp_data(uint16_t v) { mdrt_vdp_data16(vdp(), v); }
void vdp_data32(uint32_t v) { mdrt_vdp_data32(vdp(), v); }

void psg_write(uint8_t v) { mdrt_psg_port(native_m, now(), v); }
uint8_t ym_read8(uint16_t off) { (void)off; return 0; }                  /* never busy */
void ym_write8(uint16_t off, uint8_t v) { mdrt_ym_write(native_m, now(), off & 3, v); }

uint8_t z80_ram_read8(uint16_t off) { return z80_ram[off & 0x1FFF]; }
void z80_ram_write8(uint16_t off, uint8_t v) { z80_ram[off & 0x1FFF] = v; }
void z80_ram_write32(uint16_t off, uint32_t v)
{
    for (int i = 0; i < 4; i++) z80_ram[(off + i) & 0x1FFF] = (uint8_t)(v >> (24 - 8 * i));
}
void z80_busreq(uint16_t v) { bus_taken = (v & 0x100) != 0; }
uint8_t z80_busack(void) { return bus_taken ? 0 : 1; }                  /* granted at once */
void z80_reset(uint16_t v) { if (!(v & 0x100)) mdrt_ym_reset(native_m, now()); }

uint8_t io_read8(uint32_t addr) { return mdrt_pad_read(mdrt_get_pads(native_m), addr | 1); }
void io_write8(uint32_t addr, uint8_t v) { mdrt_pad_write(mdrt_get_pads(native_m), addr | 1, v); }
static uint16_t io16(uint32_t addr) { uint8_t b = io_read8(addr); return (uint16_t)(b << 8 | b); }
uint16_t io_read16(uint32_t addr) { return io16(addr); }
uint32_t io_read32(uint32_t addr) { return (uint32_t)io16(addr) << 16 | io16(addr + 2); }
void tmss_write(uint32_t v) { (void)v; }                                 /* (no TMSS: the VDP is unlocked) */

uint16_t cpu_get_sr(void) { return sr; }
void cpu_set_sr(uint16_t v) { sr = v; }

/* a busy loop: the time it takes (dbf, ~10 cycles a turn) */
void cpu_spin(uint32_t loops) { mdrt_charge(native_m, loops * 10); }

/* the frame's end: the handshake, then the next frame's vertical blank interrupt */
void hal_wait_vblank(void)
{
    if (mdrt_wait_vblank(native_m)) native_quit();
    native_frame_done(++frames);
    if (mdrt_vdp_reg(vdp(), 1) & 0x20) VBlank();
    mdrt_vblank_end(native_m);
}
