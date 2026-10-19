/* The native game's entry: a window (pulseman's SDL2 platform), or headless with pictures.
 *
 *   gunstar [--rom FILE] [--headless] [--frames N] [--shot FILE.ppm] [--shots DIR --every N]
 *           [--input FILE] (both pads from a script, as tools/oracle/script.py's)
 *           [--sound FILE.sndtrace] (headless: the runtime's sound trace, as the oracle's)
 *           [--golden FILE] (headless: a line of hashes a frame, tools/golden.py)
 *           [--host [PORT] | --join ADDR[:PORT] | --peer ADDR[:PORT]] [--port PORT] [--delay N]
 *           [--net-sim MS,MS,%,%] (netplay, net.c: from power on; in the window each player's
 *           controls, headless their column of the --input script)
 *           [--fm-core nuked|ymfm] (the FM core; pulseman's default: Nuked-OPN2)
 *           [--version]
 *           [--save-state FRAME:FILE] [--load-state FILE] [--requests FILE --state FILE]
 *           (headless: native/state.c)
 *           [--hold ADDR=VAL,...] [--poke FRAME:ADDR=VAL,...;...] (RAM words, hex, as the oracle's
 *           run.py: held before every frame's vertical blank code, or written before frame F's)
 *           [--hold-if WADDR=WVAL:ADDR=VAL,...] (held while the word at WADDR is WVAL: an option
 *           set while a screen is up, whatever frame it comes up at)
 *
 * A routine not ported yet stops the game with its address (call68k_missing). */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "native.h"
#include "z80.h"

#ifndef GUNSTAR_VERSION
#define GUNSTAR_VERSION "0.2.0"
#endif

static jmp_buf out, again;
static long stop_after = -1;
static const char *shot_path, *shots_dir;
static long shots_every = 60;
static uint16_t picture[320 * 240];
static int pic_w, pic_h;

void native_quit(void) { longjmp(out, 1); }
void native_restart(void) { longjmp(again, 1); }

/* --input FILE: both pads from a script as tools/oracle/script.py reads it ("FRAME:P1|P2"
 * entries, by newline or comma; buttons joined by '+'; held until the next entry; '#' a
 * comment), by the native frame count */
static struct { long frame; uint16_t pads[2]; } *script;
static size_t script_n;

static uint16_t buttons_of(const char *s, const char *end)
{
    static const struct { const char *name; uint16_t bit; } names[] = {
        { "UP", MDRT_PAD_UP }, { "DOWN", MDRT_PAD_DOWN }, { "LEFT", MDRT_PAD_LEFT }, { "RIGHT", MDRT_PAD_RIGHT },
        { "A", MDRT_PAD_A }, { "B", MDRT_PAD_B }, { "C", MDRT_PAD_C }, { "START", MDRT_PAD_START },
        { "X", MDRT_PAD_X }, { "Y", MDRT_PAD_Y }, { "Z", MDRT_PAD_Z }, { "MODE", MDRT_PAD_MODE },
    };
    uint16_t b = 0;
    while (s < end) {
        const char *e = s;
        while (e < end && *e != '+') e++;
        for (unsigned k = 0; k < sizeof names / sizeof *names; k++)
            if ((size_t)(e - s) == strlen(names[k].name) && !strncmp(s, names[k].name, (size_t)(e - s))) b |= names[k].bit;
        s = e + 1;
    }
    return b;
}

static int read_script(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); return 0; }
    char line[4096];
    while (fgets(line, sizeof line, f)) {
        char *hash = strchr(line, '#');
        if (hash) *hash = 0;
        for (char *item = strtok(line, ",\n\r"); item; item = strtok(NULL, ",\n\r")) {
            while (*item == ' ') item++;
            char *colon = strchr(item, ':');
            if (!colon) continue;
            char *end = colon + 1 + strlen(colon + 1);
            while (end > colon + 1 && (end[-1] == ' ' || end[-1] == '\t')) end--;
            char *bar = memchr(colon + 1, '|', (size_t)(end - colon - 1));
            script = realloc(script, (script_n + 1) * sizeof *script);
            script[script_n].frame = atol(item);
            script[script_n].pads[0] = buttons_of(colon + 1, bar ? bar : end);
            script[script_n].pads[1] = bar ? buttons_of(bar + 1, end) : 0;
            script_n++;
        }
    }
    fclose(f);
    return 1;
}

