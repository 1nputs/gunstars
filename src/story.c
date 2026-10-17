/* The story (mode $18 after the title, $4C after the title's second showing): pictures with
 * text, each a step pair (its set-up, then its frames until it fades out); Start skips to the
 * title. The pictures' actors are scene objects (actors.c), the text the text box (type $4F8). */
#include "game.h"

#define SCROLL_ROWS 0xE482       /* plane B's scroll by rows of 8 lines ($20 bytes apart) */

/* loc_01DD16: Start: back to the title */
static void story_skip(void)
{
    wr16(v_fadeColor, 0);
    QueueSound(0xF6);
    clr8(VDP_REG(1) + 1, 6);
    wr8(v_displayOn, 0);
    wr16(v_mode, 8);
    wr16(v_modeState, 0);
}

/* the pictures' common part: faded out, the next step; Start skips; the timer out, the fade
 * (white for `white`, with the sound for `sound`). 1: the frame ends here */
static int scene_end(uint16_t speed, int white, int sound)
{
    if (rd16(v_fadeSpeed)) return 0;
    if (rd16(v_fadeLevel)) {
        wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
        return 1;
    }
    if ((rd8(v_padPressed) | rd8(v_padPressed + 1)) & 0x80) {
        story_skip();
        return 1;
    }
    if (rd16(v_modeTimer)) return 0;
    if (sound) QueueSound(0xF6);
    if (white) wr16(v_fadeColor, 0xEEE);
    wr16(v_fadeSpeed, speed);
    wr16(v_fadeLevel, 0);
    return 0;
}

/* a picture's start: the display on, fading in */
static void scene_show(void)
{
    set8(VDP_REG(1) + 1, 6);
    wr8(v_displayOn, 0x80);
    wr16(v_fadeSpeed, 0xFF00);
    wr16(v_fadeLevel, 0x1000);
}

/* a picture's tiles: the shared ones, the stage's (`stage`), the picture's own, its palettes */
static void scene_load(uint32_t stage, uint32_t extra, uint32_t own, uint32_t palettes)
{
    ClearObjectRam();
    RunCommands(0x008FC0);
    StageTilesJapan();
    RunCommands(stage);
    if (extra) RunCommands(extra);
    RunCommands(own);
    LoadPalettes(palettes);
}

/* a picture's planes: the camera put, both planes drawn, nothing moving */
static void scene_place(uint16_t ax, uint16_t ay, uint16_t bx, uint16_t by)
{
    CameraPlace(ax, ay, bx, by);
    PlaneARedraw();
    PlaneBRedraw();
    SceneStill();
}

/* an object made a scene actor: its type and its scene */
static void actor(uint16_t slot, uint16_t type, uint16_t scene)
{
    wr16(slot + O_TYPE, type);
    wr16(slot + 0x48, scene);
    wr16(slot + O_STATE, 0);
}

/* the players and the text box's speaker as actors in scene `scene` */
static void scene_players(uint16_t scene)
{
    actor(v_player1, 0x5BC, scene);
    actor(v_player2, 0x5C0, scene);
}

/* plane B's rows scrolled: `n` rows from `row` by `by` */
static uint16_t rows(uint16_t row, int n, uint16_t by)
{
    for (int k = 0; k < n; k++, row = (uint16_t)(row + 0x20)) wr16(row, by);
    return row;
}

/* $01DD66. Nothing scrolling. */
void SceneStill(void)
{
    wr32(v_camSpeedX, 0);
    wr32(v_planeSpeeds, 0);
    wr32(v_camSpeedY, 0);
    wr32(0xA934, 0);
    wr32(0xA918, 0);
    wr32(0xA938, 0);
    wr32(0xA91C, 0);
    wr32(0xA93C, 0);
    wr8(VDP_REG(11) + 1, 0);
    wr8(v_planeFlagsA, 0);
    wr8(v_planeFlagsB, 0);
}

