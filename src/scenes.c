/* The players in the scenes: a player's object becomes type $5BC (player 1, Red) or $5C0
 * (player 2, Blue) for the story's pictures and the stages' openings and ends, where a script
 * moves it (PlayerSceneType). The scene is P_SCENE (times 4, each player's table), its step the
 * object's state; a step sets the animation (the long at +8, its frame 0) and a speed. */
#include "game.h"

#define P_SPRITE   0x0E
#define P_VX       0x18
#define P_VY       0x1C
#define P_SCENE    0x48          /* the scene asked for (times 4) */
#define P_SCENE_NOW 0x4A         /* the scene running (a new one starts at step 0) */
#define P_TIME     0x4C
#define P_CHARACTER 0xCA
#define P_DMA      0xDA          /* the player's tiles in VRAM */


static void anim(uint16_t obj, uint32_t frames)
{
    wr32(obj + 0x08, frames);
    wr16(obj + 0x0C, 0);
}

static int paused(void) { return (rd8(v_pauseFlags) & 0x80) != 0; }

static void next_frame(uint16_t obj) { wr8(obj + 0x0D, (uint8_t)(rd8(obj + 0x0D) + 1)); }

/* the scene's step from its table */
static uint32_t step_of(uint16_t obj, uint32_t table)
{
    return rom32(table + (uint32_t)(int16_t)rd16(obj + O_STATE));
}

/* $020DDA, $020E14. The player's scene (types $5BC, $5C0). */
static uint32_t scene_of(uint16_t obj, uint32_t table)
{
    uint16_t s = rd16(obj + P_SCENE);
    if (s != rd16(obj + P_SCENE_NOW)) {
        wr16(obj + P_SCENE_NOW, s);
        wr16(obj + O_STATE, 0);
    }
    return rom32(table + (uint32_t)(int16_t)s);
}

