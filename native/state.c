/* States of the native game: pulseman's whole console (mdrt_get_state) with the game's own parts,
 * work RAM ("RAM "), the Z80's sample player ("Z80 ": its RAM and registers) and "GSTR" (what the
 * C port keeps outside RAM from one frame to the next: reg_a6, the H-interrupt handler's table,
 * which the next frame's lines read; reg_a5_left, reg_sp; and the HAL's clock and frame count).
 *
 * A state is taken at MainLoop's wait (hal_wait_vblank_main), once the frame's vertical blank code
 * has run: the job list is done there, so nothing on the C stack is needed again, and a loaded
 * state goes on by calling MainLoop afresh (native_restart). A job under way (a stage's graphics
 * being decoded) waits for the vertical blank inside itself: a save asked for then is taken at
 * the next MainLoop wait, if it comes within a second (the platform gives up on it after 1.5);
 * a load is taken at any wait.
 *
 * In the window: the menu's save slots and quick resume (the file each request names:
 * mdrt_request_path), F4 held steps back through the last ten seconds (a state every third frame).
 * Headless: --save-state FRAME:FILE, --load-state FILE (at the first wait). */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "native.h"
#include "z80.h"

static mdrt *m;
static long seen = -1;                  /* the handshake whose requests were taken */
static char pending[1100];              /* a save asked for out of MainLoop's wait: its file */
static long pending_until;
static long save_frame = -1;            /* --save-state */
static const char *save_path, *load_path;

/* ---- the game's parts ---------------------------------------------------------------------- */

static size_t z80_save(void *user, uint8_t *out)
{
    (void)user;
    if (out) {
        memcpy(out, z80_mem, sizeof z80_mem);
        memcpy(out + sizeof z80_mem, &zp, sizeof zp);
    }
    return sizeof z80_mem + sizeof zp;
}

static int z80_load(void *user, const uint8_t *in, size_t size)
{
    (void)user;
    if (size != sizeof z80_mem + sizeof zp) return -1;
    memcpy(z80_mem, in, sizeof z80_mem);
    memcpy(&zp, in + sizeof z80_mem, sizeof zp);
    return 0;
}

#define GSTR_PORT 10                    /* reg_a6, reg_sp, reg_a5_left */

static size_t gstr_save(void *user, uint8_t *out)
{
    (void)user;
    if (out) {
        memcpy(out, &reg_a6, 4);
        memcpy(out + 4, &reg_sp, 4);
        memcpy(out + 8, &reg_a5_left, 2);
    }
    return GSTR_PORT + native_hal_save(out ? out + GSTR_PORT : NULL);
}

static int gstr_load(void *user, const uint8_t *in, size_t size)
{
    (void)user;
    if (size < GSTR_PORT || native_hal_load(in + GSTR_PORT, size - GSTR_PORT)) return -1;
    memcpy(&reg_a6, in, 4);
    memcpy(&reg_sp, in + 4, 4);
    memcpy(&reg_a5_left, in + 8, 2);
    return 0;
}

/* ---- rewind: the last ten seconds, a state every third frame -------------------------------- */

#define REWIND_EVERY 3
#define REWIND_STATES 200
static struct { uint8_t *buf; size_t len, cap; } ring[REWIND_STATES];
static int ring_first, ring_n;

static void ring_push(void)
{
    int i = (ring_first + ring_n) % REWIND_STATES;
    if (ring_n == REWIND_STATES) {
        ring_first = (ring_first + 1) % REWIND_STATES;
        ring_n--;
    }
    size_t n = mdrt_get_state(m, NULL, 0);
    if (ring[i].cap < n) {
        uint8_t *b = realloc(ring[i].buf, n);
        if (!b) return;
        ring[i].buf = b;
        ring[i].cap = n;
    }
    ring[i].len = mdrt_get_state(m, ring[i].buf, ring[i].cap);
    ring_n++;
}

/* ---- saving and loading -------------------------------------------------------------------- */

void native_states_init(mdrt *console)
{
    m = console;
    ring_first = ring_n = 0;                              /* (a new game: the last one's states are not its own) */
    pending[0] = 0;
    seen = -1;
    mdrt_add_state_memory(m, "RAM ", ram, RAM_SIZE);
    mdrt_add_state_part(m, "Z80 ", z80_save, z80_load, NULL);
    mdrt_add_state_part(m, "GSTR", gstr_save, gstr_load, NULL);
}

void native_states_options(long frame, const char *save, const char *load)
{
    save_frame = frame;
    save_path = save;
    load_path = load;
}

static int save(const char *path)
{
    size_t n = mdrt_get_state(m, NULL, 0);
    uint8_t *b = malloc(n);
    FILE *f = b ? fopen(path, "wb") : NULL;
    int ok = f && mdrt_get_state(m, b, n) == n && fwrite(b, 1, n, f) == n;
    if (f && fclose(f)) ok = 0;
    free(b);
    return ok;
}

static int load(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "gunstar: cannot open %s\n", path); return -1; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = n > 0 ? malloc((size_t)n) : NULL;
    int bad = !b || fread(b, 1, (size_t)n, f) != (size_t)n || mdrt_set_state(m, b, (size_t)n);
    fclose(f);
    free(b);
    if (bad) fprintf(stderr, "gunstar: %s is not a state of this build\n", path);
    return bad ? -1 : 0;
}

/* at every wait, once the frame's vertical blank code has run; at_main: MainLoop's. A load goes
 * on from MainLoop (native_restart does not return). */
void native_states_wait(int at_main)
{
    if (!m) return;
    unsigned req = 0;
    long frame = native_frames();
    if (native_handshakes() != seen) {
        seen = native_handshakes();
        req = mdrt_requests(m);
    }
    const char *path = mdrt_request_path(m);
    if (pending[0] && frame > pending_until) pending[0] = 0;   /* (not in play: the platform gives up on it) */
    if (load_path) {                                      /* --load-state */
        const char *p = load_path;
        load_path = NULL;
        if (load(p) == 0) {
            printf("gunstar: state loaded from %s (frame %ld)\n", p, native_frames());
            native_restart();
        }
    }
    if ((req & MDRT_REQUEST_LOAD) && path && load(path) == 0) {
        printf("gunstar: state loaded from %s (frame %ld)\n", path, native_frames());
        ring_n = 0;
        pending[0] = 0;
        native_restart();
    }
    if ((req & MDRT_REQUEST_SAVE) && path) {
        snprintf(pending, sizeof pending, "%s", path);
        pending_until = frame + 60;
    }
    if (!at_main) return;
    if (pending[0]) {
        if (save(pending)) printf("gunstar: state saved at frame %ld into %s\n", frame, pending);
        else fprintf(stderr, "gunstar: cannot write %s\n", pending);
        pending[0] = 0;
    }
    if (save_frame >= 0 && frame >= save_frame) {         /* --save-state */
        save_frame = -1;
        if (save(save_path)) printf("gunstar: state saved at frame %ld into %s\n", frame, save_path);
        else fprintf(stderr, "gunstar: cannot write %s\n", save_path);
    }
    if ((req & MDRT_REQUEST_REWIND) && ring_n) {          /* a step back: the newest, kept when it is the last */
        int i = (ring_first + ring_n - 1) % REWIND_STATES;
        if (ring_n > 1) ring_n--;
        if (mdrt_set_state(m, ring[i].buf, ring[i].len) == 0) native_restart();
    } else if (!(req & MDRT_REQUEST_REWIND) && frame % REWIND_EVERY == 0) {
        ring_push();
    }
}
