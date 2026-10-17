/* Capture records and their replay (replay.h). From ninety-four's test/replay.c, with this game's
 * interrupts: H-interrupts come many times a frame, inside most captured calls, so each one's RAM
 * changes are applied as the C code passes the point in the record where it came. A vertical
 * blank the 68k took in one of its wait loops is replayed where the C waits for one; one that
 * came anywhere else (the job loop runs on the main thread, and the vertical blank interrupts it)
 * is applied like an H-interrupt, at the C code's next access the record has after it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <zlib.h>
#include "replay.h"
#include "mem.h"
#include "symbols_gen.h"

#define CAP_MAGIC 0x43434C48u
static uint8_t *cap_data;
static Record *cur;
static uint32_t cursor;
int replay_failures;

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

static int cmp_seq(const void *a, const void *b)
{
    uint32_t x = ((const Call *)a)->seq, y = ((const Call *)b)->seq;
    return x < y ? -1 : x > y;
}

int cap_load(const char *path, Record **out, size_t *n)
{
    gzFile f = gzopen(path, "rb");                 /* gzip as the shim writes it, or plain */
    if (!f) return 0;
    size_t size = 1 << 24;
    long len = 0;
    cap_data = malloc(size);
    for (int got; cap_data && (got = gzread(f, cap_data + len, (unsigned)(size - (size_t)len))) > 0;) {
        len += got;
        if ((size_t)len == size) cap_data = realloc(cap_data, size *= 2);
    }
    gzclose(f);
    if (!cap_data) return 0;
    size_t cap = 16, cnt = 0;
    Record *recs = malloc(cap * sizeof *recs);
    for (long o = 0; o < len;) {
        const uint8_t *p = cap_data + o;
        if (le32(p) != CAP_MAGIC) { fprintf(stderr, "bad magic at %ld\n", o); return 0; }
        uint32_t total = le32(p + 4);
        if (total < 24 + 2 * NREGS * 4 + 2 * RAM_SIZE || o + total > len) { fprintf(stderr, "record %zu: bad size\n", cnt); return 0; }
        if (cnt == cap) recs = realloc(recs, (cap *= 2) * sizeof *recs);
        Record *r = &recs[cnt++];
        r->pc = le32(p + 8); r->frame = le32(p + 12); r->flags = le32(p + 16); r->n_calls = le32(p + 20);
        const uint8_t *q = p + 24;
        for (int i = 0; i < NREGS; i++, q += 4) r->regs_in[i] = le32(q);
        for (int i = 0; i < NREGS; i++, q += 4) r->regs_out[i] = le32(q);
        r->ram_in = q; q += RAM_SIZE; r->ram_out = q; q += RAM_SIZE;
        r->calls = calloc(r->n_calls ? r->n_calls : 1, sizeof(Call));
        const uint8_t *end = p + total;
        for (uint32_t c = 0; c < r->n_calls; c++) {
            Call *k = &r->calls[c];
            if (q + 12 + 2 * NREGS * 4 + 16 + 4 > end) { fprintf(stderr, "record %zu: truncated call block\n", cnt - 1); return 0; }
            k->pc = le32(q); k->depth = le32(q + 4) & 0x7FFFFFFF; k->seq = le32(q + 8); q += 12;
            for (int i = 0; i < NREGS; i++, q += 4) k->regs_in[i] = le32(q);
            for (int i = 0; i < NREGS; i++, q += 4) k->regs_out[i] = le32(q);
            k->stack = q; q += 16;
            k->n_diff = le32(q); q += 4; k->diff = q;
            if (q + 4 * (size_t)k->n_diff > end) { fprintf(stderr, "record %zu: truncated diff\n", cnt - 1); return 0; }
            q += 4 * k->n_diff;
        }
        if (q != end) { fprintf(stderr, "record %zu: trailer mismatch\n", cnt - 1); return 0; }
        qsort(r->calls, r->n_calls, sizeof(Call), cmp_seq);
        r->has_writes = 0;
        for (uint32_t c = 0; c < r->n_calls; c++) if (r->calls[c].pc & IO_WRITE_FLAG) r->has_writes = 1;
        o += total;
    }
    *out = recs; *n = cnt;
    return 1;
}