/* $01DE32. The text box's tiles in VRAM cleared ($480 to $8800... eight rows of $80 bytes). */
void SceneTextClear(void)
{
    for (uint32_t k = 0; k < 8; k++) {
        uint16_t sr = cpu_get_sr();
        cpu_set_sr(0x2700);
        vdp_ctrl16(0x8F02);
        vdp_ctrl(0x44800000 + (k << 23));
        for (int i = 0; i < 0x40; i++) vdp_data(0);
        cpu_set_sr(sr);
    }
}

/* the text box (slot $CF80) with text `text`, its tiles' word `look` */
static void scene_text(uint16_t text, uint16_t look)
{
    uint16_t box = 0xCF80;
    wr16(box + 0x48, text);
    wr16(box + O_TYPE, 0x4F8);
    wr16(box + O_STATE, 0);
    wr16(box + 0x0E, look);
    wr16(box + 0x50, 4);
    wr16(box + O_X, 0x120);
    wr16(box + O_Y, 0x144);
    wr8(box + 0x58, 0xAD);
    wr8(box + 0x59, 0);
    SceneTextClear();
}

/* $01DD94 */
void SceneText(uint16_t text)
{
    scene_text(text, 0x8200);
}

/* $01DDCA */
void SceneTextB(uint16_t text)
{
    scene_text(text, 0x816D);
}

/* $01DE00 */
void SceneTextC(uint16_t text)
{
    scene_text(text, 0x8480);
}

/* $01DFAC. Two sprites ($A100's list) at (x, y) and below it. */
void SceneSprite(uint16_t x, uint16_t y)
{
    static const uint16_t rest[2][2] = { { 0, 1 }, { 0, 0 } };
    uint16_t a = 0xA100;
    for (int k = 0; k < 2; k++) {
        wr16(a, x);
        wr16((uint16_t)(a + 2), (uint16_t)(y + 4 * k));
        wr16((uint16_t)(a + 4), rest[k][0]);
        wr16((uint16_t)(a + 6), rest[k][1]);
        a = (uint16_t)(a + 8);
    }
    wr16(a, 0xFFFF);
    DrawSpriteList(0xFFFFA100);
}

/* $01DFD8. count*16 tiles of a plane's map (at `map`) to the front (priority). */
void PlanePriority(uint32_t map, uint16_t count)
{
    uint16_t a = (uint16_t)map;
    for (uint16_t k = (uint16_t)(count - 1);; k--) {
        for (int i = 0; i < 16; i++, a = (uint16_t)(a + 2)) wr16(a, rd16(a) | 0x8000);
        if (k == 0) break;
    }
}

/* ---- mode $18's pictures ---- */

/* $01CFA2. The planet, the two heroes. */
void StoryA1Start(void)
{
    scene_load(0x0093C8, 0, 0x01E004, 0x01E12C);
    scene_place(0x1480, 0x30, 0, 0x30);
    wr8(VDP_REG(11) + 1, 2);
    wr8(v_planeFlagsA, 0);
    wr8(v_planeFlagsB, 0x80);
    wr32(v_layout, 0x01B45A);
    wr8(0xF765, 0);
    wr32(v_camX, 0);
    wr32(0xA104, 0);
    wr32(0xA108, 0);
    SceneText(0x74);
    scene_players(0);
    actor(0xCFE0, 0x5D0, 0);
    wr16(v_modeTimer, 0x280);
    scene_show();
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
    StoryA1();
}

/* $01D094. The sky's layers sliding at three speeds. */
void StoryA1(void)
{
    if (scene_end(0x100, 0, 0)) return;
    wr32(v_camX, rd32(v_camX) + 0x80000);
    wr32(0xA104, rd32(0xA104) + 0x4000);
    wr32(0xA108, rd32(0xA108) + 0xC0000);
    uint16_t row = rows(SCROLL_ROWS, 6, 0);
    row = rows(row, 5, (uint16_t)-rd16(0xA104));
    rows(row, 8, (uint16_t)-rd16(0xA108));
}