void ScenePlayer1(uint16_t obj)
{
    uint32_t fn = scene_of(obj, 0x020DF4);
    switch (fn) {
    case 0x020E4E: SceneTitle1(obj); return;
    case 0x020F06: SceneRun1(obj); return;
    case 0x02103A: SceneWait1(obj); return;
    case 0x021146: SceneWalkIn1(obj); return;
    case 0x0212A6: SceneJumpIn1(obj); return;
    case 0x0213BE: SceneTakeItem(obj); return;
    case 0x0214A4: SceneAfterItem(obj); return;
    case 0x0214EA: SceneCue(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

void ScenePlayer2(uint16_t obj)
{
    uint32_t fn = scene_of(obj, 0x020E2E);
    switch (fn) {
    case 0x020EAA: SceneTitle2(obj); return;
    case 0x020FA0: SceneRun2(obj); return;
    case 0x0210D8: SceneWait2(obj); return;
    case 0x0211F6: SceneWalkIn2(obj); return;
    case 0x021332: SceneJumpIn2(obj); return;
    case 0x0213BE: SceneTakeItem(obj); return;
    case 0x0214A4: SceneAfterItem(obj); return;
    case 0x0214EA: SceneCue(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* the characters' sprites: Red's (palette line 2) and Blue's (3), their tiles */
void SceneRed(uint16_t obj)                          /* $02161C */
{
    wr16(obj + P_SPRITE, 0x45C0);
    wr16(obj + P_DMA, 0xB800);
}

void SceneBlue(uint16_t obj)                         /* $02162A */
{
    wr16(obj + P_SPRITE, 0x65E0);
    wr16(obj + P_DMA, 0xBC00);
}

/* a pacing scene: from x, y at vx, turning every `time` frames */
static void pace(uint16_t obj, void (*who)(uint16_t), uint8_t pri, uint32_t frames, uint16_t x, uint16_t y,
                 uint32_t vx, uint16_t time)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 4);
        who(obj);
        wr8(obj + 0x20, pri);
        wr16(obj + O_FLAGS, 0xEC00);
        anim(obj, frames);
        wr16(obj + O_X, x);
        wr16(obj + O_Y, y);
        wr32(obj + P_VX, vx);
        wr32(obj + P_VY, 0);
        wr16(obj + P_TIME, time);
    }
    wr16(obj + P_TIME, (uint16_t)(rd16(obj + P_TIME) - 1));
    if (rd16(obj + P_TIME)) return;
    wr16(obj + P_TIME, time);
    wr32(obj + P_VX, 0u - rd32(obj + P_VX));
}

/* $020E4E, $020EAA. Scene 0: walking to and fro. */
void SceneTitle1(uint16_t obj) { pace(obj, SceneRed, 0x88, 0x0D817C, 0x140, 0x110, 0x4000, 0x80); }
void SceneTitle2(uint16_t obj) { pace(obj, SceneBlue, 0x80, 0x0DB500, 0xC0, 0x118, 0x6000, 0x60); }

/* scene 4: running in at 2 a frame from x, y, stopping at the mode's step 4, then a pose */
static void run_start(uint16_t obj, void (*who)(uint16_t), uint32_t frames, uint16_t x, uint16_t y)
{
    wr16(obj + O_STATE, 4);
    who(obj);
    wr8(obj + 0x20, 0x80);
    wr16(obj + O_FLAGS, 0xED00);
    anim(obj, frames);
    wr16(obj + O_X, x);
    wr16(obj + O_Y, y);
    wr32(obj + P_VX, 0x20000);
    wr32(obj + P_VY, 0);
}

static int run_stop(uint16_t obj, uint32_t frames)
{
    if ((int16_t)rd16(v_sceneStep) < 4) return 0;
    wr16(obj + O_STATE, 8);
    anim(obj, frames);
    wr32(obj + P_VX, 0);
    wr16(obj + P_TIME, 0x20);
    return 1;
}

static int run_pose(uint16_t obj, uint32_t frames)
{
    wr16(obj + P_TIME, (uint16_t)(rd16(obj + P_TIME) - 1));
    if (!(rd16(obj + P_TIME) & 0x8000)) return 0;
    wr16(obj + O_STATE, 0xC);
    anim(obj, frames);
    return 1;
}

/* $020F06 */
void SceneRun1(uint16_t obj)
{
    uint32_t fn = step_of(obj, 0x020F10);
    switch (fn) {
    case 0x020F20: SceneRun1Start(obj); return;
    case 0x020F5A: SceneRun1Running(obj); return;
    case 0x020F82: SceneRun1Stopped(obj); return;
    case 0x020F9E: SceneRun1Posed(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $020F20, $020F5A, $020F82, $020F9E */
void SceneRun1Start(uint16_t obj) { run_start(obj, SceneRed, 0x0D8160, 0x100, 0x100); SceneRun1Running(obj); }
void SceneRun1Running(uint16_t obj) { if (run_stop(obj, 0x0D73B0)) SceneRun1Stopped(obj); }
void SceneRun1Stopped(uint16_t obj) { if (run_pose(obj, 0x0D9014)) SceneRun1Posed(obj); }
void SceneRun1Posed(uint16_t obj) { (void)obj; }

/* $020FA0 */
void SceneRun2(uint16_t obj)
{
    uint32_t fn = step_of(obj, 0x020FAA);
    switch (fn) {
    case 0x020FBA: SceneRun2Start(obj); return;
    case 0x020FF4: SceneRun2Running(obj); return;
    case 0x02101C: SceneRun2Stopped(obj); return;
    case 0x021038: SceneRun2Posed(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $020FBA, $020FF4, $02101C, $021038 */
void SceneRun2Start(uint16_t obj) { run_start(obj, SceneBlue, 0x0DB4E4, 0x70, 0x108); SceneRun2Running(obj); }
void SceneRun2Running(uint16_t obj) { if (run_stop(obj, 0x0D9E5C)) SceneRun2Stopped(obj); }
void SceneRun2Stopped(uint16_t obj) { if (run_pose(obj, 0x0DB710)) SceneRun2Posed(obj); }
void SceneRun2Posed(uint16_t obj) { (void)obj; }

/* $02103A. Scene 8 (Red): standing $80 frames, a pose $80, then standing; the animation held
 * (a frame added each frame). */
void SceneWait1(uint16_t obj)
{
    uint32_t fn = step_of(obj, 0x021044);
    switch (fn) {
    case 0x021054: SceneWait1Start(obj); return;
    case 0x021090: SceneWait1First(obj); return;
    case 0x0210B4: SceneWait1Pose(obj); return;
    case 0x0210D2: SceneWait1End(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $021054 */
void SceneWait1Start(uint16_t obj)
{
    wr16(obj + O_STATE, 4);
    SceneRed(obj);
    wr8(obj + 0x20, 0x80);
    wr16(obj + O_FLAGS, 0xEC00);
    anim(obj, 0x0D73B0);
    wr16(obj + O_X, 0xA8);
    wr16(obj + O_Y, 0x118);
    wr32(obj + P_VX, 0);
    wr32(obj + P_VY, 0);
    wr16(obj + P_TIME, 0x80);
    SceneWait1First(obj);
}

/* $021090 */
void SceneWait1First(uint16_t obj)
{
    wr16(obj + P_TIME, (uint16_t)(rd16(obj + P_TIME) - 1));
    if (!(rd16(obj + P_TIME) & 0x8000)) { next_frame(obj); return; }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    anim(obj, 0x0D9034);
    wr16(obj + P_TIME, 0x80);
    SceneWait1Pose(obj);
}

/* $0210B4 */
void SceneWait1Pose(uint16_t obj)
{
    wr16(obj + P_TIME, (uint16_t)(rd16(obj + P_TIME) - 1));
    if (!(rd16(obj + P_TIME) & 0x8000)) { next_frame(obj); return; }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    anim(obj, 0x0D73B0);
    SceneWait1End(obj);
}

/* $0210D2 */
void SceneWait1End(uint16_t obj) { next_frame(obj); }

/* $0210D8. Scene 8 (Blue): standing $B0 frames, then a pose. */
void SceneWait2(uint16_t obj)
{
    int16_t s = (int16_t)(rd16(obj + O_STATE) - 4);
    if (s > 0) { next_frame(obj); return; }
    if (s < 0) {
        wr16(obj + O_STATE, 4);
        SceneBlue(obj);
        wr8(obj + 0x20, 0x80);
        wr16(obj + O_FLAGS, 0xEC00);
        anim(obj, 0x0D9E5C);
        wr16(obj + O_X, 0x98);
        wr16(obj + O_Y, 0xD8);
        wr32(obj + P_VX, 0);
        wr32(obj + P_VY, 0);
        wr16(obj + P_TIME, 0xB0);
    }
    wr16(obj + P_TIME, (uint16_t)(rd16(obj + P_TIME) - 1));
    if (!(rd16(obj + P_TIME) & 0x8000)) { next_frame(obj); return; }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    anim(obj, 0x0DB738);
    next_frame(obj);
}

/* scene $C: walking in from x at 1.5 a frame, waiting for the director, walking on; then the
 * walk-in is done */
static void walk_in_start(uint16_t obj, void (*who)(uint16_t), uint16_t x)
{
    wr16(obj + O_STATE, 4);
    who(obj);
    set8(obj + P_SPRITE, 7);
    wr8(obj + 0x20, 0x80);
    wr16(obj + O_FLAGS, 0xED00);
    SceneAnimRun(obj);
    wr16(obj + O_X, x);
    wr16(obj + O_Y, 0x140);
    wr32(obj + P_VX, 0x18000);
    wr32(obj + P_VY, 0);
    wr16(obj + P_TIME, 0x60);
}

static int walk_in_walking(uint16_t obj)
{
    wr16(obj + P_TIME, (uint16_t)(rd16(obj + P_TIME) - 1));
    if (!(rd16(obj + P_TIME) & 0x8000)) return 0;
    wr8(v_sceneSignal, 0);
    wr16(obj + O_STATE, 8);
    SceneAnimStand(obj);
    wr32(obj + P_VX, 0);
    return 1;
}

static int walk_in_waiting(uint16_t obj)
{
    if (!rd8(v_sceneSignal)) return 0;
    wr16(obj + O_STATE, 0xC);
    SceneAnimRun(obj);
    wr32(obj + P_VX, 0x18000);
    wr16(obj + P_TIME, 0x100);
    return 1;
}

static void walk_in_on(uint16_t obj)
{
    wr16(obj + P_TIME, (uint16_t)(rd16(obj + P_TIME) - 1));
    if (!(rd16(obj + P_TIME) & 0x8000)) clr8(v_cameraHold, 7);
}

/* $021146 */
void SceneWalkIn1(uint16_t obj)
{
    if (paused()) return;
    uint32_t fn = step_of(obj, 0x02115A);
    switch (fn) {
    case 0x02116A: SceneWalkIn1Start(obj); return;
    case 0x0211A8: SceneWalkIn1Walking(obj); return;
    case 0x0211C4: SceneWalkIn1Waiting(obj); return;
    case 0x0211E6: SceneWalkIn1On(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $02116A, $0211A8, $0211C4, $0211E6 */
void SceneWalkIn1Start(uint16_t obj) { walk_in_start(obj, SceneRed, 0x70); SceneWalkIn1Walking(obj); }
void SceneWalkIn1Walking(uint16_t obj) { if (walk_in_walking(obj)) SceneWalkIn1Waiting(obj); }
void SceneWalkIn1Waiting(uint16_t obj) { if (walk_in_waiting(obj)) SceneWalkIn1On(obj); }
void SceneWalkIn1On(uint16_t obj) { walk_in_on(obj); }

/* $0211F6 */
void SceneWalkIn2(uint16_t obj)
{
    if (paused()) return;
    uint32_t fn = step_of(obj, 0x02120A);
    switch (fn) {
    case 0x02121A: SceneWalkIn2Start(obj); return;
    case 0x021258: SceneWalkIn2Walking(obj); return;
    case 0x021274: SceneWalkIn2Waiting(obj); return;
    case 0x021296: SceneWalkIn2On(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $02121A, $021258, $021274, $021296 */
void SceneWalkIn2Start(uint16_t obj) { walk_in_start(obj, SceneBlue, 0x50); SceneWalkIn2Walking(obj); }
void SceneWalkIn2Walking(uint16_t obj) { if (walk_in_walking(obj)) SceneWalkIn2Waiting(obj); }
void SceneWalkIn2Waiting(uint16_t obj) { if (walk_in_waiting(obj)) SceneWalkIn2On(obj); }
void SceneWalkIn2On(uint16_t obj) { walk_in_on(obj); }

/* scene $10: jumping in from below the screen (-8 up), falling onto y $120 */
static void jump_in(uint16_t obj, void (*who)(uint16_t), uint32_t up, uint32_t top, uint32_t land, uint16_t x)
{
    int16_t s = (int16_t)(rd16(obj + O_STATE) - 4);
    if (s > 0) return;
    if (s < 0) {
        wr16(obj + O_STATE, 4);
        who(obj);
        wr8(obj + 0x20, 0x80);
        wr16(obj + O_FLAGS, 0xEC00);
        anim(obj, up);
        wr16(obj + O_X, x);
        wr16(obj + O_Y, 0x180);
        wr32(obj + P_VX, 0);
        wr32(obj + P_VY, 0xFFF80000u);
    }
    wr32(obj + P_VY, rd32(obj + P_VY) + 0x5000);
    int32_t vy = (int32_t)rd32(obj + P_VY);
    if (vy < 0) return;
    if (!vy) { anim(obj, top); return; }
    if ((int16_t)rd16(obj + O_Y) < 0x120) return;
    wr16(obj + O_Y, 0x120);
    wr16(obj + O_STATE, 8);
    anim(obj, land);
    wr32(obj + P_VY, 0);
}

/* $0212A6, $021332 */
void SceneJumpIn1(uint16_t obj) { jump_in(obj, SceneRed, 0x0D8F50, 0x0D8F58, 0x0D7F58, 0x170); }
void SceneJumpIn2(uint16_t obj) { jump_in(obj, SceneBlue, 0x0DAFF0, 0x0DAFB4, 0x0DA03C, 0x1A0); }

/* $02148A. The stage's item object for the scene (the table's four, then the code after). */
uint32_t SceneItem(void)
{
    return rom32(0x021494 + (uint32_t)(int16_t)rd16(v_stage4));
}

/* $0213BE. Scene $14: walking to the item, taking it (it goes when the take's frame 8 comes),
 * the take's sound and pose; then v_sceneDone. */
void SceneTakeItem(uint16_t obj)
{
    if (paused()) return;
    uint32_t item = SceneItem();
    uint32_t fn = step_of(obj, 0x0213D6);
    switch (fn) {
    case 0x0213E6: SceneTakeStart(obj, item); return;
    case 0x0213F8: SceneTakeWalk(obj, item); return;
    case 0x021452: SceneTakeTaking(obj, item); return;
    case 0x021480: SceneTakeTaken(obj, item); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $0213E6 */
void SceneTakeStart(uint16_t obj, uint32_t item)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, 0xED00);
    SceneAnimRun(obj);
    wr32(obj + P_VY, 0);
    SceneTakeWalk(obj, item);
}

/* $0213F8. To $1C left of the item at 1.75. */
void SceneTakeWalk(uint16_t obj, uint32_t item)
{
    int16_t d = (int16_t)(bus_rd16(item + O_X) - 0x1C - rd16(obj + O_X));
    if (d < 0) {
        set8(obj + P_SPRITE, 3);
        wr32(obj + P_VX, 0xFFFE4000u);
        return;
    }
    if ((uint16_t)d >= 2) {
        clr8(obj + P_SPRITE, 3);
        wr32(obj + P_VX, 0x1C000);
        return;
    }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    SceneAnimTake(obj);
    clr8(obj + P_SPRITE, 3);
    wr32(obj + P_VX, 0);
    wr16(obj + P_TIME, 0x20);
    QueueSound(0xA7);
    SceneTakeTaking(obj, item);
}

/* $021452 */
void SceneTakeTaking(uint16_t obj, uint32_t item)
{
    wr16(obj + P_TIME, (uint16_t)(rd16(obj + P_TIME) - 1));
    if (!(rd16(obj + P_TIME) & 0x8000)) {
        if ((int8_t)rd8(obj + 0x0C) < 8) return;
        bus_wr16(item + O_X, 0);
        bus_wr16(item + O_Y, 0);
        return;
    }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    SceneAnimPose(obj);
    SceneVoice(obj);
    wr16(v_sceneDone, 4);
    SceneTakeTaken(obj, item);
}

/* $021480 */
void SceneTakeTaken(uint16_t obj, uint32_t item)
{
    (void)obj;
    bus_wr16(item + O_X, 0);
    bus_wr16(item + O_Y, 0);
}

/* $0214A4. Scene $18: standing until v_sceneDone, then the pose. */
void SceneAfterItem(uint16_t obj)
{
    if (paused()) return;
    uint32_t fn = step_of(obj, 0x0214B8);
    switch (fn) {
    case 0x0214C4: SceneAfterStart(obj); return;
    case 0x0214D6: SceneAfterWait(obj); return;
    case 0x0214E8: SceneAfterEnd(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $0214C4, $0214D6, $0214E8 */
void SceneAfterStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, 0xED00);
    wr32(obj + P_VX, 0);
    wr32(obj + P_VY, 0);
    SceneAfterWait(obj);
}

void SceneAfterWait(uint16_t obj)
{
    if (!rd16(v_sceneDone)) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    SceneAnimPose(obj);
    SceneAfterEnd(obj);
}

void SceneAfterEnd(uint16_t obj) { (void)obj; }

/* $0214EA. Scene $1C: standing until the director's bit 1, a gesture, standing. */
void SceneCue(uint16_t obj)
{
    if (paused()) return;
    uint32_t fn = step_of(obj, 0x0214FE);
    switch (fn) {
    case 0x02150E: SceneCueStart(obj); return;
    case 0x021524: SceneCueWait(obj); return;
    case 0x021538: SceneCueGesture(obj); return;
    case 0x02154A: SceneCueEnd(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $02150E, $021524, $021538, $02154A */
void SceneCueStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, 0xED00);
    wr32(obj + P_VX, 0);
    wr32(obj + P_VY, 0);
    SceneAnimStand(obj);
    SceneCueWait(obj);
}

void SceneCueWait(uint16_t obj)
{
    if (!bit8(v_sceneSignal, 1)) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    SceneAnimGesture(obj);
    SceneCueGesture(obj);
}

void SceneCueGesture(uint16_t obj)
{
    if (!(rd8(obj + 0x0D) & 0x80)) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    SceneAnimStand(obj);
    SceneCueEnd(obj);
}

void SceneCueEnd(uint16_t obj) { (void)obj; }

/* the character's animations: $02154C running, $021570 standing, $021594 taking, $0215B8 the
 * pose, $0215DC the gesture; $021600 the voice */
void SceneAnimRun(uint16_t obj) { anim(obj, rd16(obj + P_CHARACTER) ? 0x0DB4E4 : 0x0D8160); }
void SceneAnimStand(uint16_t obj) { anim(obj, rd16(obj + P_CHARACTER) ? 0x0D9E5C : 0x0D73B0); }
void SceneAnimTake(uint16_t obj) { anim(obj, rd16(obj + P_CHARACTER) ? 0x0DB6E8 : 0x0D8FCC); }
void SceneAnimPose(uint16_t obj) { anim(obj, rd16(obj + P_CHARACTER) ? 0x0DA03C : 0x0D7F58); }
void SceneAnimGesture(uint16_t obj) { anim(obj, rd16(obj + P_CHARACTER) ? 0x0DB020 : 0x0D7698); }
void SceneVoice(uint16_t obj) { QueueSound(rd16(obj + P_CHARACTER) ? 0xE8 : 0xA1); }
