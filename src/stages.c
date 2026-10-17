/* The stages' own frames (StageFrame runs the one its table at $3934 names for v_stage4). Each
 * moves the camera by a script of steps (v_cameraStep, times 4): a step follows the players
 * until the camera reaches a place, holds it there for a fight (v_cameraHold bit 7, until the
 * fight's objects clear it), turns a corner, starts the next step. A second script
 * (v_backStep) moves the background. A step that is done goes straight on into the next one's
 * routine, as the 68k falls into it. */
#include "game.h"

#define P_SPRITE 0x0E            /* byte: the sprite's attributes' high byte: bit 7 in front */
#define P_VX     0x18
#define P_VY     0x1C
#define P_INPUT  0x74            /* the input's kind, times 4 (PlayerInput's table) */
#define P_FLOOR  0x82            /* the floor's attribute */
#define P_LIMITS 0x86            /* byte: bit 7 set while the stage's script moves the players */
#define P_STAGE_HOLD 0xC0        /* word: bit 15 set while a stage's scene runs */

#define INPUT_DIRECTIONS 0x24    /* PlayerInputDirections */

#define SCROLL_MODE (VDP_REG(11) + 1)

#define BACK_X1 0xA9C0           /* stage 0's background: two layers' x (longs), their speeds, */
#define BACK_X2 0xA9C4           /* and a wait */
#define BACK_SPEED1 0xA9C8
#define BACK_SPEED2 0xA9CC
#define BACK_WAIT 0xA9D0

/* a table of word offsets from itself, indexed by a step (times 4) */
static uint32_t step_routine(uint32_t table, uint16_t step)
{
    uint32_t at = table + (uint32_t)(int16_t)((int16_t)step >> 1);
    return at + (uint32_t)(int16_t)rom16(at);
}

static void next_step(void) { wr16(v_cameraStep, (uint16_t)(rd16(v_cameraStep) + 4)); }
static void next_back(void) { wr16(v_backStep, (uint16_t)(rd16(v_backStep) + 4)); }

/* ---- the stages' shared helpers ---- */

/* $0041AC. The camera and plane A stopped. */
void CameraStop(void)
{
    wr32(v_camSpeedX, 0);
    wr32(v_planeSpeeds, 0);
    wr32(v_camSpeedY, 0);
    wr32(v_planeSpeeds + 4, 0);
}

/* $004188. The camera and plane A moved at these speeds. */
void CameraSpeed(uint32_t x, uint32_t y)
{
    wr32(v_camSpeedX, x);
    wr32(v_planeSpeeds, x);
    wr32(v_camSpeedY, y);
    wr32(v_planeSpeeds + 4, y);
}

/* the camera held at x: the script waits for the fight there (bit 0: no longer scrolling by
 * itself) */
static void hold_x(uint32_t x)
{
    set8(v_cameraHold, 7);
    clr8(v_cameraHold, 0);
    CameraStop();
    wr32(v_camX, x);
    wr32(v_planes, x);
}

static void set_x(uint32_t x) { wr32(v_camX, x); wr32(v_planes, x); }
static void set_y(uint32_t y) { wr32(v_camY, y); wr32(v_planes + 4, y); }

/* $008DEE. Where the players will be, in pixels: each one's position plus its speed, halfway
 * between the two when both play. With neither the 68k leaves its caller's d0 and d1 (no run
 * has a stage without a player). */
void PlayersCentre(uint16_t *x, uint16_t *y)
{
    uint8_t p = rd8(v_players);
    if (p == 3) {
        uint32_t sx = rd32(v_player1 + O_X) + rd32(v_player1 + P_VX) + rd32(v_player2 + O_X) + rd32(v_player2 + P_VX);
        uint32_t sy = rd32(v_player1 + O_Y) + rd32(v_player1 + P_VY) + rd32(v_player2 + O_Y) + rd32(v_player2 + P_VY);
        *x = (uint16_t)((int16_t)(sx >> 16) >> 1);
        *y = (uint16_t)((int16_t)(sy >> 16) >> 1);
        return;
    }
    uint16_t obj = (p & 1) ? v_player1 : (p & 2) ? v_player2 : 0;
    if (!obj) return;
    *x = (uint16_t)((rd32(obj + O_X) + rd32(obj + P_VX)) >> 16);
    *y = (uint16_t)((rd32(obj + O_Y) + rd32(obj + P_VY)) >> 16);
}

