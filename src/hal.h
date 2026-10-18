/* The hardware the game touches. One function per 68k access of the width it names, in the
 * order the 68k makes them: the tests check every write against the capture's, in order and
 * width (test/hal_replay.c); the native build maps them onto pulseman (native/hal_native.c). */
#pragma once
#include <stdint.h>

#define VDP_DATA_PORT 0xC00000u
#define VDP_CTRL_PORT 0xC00004u
#define PSG_PORT      0xC00011u
#define Z80_RAM_BASE  0xA00000u      /* Z80 RAM; $A04000-$A04003 is the YM2612 */
#define YM_BASE       0xA04000u
#define IO_VERSION    0xA10001u
#define PAD1_DATA     0xA10003u
#define PAD2_DATA     0xA10005u
#define PAD1_CTRL     0xA10009u
#define PAD2_CTRL     0xA1000Bu
#define Z80_BUSREQ    0xA11100u
#define Z80_RESET     0xA11200u
#define TMSS_REG      0xA14000u

/* VDP */
uint16_t vdp_status(void);           /* move.w (VDP_CTRL).l: the status word */
uint16_t vdp_read(void);             /* move.w (VDP_DATA).l */
void vdp_ctrl(uint32_t v);           /* move.l to the control port (a command, or two register writes) */
void vdp_ctrl16(uint16_t v);         /* move.w to the control port */
void vdp_data(uint16_t v);           /* move.w to the data port */
void vdp_data32(uint32_t v);         /* move.l to the data port */

/* sound */
void psg_write(uint8_t v);           /* move.b to $C00011 */
uint8_t ym_read8(uint16_t off);      /* move.b from $A04000 + off (the busy flag), the Z80's bus held */
void ym_write8(uint16_t off, uint8_t v);       /* move.b to $A04000 + off */
uint8_t z80_ram_read8(uint16_t off);           /* move.b from Z80 RAM, the bus held */
void z80_ram_write8(uint16_t off, uint8_t v);  /* move.b to Z80 RAM */
void z80_ram_write32(uint16_t off, uint32_t v); /* move.l to Z80 RAM (the boot's clear) */
void z80_busreq(uint16_t v);         /* move.w to $A11100: $100 takes the Z80's bus, 0 gives it back */
uint8_t z80_busack(void);            /* btst #0,($A11100).l reads this byte: bit 0 set while the Z80 has it */
void z80_reset(uint16_t v);          /* move.w to $A11200: 0 holds the Z80 (and the YM2612) in reset */

/* the I/O chip */
uint8_t io_read8(uint32_t addr);     /* move.b from $A10001-$A1001F (version, pad data and control) */
void io_write8(uint32_t addr, uint8_t v);
uint16_t io_read16(uint32_t addr);   /* tst.w at boot */
uint32_t io_read32(uint32_t addr);   /* tst.l at boot */
void tmss_write(uint32_t v);         /* move.l 'SEGA' to $A14000 */

/* the 68k's status register: its interrupt mask (move #x,sr / move sr,-(sp) / move (sp)+,sr) */
uint16_t cpu_get_sr(void);
void cpu_set_sr(uint16_t sr);

/* a wait for the vertical blank: the 68k spun on something the interrupt changes */
void hal_wait_vblank(void);
/* MainLoop's (the job list run, nothing under way): natively, where a save state is taken and
 * where the game goes on from after one is loaded */
void hal_wait_vblank_main(void);

/* a busy loop that only takes time: `loops` turns of a dbf (about 10 cycles each). With the
 * interrupts masked, the vertical blanks it spans are lost but one (stage 5's ~21 frames) */
void cpu_spin(uint32_t loops);
