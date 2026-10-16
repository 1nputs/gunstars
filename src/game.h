/* Gunstar Heroes in C: the ported routines, by the part of the game they belong to. Each is the
 * 68k routine of the same name (tools/disasm/names.txt), its address in the comment, with the
 * registers it takes and returns noted as (d0, a0 -> d0). */
#pragma once
#include <stdint.h>
#include "mem.h"
#include "hal.h"
#include "symbols_gen.h"

/* ---- the vertical blank (vblank.c) ---- */
void CheckPause(void);                   /* $000C36 */

/* ---- H-interrupt effects (hint.c) ---- */
void HintNone(void);                     /* $001440 */

/* ---- dispatch (dispatch.c): a jsr / jmp through an address the game computed ---- */
void call68k(uint32_t addr);
void call68k_missing(uint32_t addr);     /* the tests' recorded call; natively a fatal error */