/* $008EAE. Both held within -4..4 (not while paused). */
void ClampFour(uint16_t *x, uint16_t *y)
{
    if (rd8(v_pauseFlags) & 0x80) return;
    if ((int16_t)*x > 4) *x = 4;
    if ((int16_t)*x < -4) *x = (uint16_t)-4;
    if ((int16_t)*y > 4) *y = 4;
    if ((int16_t)*y < -4) *y = (uint16_t)-4;
}

/* how far right of the screen's middle the players are (0 when left of it) */
static uint16_t players_ahead(uint16_t *y)
{
    uint16_t x = 0;
    PlayersCentre(&x, y);
    x = (uint16_t)(x - 0x120);
    return (x & 0x8000) ? 0 : x;
}

/* $004094. The camera follows the players right, as fast as they are ahead of the middle (4 at
 * most); with v_cameraHold bit 0 it scrolls 2 pixels a frame by itself. */
void CameraFollowRight(void)
{
    if (bit8(v_cameraHold, 0)) {
        wr16(v_planeSpeeds, 2);
        wr16(v_camSpeedX, 2);
        wr16(v_planeSpeeds + 4, 0);
        wr16(v_camSpeedY, 0);
        return;
    }
    uint16_t y = 0, x = players_ahead(&y);
    ClampFour(&x, &y);
    wr16(v_planeSpeeds, x);
    wr16(v_camSpeedX, x);
    wr16(v_planeSpeeds + 4, 0);
    wr16(v_camSpeedY, 0);
}

/* $0041BE. The players' sprites in front of the planes. */
void PlayersInFront(void)
{
    if (bit8(v_players, 0)) set8(v_player1 + P_SPRITE, 7);
    if (bit8(v_players, 1)) set8(v_player2 + P_SPRITE, 7);
}

/* $0041FE. The players no longer carried with the level as the camera moves. */
void PlayersFree(void)
{
    if (bit8(v_players, 0)) {
        wr16(v_player1 + O_FLAGS, rd16(v_player1 + O_FLAGS) & 0xFEFF);
        set8(v_player1 + P_LIMITS, 7);
    }
    if (bit8(v_players, 1)) {
        wr16(v_player2 + O_FLAGS, rd16(v_player2 + O_FLAGS) & 0xFEFF);
        set8(v_player2 + P_LIMITS, 7);
    }
}

/* $00428C. And carried again. */
void PlayersFixed(void)
{
    if (bit8(v_players, 0)) {
        wr16(v_player1 + O_FLAGS, rd16(v_player1 + O_FLAGS) | 0x100);
        clr8(v_player1 + P_LIMITS, 7);
    }
    if (bit8(v_players, 1)) {
        wr16(v_player2 + O_FLAGS, rd16(v_player2 + O_FLAGS) | 0x100);
        clr8(v_player2 + P_LIMITS, 7);
    }
}

/* $004038. A scene's start: both held for it, the first player in the game reads only its pad's
 * directions. */
void StageSceneStart(void)
{
    set8(v_player1 + P_STAGE_HOLD, 7);
    set8(v_player2 + P_STAGE_HOLD, 7);
    uint8_t p = rd8(v_players);
    if (p & 1) wr16(v_player1 + P_INPUT, INPUT_DIRECTIONS);
    else if (p & 2) wr16(v_player2 + P_INPUT, INPUT_DIRECTIONS);
}

/* $004068. Its end: the pad again. */
void StageSceneEnd(void)
{
    clr8(v_player1 + P_STAGE_HOLD, 7);
    clr8(v_player2 + P_STAGE_HOLD, 7);
    uint8_t p = rd8(v_players);
    if (p & 1) wr16(v_player1 + P_INPUT, 0);
    else if (p & 2) wr16(v_player2 + P_INPUT, 0);
}

/* ---- stage 0, the ruins ---- */

#define STAGE0_CAMERA 0x003B4Eu
#define STAGE0_BACK   0x0042FAu

