/* Netplay (pulseman's mdrt/netplay.h over mdrt/net_udp.h): two players, each on their own
 * machine, the host pad 1 and the guest pad 2, from power on. The game's resumable point is
 * MainLoop's wait (where states are taken: state.c), and a rollback goes on from there as a loaded
 * state does (native_restart), as does the host's state loaded for everyone (the window's Load
 * state, or --net-load); the host's reset powers every console on again at the same frame, and the
 * game starts again from its power on (native_power_on_again). The game plays whatever session
 * its console has (mdrt_get_net): the window's, made on its Online page or from the command line,
 * or a headless run's own. The
 * identity the peers compare: the version, the build (its commit), the ROM, and what changes the
 * simulation on the command line (--hold, --hold-if, --poke). Each player's buttons are their own
 * controls' (the window's players in turn; headless, or with --input, their column of the script).
 * Headless, a run that reaches --frames waits there until both have every button up to it, then a
 * second more for the other, then says goodbye. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "native.h"
#include "mdrt/net_udp.h"
#include "build_id.h"

static mdrt_udp *udp;                   /* the command line's session (net) and its link */
static mdrt_net *net;
static mdrt *console;
static int headless, started;           /* (headless: this file connects, starts and ends the session) */
static native_input_fn local_fn;
static long stop_at = -1;
static long load_at = -1;               /* --net-load: the host's state for everyone, from that frame */
static const char *load_path;
static long loads_seen;
static long save_at = -1;               /* --net-save: a state every machine played, from that frame */
static const char *save_path;

static void local_input(void *user, uint16_t b[MDRT_PADS])
{
    (void)user;
    uint8_t pads = mdrt_net_pads(net);
    int k = 0;
    for (int p = 0; p < MDRT_PADS; p++)
        if (pads >> p & 1) { b[k] = local_fn ? local_fn(p, k) : 0; k++; }
}

const char *native_net_identity(const char *version, const char *setup)
{
    static char identity[256];
    uint64_t h = 0xCBF29CE484222325ull;                   /* (the command line's part, hashed: the string stays short) */
    for (const char *s = setup ? setup : ""; *s; s++) h = (h ^ (uint8_t)*s) * 0x100000001B3ull;
    snprintf(identity, sizeof identity, "gunstar %s %s, Gunstar Heroes (U) 7721e302, setup %016llx", version, GUNSTAR_BUILD,
             (unsigned long long)h);
    return identity;
}

int native_net_open(const mdrt_net_options *o, const char *version, const char *setup, native_input_fn input, char *err, size_t errsize)
{
    udp = mdrt_net_open_udp(o, err, errsize);
    if (!udp) return 0;
    mdrt_net_config c = { .role = o->role, .identity = native_net_identity(version, setup), .name = o->name, .delay = o->delay,
                          .window = 8, .rollback = 30, .check_every = 60 };
    c.link = mdrt_udp_link(udp);
    c.local_input = local_input;
    local_fn = input;
    net = mdrt_net_new(&c);
    if (!net) { snprintf(err, errsize, "netplay: a bad configuration"); mdrt_udp_close(udp); udp = NULL; return 0; }
    return 1;
}

/* the session the game plays in now, while it lasts */
static mdrt_net *playing(void)
{
    mdrt_net *n = console ? mdrt_get_net(console) : NULL;
    return n && mdrt_net_state(n) == MDRT_NET_GO ? n : NULL;
}

int native_net_on(void) { return playing() != NULL || (headless && !started); }

/* headless: connect, then the game powers on (native_net_start) */
int native_net_connect(void)
{
    char peer[96];
    fprintf(stderr, "netplay: on port %u, connecting\n", (unsigned)mdrt_udp_port(udp));
    for (;;) {
        int r = mdrt_net_connect(net);
        if (r == MDRT_NET_READY) break;
        if (r != MDRT_NET_CONNECTING) { fprintf(stderr, "netplay: %s\n", mdrt_net_why(net)); return 0; }
        mdrt_udp_wait(udp, 10);
    }
    mdrt_udp_lock(udp);
    headless = 1;
    fprintf(stderr, "netplay: playing with %s, %s, pad %d, input delay %d\n", mdrt_udp_peer(udp, peer, sizeof peer),
            mdrt_net_is_host(net) ? "hosting" : "joined", mdrt_net_pads(net) == 1 ? 1 : 2, mdrt_net_delay(net));
    const char *who = mdrt_net_peer_name(net);
    fprintf(stderr, "netplay: %s has joined\n", who[0] ? who : mdrt_net_is_host(net) ? "player 2" : "player 1");
    return 1;
}

mdrt_net *native_net(void) { return net; }
mdrt_udp *native_net_udp(void) { return udp; }

/* a console powered on: in the window, its session (if any) is the window's, started already;
 * headless, this run's own, from this power on */
void native_net_start(mdrt *m)
{
    console = m;
    if (!headless || started || mdrt_get_net(m)) return;
    mdrt_set_net(m, net);
    mdrt_net_start(net, m);
    started = 1;
}

void native_net_stop_at(long frame) { stop_at = frame; }
void native_net_load_at(long frame, const char *path) { load_at = path ? frame : -1; load_path = path; }
void native_net_save_at(long frame, const char *path) { save_at = path ? frame : -1; save_path = path; }