/* --requests FILE: the player's requests as the window's menu and F4 make them ("N:SAVE",
 * "N:LOAD", "N:REWIND", "N:" none, by the handshakes since power on: a rewind sets the frame count
 * back), SAVE and LOAD at their handshake, REWIND until the next entry; --state FILE the file
 * they name */
static struct { long at; unsigned req; } *reqs;
static size_t reqs_n;
static long handshakes;
static const char *state_file;

static int read_requests(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); return 0; }
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char *colon = strchr(line, ':');
        if (!colon) continue;
        reqs = realloc(reqs, (reqs_n + 1) * sizeof *reqs);
        reqs[reqs_n].at = atol(line);
        reqs[reqs_n].req = strstr(colon, "SAVE") ? MDRT_REQUEST_SAVE : strstr(colon, "LOAD") ? MDRT_REQUEST_LOAD
                         : strstr(colon, "REWIND") ? MDRT_REQUEST_REWIND : 0;
        reqs_n++;
    }
    fclose(f);
    return 1;
}

static unsigned script_requests(void *user)
{
    (void)user;
    long h = ++handshakes;
    unsigned r = 0;
    for (size_t i = 0; i < reqs_n && reqs[i].at <= h; i++) r = reqs[i].at == h ? reqs[i].req : reqs[i].req & MDRT_REQUEST_REWIND;
    return r;
}

static const char *script_request_path(void *user) { (void)user; return state_file; }

/* netplay's local player, headless: this pad's column of the script */
static uint16_t script_pad(int pad, int k)
{
    (void)k;
    long frame = native_frames();
    uint16_t b = 0;
    for (size_t i = 0; i < script_n && script[i].frame <= frame; i++) b = pad < 2 ? script[i].pads[pad] : 0;
    return b;
}

/* netplay's local player in the window: the window's pad 1 controls (pad 2's for a second pad) */
static uint16_t window_pad(int pad, int k)
{
    (void)pad;
    uint16_t b[MDRT_PADS];
    mdrt_platform_input(native_m, b);
    return b[k];
}

static void script_source(void *user, uint16_t b[2])
{
    (void)user;
    long frame = native_frames();
    b[0] = b[1] = 0;
    for (size_t i = 0; i < script_n && script[i].frame <= frame; i++) {
        b[0] = script[i].pads[0];
        b[1] = script[i].pads[1];
    }
}

/* a routine the game reached that is not ported: the game stops there */
void call68k_missing(uint32_t addr)
{
    fprintf(stderr, "frame %ld: routine $%06X is not ported yet\n", native_frames(), addr & 0xFFFFFF);
    native_quit();
}

void call_object_missing(uint32_t addr, uint16_t obj)
{
    fprintf(stderr, "frame %ld: object type routine $%06X (slot $%04X, type $%03X) is not ported yet\n",
            native_frames(), addr & 0xFFFFFF, obj, rd16(obj));
    native_quit();
}

static uint8_t read68k(void *user, uint32_t addr)
{
    (void)user;
    addr &= 0xFFFFFF;
    if (addr >= 0xFF0000) return ram[addr & 0xFFFF];
    if (addr < ROM_SIZE) return rom8(addr);
    return 0;
}

static void write_ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); return; }
    fprintf(f, "P6\n%d %d\n255\n", pic_w, pic_h);
    for (int i = 0; i < pic_w * pic_h; i++) {
        uint32_t rgb = mdrt_vdp_rgb(picture[i], 0);
        uint8_t px[3] = { (uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb };
        fwrite(px, 1, 3, f);
    }
    fclose(f);
}

/* --golden FILE: a line a frame, at its handshake (before its vertical blank code): the frame,
 * hashes of work RAM, the VDP's state, the Z80's RAM, the picture and the frame's sound, and the
 * sound's samples, so that two builds can be compared frame for frame, bit for bit (make golden,
 * make golden-check: tools/golden.py) */
static FILE *golden;
static uint64_t golden_pic, golden_snd;
static int golden_samples;

static uint64_t fnv(uint64_t h, const void *p, size_t n)
{
    const uint8_t *b = p;
    for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 0x100000001B3ull;
    return h;
}
#define FNV0 0xCBF29CE484222325ull

static void golden_audio(void *user, const int16_t *s, int n)
{
    (void)user;
    golden_snd = fnv(FNV0, s, (size_t)n * 4);
    golden_samples = n;
}