/* $003B38 */
void Stage0Frame(void)
{
    Stage0Camera();
    call68k(step_routine(STAGE0_BACK, rd16(v_backStep)));
}

/* $003B40 */
void Stage0Camera(void)
{
    call68k(step_routine(STAGE0_CAMERA, rd16(v_cameraStep)));
}

/* $004134: up the pyramid's side, as far as the players are */
static void follow_up(void)
{
    uint16_t y = 0, x = players_ahead(&y);
    uint16_t d = (uint16_t)(0x420 - x - rd16(v_camY));
    if (!(d & 0x8000)) d = 0;
    wr16(v_camSpeedX, 0);
    wr16(v_planeSpeeds, 0);
    wr16(v_camSpeedY, d);
    wr16(v_planeSpeeds + 4, d);
}

/* $00410A: right and up together, as fast as the players are ahead */
static void follow_diagonal(void)
{
    uint16_t y = 0, x = players_ahead(&y);
    ClampFour(&x, &y);
    wr16(v_planeSpeeds, x);
    wr16(v_camSpeedX, x);
    wr16(v_planeSpeeds + 4, (uint16_t)-x);
    wr16(v_camSpeedY, (uint16_t)-x);
}

/* $004166: falling, faster each frame to 7 */
static void fall(void)
{
    int32_t v = (int32_t)(rd32(v_camSpeedY) + 0x5000);
    if (v > 0x70000) v = 0x70000;
    wr32(v_camSpeedY, (uint32_t)v);
    wr32(v_planeSpeeds + 4, (uint32_t)v);
}

/* the camera and plane A $200 back in x and y (the endless shaft while it falls) */
static void wrap_back(void)
{
    wr16(v_camX, (uint16_t)(rd16(v_camX) - 0x200));
    wr16(v_planes, (uint16_t)(rd16(v_planes) - 0x200));
    wr16(v_camY, (uint16_t)(rd16(v_camY) - 0x200));
    wr16(v_planes + 4, (uint16_t)(rd16(v_planes + 4) - 0x200));
}

/* the camera at the speed the boss moves (the difference its speed makes to its pixel) */
static uint16_t boss_step(uint16_t pos, uint16_t speed)
{
    uint32_t p = rd32(pos);
    return (uint16_t)((uint16_t)((rd32(speed) + p) >> 16) - (uint16_t)(p >> 16));
}

/* $003B7C. Step 0: to x $5B0. */
void Stage0Cam00(void)
{
    if ((int32_t)rd32(v_camX) < 0x5B00000) { CameraFollowRight(); return; }
    next_step();
    hold_x(0x5B00000);
    Stage0Cam04();
}

/* $003BAC. Step 4 (and $C, $34, $4C's like it): the fight. */
void Stage0Cam04(void)
{
    if (rd8(v_cameraHold) & 0x80) return;
    next_step();
    Stage0Cam08();
}

/* $003BBA. To x $6F0 (the per-line scroll on while plane A has it). */
void Stage0Cam08(void)
{
    if ((int32_t)rd32(v_camX) < 0x6F00000) {
        CameraFollowRight();
        if (bit8(v_planeFlagsA, 7)) wr8(SCROLL_MODE, 3);
        return;
    }
    next_step();
    hold_x(0x6F00000);
    Stage0Cam0C();
}

/* $003C00 */
void Stage0Cam0C(void)
{
    if (rd8(v_cameraHold) & 0x80) return;
    next_step();
    Stage0Cam10();
}

/* $003C0E. To x $B00. */
void Stage0Cam10(void)
{
    if ((int32_t)rd32(v_camX) < 0xB000000) { CameraFollowRight(); return; }
    next_step();
    hold_x(0xB000000);
    Stage0Cam14();
}

/* $003C3E. After the fight, the next tiles. */
void Stage0Cam14(void)
{
    if (rd8(v_cameraHold) & 0x80) return;
    next_step();
    uint32_t list = 0x009094;
    NextJob(&list);
    Stage0Cam18();
}