/* $01D142. The house in the hills, looked down on. */
void StoryA2Start(void)
{
    scene_load(0x00912E, 0, 0x01E02E, 0x00968C);
    scene_place(0xD20, 0xB40, 0x200, 0xA00);
    wr8(v_planeFlagsA, 0x30);
    wr32(v_layout, 0x01B45A);
    wr8(0xF765, 0);
    SceneText(0xFFFF);
    scene_players(4);
    actor(0xCFE0, 0x5D0, 4);
    wr16(v_modeTimer, 0x2C0);
    wr16(0xA29A, 0x20);
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
    wr16(v_sceneStep, 0);
    scene_show();
    StoryA2();
}

/* loc_01D30C: the camera speeding up its climb (to 16 a frame, plane B to 8) */
static void climb(void)
{
    int32_t v = (int32_t)(rd32(v_camSpeedY) - 0x2000);
    if (v < (int32_t)0xFFF00000) v = (int32_t)0xFFF00000;
    wr32(v_camSpeedY, (uint32_t)v);
    wr32(0xA934, (uint32_t)v);
    v = (int32_t)(rd32(0xA91C) - 0x1000);
    if (v < (int32_t)0xFFF80000) v = (int32_t)0xFFF80000;
    wr32(0xA91C, (uint32_t)v);
    wr32(0xA93C, (uint32_t)v);
}

/* $01D2E0. Climbing to the hill's top: there, still. */
void StoryA2Top(void)
{
    if ((int32_t)rd32(0xA924) > 0x1B00000) {
        climb();
        return;
    }
    wr16(v_sceneStep, (uint16_t)(rd16(v_sceneStep) + 4));
    wr32(v_camSpeedY, 0);
    wr32(0xA934, 0);
    wr32(0xA91C, 0);
    wr32(0xA93C, 0);
    wr8(0xF765, 0);
    wr16(0xCFC8, 0x78);
}

/* $01D2C4. Climbing past the house: the hill's tiles. */
void StoryA2Climb(void)
{
    if ((int32_t)rd32(0xA92C) > 0x6800000) {
        climb();
        return;
    }
    wr16(v_sceneStep, (uint16_t)(rd16(v_sceneStep) + 4));
    uint32_t list = 0x00922A;
    NextJob(&list);
    StoryA2Top();
}

/* $01D284. $40 frames, then the climb (the gem's glint, $644, at $A800). */
void StoryA2Wait(void)
{
    wr16(0xA29A, (uint16_t)(rd16(0xA29A) - 1));
    if (!(rd16(0xA29A) & 0x8000)) return;
    wr16(v_sceneStep, (uint16_t)(rd16(v_sceneStep) + 4));
    wr32(v_camSpeedY, 0xFFFF8000);
    wr32(0xA934, 0xFFFF8000);
    wr32(0xA91C, 0xFFFF8000);
    wr32(0xA93C, 0xFFFF8000);
    actor(0xA800, 0x644, 0);
    wr8(0xF765, 1);
    StoryA2Climb();
}

/* $01D270. $20 frames. */
void StoryA2Pause(void)
{
    wr16(0xA29A, (uint16_t)(rd16(0xA29A) - 1));
    if (!(rd16(0xA29A) & 0x8000)) return;
    wr16(v_sceneStep, (uint16_t)(rd16(v_sceneStep) + 4));
    wr16(0xA29A, 0x40);
    StoryA2Wait();
}

/* $01D30A */
void StoryA2Done(void)
{
}

/* $01D21A */
void StoryA2(void)
{
    if (scene_end(0x100, 0, 0)) return;
    call68k(rom32(0x01D25C + (uint32_t)(int16_t)rd16(v_sceneStep)));
}

/* $01D34E. The house, the brothers and the soldiers. */
void StoryA3Start(void)
{
    scene_load(0x00912E, 0x00922A, 0x01E058, 0x00968C);
    scene_place(0xA00, 0x308, 0x200, 0x220);
    wr8(VDP_REG(11) + 1, 2);
    wr8(v_planeFlagsA, 0);
    wr8(v_planeFlagsB, 0x80);
    wr32(v_layout, 0x01B45A);
    wr8(0xF765, 0);
    SceneText(0x7C);
    scene_players(8);
    actor(0xCFE0, 0x5D0, 8);
    actor(0xD0A0, 0x5CC, 0);
    actor(0xD100, 0x5CC, 4);
    actor(0xD160, 0x5CC, 8);
    actor(0xD040, 0x648, 0);
    wr16(v_modeTimer, 0x2C0);
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
    scene_show();
    StoryA3();
}

