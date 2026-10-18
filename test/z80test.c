/* The Z80 sample player's port (src/z80.c) against a .sndtrace (tools/oracle/sndtrace.py): the
 * 68k's writes to Z80 RAM, the bus and the reset are applied at their times, the player runs in
 * between, and every YM2612 port write it makes (the DAC's address and data, the DAC on and off)
 * must be the oracle's: the same port and value at the same master clock. The YM2612 writes made
 * while the 68k holds the Z80's bus are the 68k's own (its sound driver): they only set the chip's
 * busy flag the player polls. Which they are goes by their times: the oracle logs the bus request
 * before the Z80 catches up to it, so the Z80's last writes come after it in the trace; and the
 * Z80 finishes the instruction under way, so its write can come a little after the request.
 * usage: z80test FILE.sndtrace...   (Z80_DEBUG=1: the writes around a difference) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mem.h"
#include "z80.h"

struct w { uint64_t t; uint8_t port, val; };
static struct w *got;
static size_t ngot, capgot;

static void ym(int port, uint8_t value)
{
    if (ngot == capgot) got = realloc(got, (capgot = capgot ? capgot * 2 : 1 << 16) * sizeof *got);
    got[ngot++] = (struct w){ zp.zc, (uint8_t)port, value };
}

static uint32_t u32(const uint8_t *p) { return (uint32_t)(p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24); }
static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

#define FRAME 896040u
#define SETTLE 400                                        /* master clocks: longer than any of the player's instructions */                                     /* master clocks a frame (262 lines of 3420) */

/* the bus requests: whether the 68k held the bus at time t */
static struct { uint64_t t; int taken; } *reqs;
static size_t nreqs;
static size_t req_at(uint64_t t)                         /* the requests at or before t */
{
    size_t lo = 0, hi = nreqs;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (reqs[mid].t <= t) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}
/* the 68k held the bus all through [t0, t1] */
static int bus_held(uint64_t t0, uint64_t t1)
{
    size_t k = req_at(t1);
    return k && reqs[k - 1].taken && req_at(t0) == k;
}

static uint64_t unwrap(uint32_t t32, uint32_t *last, uint64_t *high)
{
    if (t32 < *last && *last - t32 > 0x80000000u) *high += 1ull << 32;
    *last = t32;
    return *high | t32;
}

/* the events' sizes after their time */
static size_t skip(const uint8_t *d, size_t o, uint8_t t)
{
    switch (t) {
    case 1: return 2;
    case 2: return 1;
    case 3: return 3;
    case 4: case 5: return 2;
    case 6: return 4 + 4 * (size_t)u32(d + o);
    default: return 0;
    }
}

static void run_to(uint64_t t)
{
    while (t > zp.now) {
        uint64_t step = t - zp.now > 0x40000000u ? 0x40000000u : t - zp.now;
        z80_player_run((uint32_t)step);
    }
}