/* $003C58. To x $CE0. */
void Stage0Cam18(void)
{
    if ((int32_t)rd32(v_camX) < 0xCE00000) { CameraFollowRight(); return; }
    next_step();
    clr8(v_cameraHold, 0);
    uint32_t list = 0x00909E;
    NextJob(&list);
    Stage0Cam1C();
}

/* $003C7A. Up the pyramid to x $FC0. */
void Stage0Cam1C(void)
{
    if ((int32_t)rd32(v_camX) < 0xFC00000) { follow_diagonal(); return; }
    next_step();
    PlayersInFront();
    Stage0Cam20();
}

/* $003C8E. To its top, y $398. */
void Stage0Cam20(void)
{
    if ((int32_t)rd32(v_camY) > 0x3980000) { follow_up(); return; }
    next_step();
    set_y(0x3980000);
    CameraStop();
    Stage0Cam24();
}

static int on_top(uint16_t obj) { return rd16(obj + P_FLOOR) == 0x38; }

/* $003CB2. Until the players stand on the top (attribute $38), both when both play. */
void Stage0Cam24(void)
{
    uint8_t p = rd8(v_players);
    if (!p) return;
    if (p == 3) {
        if (!on_top(v_player1) || !on_top(v_player2)) return;
    } else if (p == 1) {
        if (!on_top(v_player1)) return;
    } else if (!on_top(v_player2)) {
        return;
    }
    next_step();
    CameraSpeed(0, 0xFFFE0000);
    Stage0Cam28();
}

/* $003D08. Up at 2 to y $320, then right at 2. */
void Stage0Cam28(void)
{
    if ((int32_t)rd32(v_camY) > 0x3200000) return;
    next_step();
    set_y(0x3200000);
    CameraSpeed(0x20000, 0);
    QueueSound(0xF6);
    Stage0Cam2C();
}

/* $003D40. To x $1100: the checkpoint, the scene. */
void Stage0Cam2C(void)
{
    if ((int32_t)rd32(v_camX) < 0x11000000) return;
    next_step();
    hold_x(0x11000000);
    wr16(v_checkpoint, 4);
    wr16(v_checkpoints, rd16(v_checkpoint));
    wr8(v_sceneSignal, 0);
    Stage0Cam30();
}

/* $003D82. The scene (only the directions for the first player while it runs), then the camera
 * at the shaft (x $1400, y $200), redrawn, its attributes, its tiles. */
void Stage0Cam30(void)
{
    if (rd8(v_sceneSignal)) {
        wr8(v_sceneSignal, 0);
        StageSceneStart();
    }
    if (rd8(v_cameraHold) & 0x80) return;
    next_step();
    set_x(0x14000000);
    set_y(0x2000000);
    PlaneARedraw();
    Stage0ShaftAttributes();
    uint32_t list = 0x0090A8;
    NextJob(&list);
    clr8(v_cameraHold, 0);
    Stage0Cam34();
}

/* $003DDC. Down the shaft to y $3E0. */
void Stage0Cam34(void)
{
    if ((int32_t)rd32(v_camY) < 0x3E00000) { fall(); return; }
    next_step();
    set_y(0x3E00000);
    PlayersFree();
    StageSceneEnd();
    CameraStop();
    Stage0Cam38();
}

/* $003E08. Faster and faster down and right (round the shaft's loop from y $500) to 8, then the
 * boss. */
void Stage0Cam38(void)
{
    if ((int32_t)rd32(v_planeSpeeds) >= 0x80000) {
        next_step();
        CameraSpeed(0x80000, 0x80000);
        wr16(v_boss + O_TYPE, 0x194);
        wr16(v_stageEvent, 0);
        Stage0Cam3C();
        return;
    }
    if ((int32_t)rd32(v_camY) >= 0x5000000) wrap_back();
    wr32(v_planeSpeeds, rd32(v_planeSpeeds) + 0x1000);
    wr32(v_camSpeedX, rd32(v_camSpeedX) + 0x1000);
    wr32(v_planeSpeeds + 4, rd32(v_planeSpeeds + 4) + 0x1000);
    wr32(v_camSpeedY, rd32(v_camSpeedY) + 0x1000);
}