/* --net-save: once the frame comes, the newest state every machine played (as the window's Save
 * state writes it in a session); none yet, and the next wait tries again */
static void keep_state(mdrt_net *n)
{
    size_t size;
    uint32_t frame;
    const void *st = save_at >= 0 && native_frames() >= save_at ? mdrt_net_confirmed_state(n, &size, &frame) : NULL;
    if (!st) return;
    save_at = -1;
    FILE *f = fopen(save_path, "wb");
    int ok = f && fwrite(st, 1, size, f) == size;
    if (f && fclose(f)) ok = 0;
    if (ok) fprintf(stderr, "netplay: the state of frame %u saved into %s\n", (unsigned)frame, save_path);
    else fprintf(stderr, "netplay: cannot write %s\n", save_path);
}

/* --net-load: the host sends its state to everyone once the frame comes (netplay then loads it on
 * every machine at the same frame); checked first, as a session ends where a state fails to load */
static void send_state(mdrt_net *n)
{
    if (load_at < 0 || native_frames() < load_at) return;
    load_at = -1;
    size_t size;
    uint8_t *b = native_read_state(load_path, &size);
    const char *why = !mdrt_net_is_host(n) ? "only the host loads states" : !b ? "cannot read it"
                    : !mdrt_state_fits(console, b, size) ? "not a state of this build" : native_state_check(b, size, NULL);
    if (!why && mdrt_net_load(n, b, size)) why = "it could not be sent";
    if (why) fprintf(stderr, "netplay: the state %s: %s\n", load_path, why);
    else fprintf(stderr, "netplay: sending the state %s to everyone (frame %ld)\n", load_path, native_frames());
    free(b);
}

/* a restart: a rollback, or the host's state loaded for everyone (said, with its frame) */
static void restart(mdrt_net *n)
{
    mdrt_net_stats s;
    mdrt_net_stats_get(n, &s);
    if (s.loads != loads_seen) {
        loads_seen = s.loads;
        fprintf(stderr, "netplay: the host's state loaded at frame %u\n", (unsigned)s.loaded_at);
    }
    native_restart();
}

static void report(mdrt_net *n)
{
    mdrt_net_stats s;
    mdrt_net_stats_get(n, &s);
    fprintf(stderr, "netplay: frame %u, %ld rollbacks (%ld frames played again, at most %d), %ld frames given back, %ld desync checks, round trip %d ms, %ld resets, %ld loads\n",
            (unsigned)s.frame, s.rollbacks, s.replayed, s.most_replayed, s.gave_back, s.checks, s.rtt_ms, s.resets, s.loads);
}

static void over(mdrt_net *n)
{
    if (headless) fprintf(stderr, "netplay: %s\n", mdrt_net_why(n));   /* (the window says why itself) */
    report(n);
}

/* the end of a headless run: every button up to the last frame known here and there, then a
 * second for the other */
static void finish(mdrt_net *n, int main_wait)
{
    uint64_t linger = 0;
    for (;;) {
        int r = mdrt_net_frame(n, main_wait);
        if (r == MDRT_NET_RESTART) restart(n);
        if (r == MDRT_NET_RESET) native_power_on_again();
        mdrt_net_stats s;
        mdrt_net_stats_get(n, &s);
        if (r == MDRT_NET_ENDED || r == MDRT_NET_REFUSED) {   /* (the other done first, or gone) */
            if (s.confirmed < (uint32_t)stop_at) over(n);
            else report(n);
            native_quit();
        }
        mdrt_net_link l = mdrt_udp_link(udp);
        if (!linger && s.confirmed >= (uint32_t)stop_at && s.remote_has >= (uint32_t)stop_at && s.remote_frame >= (uint32_t)stop_at)
            linger = l.ticks(l.user) + 1000000u;
        if (linger && l.ticks(l.user) >= linger) break;
        mdrt_udp_wait(udp, 1);
    }
    report(n);
    mdrt_net_quit(n);
    native_quit();
}

/* at every wait, once the frame's vertical blank code has run (main_wait: MainLoop's) */
void native_net_wait(int main_wait)
{
    mdrt_net *n = playing();
    if (!n) return;                                       /* (none, or over: the console plays on alone) */
    send_state(n);
    keep_state(n);
    int r;
    while ((r = mdrt_net_frame(n, main_wait)) == MDRT_NET_WAIT) {
        if (mdrt_idle(console)) {                         /* (the window closing: goodbye) */
            mdrt_net_quit(n);
            native_quit();
        }
        if (headless) mdrt_udp_wait(udp, 1);              /* (the window waits in mdrt_idle) */
    }
    if (r == MDRT_NET_RESTART) restart(n);
    if (r == MDRT_NET_RESET) native_power_on_again();     /* (the host reset the console: powered on again, the game from its start) */
    if (r == MDRT_NET_ENDED || r == MDRT_NET_REFUSED) { over(n); return; }
    if (headless && stop_at >= 0 && native_frames() >= stop_at && !mdrt_quiet(console)) finish(n, main_wait);
}

void native_net_close(void)
{
    if (net && mdrt_net_state(net) == MDRT_NET_GO) { report(net); fprintf(stderr, "netplay: goodbye\n"); }
    if (net) mdrt_net_quit(net);
    mdrt_net_free(net);
    mdrt_udp_close(udp);
    net = NULL;
    udp = NULL;
}
