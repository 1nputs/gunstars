/* The native game: Gunstar Heroes on pulseman (ext/pulseman). The game's C runs on the
 * runtime's thread: the reset (Start), then the job loop (MainLoop), whose waits run the
 * vertical blank (VBlank, the whole frame's logic) at each handshake. */
#pragma once
#include <stdint.h>
#include "mdrt/mdrt.h"
#include "mdrt/net_udp.h"
#include "mdrt/platform_sdl2.h"

extern mdrt *native_m;                  /* the console */

/* the game's session: from power on, until it stops (main.c) */
void native_game(mdrt *m, void *user);
/* stop the session (the window closed, the run's frames done, a routine not ported): back out */
void native_quit(void);
/* after each frame's handshake: the frame count, for the run's stop and its pictures (main.c) */
void native_frame_done(long frame);
/* the HAL from power on (hal_native.c) */
void native_hal_start(mdrt *m);
long native_frames(void);
long native_handshakes(void);           /* (the session's: a state does not set it back) */
/* the HAL's part of a state (its clock and frame count): written to out (NULL: only the size) */
size_t native_hal_save(uint8_t *out);
int native_hal_load(const uint8_t *in, size_t size);
/* states (state.c): the console's parts from power on; the headless options; at every wait once
 * the frame's vertical blank code has run (at_main: MainLoop's) */
void native_states_init(mdrt *m);
void native_states_options(long save_frame, const char *save_path, const char *load_path);
void native_states_wait(int at_main);
/* the game from MainLoop again, after a state was loaded (main.c; does not return) */
void native_restart(void);
/* netplay (net.c): the session the options ask for (version and setup for the identity: the
 * build's, and the command line's options that change the simulation); input gives this player's
 * buttons now for pad (0-3), the k-th pad this peer plays. Headless: connect, then power on. At
 * every wait once the vertical blank code has run, native_net_wait (it may restart the game);
 * native_net_stop_at: a headless run's last frame. */
typedef uint16_t (*native_input_fn)(int pad, int k);
int native_net_open(const mdrt_net_options *o, const char *version, const char *setup, native_input_fn input, char *err, size_t errsize);
int native_net_on(void);
int native_net_connect(void);
void native_net_start(mdrt *m);
void native_net_stop_at(long frame);
void native_net_wait(int main_wait);
void native_net_close(void);
const char *native_net_identity(const char *version, const char *setup);   /* (the window's Online page's) */
mdrt_net *native_net(void);                     /* (the command line's session, and its UDP link, for the window) */
mdrt_udp *native_net_udp(void);
