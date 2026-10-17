/* The vertical-blank interrupt and what it runs every frame. */
#include "game.h"

/* $000BB2. The vertical blank, where the game runs. With the last frame's logic finished
 * (v_frameReady), a whole frame: the VDP's registers and DMAs, the pads, the pause, the sound,
 * the random number, the game, the fade, the scroll tables queued for the next blank; the
 * interrupt mask lowered to 3 meanwhile, so a vertical blank can come during the game's logic.
 * When that happens (the logic still running), only the H-interrupt effect, register 0 and the
 * sound: a frame the game lags. On a PAL console (v_consoleVersion bit 6) the 68k waits a while
 * first. */
void VBlank(void)
{
    cpu_set_sr(0x2700);
    while (!(vdp_status() & 8)) {}
    HintEffect();
    VdpWriteReg0();
    if (!rd8(v_frameReady)) {
        cpu_set_sr(0x2300);
        UpdateSound();
        return;
    }
    wr8(v_frameReady, 0);
    VdpWriteRegs();
    VBlankDma();
    ReadPads();
    cpu_set_sr(0x2300);
    CheckPause();
    UpdateSound();
    Random();
    GameFrame();
    FadePalette();
    QueueHScroll();
    QueueVScroll();
    wr8(v_frameReady, 1);
}

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