/* $01D474. The sprite pair, the clouds at three speeds. */
void StoryA3(void)
{
    if (scene_end(0x80, 0, 0)) return;
    SceneSprite(0x106, 0x3A0);
    wr32(0xA180, rd32(0xA180) + 0x20000);
    wr32(0xA184, rd32(0xA184) + 0x30000);
    wr32(0xA188, rd32(0xA188) + 0x40000);
    uint16_t row = rows(SCROLL_ROWS, 8, 0xA0);
    row = rows(row, 4, (uint16_t)-rd16(0xA180));
    row = rows(row, 5, (uint16_t)-rd16(0xA184));
    rows(row, 2, (uint16_t)-rd16(0xA188));
}

/* $01D656. Seven of the empire's figures ($64C). */
void StoryA4Actors(void)
{
    for (uint16_t k = 0; k < 7; k++) actor(rom16(0x01D676 + 2 * k), 0x64C, rom16(0x01D684 + 2 * k));
}

/* $01D534. The empire's base. */
void StoryA4Start(void)
{
    scene_load(0x00901A, 0, 0x01E082, 0x00966C);
    scene_place(0x80, 0x710, 0x200, 0x80);
    wr32(0xA918, 0x60000);
    wr32(v_layout, 0x01B45A);
    wr8(0xF765, 0);
    SceneTextB(0x80);
    actor(0xCFE0, 0x5C4, 0);
    actor(0xD040, 0x648, 4);
    StoryA4Actors();
    wr16(v_modeTimer, 0x2C0);
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
    scene_show();
    StoryA4();
}

/* $01D5F8. A fade to white; a sound $C0 frames before. */
void StoryA4(void)
{
    if (scene_end(0x80, 1, 0)) return;
    if (rd16(v_modeTimer) == 0xC0) QueueSound(0xF6);
    SceneSprite(0x11D, 0x1A0);
}

/* $01DCEE. A pause of $40 frames. */
void StoryPauseStart(void)
{
    wr16(v_modeTimer, 0x40);
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
    StoryPause();
}

/* $01DCF8 */
void StoryPause(void)
{
    if ((rd8(v_padPressed) | rd8(v_padPressed + 1)) & 0x80) {
        story_skip();
        return;
    }
    if (rd16(v_modeTimer)) return;
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
}

/* $01DD3A. The title again (mode $50). */
void StoryToTitle(void)
{
    clr8(VDP_REG(1) + 1, 6);
    wr8(v_displayOn, 0);
    wr16(v_mode, 0x50);
    wr16(v_modeState, 0);
}

/* $01DD50. The demo (mode $10). */
void StoryToDemo(void)
{
    clr8(VDP_REG(1) + 1, 6);
    wr8(v_displayOn, 0);
    wr16(v_mode, 0x10);
    wr16(v_modeState, 0);
}

/* ---- mode $4C's pictures ---- */

/* $01D778. The empire's soldiers ($620) on parade. */
void StoryB1Actors(void)
{
    actor(0xD040, 0x620, 0);
    wr32(0xD048, 0x0DFC6E);
    wr16(0xD04C, 0);
    wr16(0xD050, 0xE0);
    wr16(0xD050, 0xE0);
    for (uint16_t k = 0; k < 3; k++) {
        uint16_t s = rom16(0x01D7D0 + 2 * k);
        actor(s, 0x620, 0x24);
        wr16(s + O_X, rom16(0x01D7D6 + 2 * k));
        wr16(s + O_Y, 0x120);
    }
}

/* $01D692. The empire's leader. */
void StoryB1Start(void)
{
    scene_load(0x00901A, 0, 0x01E0A4, 0x00966C);
    scene_place(0x100, 0x720, 0x200, 0x200);
    wr32(v_layout, 0x01B45A);
    wr8(0xF765, 0);
    SceneTextB(0x84);
    actor(0xCFE0, 0x640, 4);
    StoryB1Actors();
    wr16(v_modeTimer, 0x2C0);
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
    scene_show();
    StoryB1();
}

