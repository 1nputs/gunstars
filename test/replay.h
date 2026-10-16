/* Replay of a captured call (tools/hybrid/oracle_shim.c has the format) for the function-level
 * tests: a stub for a routine not ported yet takes that routine's next recorded call, applies the
 * RAM it changed and returns its results; the hardware reads come from the record and the writes
 * are checked against it, in order; the interrupts the 68k took are applied where they came. */
#pragma once
#include <stdint.h>
#include <stddef.h>

#define NREGS 18
enum { R_D0, R_D1, R_D2, R_D3, R_D4, R_D5, R_D6, R_D7, R_A0, R_A1, R_A2, R_A3, R_A4, R_A5, R_A6, R_A7, R_PC, R_SR };

typedef struct {
    uint32_t pc, depth, seq;
    uint32_t regs_in[NREGS], regs_out[NREGS];
    const uint8_t *stack;         /* 16 bytes at sp on entry: the return address, then stacked arguments */
    uint32_t n_diff;
    const uint8_t *diff;          /* n_diff x {u16 offset le, u8 old, u8 new} */
    int consumed;
} Call;

typedef struct {
    uint32_t pc, frame, flags, n_calls;
    uint32_t regs_in[NREGS], regs_out[NREGS];
    const uint8_t *ram_in, *ram_out;
    Call *calls;                  /* in entry order */
    int has_writes;
} Record;

#define IO_READ_FLAG 0x80000000u
#define IO_WRITE_FLAG 0x40000000u
#define IRQ_FLAG 0x20000000u
#define HINT_HANDLER 0xFFFFEE00u  /* the H-interrupt vector: work RAM (PLAN.md) */

int cap_load(const char *path, Record **recs, size_t *n);
void cap_free(Record *recs, size_t n);

void replay_begin(Record *r);
void replay_end(void);                        /* the H-interrupts after the C's last access */
const Call *replay_next(uint32_t pc);         /* the next recorded call of routine pc; NULL: none */
void replay_apply(const Call *c);             /* a call's RAM changes, into ram[] */
int replay_unconsumed(const Record *r, int (*is_ported)(uint32_t));
int replay_irq(void);                         /* the next recorded vertical blank (a wait); 0: none */
int record_irqs_clean(const Record *r);       /* every interrupt can be replayed: H-interrupts, and
                                                 vertical blanks that came in a wait loop */
uint32_t replay_io_read(uint32_t addr, int width);
void replay_io_write(uint32_t addr, int width, uint32_t value);
const char *sym_name(uint32_t addr);
int sym_trivial(uint32_t addr);
const char *ram_name(uint32_t off, uint32_t *base);

void replay_fail(const char *fmt, ...);
extern int replay_failures;
