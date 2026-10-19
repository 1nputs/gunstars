/* hal.h on pulseman: the VDP, PSG, YM2612 and pads are the runtime's; the Z80 is src/z80.c, the
 * sample player ported to C, whose YM2612 writes go to the runtime's chip at their time.
 *
 * The 68k's own sound driver writes the YM2612 directly (holding the Z80's bus). Time: the
 * runtime's position in the frame (mdrt_now), plus a little for each of the game's accesses to the
 * Z80's bus (the game's code moves no clock of its own): when the game waits on the Z80 (its busy
 * flag at $1FFD, before it writes the chip) the player runs on meanwhile, as beside a real 68k.
 * The runtime's sound tick runs the player through the rest of the frame. */
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "hal.h"
#include "native.h"
#include "z80.h"

mdrt *native_m;
static uint16_t sr = 0x2700;            /* the 68k's status register: its interrupt mask */
static long frames;
static uint64_t frame_base;             /* the player's master clock at this frame's start */
static uint32_t t_bus;                  /* master clocks of the game's Z80 bus accesses this frame */
static int at_main;                     /* in MainLoop's wait */
static long handshakes;                 /* (this session's: not in a state) */

#define BUS_ACCESS 56                   /* a 68k byte access through the Z80's bus, with its instruction */
/* the 68k's time from one YM2612 write to the next, as the oracle's sound traces show it (the
 * driver's write routine): about 250 master clocks from a register's address to its data, 1,500
 * to 3,700 from the data to the next address. Written all at once, a key off and the key on after
 * it fall in one of the chip's samples and the note is not struck again. */
#define YM_ADDRESS_TIME 250
#define YM_DATA_TIME 2100

static mdrt_vdp *vdp(void) { return mdrt_get_vdp(native_m); }
static uint32_t now(void) { return mdrt_now(native_m) + t_bus; }

static void run_to(uint64_t t)
{
    while (t > zp.now) {
        uint64_t step = t - zp.now > 1u << 30 ? 1u << 30 : t - zp.now;
        z80_player_run((uint32_t)step);
    }
}

/* the player catches up with the game, after the access's time */
static void z80_now(void)
{
    t_bus += BUS_ACCESS;
    run_to(frame_base + now());
}

/* the player's YM2612 writes, to the runtime's chip at their time in the frame */
static void player_ym(int port, uint8_t value)
{
    mdrt_ym_write(native_m, zp.zc > frame_base ? (uint32_t)(zp.zc - frame_base) : 0, port, value);
}

/* the runtime's sound tick: the player through the frame */
static void sound_tick(void *user, uint32_t until)
{
    (void)user;
    run_to(frame_base + until);
}

static void hint(void *user) { (void)user; HintHandler(); }

void native_hal_start(mdrt *m)
{
    native_m = m;
    mdrt_set_hint(m, hint, NULL);
    mdrt_set_sound_tick(m, sound_tick, NULL);
    sr = 0x2700;
    frames = 0;
    at_main = 0;
    frame_base = 0;
    t_bus = 0;
    z80_player_power_on();
    z80_player_ym = player_ym;
}

long native_frames(void) { return frames; }
long native_handshakes(void) { return handshakes; }

/* a state's: the status register, the frame count, the clock */
#define HAL_STATE (2 + 8 + 8 + 4)
size_t native_hal_save(uint8_t *out)
{
    if (out) {
        int64_t f = frames;
        memcpy(out, &sr, 2);
        memcpy(out + 2, &f, 8);
        memcpy(out + 10, &frame_base, 8);
        memcpy(out + 18, &t_bus, 4);
    }
    return HAL_STATE;
}
int native_hal_load(const uint8_t *in, size_t size)
{
    if (size != HAL_STATE) return -1;
    int64_t f;
    memcpy(&sr, in, 2);
    memcpy(&f, in + 2, 8);
    memcpy(&frame_base, in + 10, 8);
    memcpy(&t_bus, in + 18, 4);
    frames = (long)f;
    return 0;
}

uint16_t vdp_status(void) { return mdrt_vdp_status(vdp()); }
uint16_t vdp_read(void) { return mdrt_vdp_read16(vdp()); }
void vdp_ctrl(uint32_t v) { mdrt_vdp_ctrl32(vdp(), v); }
void vdp_ctrl16(uint16_t v) { mdrt_vdp_ctrl16(vdp(), v); }
void vdp_data(uint16_t v) { mdrt_vdp_data16(vdp(), v); }
void vdp_data32(uint32_t v) { mdrt_vdp_data32(vdp(), v); }

void psg_write(uint8_t v) { mdrt_psg_port(native_m, now(), v); }
uint8_t ym_read8(uint16_t off) { (void)off; return 0; }                  /* never busy */
void ym_write8(uint16_t off, uint8_t v)
{
    z80_now();
    mdrt_ym_write(native_m, now(), off & 3, v);
    if (off & 1) z80_player_ym_busy(frame_base + now());   /* (the player polls the busy flag) */
    t_bus += (off & 1) ? YM_DATA_TIME - BUS_ACCESS : YM_ADDRESS_TIME - BUS_ACCESS;
}

uint8_t z80_ram_read8(uint16_t off) { z80_now(); return z80_mem[off & 0x1FFF]; }
void z80_ram_write8(uint16_t off, uint8_t v) { z80_now(); z80_mem[off & 0x1FFF] = v; }
void z80_ram_write32(uint16_t off, uint32_t v)
{
    z80_now();
    for (int i = 0; i < 4; i++) z80_mem[(off + i) & 0x1FFF] = (uint8_t)(v >> (24 - 8 * i));
}
void z80_busreq(uint16_t v) { z80_now(); z80_player_busreq(v); }
uint8_t z80_busack(void) { z80_now(); return zp.bus_taken ? 0 : 1; }    /* granted at once */
void z80_reset(uint16_t v)
{
    z80_now();
    if (!(v & 0x100)) mdrt_ym_reset(native_m, now());
    z80_player_reset(v);
}

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
    uint32_t len = mdrt_frame_mclks(native_m), end = now();
    int main_wait = at_main;
    at_main = 0;
    if (mdrt_wait_vblank(native_m)) native_quit();
    handshakes++;
    run_to(frame_base + len);                             /* (if the runtime's tick has not) */
    frame_base += len;
    t_bus = end > len ? end - len : 0;                    /* (the game's writes ran past the frame) */
    z80_player_frame();
    native_frame_done(++frames);
    if (mdrt_vdp_reg(vdp(), 1) & 0x20) VBlank();
    mdrt_vblank_end(native_m);
    native_net_wait(main_wait);                           /* (netplay: a rollback goes on from MainLoop) */
    native_states_wait(main_wait);                        /* (a state saved or loaded: state.c) */
}

/* MainLoop's wait: where states are taken and the game goes on from after a load */
void hal_wait_vblank_main(void)
{
    at_main = 1;
    hal_wait_vblank();
}