/* $01D73E */
void StoryB1(void)
{
    scene_end(0x100, 0, 0);
}

/* $01D968. The camera bobbing (a $40 frames each way). */
void StoryBob(void)
{
    uint16_t t = rd16(0xA10C);
    wr32(v_camSpeedY, (t & 0x40) ? 0xFFFFC000 : 0x4000);
    wr16(0xA10C, (uint16_t)(t + 1));
}

/* $01D990. Five soldiers ($620) with their looks. */
void StoryB2Soldiers(void)
{
    for (uint16_t k = 0; k < 5; k++) {
        uint16_t s = rom16(0x01D9C6 + 2 * k);
        actor(s, 0x620, 0);
        wr32(s + 0x08, rom32(0x01D9D2 + 4 * k));
        wr16(s + O_X, rom16(0x01D9E6 + 2 * k));
        wr16(s + O_Y, 0x111);
    }
}

/* $01D9FA. Two of $624. */
void StoryB2Others(void)
{
    for (uint16_t k = 0; k < 2; k++) {
        uint16_t s = rom16(0x01DA34 + 2 * k);
        actor(s, 0x624, 0);
        wr32(s + 0x08, rom32(0x01DA38 + 4 * k));
        wr16(s + 0x0C, 0);
        wr16(s + O_X, rom16(0x01DA40 + 2 * k));
        wr16(s + O_Y, 0xC0);
    }
}

/* $01D7DC. The airship. */
void StoryB2Start(void)
{
    scene_load(0x0091B0, 0, 0x01E0B6, 0x00968C);
    PlanePriority(0xFFFF4000, 0x100);
    scene_place(0x11E0, 0x210, 0x200, 0x260);
    wr8(VDP_REG(11) + 1, 2);
    wr8(v_planeFlagsA, 0);
    wr8(v_planeFlagsB, 0x80);
    wr32(v_camSpeedY, 0);
    wr32(v_layout, 0x01B45A);
    wr8(0xF765, 0);
    SceneTextC(0x88);
    actor(0xA800, 0x61C, 0);
    StoryB2Soldiers();
    StoryB2Others();
    wr16(v_modeTimer, 0x2C0);
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
    scene_show();
    StoryB2();
}

/* $01D8B0. Bobbing, the clouds at three speeds. */
void StoryB2(void)
{
    if (scene_end(0x100, 0, 0)) return;
    StoryBob();
    wr32(0xA100, rd32(0xA100) + 0x20000);
    wr32(0xA104, rd32(0xA104) + 0x30000);
    wr32(0xA108, rd32(0xA108) + 0x40000);
    uint16_t row = rows(SCROLL_ROWS, 4, (uint16_t)-rd16(0xA100));
    row = rows(row, 5, (uint16_t)-rd16(0xA104));
    rows(row, 10, (uint16_t)-rd16(0xA108));
}

/* $01DB66. Seven soldiers ($620). */
void StoryB3Actors(void)
{
    for (uint16_t k = 0; k < 7; k++) actor(rom16(0x01DB86 + 2 * k), 0x620, rom16(0x01DB94 + 2 * k));
}

/* $01DA44. The machine (the boss's, $5D8, in its slot). */
void StoryB3Start(void)
{
    scene_load(0x0090CA, 0, 0x01E0C8, 0x00967C);
    PlanePriority(0xFFFF4000, 0x20);
    scene_place(0x840, 0x28, 0x100, 0x20);
    wr32(v_layout, 0x01B45A);
    wr8(0xF765, 0);
    SceneTextB(0x8C);
    actor(0xCFE0, 0x5C8, 0);
    StoryB3Actors();
    wr16(v_boss, 0x5D8);
    wr16(0xC984, 0);
    wr16(0xC990, 0xB0);
    wr16(0xC994, 0xF8);
    wr32(0xC998, 0);
    wr32(0xC99C, 0);
    wr16(0xC9D6, 0xE0);
    wr16(v_modeTimer, 0x2C0);
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
    scene_show();
    StoryB3();
}