void cap_free(Record *recs, size_t n)
{
    for (size_t i = 0; i < n; i++) free(recs[i].calls);
    free(recs);
    free(cap_data);
    cap_data = NULL;
}

static int is_hint(const Call *c) { return (c->pc & IRQ_FLAG) && (c->pc & ~IRQ_FLAG) == (HINT_HANDLER & 0x1FFFFFFFu); }
static int in_wait_loop(uint32_t pc);
static uint32_t irq_return(const Call *c) { return ((uint32_t)c->stack[2] << 24 | c->stack[3] << 16 | c->stack[4] << 8 | c->stack[5]) & 0xFFFFFF; }
/* an interrupt the C code does not wait for: an H-interrupt, or a vertical blank outside a wait */
static int is_async(const Call *c) { return (c->pc & IRQ_FLAG) && (is_hint(c) || !in_wait_loop(irq_return(c))); }

/* consume block i and everything nested under it */
static uint32_t take(uint32_t i)
{
    Call *c = &cur->calls[i];
    c->consumed = 1;
    uint32_t j = i + 1;
    while (j < cur->n_calls && cur->calls[j].depth > c->depth) cur->calls[j++].consumed = 1;
    return j;
}

/* the H-interrupts that came before block i: their RAM changes, now */
static void hints_before(uint32_t i)
{
    for (uint32_t k = cursor; k < i; k++) {
        Call *c = &cur->calls[k];
        if (!c->consumed && is_async(c)) { replay_apply(c); take(k); }
    }
}

void replay_begin(Record *r)
{
    cur = r; cursor = 0;
    for (uint32_t i = 0; i < r->n_calls; i++) r->calls[i].consumed = 0;
}

void replay_end(void)
{
    hints_before(cur->n_calls);
}

const Call *replay_next(uint32_t pc)
{
    for (uint32_t i = cursor; i < cur->n_calls; i++) {
        Call *c = &cur->calls[i];
        if (c->consumed || c->pc != pc) continue;
        hints_before(i);
        if (c->consumed) continue;                 /* it was the interrupt's own */
        cursor = take(i);
        return c;
    }
    return NULL;
}

uint32_t replay_io_read(uint32_t addr, int width)
{
    for (uint32_t i = cursor; i < cur->n_calls; i++) {
        Call *c = &cur->calls[i];
        if (c->consumed || c->pc != (addr | IO_READ_FLAG)) continue;
        hints_before(i);
        if (c->consumed) continue;
        c->consumed = 1; cursor = i + 1;
        if ((int)c->regs_in[1] != width) replay_fail("io read %06x: width %d, recorded %d", addr, width, c->regs_in[1]);
        return c->regs_in[0];
    }
    replay_fail("no recorded hardware read of %06x", addr);
    return 0;
}

/* A wait: the next vertical blank. One the 68k took outside a wait loop that comes first is
 * the one (the 68k's wait then found its condition met; the C's loop checks again). */
int replay_irq(void)
{
    for (uint32_t i = cursor; i < cur->n_calls; i++) {
        Call *c = &cur->calls[i];
        if (c->consumed || !(c->pc & IRQ_FLAG) || is_hint(c)) continue;
        if (is_async(c)) {
            hints_before(i + 1);
            return 1;
        }
        hints_before(i);
        if (c->consumed) continue;
        cursor = take(i);
        replay_apply(c);
        return 1;
    }
    return 0;
}

/* A wait loop: a conditional Bcc.s back (within 16 bytes) over code that reads one of the
 * variables the game waits on, by absolute short address, at or just after pc. */
static const uint16_t wait_vars[] = { v_jobBufferFull, v_frameReady };

