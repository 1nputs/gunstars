/* The ported routines and their test adapters (ports.c). */
#pragma once
#include "replay.h"
typedef struct { uint32_t pc; const char *name; void (*run)(const Record *r); } Port;
extern const Port ports[];
/* labels inside a ported routine that the 68k reaches only by branching (no C function of their
 * own): their recorded "calls" are the routine's own work */
extern const uint32_t inner_labels[];
void expect_reg16(const Record *r, int reg, uint32_t got, const char *what);
void expect_reg32(const Record *r, int reg, uint32_t got, const char *what);