/* $01DB22. The machine's turn held, the lamps cycling. */
void StoryB3(void)
{
    if (scene_end(0x100, 0, 0)) return;
    wr16(0xC9D6, 0xE0);
    LampColours();
}

/* $01DBA2. The brothers on the gem's fall (the screen shakes when it lands). */
void StoryB4Start(void)
{
    wr16(v_hintEffect, 0);
    wr16(v_hintState, 0);
    ClearObjectRam();
    RunCommands(0x01E0FA);
    wr16(0x7400, 0);
    LoadPalettes(0x01E13C);
    scene_place(0x2E0, 0x200, 0, 0);
    wr32(v_layout, 0x01B45A);
    scene_players(0x10);
    actor(0xD580, 0x648, 8);
    wr32(v_camSpeedY, 0xFFFA0000);
    wr32(0xA934, 0xFFFA0000);
    wr16(v_modeTimer, 0x100);
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
    wr16(v_sceneStep, 0);
    scene_show();
    QueueSound(0xF6);
    StoryB4();
}

/* $01DC6E. Rising until $E0: there it stops with a thud. */
void StoryB4(void)
{
    if (scene_end(0x100, 0, 1)) return;
    if (rd16(v_sceneStep)) return;
    if ((int16_t)rd16(0xA924) > 0xE0) return;
    wr16(v_sceneStep, (uint16_t)(rd16(v_sceneStep) + 4));
    wr32(v_camSpeedY, 0);
    wr32(0xA934, 0);
    wr16(v_camY, 0xE0);
    wr16(0xA924, 0xE0);
    wr16(v_shakeSprites, 0x10);
    QueueSound(0xD1);
}

/* ---- the modes ---- */

/* the modes' set-up */
static void story_start(void)
{
    wr16(v_fadeColor, 0);
    ScreenReset();
    clr8(v_pauseFlags, 6);
    wr8(VDP_REG(7) + 1, 0x10);
    wr32(v_speedLimitX, 0x1000000);
    wr32(v_speedLimitY, 0x1000000);
    wr16(v_hintEffect, 0x14);
    wr16(v_hintState, 0);
    wr8(0xF765, 0);
}

/* $01CEA2 */
void StoryAStep(void)
{
    call68k(rom32(0x01CEAE + (uint32_t)(int16_t)(uint16_t)(rd16(v_modeState) - 4)));
}

/* $01CF72 */
void StoryBStep(void)
{
    call68k(rom32(0x01CF7E + (uint32_t)(int16_t)(uint16_t)(rd16(v_modeState) - 4)));
}

/* $01CDFE. Mode $18: the story's first part, its music. */
void StoryA(void)
{
    if (!rd16(v_modeState)) {
        story_start();
        wr8(v_soundNext, 0x93);
        wr16(v_soundDelay, 0xB0);
        wr16(v_modeState, 4);
        wr16(v_sceneStep, 0);
    }
    ObjectsMove();
    CameraMove();
    Collisions();
    SpritesStart();
    DrawListStart();
    PlayersUpdate();
    EffectsUpdate();
    StoryAStep();
    ObjectsUpdate();
    DrawListEnd();
    ScreenShake();
    BuildSprites();
    ScrollFrame();
    wr16(v_gameFrames, (uint16_t)(rd16(v_gameFrames) + 1));
}

/* $01CEDA. Mode $4C: its second part. */
void StoryB(void)
{
    if (!rd16(v_modeState)) {
        story_start();
        wr16(v_modeState, 4);
        wr16(v_sceneStep, 0);
    }
    ObjectsMove();
    CameraMove();
    Collisions();
    SpritesStart();
    DrawListStart();
    PlayersUpdate();
    EffectsUpdate();
    StoryBStep();
    ObjectsUpdate();
    DrawListEnd();
    ScreenShake();
    BuildSprites();
    ScrollFrame();
    wr16(v_gameFrames, (uint16_t)(rd16(v_gameFrames) + 1));
}
