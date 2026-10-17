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