static int run(const char *path)
{
    const char *name = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
    FILE *f = fopen(path, "rb");
    if (!f) { printf("%s: cannot open\n", name); return 1; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)len);
    if (fread(d, 1, (size_t)len, f) != (size_t)len) { fclose(f); free(d); return 1; }
    fclose(f);
    if (memcmp(d, "MDST", 4) || u32(d + 4) != 3) { printf("%s: not a version 3 sndtrace\n", name); free(d); return 1; }
    if (u32(d + 16) & 2) { printf("%s: recorded with the Nuked FM core (another busy flag), skipped\n", name); free(d); return 0; }
    if (!(u32(d + 16) & 1)) { printf("%s: not from power on (the player's registers unknown), skipped\n", name); free(d); return 0; }
    size_t o = 20;
    z80_player_power_on();
    memcpy(z80_mem, d + o, 0x2000);
    o += 0x2000 + 12;
    ngot = 0;
    z80_player_ym = ym;
    nreqs = 0;
    reqs = realloc(reqs, sizeof *reqs * (size_t)(len / 7 + 1));
    {
        uint64_t hi2 = 0;
        uint32_t last2 = 0;
        for (size_t p = o; p < (size_t)len;) {
            uint8_t t = d[p++];
            if (t == 7 || t > 7) break;
            uint64_t time = unwrap(u32(d + p), &last2, &hi2);
            p += 4;
            if (t == 4) {
                reqs[nreqs].t = time;
                reqs[nreqs++].taken = (u16(d + p) & 0x100) != 0;
            }
            p += skip(d, p, t);
        }
    }
    struct w *want = malloc(sizeof *want * (size_t)(len / 6 + 1));
    size_t nwant = 0, n68k = 0;
    uint64_t high = 0;
    uint32_t last = 0;
    while (o < (size_t)len) {
        uint8_t t = d[o++];
        if (t == 7) break;
        uint32_t t32 = u32(d + o);
        o += 4;
        uint64_t time = unwrap(t32, &last, &high);
        run_to(time);
        switch (t) {
        case 1:
            if (time >= SETTLE && bus_held(time - SETTLE, time)) {   /* the 68k's own */
                if (d[o] & 1) z80_player_ym_busy(time);
                n68k++;
            } else {
                want[nwant++] = (struct w){ time, d[o], d[o + 1] };
            }
            o += 2;
            break;
        case 2: o += 1; break;
        case 3: z80_mem[u16(d + o) & 0x1FFF] = d[o + 2]; o += 3; break;
        case 4: z80_player_busreq(u16(d + o)); o += 2; break;
        case 5: z80_player_reset(u16(d + o)); o += 2; break;
        case 6:
            o += 4 + 4 * (size_t)u32(d + o);
            zp.frame = time;
            break;
        default: printf("%s: bad tag %d\n", name, t); free(d); free(want); return 1;
        }
    }
    size_t i = 0, ndata = 0;
#define SAME(a, b) ((a).t == (b).t && (a).port == (b).port && (a).val == (b).val)
    while (i < ngot && i < nwant && SAME(got[i], want[i])) ndata += want[i++].port & 1;
    int ok = i == ngot && i == nwant;
    printf("%s: %zu / %zu of the player's YM2612 writes match at their times (%zu of data; %zu the 68k's)",
           name, i, nwant, ndata, n68k);
    if (!ok) {
        printf("; first difference at %zu:", i);
        if (i < ngot) printf(" got port %d = %02x at frame %u + %u,", got[i].port, got[i].val, (unsigned)(got[i].t / FRAME), (unsigned)(got[i].t % FRAME));
        else printf(" got none,");
        if (i < nwant) printf(" expected port %d = %02x at frame %u + %u", want[i].port, want[i].val, (unsigned)(want[i].t / FRAME), (unsigned)(want[i].t % FRAME));
        else printf(" expected none");
        if (getenv("Z80_DEBUG"))
            for (size_t k = i > 8 ? i - 8 : 0; k < i + 8 && (k < ngot || k < nwant); k++)
                printf("\n  %6zu  got %d = %02x @%-12llu  want %d = %02x @%llu", k,
                       k < ngot ? got[k].port : -1, k < ngot ? got[k].val : 0, (unsigned long long)(k < ngot ? got[k].t : 0),
                       k < nwant ? want[k].port : -1, k < nwant ? want[k].val : 0, (unsigned long long)(k < nwant ? want[k].t : 0));
    }
    printf("\n");
    free(d);
    free(want);
    return !ok;
}

int main(int argc, char **argv)
{
    const char *rom = getenv("GUNSTAR_ROM");
    if (!rom_load(rom ? rom : "Gunstar Heroes (U) [!].gen")) { fprintf(stderr, "no ROM\n"); return 2; }
    int bad = 0;
    for (int i = 1; i < argc; i++) bad |= run(argv[i]);
    return bad;
}
