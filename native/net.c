/* Netplay (pulseman's mdrt/netplay.h over mdrt/net_udp.h): two players, each on their own
 * machine, the host pad 1 and the guest pad 2, from power on. The game's resumable point is
 * MainLoop's wait (where states are taken: state.c), and a rollback goes on from there as a loaded
 * state does (native_restart). Each player's buttons are their own controls' (the window's pad 1;
 * headless, their column of the --input script). The identity the peers compare: the build (its
 * commit), the ROM, and what changes the simulation on the command line (--hold, --hold-if,
 * --poke). Headless, a run that reaches --frames waits there until both have every button up to
 * it, then a second more for the other, then says goodbye. In the window, the platform connects
 * (mdrt_sdl_options.net) and keeps the session's rules; the players' controls are each window's
 * pad 1. */
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "native.h"
#include "mdrt/net_udp.h"
#include "build_id.h"

static mdrt_udp *udp;
static mdrt_net *net;
static mdrt *console;
static int running, ended;
static native_input_fn local_fn;
static long stop_at = -1;

static void local_input(void *user, uint16_t b[MDRT_PADS])
{
    (void)user;
    uint8_t pads = mdrt_net_pads(net);
    int k = 0;
    for (int p = 0; p < MDRT_PADS; p++)
        if (pads >> p & 1) { b[k] = local_fn ? local_fn(p, k) : 0; k++; }
}

int native_net_open(const mdrt_net_options *o, const char *version, const char *setup, native_input_fn input, char *err, size_t errsize)
{
    udp = mdrt_net_open_udp(o, err, errsize);
    if (!udp) return 0;
    static char identity[256];
    uint64_t h = 0xCBF29CE484222325ull;                   /* (the command line's part, hashed: the string stays short) */
    for (const char *s = setup ? setup : ""; *s; s++) h = (h ^ (uint8_t)*s) * 0x100000001B3ull;
    snprintf(identity, sizeof identity, "gunstar %s %s, Gunstar Heroes (U) 7721e302, setup %016llx", version, GUNSTAR_BUILD,
             (unsigned long long)h);
    mdrt_net_config c = { .role = o->role, .identity = identity, .delay = o->delay, .window = 8, .rollback = 30, .check_every = 60 };
    c.link = mdrt_udp_link(udp);
    c.local_input = local_input;
    local_fn = input;
    net = mdrt_net_new(&c);
    if (!net) { snprintf(err, errsize, "netplay: a bad configuration"); mdrt_udp_close(udp); udp = NULL; return 0; }
    return 1;
}

int native_net_on(void) { return net && !ended; }

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
    fprintf(stderr, "netplay: playing with %s, %s, pad %d, input delay %d\n", mdrt_udp_peer(udp, peer, sizeof peer),
            mdrt_net_is_host(net) ? "hosting" : "joined", mdrt_net_pads(net) == 1 ? 1 : 2, mdrt_net_delay(net));
    return 1;
}

mdrt_net *native_net(void) { return net; }
mdrt_udp *native_net_udp(void) { return udp; }

void native_net_start(mdrt *m)
{
    if (!net || ended || running) return;                 /* (once: a session is one power on) */
    console = m;
    mdrt_net_start(net, m);
    running = 1;
}

void native_net_stop_at(long frame) { stop_at = frame; }

static void report(void)
{
    mdrt_net_stats s;
    mdrt_net_stats_get(net, &s);
    fprintf(stderr, "netplay: frame %u, %ld rollbacks (%ld frames played again, at most %d), %ld frames given back, %ld desync checks, round trip %d ms\n",
            (unsigned)s.frame, s.rollbacks, s.replayed, s.most_replayed, s.gave_back, s.checks, s.rtt_ms);
}

static void over(void)
{
    ended = 1;
    fprintf(stderr, "netplay: %s\n", mdrt_net_why(net));
    report();
}

/* the end of a headless run: every button up to the last frame known here and there, then a
 * second for the other */
static void finish(int main_wait)
{
    uint64_t linger = 0;
    for (;;) {
        int r = mdrt_net_frame(net, main_wait);
        if (r == MDRT_NET_RESTART) native_restart();
        mdrt_net_stats s;
        mdrt_net_stats_get(net, &s);
        if (r == MDRT_NET_ENDED || r == MDRT_NET_REFUSED) {   /* (the other done first, or gone) */
            if (s.confirmed < (uint32_t)stop_at) over();
            else report();
            ended = 1;
            native_quit();
        }
        mdrt_net_link l = mdrt_udp_link(udp);
        if (!linger && s.confirmed >= (uint32_t)stop_at && s.remote_has >= (uint32_t)stop_at && s.remote_frame >= (uint32_t)stop_at)
            linger = l.ticks(l.user) + 1000000u;
        if (linger && l.ticks(l.user) >= linger) break;
        mdrt_udp_wait(udp, 1);
    }
    report();
    mdrt_net_quit(net);
    ended = 1;
    native_quit();
}

/* at every wait, once the frame's vertical blank code has run (main_wait: MainLoop's) */
void native_net_wait(int main_wait)
{
    if (!running || ended) return;
    int r;
    while ((r = mdrt_net_frame(net, main_wait)) == MDRT_NET_WAIT) {
        if (mdrt_idle(console)) {                         /* (the window closing: goodbye) */
            mdrt_net_quit(net);
            ended = 1;
            native_quit();
        }
        mdrt_udp_wait(udp, 1);
    }
    if (r == MDRT_NET_RESTART) native_restart();
    if (r == MDRT_NET_ENDED || r == MDRT_NET_REFUSED) { over(); return; }
    if (stop_at >= 0 && native_frames() >= stop_at && !mdrt_quiet(console)) finish(main_wait);
}

void native_net_close(void)
{
    if (net && running && !ended) { report(); fprintf(stderr, "netplay: goodbye\n"); }
    if (net && !ended) mdrt_net_quit(net);
    mdrt_net_free(net);
    mdrt_udp_close(udp);
    net = NULL;
    udp = NULL;
}
