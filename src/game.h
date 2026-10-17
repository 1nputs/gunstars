/* Gunstar Heroes in C: the ported routines, by the part of the game they belong to. Each is the
 * 68k routine of the same name (tools/disasm/names.txt). The comment after each prototype gives
 * its address and registers, "$003156 a2 -> d0.b": the registers its parameters were (results
 * aside), then its results (the return value, then the pointer parameters), a flag Z / N / C
 * when it returns one. The tests' adapters are generated from these (tools/gen_ports.py); a
 * comment with "manual" has its adapter in test/ports.c. */
#pragma once
#include <stdint.h>
#include "mem.h"
#include "hal.h"
#include "symbols_gen.h"

/* VDP register n's word in v_vdpRegs ($8rvv; its value at +1) */
#define VDP_REG(n) (v_vdpRegs + 2 * (n))

/* Registers the 68k keeps from one routine to another, kept in C globals (states carry them) */
extern uint32_t hint_data;     /* a6: the table the H-interrupt handler reads (HintEffect sets it) */

/* ---- the vertical blank (vblank.c) ---- */
void CheckPause(void);                                               /* $000C36 */

/* ---- the VDP and the palette (vdp.c) ---- */
void VBlankDma(void);                                                /* $000D36 */
void VdpWriteRegs(void);                                             /* $000F4C */
void VdpWriteReg0(void);                                             /* $000F9C */
void FadePalette(void);                                              /* $000FB4 */
void QueueHScroll(void);                                             /* $0012F4 */
void QueueVScroll(void);                                             /* $00134A */
uint32_t Random(void);                                               /* $003630 -> d0 */
void QueueDma(uint32_t command, uint32_t src, uint32_t length_regs); /* (a helper: a DMA into the queue) */

/* ---- background graphics: the job list, command lists, the decoders (jobs.c) ---- */
void RunCommands(uint32_t list);                                     /* $0022DE a0 */
uint32_t CmdCopyToRam(uint32_t list);                                /* $002310 a0 -> a0 */
uint32_t CmdCopyToVram(uint32_t list);                               /* $002326 a0 -> a0 */
uint32_t CmdDecodeToRam(uint32_t list);                              /* $002374 a0 -> a0 */
uint32_t CmdDecodeToVram(uint32_t list);                             /* $0023A0 a0 -> a0 */
uint32_t CmdRleToRam(uint32_t list);                                 /* $0023CC a0 -> a0 */
uint32_t CmdRleToVram(uint32_t list);                                /* $0023E6 a0 -> a0 */
uint16_t NextJob(uint32_t *list);                                    /* $002478 -> d0.w, a0 */
void RunJobs(void);                                                  /* $002506 */
void JobCopyToRam(void);                                             /* $00254C */
void JobCopyToVram(void);                                            /* $002562 */
void JobDecodeToRam(void);                                           /* $00258A */
void JobDecodeToVram(void);                                          /* $0025A6 */
void JobRleToRam(void);                                              /* $0025FC */
void JobRleToVram(void);                                             /* $002616 */
void RleChunk(uint32_t end, uint32_t *src, uint32_t *dst);           /* $002660 a4 -> a1, a2 */
void RleRun(uint32_t *src, uint32_t *dst);                           /* $00267E -> a1, a2 */
void ClearTilePixels(void);                                          /* $002702 */
uint32_t DecodeTile(uint32_t src);                                   /* $002712 a1 -> a1 */
uint32_t PackTile(uint32_t dst);                                     /* $002886 a2 -> a2 */
uint32_t WriteTileVram(uint32_t vram);                               /* $0028B2 a3 -> a3 */
void QueueTileDma(uint32_t bytes, uint32_t *src, uint32_t *vram);    /* $00290C d1 -> a2, a3 */

/* ---- H-interrupt effects (hint.c) ---- */
void HintEffect(void);                                               /* $001416 a6=hint_data -> a6=hint_data */
void HintNone(void);                                                 /* $001440 */
void HintEffect1(void);                                              /* $001462 -> a6=hint_data */
void HintEffect3(void);                                              /* $0014A6 -> a6=hint_data */
void HintEffect4(void);                                              /* $0014EA -> a6=hint_data */
void HintEffect2(void);                                              /* $001534 -> a6=hint_data */
void HintEffect5(void);                                              /* $0015C8 */
void HintEffect6(void);                                              /* $00161A -> a6=hint_data */
void HintEffect7(void);                                              /* $00165E -> a6=hint_data */
void HintShiftLines(uint16_t end);                                   /* $00176E d0.w */

/* ---- the pads (pads.c) ---- */
void ReadPads(void);                                                 /* $003066 */
void ReadPad1(void);                                                 /* $003094 */
void ReadPad2(void);                                                 /* $0030B6 */
uint8_t RemapButtons(uint8_t buttons, uint16_t map);                 /* $00311E d0.b, a3 -> d0.b */
uint8_t CopyBit(uint8_t buttons, uint8_t raw, uint8_t to, uint8_t from);   /* $003148 d0.b, d1.b, d2.b, d3.b -> d0.b */
uint8_t PadId(uint32_t port);                                        /* $003156 a2 -> d0.b */

/* ---- dispatch (dispatch.c): a jsr / jmp through an address the game computed ---- */
void call68k(uint32_t addr);
void call68k_missing(uint32_t addr);     /* the tests' recorded call; natively a fatal error */