static int in_wait_loop(uint32_t pc)
{
    if (pc + 14 >= ROM_SIZE) return 0;
    for (uint32_t b = pc; b <= pc + 12; b += 2) {
        uint8_t op = rom_image[b], disp = rom_image[b + 1];
        if (op < 0x62 || op > 0x6F || !(disp & 0x80) || disp == 0xFF) continue;
        uint32_t target = b + 2 - (uint32_t)(0x100 - disp);
        if (target > pc || pc - target > 16) continue;
        for (uint32_t a = target; a < b; a += 2) {
            uint16_t w = (uint16_t)(rom_image[a] << 8 | rom_image[a + 1]);
            for (size_t k = 0; k < sizeof wait_vars / sizeof *wait_vars; k++) if (w == wait_vars[k]) return 1;
        }
    }
    return 0;
}

int record_irqs_clean(const Record *r)
{
    if (r->flags & 2) return 0;                    /* left by unwinding */
    if (r->flags & 16) return 0;                   /* the host wrote RAM meanwhile (a session's hold) */
    if ((r->flags & 1) && !(r->flags & 4)) return 0;
    return 1;
}

void replay_io_write(uint32_t addr, int width, uint32_t value)
{
    uint32_t mask = width == 4 ? 0xFFFFFFFFu : width == 2 ? 0xFFFFu : 0xFFu;
    if (addr == SR_PSEUDO) mask = 0xFF00;          /* the interrupt mask: the C keeps no flags */
    for (uint32_t i = cursor; i < cur->n_calls; i++) {
        Call *c = &cur->calls[i];
        if (c->consumed || !(c->pc & IO_WRITE_FLAG)) continue;
        hints_before(i);
        if (c->consumed) continue;
        c->consumed = 1; cursor = i + 1;
        if ((c->pc & 0xFFFFFF) != addr || (int)c->regs_in[1] != width || (c->regs_in[0] & mask) != (value & mask))
            replay_fail("hardware write %06x.%d = %0*x, recorded %06x.%d = %0*x", addr, width, 2 * width, value & mask,
                        c->pc & 0xFFFFFF, c->regs_in[1], 2 * (int)c->regs_in[1], c->regs_in[0]);
        return;
    }
    if (cur->has_writes) replay_fail("hardware write %06x.%d = %0*x was not made by the 68k", addr, width, 2 * width, value & mask);
}

void replay_apply(const Call *c)
{
    for (uint32_t i = 0; i < c->n_diff; i++) {
        const uint8_t *e = c->diff + 4 * i;
        ram[e[0] | e[1] << 8] = e[3];
    }
}

int replay_unconsumed(const Record *r, int (*is_ported)(uint32_t))
{
    int n = 0;
    for (uint32_t i = 0; i < r->n_calls; i++) {
        const Call *c = &r->calls[i];
        if (c->consumed) continue;
        if (c->pc & IO_READ_FLAG) continue;
        if ((c->pc & (IO_WRITE_FLAG | IRQ_FLAG)) || (!is_ported(c->pc) && !sym_trivial(c->pc))) n++;
    }
    return n;
}

int sym_trivial(uint32_t addr)
{
    for (size_t i = 0; i < sizeof trivial_syms / sizeof *trivial_syms; i++) if (trivial_syms[i] == addr) return 1;
    return 0;
}

const char *sym_name(uint32_t addr)
{
    for (size_t i = 0; i < sizeof rom_syms / sizeof *rom_syms; i++) if (rom_syms[i].addr == addr) return rom_syms[i].name;
    return NULL;
}

const char *ram_name(uint32_t off, uint32_t *base)
{
    const char *best = NULL; uint32_t b = 0;
    for (size_t i = 0; i < sizeof ram_syms / sizeof *ram_syms; i++) {
        if (ram_syms[i].addr <= off) { best = ram_syms[i].name; b = ram_syms[i].addr; } else break;
    }
    if (base) *base = b;
    return best;
}

void replay_fail(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    printf("    mismatch: "); vprintf(fmt, ap); printf("\n");
    va_end(ap);
    replay_failures++;
}