/* $003E6A. Round the loop until the boss says. */
void Stage0Cam3C(void)
{
    if ((int32_t)rd32(v_camY) < 0x5000000) return;
    if (rd16(v_stageEvent)) {
        next_step();
        Stage0Cam40();
        return;
    }
    wrap_back();
}

/* $003E9C. Out of the loop to y $700. */
void Stage0Cam40(void)
{
    if ((int32_t)rd32(v_camY) < 0x7000000) return;
    next_step();
    set_y(0x7000000);
    wr32(v_planeSpeeds + 4, 0);
    wr32(v_camSpeedY, 0);
    Stage0Cam44();
}

/* $003EC6. Slowing to a stop. */
void Stage0Cam44(void)
{
    wr32(v_planeSpeeds, rd32(v_planeSpeeds) - 0x1000);
    wr32(v_camSpeedX, rd32(v_camSpeedX) - 0x1000);
    if (!(rd32(v_camSpeedX) & 0x80000000u)) return;
    next_step();
    wr32(v_planeSpeeds, 0);
    wr32(v_camSpeedX, 0);
    PlayersFixed();
    wr16(v_stageEvent, 0);
    Stage0Cam48();
}

/* $003EEE. To x $1D80, the boss's room. */
void Stage0Cam48(void)
{
    if ((int32_t)rd32(v_camX) < 0x1D800000) { CameraFollowRight(); return; }
    next_step();
    hold_x(0x1D800000);
    Stage0BossTiles();
    Stage0Cam4C();
}

/* $0040D8 */
void Stage0BossTiles(void)
{
    RunCommands(0x0040E4);
}

/* $003F22. When the boss appears: no falling out, the floor lower. */
void Stage0Cam4C(void)
{
    if (rd8(v_cameraHold) & 0x80) return;
    next_step();
    wr16(v_deathLine, 0);
    wr16(v_limitBottom, 0x1A0);
    CameraStop();
    Stage0Cam50();
}

/* $003F3E. With the boss to x $1EC0. */
void Stage0Cam50(void)
{
    if ((int32_t)rd32(v_camX) >= 0x1EC00000) {
        next_step();
        set_x(0x1EC00000);
        wr32(v_camSpeedX, 0);
        wr32(v_planeSpeeds, 0);
        Stage0Cam54();
        return;
    }
    uint16_t dx = boss_step(v_boss + O_X, v_boss + P_VX);
    wr16(v_camSpeedX, dx);
    wr16(v_planeSpeeds, dx);
    uint16_t dy = boss_step(v_boss + O_Y, v_boss + P_VY);
    wr16(v_camSpeedY, dy);
    wr16(v_planeSpeeds + 4, dy);
}

/* $003F98. And down to y $700. */
void Stage0Cam54(void)
{
    if ((int32_t)rd32(v_camY) >= 0x7000000) {
        next_step();
        set_y(0x7000000);
        set8(v_cameraHold, 7);
        clr8(v_cameraHold, 0);
        CameraStop();
        wr8(v_sceneSignal, 0);
        Stage0Cam58();
        return;
    }
    uint16_t dy = boss_step(v_boss + O_Y, v_boss + P_VY);
    wr16(v_camSpeedY, dy);
    wr16(v_planeSpeeds + 4, dy);
}

/* $003FE6. The boss beaten: the players' scenes, once. */
void Stage0Cam58(void)
{
    uint8_t s = rd8(v_sceneSignal);
    if (!s || (s & 0x80)) return;
    set8(v_sceneSignal, 7);
    wr16(v_sceneDone, 0);
    uint8_t p = rd8(v_players);
    if (p == 3) {
        wr16(v_player1 + P_INPUT, 0x14);
        wr16(v_player2 + P_INPUT, 0x18);
    } else if (p & 1) {
        wr16(v_player1 + P_INPUT, 0x14);
    } else if (p & 2) {
        wr16(v_player2 + P_INPUT, 0x14);
    }
}

/* $0042BA. The shaft's collision: $7872 on cleared, four cells of attribute $44. */
void Stage0ShaftAttributes(void)
{
    for (uint16_t i = 0; i <= 0x7C; i++) wr16((uint16_t)(0x7872 + 2 * i), 0);
    wr8(0x7800 + 0x127, 0x44);
    wr8(0x7800 + 0x128, 0x44);
    wr8(0x7800 + 0x12C, 0x44);
    wr8(0x7800 + 0x12E, 0x44);
}

