/* The vertical-blank interrupt and what it runs every frame. */
#include "game.h"

/* $000C36. Start pressed on a pad that may pause, while pausing is allowed, pauses the game, or
 * resumes it, and asks the sound driver to do the same. */
void CheckPause(void)
{
    uint8_t flags = rd8(v_pauseFlags);
    if (!(flags & 0x40)) return;
    uint8_t pressed = 0;
    if (flags & 0x01) pressed |= rd8(v_padPressed);
    if (flags & 0x02) pressed |= rd8(v_padPressed + 1);
    if (!(pressed & 0x80)) return;
    if (!(flags & 0x80)) {
        wr8(v_pauseFlags, flags | 0x80);
        wr8(v_soundPause, 0x01);
    } else {
        wr8(v_pauseFlags, flags & 0x7F);
        wr8(v_soundPause, 0x80);
    }
}
