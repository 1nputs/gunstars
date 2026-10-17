/* The demo (modes $10 and $14): one player playing a stage from recorded input, four demos in
 * turn (v_gameMode); Start goes back to the title. */
#include "game.h"

/* $01CC9C. The demo's stage and where in it, by which demo this is. */
void DemoStage(void)
{
    uint16_t k = rd16(v_gameMode) & 6;
    uint16_t stage = rom16(0x01CCCC + k);
    wr16(v_stage4, stage);
    wr16((uint16_t)(v_checkpoints + (uint16_t)((int16_t)stage >> 1)), rom16(0x01CCD4 + k));
    wr16(0xA46E, rom16(0x01CCDC + k));
    wr16(0xA470, rom16(0x01CCE4 + k));
    wr16(0xA4CA, rom16(0x01CCEC + k));
}

/* $01CCF4. Its recorded input (or, recording, the buffer at $FFAA00). */
void DemoInput(void)
{
    wr16(0xA474, 0x10);
    wr8(0xA4C6, 0);
    if (rd8(v_demoRecord)) {
        wr32(0xA4C2, 0xFFFFAA00);
        return;
    }
    wr32(0xA4C2, rom32(0x01CD22 + (uint32_t)((rd16(v_gameMode) & 6) << 1)));
}

/* $01CBF4. Mode $10: the demo set up (player 1, Red with the second weapon, the top difficulty). */
void DemoStart(void)
{
    ScreenReset();
    wr16(v_mode, 0x14);
    wr16(v_modeState, 0);
    wr8(VDP_REG(7) + 1, 0x10);
    wr32(v_randomSeed, 0);
    wr16(v_difficulty, 6);
    wr8(v_players, 1);
    wr16(0xA220, 0);
    wr16(0xA224, 4);
    Player1JoinReset();
    wr16(v_player1 + 0xD4, 0xC);
    DemoStage();
    if (rd8(v_demoRecord))
        for (uint16_t a = 0xAA00; a < 0xB200; a += 4) wr32(a, 0);
    StageStart();
    DemoInput();
    PlayersUpdate();
    HudStart();
    clr8(v_pauseFlags, 6);
    wr16(v_modeTimer, 0x400);
    wr16(v_gameMode, (uint16_t)(rd16(v_gameMode) + 2));
    set8(VDP_REG(1) + 1, 6);
    wr8(v_displayOn, 0x80);
    wr16(v_fadeSpeed, 0xFE00);
    wr16(v_fadeLevel, 0x1000);
    QueueSound(0x9B);
}

/* $01CD42. Mode $14: the demo, until Start (the title) or the timer or the player's end (a fade,
 * then the SEGA logo). */
void Demo(void)
{
    if (!rd16(v_fadeSpeed)) {
        if (rd16(v_fadeLevel)) {
            wr16(v_mode, 4);
            wr16(v_modeState, 0);
            return;
        }
        if ((rd8(v_padPressed) | rd8(v_padPressed + 1)) & 0x80) {
            clr8(VDP_REG(1) + 1, 6);
            wr8(v_displayOn, 0);
            wr16(v_mode, 8);
            wr16(v_modeState, 0);
            return;
        }
        if (!rd8(v_players) || !rd16(v_modeTimer)) {
            wr16(v_fadeSpeed, 0x200);
            wr16(v_fadeLevel, 0);
        }
    }
    ObjectsMove();
    CameraMove();
    Collisions();
    SpritesStart();
    DrawListStart();
    PlayersUpdate();
    EffectsUpdate();
    StageFrame();
    DifficultyUpdate();
    ObjectsUpdate();
    DrawListEnd();
    HudUpdate();
    ScreenShake();
    BuildSprites();
    ScrollFrame();
    wr16(v_gameFrames, (uint16_t)(rd16(v_gameFrames) + 1));
}
