/* The native game: Gunstar Heroes on pulseman (ext/pulseman). The game's C runs on the
 * runtime's thread: the reset (Start), then the job loop (MainLoop), whose waits run the
 * vertical blank (VBlank, the whole frame's logic) at each handshake. */
#pragma once
#include <stdint.h>
#include "mdrt/mdrt.h"
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