static void golden_line(long frame)
{
    mdrt_vdp_state s;
    mdrt_vdp_get_state(mdrt_get_vdp(native_m), &s);
    uint64_t v = fnv(fnv(fnv(fnv(FNV0, s.reg, 24), s.vram, 0x10000), s.cram, sizeof s.cram), s.vsram, sizeof s.vsram);
    fprintf(golden, "%ld %016llx %016llx %016llx %016llx %016llx %d\n", frame, (unsigned long long)fnv(FNV0, ram, RAM_SIZE),
            (unsigned long long)v, (unsigned long long)fnv(FNV0, z80_mem, sizeof z80_mem), (unsigned long long)golden_pic,
            (unsigned long long)golden_snd, golden_samples);
}

static void keep_picture(void *user, const uint16_t *px, int w, int h, int pitch)
{
    (void)user;
    if (golden) {
        golden_pic = FNV0;
        for (int y = 0; y < h; y++) golden_pic = fnv(golden_pic, px + y * pitch, (size_t)w * 2);
    }
    pic_w = w > 320 ? 320 : w;
    pic_h = h > 240 ? 240 : h;
    for (int y = 0; y < pic_h; y++) memcpy(picture + y * pic_w, px + y * pitch, (size_t)pic_w * 2);
}

/* --hold, --poke: RAM words (hex addresses in work RAM) */
static struct { long frame; uint16_t addr, value; } *pokes;
static size_t npokes, nholds;
static struct { uint16_t addr, value, when_addr, when_value; int when; } *holds;

static int read_holds(const char *spec, int when)
{
    for (const char *p = spec; *p;) {
        unsigned a, v, wa = 0, wv = 0;
        int n;
        if (when) {
            if (sscanf(p, "%x=%x:%n", &wa, &wv, &n) != 2) return 0;
            p += n;
        }
        if (sscanf(p, "%x=%x%n", &a, &v, &n) != 2) return 0;
        holds = realloc(holds, (nholds + 1) * sizeof *holds);
        holds[nholds].addr = (uint16_t)a;
        holds[nholds].when = when;
        holds[nholds].when_addr = (uint16_t)wa;
        holds[nholds].when_value = (uint16_t)wv;
        holds[nholds++].value = (uint16_t)v;
        p += n;
        if (*p == ',') p++;
    }
    return 1;
}

static int read_pokes(const char *spec)
{
    for (const char *p = spec; *p;) {
        long f;
        int n;
        if (sscanf(p, "%ld:%n", &f, &n) != 1) return 0;
        p += n;
        while (*p && *p != ';') {
            unsigned a, v;
            if (sscanf(p, "%x=%x%n", &a, &v, &n) != 2) return 0;
            pokes = realloc(pokes, (npokes + 1) * sizeof *pokes);
            pokes[npokes].frame = f;
            pokes[npokes].addr = (uint16_t)a;
            pokes[npokes++].value = (uint16_t)v;
            p += n;
            if (*p == ',') p++;
        }
        if (*p == ';') p++;
    }
    return 1;
}

void native_frame_done(long frame)
{
    if (golden) golden_line(frame);
    for (size_t i = 0; i < npokes; i++)
        if (pokes[i].frame == frame) wr16(pokes[i].addr, pokes[i].value);
    for (size_t i = 0; i < nholds; i++)
        if (!holds[i].when || rd16(holds[i].when_addr) == holds[i].when_value) wr16(holds[i].addr, holds[i].value);
    static int trace = -1;
    if (trace < 0) trace = getenv("GUNSTAR_TRACE") != NULL;
    if (trace && frame % 30 == 0)
        fprintf(stderr, "frame %ld: mode %02X step %02X stage %02X fade %04X/%04X display %02X timer %04X jobs %04X\n", frame,
                rd16(v_mode), rd16(v_modeState), rd16(0xA204), rd16(v_fadeSpeed), rd16(v_fadeLevel), rd8(v_displayOn),
                rd16(v_modeTimer), rd16(0xF720));
    if (shots_dir && frame % shots_every == 0) {
        char path[1024];
        snprintf(path, sizeof path, "%s/%06ld.ppm", shots_dir, frame);
        write_ppm(path);
    }
    if (stop_after >= 0 && frame >= stop_after && !native_net_on()) native_quit();   /* (netplay: net.c ends it) */
}

