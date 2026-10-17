/* The function-level tests: replays capture records (tools/hybrid/capture.py) against the ported
 * routines. For each record of a ported routine: work RAM as the 68k had it at the call, the
 * adapter, then work RAM (all but the stack), the registers the adapter checks, every hardware
 * write and every recorded callee call must come out as the 68k's did.
 * usage: captest FILE.cap... [routine ...] [-v]   (exits 1 on any mismatch) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "game.h"
#include "replay.h"
#include "ports.h"

static int verbose, reg_fail;
static jmp_buf abandon;

void replay_abandon(void) { longjmp(abandon, 1); }

void expect_reg16(const Record *r, int reg, uint32_t got, const char *what)
{
    if ((uint16_t)got != (uint16_t)r->regs_out[reg]) {
        printf("    %s: got %04x, the 68k returned %04x\n", what, (uint16_t)got, (uint16_t)r->regs_out[reg]);
        reg_fail++;
    }
}

void expect_reg32(const Record *r, int reg, uint32_t got, const char *what)
{
    if (got != r->regs_out[reg]) {
        printf("    %s: got %08x, the 68k returned %08x\n", what, got, r->regs_out[reg]);
        reg_fail++;
    }
}

void expect_reg(const Record *r, int reg, uint32_t got, uint32_t mask, const char *what)
{
    if ((got & mask) != (r->regs_out[reg] & mask)) {
        printf("    %s: got %0*x, the 68k returned %0*x\n", what, mask > 0xFFFF ? 8 : mask > 0xFF ? 4 : 2, got & mask,
               mask > 0xFFFF ? 8 : mask > 0xFF ? 4 : 2, r->regs_out[reg] & mask);
        reg_fail++;
    }
}

void expect_flag(const Record *r, int got, int want, const char *what)
{
    (void)r;
    if (!got != !want) {
        printf("    %s: got %d, the 68k's flags say %d\n", what, !!got, !!want);
        reg_fail++;
    }
}

static int is_ported(uint32_t pc)
{
    for (const Port *p = ports; p->name; p++) if (p->pc == pc) return 1;
    for (const uint32_t *l = inner_labels; *l; l++) if (*l == pc) return 1;
    return 0;
}

static int run_record(Record *r, const Port *p)
{
    memcpy(ram, r->ram_in, RAM_SIZE);
    hal_replay_sr((uint16_t)r->regs_in[R_SR]);
    replay_begin(r);
    replay_failures = 0; reg_fail = 0;
    if (!setjmp(abandon)) {
        p->run(r);
        replay_end();
    }
    int bad = 0;
    for (uint32_t i = 0; i < RAM_SIZE; i++) {
        if (i >= STACK_LO && i < STACK_HI) continue;
        if (ram[i] != r->ram_out[i]) {
            if (bad < 12) {
                uint32_t base; const char *nm = ram_name(i, &base);
                printf("    RAM %04X %s+%X: got %02x, expected %02x (entry %02x)\n", i, nm ? nm : "?", i - base, ram[i], r->ram_out[i], r->ram_in[i]);
            }
            bad++;
        }
    }
    int left = replay_unconsumed(r, is_ported);
    if (left && verbose) {
        printf("    %d recorded calls or writes the C code never made:", left);
        int shown = 0;
        for (uint32_t i = 0; i < r->n_calls && shown < 8; i++) {
            const Call *c = &r->calls[i];
            if (c->consumed || (c->pc & IO_READ_FLAG)) continue;
            if (c->pc & IO_WRITE_FLAG) printf(" write:%06x", c->pc & 0xFFFFFF);
            else if (c->pc & IRQ_FLAG) printf(" irq:%06x", c->pc & 0xFFFFFF);
            else if (!is_ported(c->pc) && !sym_trivial(c->pc)) { const char *n = sym_name(c->pc); printf(" %s", n ? n : "?"); }
            else continue;
            shown++;
        }
        printf("\n");
    }
    return bad == 0 && replay_failures == 0 && reg_fail == 0 && left == 0;
}

static void run_capture(Record *recs, size_t n, const char **only, int nonly, long *tested_all, long *failed_all, long *skipped_all)
{
    for (const Port *p = ports; p->name; p++) {
        if (nonly) { int want = 0; for (int i = 0; i < nonly; i++) if (!strcmp(only[i], p->name)) want = 1; if (!want) continue; }
        int tested = 0, passed = 0, skipped = 0;
        for (size_t i = 0; i < n; i++) {
            if (recs[i].pc != p->pc) continue;
            if ((recs[i].flags & 27) && !record_irqs_clean(&recs[i])) { skipped++; continue; }
            tested++;
            if (verbose) printf("  %s record %zu (frame %u, %u nested blocks)\n", p->name, i, recs[i].frame, recs[i].n_calls);
            if (run_record(&recs[i], p)) passed++;
            else printf("  FAIL %s record %zu (frame %u)\n", p->name, i, recs[i].frame);
        }
        if (tested || skipped) printf("%-24s %d/%d passed (%d skipped: left by unwinding, RAM written by the host, or nested too deep)\n", p->name, passed, tested, skipped);
        *tested_all += tested;
        *failed_all += tested - passed;
        *skipped_all += skipped;
    }
}

int main(int argc, char **argv)
{
    const char *rom = getenv("GUNSTAR_ROM");
    if (!rom_load(rom ? rom : "Gunstar Heroes (U) [!].gen")) { fprintf(stderr, "cannot load the ROM (or not the retail image; GUNSTAR_ROM names another path)\n"); return 2; }
    const char *caps[256], *only[64];
    int ncaps = 0, nonly = 0;
    for (int i = 1; i < argc; i++) {
        size_t len = strlen(argv[i]);
        if (!strcmp(argv[i], "-v")) verbose = 1;
        else if (len > 4 && !strcmp(argv[i] + len - 4, ".cap")) { if (ncaps < 256) caps[ncaps++] = argv[i]; }
        else if (nonly < 64) only[nonly++] = argv[i];
    }
    if (!ncaps) { fprintf(stderr, "usage: captest FILE.cap... [routine ...] [-v]\n"); return 2; }
    long tested = 0, failed = 0, skipped = 0;
    for (int c = 0; c < ncaps; c++) {
        Record *recs; size_t n;
        if (ncaps > 1) printf("== %s\n", caps[c]);
        if (!cap_load(caps[c], &recs, &n)) { fprintf(stderr, "cannot load %s\n", caps[c]); return 2; }
        run_capture(recs, n, only, nonly, &tested, &failed, &skipped);
        cap_free(recs, n);
    }
    printf("captest: %ld / %ld records passed, %ld failed, %ld skipped, %d capture%s\n",
           tested - failed, tested, failed, skipped, ncaps, ncaps == 1 ? "" : "s");
    return failed ? 1 : 0;
}
