/* The game's modes: each frame GameFrame runs the routine the mode table names for v_mode (an
 * index times 4): the logos, the title, the menus, the stages, the ending. A mode's routine keeps
 * its own sub-state in v_modeState. */
#include "game.h"

#define MODE_TABLE 0x000CE2u
#define STAGE_START_TABLE 0x00390Eu      /* word offsets, one per stage (v_stage4 / 2) */
#define STAGE_FRAME_TABLE 0x003934u

/* $000CCA */
void GameFrame(void)
{
    if (rd16(v_modeTimer)) wr16(v_modeTimer, (uint16_t)(rd16(v_modeTimer) - 1));
    wr16(v_frameCount, (uint16_t)(rd16(v_frameCount) + 1));
    call68k(rom32(MODE_TABLE + (uint32_t)(int16_t)rd16(v_mode)));
}

/* $00048C. Mode 0, after the reset: an overseas NTSC console goes on to the logos (mode 4);
 * a Japanese or PAL one gets a screen of text, for ever. */
void Mode0(void)
{
    uint8_t version = rd8(v_consoleVersion);
    if ((version & 0x80) && !(version & 0x40)) {
        wr16(v_mode, 4);
        wr16(v_modeState, 0);
        return;
    }
    if (rd16(v_modeState)) return;
    wr16(v_fadeColor, 0);
    ScreenReset();
    QueueSound(0xF9);
    RunCommands(0x01B9D0);
    LoadPalettes(0x01BA36);
    DrawText(0x000506);
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
    set8(VDP_REG(1) + 1, 6);
    wr8(v_displayOn, 0x80);
    wr16(v_fadeSpeed, 0xFF00);
    wr16(v_fadeLevel, 0x1000);
}

/* a table of word offsets from itself, indexed by the stage */
static uint32_t stage_routine(uint32_t table)
{
    uint32_t at = table + (uint32_t)(int16_t)((int16_t)rd16(v_stage4) >> 1);
    return at + (uint32_t)(int16_t)rom16(at);
}

/* $003900. The stage's set-up. */
void StageStart(void)
{
    call68k(stage_routine(STAGE_START_TABLE));
}

/* $00391C. The stage's frame, unless paused. */
void StageFrame(void)
{
    if (rd8(v_pauseFlags) & 0x80) return;
    call68k(stage_routine(STAGE_FRAME_TABLE));
}