void native_game(mdrt *m, void *user)
{
    (void)user;
    if (setjmp(out)) return;
    native_hal_start(m);
    for (uint32_t a = 0; a < RAM_SIZE; a++) ram[a] = 0;
    native_states_init(m);
    native_net_start(m);                                  /* (netplay: from power on) */
    if (setjmp(again)) MainLoop();                        /* (a state loaded: on from MainLoop's wait) */
    Start();
    MainLoop();
}

/* an option that changes the simulation, into netplay's identity */
static void add_setup(char *setup, size_t size, const char *opt, const char *value)
{
    size_t k = strlen(setup);
    snprintf(setup + k, size - k, "%s %s ", opt, value);
}

static const char *rom_ok(const uint8_t *b, size_t n, void *user)
{
    (void)user;
    return rom_check(b, (long)n) ? NULL : "not Gunstar Heroes (U), 1 MB";
}

static void rom_use(const uint8_t *b, size_t n, void *user) { (void)n; (void)user; rom_image = b; }

static void sink(void *user, const void *bytes, size_t n) { fwrite(bytes, 1, n, (FILE *)user); }

int main(int argc, char **argv)
{
    const char *rom = "Gunstar Heroes (U) [!].gen", *rom_given = NULL;
    const char *sound_path = NULL, *save_path = NULL, *load_path = NULL, *fm_core = NULL, *golden_path = NULL;
    long save_frame = -1;
    int headless = 0, scale = 0;
    mdrt_net_options netopt;
    mdrt_net_options_init(&netopt);
    char setup[2048] = "", err[256];                      /* (the options that change the simulation: netplay's identity) */
    int net;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--version")) { printf("gunstar %s (gunstars: Gunstar Heroes, rebuilt in C)\n", GUNSTAR_VERSION); return 0; }
        else if (!strcmp(argv[i], "--rom") && i + 1 < argc) rom = rom_given = argv[++i];
        else if (!strcmp(argv[i], "--headless")) headless = 1;
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) stop_after = atol(argv[++i]);
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot_path = argv[++i];
        else if (!strcmp(argv[i], "--shots") && i + 1 < argc) shots_dir = argv[++i];
        else if (!strcmp(argv[i], "--every") && i + 1 < argc) shots_every = atol(argv[++i]);
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--input") && i + 1 < argc) { if (!read_script(argv[++i])) return 1; }
        else if (!strcmp(argv[i], "--sound") && i + 1 < argc) sound_path = argv[++i];
        else if (!strcmp(argv[i], "--fm-core") && i + 1 < argc) fm_core = argv[++i];
        else if (!strcmp(argv[i], "--golden") && i + 1 < argc) { golden_path = argv[++i]; headless = 1; }
        else if (!strcmp(argv[i], "--save-state") && i + 1 < argc && strchr(argv[i + 1], ':')) {
            save_frame = atol(argv[++i]);
            save_path = strchr(argv[i], ':') + 1;
        } else if (!strcmp(argv[i], "--load-state") && i + 1 < argc) load_path = argv[++i];
        else if (!strcmp(argv[i], "--requests") && i + 1 < argc) { if (!read_requests(argv[++i])) return 1; }
        else if (!strcmp(argv[i], "--state") && i + 1 < argc) state_file = argv[++i];
        else if (!strcmp(argv[i], "--hold") && i + 1 < argc) { add_setup(setup, sizeof setup, argv[i], argv[i + 1]); if (!read_holds(argv[++i], 0)) { fprintf(stderr, "bad --hold\n"); return 2; } }
        else if (!strcmp(argv[i], "--hold-if") && i + 1 < argc) { add_setup(setup, sizeof setup, argv[i], argv[i + 1]); if (!read_holds(argv[++i], 1)) { fprintf(stderr, "bad --hold-if\n"); return 2; } }
        else if (!strcmp(argv[i], "--poke") && i + 1 < argc) { add_setup(setup, sizeof setup, argv[i], argv[i + 1]); if (!read_pokes(argv[++i])) { fprintf(stderr, "bad --poke\n"); return 2; } }
        else if ((net = mdrt_net_parse(&netopt, argc, argv, &i, err, sizeof err)) < 0) { fprintf(stderr, "%s\n", err); return 2; }
        else if (!net) {
            fprintf(stderr, "usage: %s [--version] [--rom FILE] [--headless] [--frames N] [--shot FILE.ppm] [--shots DIR --every N] [--scale N] [--input FILE] [--sound FILE.sndtrace] [--golden FILE] [--fm-core nuked|ymfm] [--save-state FRAME:FILE] [--load-state FILE] [--requests FILE --state FILE] [--hold ADDR=VAL,...] [--hold-if WADDR=WVAL:ADDR=VAL,...] [--poke FRAME:ADDR=VAL,...;...] %s\n", argv[0], MDRT_NET_USAGE);
            return 2;
        }
    }
    if (netopt.on && (load_path || reqs_n)) { fprintf(stderr, "netplay starts at power on: no --load-state or --requests\n"); return 2; }
    mdrt_config c = { 0 };
    c.audio_rate = 48000;
    c.read68k = read68k;
    c.fm_core = fm_core;
    if (!headless) {
        mdrt_sdl_options o = { .title = "Gunstar Heroes", .scale = scale, .app = "gunstar",
                               .rom_path = rom_given, .rom_default = rom,
                               .rom_hint = "Gunstar Heroes (U): the 1 MB cartridge",
                               .rom_check = rom_ok, .rom_use = rom_use };
        o.net_identity = native_net_identity(GUNSTAR_VERSION, setup);   /* (the menu's Online page) */
        if (netopt.on) {                                  /* (the window connects, then powers on) */
            native_input_fn local = script_n ? script_pad : window_pad;   /* (--input: the script's column, for tests) */
            if (!native_net_open(&netopt, GUNSTAR_VERSION, setup, local, err, sizeof err)) { fprintf(stderr, "netplay: %s\n", err); return 1; }
            o.net = native_net();
            o.net_udp = native_net_udp();
        }
        int r = mdrt_sdl_run(&c, &o, native_game, NULL);
        native_net_close();
        return r;
    }
    if (!rom_load(rom)) { fprintf(stderr, "cannot load the ROM %s\n", rom); return 1; }
    c.platform = (mdrt_platform){ .present = keep_picture };
    if (golden_path) {
        if (!(golden = fopen(golden_path, "w"))) { fprintf(stderr, "cannot write %s\n", golden_path); return 1; }
        c.platform.audio = golden_audio;
    }
    if (reqs_n) {
        c.platform.requests = script_requests;
        c.platform.request_path = script_request_path;
    }
    mdrt *m = mdrt_new(&c);
    if (!m) return 1;
    if (script_n) mdrt_set_pad_source(m, script_source, NULL);
    native_states_options(save_frame, save_path, load_path);
    if (netopt.on) {                                      /* (from power on: the session's pads replace the script's) */
        if (!native_net_open(&netopt, GUNSTAR_VERSION, setup, script_pad, err, sizeof err)) { fprintf(stderr, "netplay: %s\n", err); return 1; }
        if (!native_net_connect()) return 1;
        native_net_stop_at(stop_after);
    }
    FILE *sound = sound_path ? fopen(sound_path, "wb") : NULL;
    if (sound_path && !sound) { fprintf(stderr, "cannot write %s\n", sound_path); return 1; }
    if (sound) mdrt_record(m, MDRT_TRACE_SOUND, sink, sound);
    native_game(m, NULL);
    native_net_close();
    if (sound) {
        mdrt_record(m, MDRT_TRACE_SOUND, NULL, NULL);
        fclose(sound);
    }
    if (golden && fclose(golden)) { fprintf(stderr, "cannot write %s\n", golden_path); return 1; }
    if (shot_path) write_ppm(shot_path);
    if (getenv("GUNSTAR_TRACE")) {
        mdrt_vdp_state st;
        mdrt_vdp_get_state(mdrt_get_vdp(m), &st);
        fprintf(stderr, "VDP registers:");
        for (int i = 0; i < 24; i++) fprintf(stderr, " %02X", mdrt_vdp_reg(mdrt_get_vdp(m), i));
        fprintf(stderr, "\nCRAM:");
        for (int i = 0; i < 64; i++) fprintf(stderr, "%s%03X", i % 16 ? " " : "\n  ", st.cram[i]);
        fprintf(stderr, "\nRAM palette:");
        for (int i = 0; i < 64; i++) fprintf(stderr, "%s%03X", i % 16 ? " " : "\n  ", rd16((uint16_t)(v_palette + 2 * i)));
        fprintf(stderr, "\n");
    }
    printf("%ld frames\n", native_frames());
    if (reqs_n) printf("%ld handshakes\n", handshakes);
    mdrt_free(m);
    return 0;
}