/* ---- stage 0's background: plane B (the second camera, v_camX + 8) and two layers of the
 * per-line scroll, drawn on alternate lines and swapped each frame ---- */

/* $004440. Plane B at a quarter of the camera's x speed, half its y. */
void BackQuarterHalf(void)
{
    wr32(v_camSpeedX + 8, (uint32_t)((int32_t)rd32(v_camSpeedX) >> 2));
    uint32_t y = (uint32_t)((int32_t)rd32(v_camSpeedY) >> 1);
    wr32(v_camSpeedY + 8, y);
    wr32(v_planeSpeeds + 0xC, y);
}

/* $00445A. The second camera only (plane B's y left). */
void BackQuarterHalfCamera(void)
{
    wr32(v_camSpeedX + 8, (uint32_t)((int32_t)rd32(v_camSpeedX) >> 2));
    wr32(v_camSpeedY + 8, (uint32_t)((int32_t)rd32(v_camSpeedY) >> 1));
}

/* $00442E. Plane B and the second camera stopped. */
void BackStop(void)
{
    wr32(v_camSpeedX + 8, 0);
    wr32(v_planeSpeeds + 8, 0);
    wr32(v_camSpeedY + 8, 0);
    wr32(v_planeSpeeds + 0xC, 0);
}

/* $004470. The layers start where the second camera is, still. */
void BackLayersStart(void)
{
    uint32_t x = rd32(v_camX + 8);
    wr32(BACK_X1, x);
    wr32(BACK_X2, x);
    wr32(BACK_SPEED1, 0);
    wr32(BACK_SPEED2, 0);
}

/* the per-line table: lines above the background's top (the second camera's y, in 8-line
 * groups from 160 down) scroll plane B not at all, the rest by the two layers on alternate lines;
 * plane A by the camera everywhere */
static void back_lines(void)
{
    uint32_t a = rd32(v_camSpeedX + 8);
    wr32(BACK_X1, rd32(BACK_X1) + a + rd32(BACK_SPEED1));
    wr32(BACK_X2, rd32(BACK_X2) + a + rd32(BACK_SPEED2));
    uint16_t groups = 0x1C;
    uint16_t top = (uint16_t)(((int16_t)((rd32(v_camY + 8) + rd32(v_camSpeedY + 8)) >> 16) >> 3) - 0x14);
    uint32_t ax = (uint32_t)(uint16_t)-rd16(v_camX) << 16;
    if (top & 0x8000) {
        uint16_t n = (uint16_t)-top;
        groups = (uint16_t)(groups - n);
        uint32_t at = rd32(v_hscrollSrc) + (uint32_t)(int16_t)(uint16_t)(n << 5);
        for (uint16_t k = (uint16_t)(n - 1);; k--) {
            for (int i = 0; i < 8; i++) { at -= 4; bus_wr32(at, ax); }
            if (k == 0) break;
        }
    }
    uint32_t at = rd32(v_hscrollSrc) + 0x380;
    uint16_t l1 = (uint16_t)-rd16(BACK_X1), l2 = (uint16_t)-rd16(BACK_X2);
    uint32_t even = ax | ((rd8(v_frameCount + 1) & 1) ? l2 : l1);
    uint32_t odd = ax | ((rd8(v_frameCount + 1) & 1) ? l1 : l2);
    for (uint16_t k = (uint16_t)(groups - 1);; k--) {
        for (int i = 0; i < 4; i++) {
            at -= 4; bus_wr32(at, odd);
            at -= 4; bus_wr32(at, even);
        }
        if (k == 0) break;
    }
}

/* $0044AA: the layers' speeds up (8 and 4 at most) */
static void back_faster(void)
{
    wr32(BACK_SPEED1, rd32(BACK_SPEED1) + 0x800);
    if ((int32_t)rd32(BACK_SPEED1) > 0x80000) wr32(BACK_SPEED1, 0x80000);
    wr32(BACK_SPEED2, rd32(BACK_SPEED2) + 0x400);
    if ((int32_t)rd32(BACK_SPEED2) > 0x40000) wr32(BACK_SPEED2, 0x40000);
    back_lines();
}

