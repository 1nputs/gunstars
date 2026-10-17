/* hal.h for the tests: reads come from the record, writes are checked against it in order. */
#include "hal.h"
#include "replay.h"

uint16_t vdp_status(void) { return (uint16_t)replay_io_read(VDP_CTRL_PORT, 2); }
uint16_t vdp_read(void) { return (uint16_t)replay_io_read(VDP_DATA_PORT, 2); }
void vdp_ctrl(uint32_t v) { replay_io_write(VDP_CTRL_PORT, 4, v); }
void vdp_ctrl16(uint16_t v) { replay_io_write(VDP_CTRL_PORT, 2, v); }
void vdp_data(uint16_t v) { replay_io_write(VDP_DATA_PORT, 2, v); }
void vdp_data32(uint32_t v) { replay_io_write(VDP_DATA_PORT, 4, v); }

void psg_write(uint8_t v) { replay_io_write(PSG_PORT, 1, v); }
uint8_t ym_read8(uint16_t off) { return (uint8_t)replay_io_read(YM_BASE + off, 1); }
void ym_write8(uint16_t off, uint8_t v) { replay_io_write(YM_BASE + off, 1, v); }
uint8_t z80_ram_read8(uint16_t off) { return (uint8_t)replay_io_read(Z80_RAM_BASE + off, 1); }
void z80_ram_write8(uint16_t off, uint8_t v) { replay_io_write(Z80_RAM_BASE + off, 1, v); }
void z80_ram_write32(uint16_t off, uint32_t v) { replay_io_write(Z80_RAM_BASE + off, 4, v); }
void z80_busreq(uint16_t v) { replay_io_write(Z80_BUSREQ, 2, v); }
uint8_t z80_busack(void) { return (uint8_t)replay_io_read(Z80_BUSREQ, 1); }
void z80_reset(uint16_t v) { replay_io_write(Z80_RESET, 2, v); }

uint8_t io_read8(uint32_t addr) { return (uint8_t)replay_io_read(addr, 1); }
void io_write8(uint32_t addr, uint8_t v) { replay_io_write(addr, 1, v); }
uint16_t io_read16(uint32_t addr) { return (uint16_t)replay_io_read(addr, 2); }
uint32_t io_read32(uint32_t addr) { return replay_io_read(addr, 4); }
void tmss_write(uint32_t v) { (void)v; }                 /* (the shim does not log $A14000) */

/* The status register: as the record's call had it, then as the C sets it. Each write is
 * checked against the 68k's (the shim logs them as writes to SR_PSEUDO): an interrupt the C
 * code does not wait for is applied at the first write after it, so interrupts land between
 * the same masked sections as on the 68k. */
static uint16_t sr = 0x2700;
void hal_replay_sr(uint16_t v) { sr = v; }
uint16_t cpu_get_sr(void) { return sr; }
void cpu_set_sr(uint16_t v) { sr = v; replay_io_write(SR_PSEUDO, 2, v); }

/* a wait for the vertical blank: the one the 68k took in its wait loop, replayed. With none
 * left the C would wait for ever: the record is abandoned (a failure). */
void cpu_spin(uint32_t loops)
{
    (void)loops;                     /* nothing to replay: no access, only time */
}

void hal_wait_vblank(void)
{
    if (replay_irq()) return;
    replay_fail("a vertical-blank wait with no recorded interrupt");
    replay_abandon();
}