/* $004486: and down to nothing */
static void back_slower(void)
{
    wr32(BACK_SPEED1, rd32(BACK_SPEED1) - 0x800);
    if (rd32(BACK_SPEED1) & 0x80000000u) wr32(BACK_SPEED1, 0);
    wr32(BACK_SPEED2, rd32(BACK_SPEED2) - 0x400);
    if (rd32(BACK_SPEED2) & 0x80000000u) wr32(BACK_SPEED2, 0);
    back_lines();
}

/* $00430A. Background step 0: down to the second camera's y $140, then the per-line scroll on
 * (plane B). */
void Stage0Back00(void)
{
    if ((int32_t)rd32(v_camY + 8) > 0x1400000) { BackQuarterHalf(); return; }
    next_back();
    wr32(v_camY + 8, 0x1400000);
    wr8(SCROLL_MODE, 3);
    wr8(v_planeFlagsB, 0x80);
    wr8(v_planeFlagsA, 0x80);
    BackLayersStart();
    Stage0Back04();
}

/* $004338. On down to $80. */
void Stage0Back04(void)
{
    if ((int32_t)rd32(v_camY + 8) > 0x800000) {
        BackQuarterHalf();
        back_faster();
        return;
    }
    next_back();
    Stage0Back08();
}

/* $004350. Back up to $100 (a quarter of the camera's y). */
void Stage0Back08(void)
{
    if ((int32_t)rd32(v_camY + 8) < 0x1000000) {
        wr32(v_camSpeedX + 8, (uint32_t)((int32_t)rd32(v_camSpeedX) >> 2));
        uint32_t y = (uint32_t)((int32_t)rd32(v_camSpeedY) >> 2);
        wr32(v_camSpeedY + 8, y);
        wr32(v_planeSpeeds + 0xC, y);
        back_faster();
        return;
    }
    next_back();
    wr32(v_planes + 0xC, 0x1000000);
    BackStop();
    Stage0Back0C();
}

/* $004388. To $110 (plane B left), then a wait of $600 frames. */
void Stage0Back0C(void)
{
    if ((int32_t)rd32(v_camY + 8) < 0x1100000) {
        wr32(v_camSpeedX + 8, (uint32_t)((int32_t)rd32(v_camSpeedX) >> 2));
        wr32(v_camSpeedY + 8, (uint32_t)((int32_t)rd32(v_camSpeedY) >> 2));
        back_faster();
        return;
    }
    next_back();
    BackStop();
    wr16(BACK_WAIT, 0x600);
    Stage0Back10();
}

/* $0043BA. After the wait, until the layers are 2 pixels apart. */
void Stage0Back10(void)
{
    wr16(BACK_WAIT, (uint16_t)(rd16(BACK_WAIT) - 1));
    if ((rd16(BACK_WAIT) & 0x8000) && ((rd16(BACK_X1) - rd16(BACK_X2)) & 0xFF) == 2) {
        next_back();
        Stage0Back14();
        return;
    }
    BackQuarterHalfCamera();
    back_faster();
}

/* $0043E2. The layers slow; when the stage says, once the background is out of sight, the
 * per-line scroll off. */
void Stage0Back14(void)
{
    if (rd16(v_stageEvent) && (((rd32(v_camY + 8) & 0xFFFFFF) - rd32(v_camSpeedY + 8)) & 0x1000000)) {
        next_back();
        wr8(SCROLL_MODE, 0);
        wr8(v_planeFlagsB, 0);
        wr8(v_planeFlagsA, 0);
        BackStop();
        Stage0Back18();
        return;
    }
    BackQuarterHalfCamera();
    back_slower();
}

/* $00441C. Plane B with the camera until x $1C00. */
void Stage0Back18(void)
{
    if ((int32_t)rd32(v_camX) < 0x1C000000) { BackQuarterHalf(); return; }
    next_back();
    Stage0Back1C();
}

/* $00442C */
void Stage0Back1C(void)
{
}
