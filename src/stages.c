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

/* $003FF4: the boss beaten: the players' scenes (player 1's $14, player 2's $18) */
static void boss_scenes(void)
{
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

/* $003FE6. Once the stage signals. */
void Stage0Cam58(void)
{
    uint8_t s = rd8(v_sceneSignal);
    if (!s || (s & 0x80)) return;
    boss_scenes();
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

/* ---- stage 1, the mine: a ride through tunnels ($200 pixels of map each, the camera put back
 * by $200 at each one's end until the stage says to go on), then a maze of them whose turns
 * v_stageEvent picks (0, 2, 4, 6: a jump table at each junction). The bounds box the players
 * are held in moves against the camera. ---- */

#define STAGE1_CAMERA 0x004816u
#define STAGE1_BACK   0x0055C6u
#define CART_X 0xA970            /* the cart's push on the players (longs), x and y */
#define CART_Y 0xA974
#define STAGE1_EVENT2 0xA982
#define HSCROLL_RAM 0xE400       /* the per-strip scroll table, VSCROLL_RAM the per-column one */
#define VSCROLL_RAM 0xEC00

/* $00424E. The players no longer carried with the level (their limits kept). */
void PlayersUnpin(void)
{
    if (bit8(v_players, 0)) wr16(v_player1 + O_FLAGS, rd16(v_player1 + O_FLAGS) & 0xFEFF);
    if (bit8(v_players, 1)) wr16(v_player2 + O_FLAGS, rd16(v_player2 + O_FLAGS) & 0xFEFF);
}

/* $00422C. And carried again. */
void PlayersPin(void)
{
    if (bit8(v_players, 0)) wr16(v_player1 + O_FLAGS, rd16(v_player1 + O_FLAGS) | 0x100);
    if (bit8(v_players, 1)) wr16(v_player2 + O_FLAGS, rd16(v_player2 + O_FLAGS) | 0x100);
}

/* $0047FC */
void Stage1Frame(void)
{
    Stage1Camera();
    Stage1Back();
    /* $00563E: the lamps' colours, a step every 4 frames */
    uint16_t i = (uint16_t)(((int16_t)rd16(v_gameFrames) >> 2) & 0xE);
    wr16(v_palette + 0x22, rom16(0x00566A + i));
    wr16(v_palette + 0x24, rom16(0x00567A + i));
    wr16(v_palette + 0x26, rom16(0x00568A + i));
    wr16(v_palette + 0x28, rom16(0x00569A + i));
}

/* $004808 */
void Stage1Camera(void)
{
    call68k(step_routine(STAGE1_CAMERA, rd16(v_cameraStep)));
}

/* $0055B8 */
void Stage1Back(void)
{
    call68k(step_routine(STAGE1_BACK, rd16(v_backStep)));
}

/* the bounds box against the camera's moves */
static void bounds_x(void)
{
    uint32_t v = rd32(v_camSpeedX);
    wr32(v_boundsX0, rd32(v_boundsX0) - v);
    wr32(v_boundsX1, rd32(v_boundsX1) - v);
}

static void bounds_y(void)
{
    uint32_t v = rd32(v_camSpeedY);
    wr32(v_boundsY0, rd32(v_boundsY0) - v);
    wr32(v_boundsY1, rd32(v_boundsY1) - v);
}

static void bounds(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    wr32(v_boundsX0, x0);
    wr32(v_boundsY0, y0);
    wr32(v_boundsX1, x1);
    wr32(v_boundsY1, y1);
}

/* the camera's speeds and plane A's */
static void speeds(uint32_t cx, uint32_t px, uint32_t cy, uint32_t py)
{
    wr32(v_camSpeedX, cx);
    wr32(v_planeSpeeds, px);
    wr32(v_camSpeedY, cy);
    wr32(v_planeSpeeds + 4, py);
}

/* $0054C6: down after the players (below y $138) */
static void follow_down(void)
{
    uint16_t x = 0, y = 0;
    PlayersCentre(&x, &y);
    y = (uint16_t)(y - 0x138);
    if ((int16_t)y <= 0) y = 0;
    wr16(v_camSpeedY, y);
    wr16(v_planeSpeeds + 4, y);
}

/* $0054E0: the per-strip scroll: the tunnel's 20 middle strips by plane A, the rest by the
 * camera */
static void tunnel_strips(void)
{
    uint16_t cam = (uint16_t)-rd16(v_camX), plane = (uint16_t)-rd16(v_planes);
    for (uint16_t k = 0; k < 28; k++) wr16((uint16_t)(HSCROLL_RAM + 0x20 * k), (k >= 5 && k < 25) ? plane : cam);
}

/* $00555E: the same up the shaft, by columns: the middle 12 by plane A */
static void shaft_columns(void)
{
    uint16_t cam = rd16(v_camY), plane = rd16(v_planes + 4);
    for (uint16_t k = 0; k < 20; k++) wr16((uint16_t)(VSCROLL_RAM + 4 * k), (k >= 4 && k < 16) ? plane : cam);
}

/* at a tunnel's end ($200 pixels of x) the camera is put back by $200; 1 when the stage says to
 * go on there */
static int tunnel_end_x(void)
{
    if (rd16(v_camX) & 0x1FF) return 0;
    wr16(v_camX, (uint16_t)(rd16(v_camX) - 0x200));
    return rd16(v_stageEvent) != 0;
}

static int tunnel_end_y(void)
{
    if (rd16(v_camY) & 0x1FF) return 0;
    wr16(v_camY, (uint16_t)(rd16(v_camY) - 0x200));
    return rd16(v_stageEvent) != 0;
}

/* a junction: the turn v_stageEvent picks (0, 2, 4, 6) from the table at `table` */
static void junction(uint32_t table)
{
    uint32_t at = table + (uint32_t)(int16_t)rd16(v_stageEvent);
    call68k(at + (uint32_t)(int16_t)rom16(at));
}

/* $004866. Step 0: down to plane A's y $80, following the players. */
void Stage1Cam00(void)
{
    if ((int32_t)rd32(v_planes + 4) < 0x800000) {
        bounds_x();
        bounds_y();
        follow_down();
        return;
    }
    next_step();
    wr32(v_boundsX1, rd32(v_boundsX0));
    wr32(v_boundsX0, 0x8800000);
    wr32(v_boundsY1, rd32(v_boundsY0));
    wr16(v_boundsY1, (uint16_t)(rd16(v_boundsY1) + 0x100));
    wr32(v_boundsY0, 0);
    Stage1Cam04();
}

/* $0048B0. On down to $100. */
void Stage1Cam04(void)
{
    if ((int32_t)rd32(v_planes + 4) < 0x1000000) {
        bounds_x();
        bounds_y();
        return;
    }
    next_step();
    wr32(v_planes + 4, 0x1000000);
    wr32(v_camY, 0x1000000);
    wr32(v_boundsY0, 0xFF800000);
    wr32(v_boundsY1, 0x1500000);
    CameraSpeed(0x20000, 0);
    Stage1Cam08();
}

/* $004906. Right to $200: the cart, a fight on it. */
void Stage1Cam08(void)
{
    if ((int32_t)rd32(v_planes) < 0x2000000) { bounds_x(); return; }
    next_step();
    wr32(v_planes, 0x2000000);
    wr32(v_camX, 0x2000000);
    wr16(v_palette + 0x2E, 0x202);
    wr16(v_paletteTarget + 0x2E, 0x202);
    set8(v_cameraHold, 7);
    clr8(v_cameraHold, 0);
    CameraStop();
    PlayersUnpin();
    wr32(CART_X, 0);
    wr16(v_stageEvent, 0);
    Stage1Cam0C();
}

/* $00495C. Until the stage says. */
void Stage1Cam0C(void)
{
    if (!rd16(v_stageEvent)) return;
    next_step();
    wr32(v_planeSpeeds, 0);
    Stage1Cam10();
}

/* $00496E. The cart speeds up to 16 a frame; at the next $200 the ride starts. */
void Stage1Cam10(void)
{
    wr32(v_camSpeedX, rd32(v_camSpeedX) + 0x2000);
    if ((int32_t)rd32(v_camSpeedX) < 0x100000) return;
    wr32(v_camSpeedX, 0x100000);
    if (!(((rd32(v_camX) & 0x1FFFFFF) - rd32(v_camSpeedX)) & 0x2000000)) return;
    next_step();
    wr32(v_camX, rd32(v_camX) & 0xFE000000);
    wr32(CART_X, 0x100000);
    wr32(CART_Y, 0x100000);
    if (bit8(v_players, 0)) wr32(v_player1 + 0x88, rd32(CART_X));
    else if (bit8(v_players, 1)) wr32(v_player2 + 0x88, rd32(CART_X));
    PlayersPin();
    wr32(v_planes, 0x2000000);
    wr32(v_camX, 0x2000000);
    bounds(0x1800000, 0xA00000, 0, 0x1500000);
    CameraSpeed(0x100000, 0);
    Stage1Cam14();
}

/* $004A1E. Plane A to $600. */
void Stage1Cam14(void)
{
    if ((int32_t)rd32(v_planes) < 0x6000000) { bounds_x(); return; }
    next_step();
    wr16(v_planeSpeeds, 0);
    wr16(v_stageEvent, 0);
    Stage1Cam18();
}

/* $004A44. Round the tunnel until the stage says: the per-strip scroll on. */
void Stage1Cam18(void)
{
    if (!tunnel_end_x()) return;
    next_step();
    wr8(SCROLL_MODE, 2);
    wr8(v_planeFlagsA, 0xB0);
    wr16(v_stageEvent, 0);
    wr16(STAGE1_EVENT2, 0);
    QueueSound(0xDA);
    Stage1Cam1C();
}

/* $004A82. The strips' tunnel, until the stage says. */
void Stage1Cam1C(void)
{
    if (!tunnel_end_x()) { tunnel_strips(); return; }
    next_step();
    wr8(SCROLL_MODE, 0);
    wr8(v_planeFlagsA, 0x30);
    wr16(v_planes, 0xC00);
    wr16(v_camX, 0xC00);
    wr16(v_planeSpeeds, 0);
    wr16(v_stageEvent, 0);
    Stage1Cam20();
}

/* $004AC4 */
void Stage1Cam20(void)
{
    if (!tunnel_end_x()) return;
    next_step();
    bounds(0x2B80000, 0x1500000, 0x3880000, 0xA00000);
    wr16(v_planeSpeeds, 0x10);
    Stage1Cam24();
}

/* $004B0A. Plane A to $E00, then down at 16. */
void Stage1Cam24(void)
{
    if ((int32_t)rd32(v_planes) < 0xE000000) { bounds_x(); return; }
    next_step();
    wr16(v_camSpeedX, 0);
    wr16(v_planeSpeeds, 0);
    wr16(v_camSpeedY, 0x10);
    wr16(v_planeSpeeds + 4, 0x10);
    Stage1Cam28();
}

/* $004B3C. To plane A's y $300. */
void Stage1Cam28(void)
{
    if ((int32_t)rd32(v_planes + 4) < 0x3000000) { bounds_y(); return; }
    next_step();
    wr16(v_planeSpeeds + 4, 0);
    wr16(v_stageEvent, 0);
    Stage1Cam2C();
}

/* $004B62. Round the shaft until the stage says: the per-column scroll on. */
void Stage1Cam2C(void)
{
    if (!tunnel_end_y()) return;
    next_step();
    wr8(SCROLL_MODE, 4);
    wr8(v_planeFlagsA, 0x70);
    wr16(v_stageEvent, 0);
    wr16(STAGE1_EVENT2, 0);
    QueueSound(0xDA);
    Stage1Cam30();
}

/* $004BA0. The columns' shaft. The 68k masks the camera's y itself to $1FF (andi.w to memory,
 * where the other steps test a copy), so it wraps here at once. */
void Stage1Cam30(void)
{
    wr16(v_camY, rd16(v_camY) & 0x1FF);
    if (rd16(v_camY)) { shaft_columns(); return; }
    wr16(v_camY, (uint16_t)(rd16(v_camY) - 0x200));
    if (!rd16(v_stageEvent)) { shaft_columns(); return; }
    next_step();
    wr8(SCROLL_MODE, 0);
    wr8(v_planeFlagsA, 0x30);
    wr16(v_planeSpeeds + 4, 0);
    wr16(v_stageEvent, 0);
    Stage1Cam34();
}

/* $004BD8 */
void Stage1Cam34(void)
{
    if (!tunnel_end_y()) return;
    next_step();
    uint32_t x0 = rd32(v_boundsX0);
    wr32(v_boundsX0, rd32(v_boundsX1));
    wr32(v_boundsX1, x0);
    wr32(v_boundsY1, 0x3500000);
    wr32(v_boundsY0, 0x2A00000);
    wr16(v_planes + 4, 0x800);
    wr16(v_planeSpeeds + 4, 0x10);
    Stage1Cam38();
}

/* $004C22. Plane A to y $A00, then right at 16. */
void Stage1Cam38(void)
{
    if ((int32_t)rd32(v_planes + 4) < 0xA000000) { bounds_y(); return; }
    next_step();
    wr16(v_planeSpeeds, 0x10);
    wr16(v_camSpeedX, 0x10);
    wr16(v_planeSpeeds + 4, 0);
    wr16(v_camSpeedY, 0);
    Stage1Cam3C();
}

/* $004C54. Plane A to x $1000: the next tiles. */
void Stage1Cam3C(void)
{
    if ((int32_t)rd32(v_planes) < 0x10000000) { bounds_x(); return; }
    next_step();
    uint32_t list = 0x009124;
    NextJob(&list);
    wr16(v_planeSpeeds, 0);
    wr16(v_stageEvent, 0);
    Stage1Cam40();
}

/* $004C86. Round the tunnel until the stage says: the strips again. */
void Stage1Cam40(void)
{
    if (!tunnel_end_x()) return;
    next_step();
    wr8(SCROLL_MODE, 2);
    wr8(v_planeFlagsA, 0xB0);
    wr16(v_stageEvent, 0);
    QueueSound(0xDA);
    Stage1Cam44();
}

/* $004CC0. The strips' tunnel until the stage says: the checkpoint, the maze. */
void Stage1Cam44(void)
{
    if (!tunnel_end_x()) { tunnel_strips(); return; }
    next_step();
    wr16(v_checkpoint, 4);
    wr16(v_checkpoints + 2, rd16(v_checkpoint));
    wr8(SCROLL_MODE, 0);
    wr8(v_planeFlagsA, 0x30);
    wr16(v_planes, 0x1800);
    wr16(v_camX, 0x1800);
    wr16(v_planeSpeeds, 0x10);
    wr16(v_camSpeedX, 0x10);
    Stage1Cam48();
}

/* $004D12. Plane A to $1A00: the maze's first tunnel. */
void Stage1Cam48(void)
{
    if ((int32_t)rd32(v_planes) < 0x1A000000) return;
    wr16(v_stageEvent, 0);
    Stage1Go4C();
}

/* The maze's tunnels: each "Go" sets one up (the step, the cart's push, the camera's place and
 * speed, the bounds), its step rides it to the junction at its end. */

/* $004D24. Right along the upper tunnel. */
void Stage1Go4C(void)
{
    wr16(v_cameraStep, 0x4C);
    wr32(CART_X, 0x100000);
    set_x(0x2000000);
    set_y(0x3000000);
    speeds(0x100000, 0, 0, 0);
    Stage1Cam4C();
}

/* $004D66 */
void Stage1Cam4C(void)
{
    if (rd16(v_camX) & 0x1FF) return;
    wr16(v_camX, (uint16_t)(rd16(v_camX) - 0x200));
    junction(0x004D86);
}

/* $004D72 */
void Stage1Stay(void)
{
}

/* $004D8E. Left along it. */
void Stage1Go50(void)
{
    wr16(v_cameraStep, 0x50);
    wr32(CART_X, 0xFFF00000);
    set_x(0x2000000);
    set_y(0x4000000);
    speeds(0xFFF00000, 0, 0, 0);
    Stage1Cam50();
}

/* $004DD0 */
void Stage1Cam50(void)
{
    if (rd16(v_camX) & 0x1FF) return;
    wr16(v_camX, (uint16_t)(rd16(v_camX) + 0x200));
    junction(0x004DF0);
}

/* $004DDC */
void Stage1Stay2(void)
{
}

/* $004DF8. Down. */
void Stage1Go54(void)
{
    wr16(v_cameraStep, 0x54);
    wr32(CART_Y, 0x100000);
    set_x(0x4000000);
    set_y(0x4000000);
    speeds(0, 0, 0x100000, 0);
    Stage1Cam54();
}

/* $004E3A */
void Stage1Cam54(void)
{
    if (rd16(v_camY) & 0x1FF) return;
    wr16(v_camY, (uint16_t)(rd16(v_camY) - 0x200));
    Stage1Junction54();
}

/* $004E46 */
void Stage1Stay3(void)
{
}

/* $004E4E */
void Stage1Junction54(void)
{
    junction(0x004E5A);
}

/* $004E62. Up. */
void Stage1Go58(void)
{
    wr16(v_cameraStep, 0x58);
    wr32(CART_Y, 0xFFF00000);
    set_x(0);
    set_y(0x6000000);
    speeds(0, 0, 0xFFF00000, 0);
    Stage1Cam58();
}

/* $004EA4 */
void Stage1Cam58(void)
{
    if (rd16(v_camY) & 0x1FF) return;
    wr16(v_camY, (uint16_t)(rd16(v_camY) + 0x200));
    junction(0x004EC4);
}

/* $004EB0 */
void Stage1Stay4(void)
{
}

/* $004ECC. A bend: up, then right. */
void Stage1Go5C(void)
{
    wr16(v_cameraStep, 0x5C);
    wr32(v_boundsX1, 0xB80000);
    wr32(v_boundsX0, 0x1880000);
    wr32(v_boundsY1, 0xFEA00000);
    wr32(v_boundsY0, 0xFF500000);
    wr32(CART_X, 0x100000);
    wr32(CART_Y, 0xFFF00000);
    set_x(0);
    set_y(0x5000000);
    speeds(0, 0, 0xFFF00000, 0xFFF00000);
    Stage1Cam5C();
}

/* $004F3A. Up to y $300. */
void Stage1Cam5C(void)
{
    if ((int32_t)rd32(v_planes + 4) > 0x3000000) { bounds_y(); return; }
    wr16(v_cameraStep, 0x60);
    speeds(0x100000, 0x100000, 0, 0);
    Stage1Cam60();
}

/* $004F72. Right to $200, the junction. */
void Stage1Cam60(void)
{
    if ((int32_t)rd32(v_planes) < 0x2000000) { bounds_x(); return; }
    junction(0x004F98);
}

/* $004FA0. Right, then down. */
void Stage1Go64(void)
{
    wr16(v_cameraStep, 0x64);
    bounds(0x2B80000, 0x1500000, 0x3880000, 0xA00000);
    wr32(CART_X, 0x100000);
    wr32(CART_Y, 0x100000);
    set_x(0x2000000);
    set_y(0x3000000);
    speeds(0x100000, 0x100000, 0, 0);
    Stage1Cam64();
}

/* $00500E */
void Stage1Cam64(void)
{
    if ((int32_t)rd32(v_planes) < 0x4000000) { bounds_x(); return; }
    wr16(v_cameraStep, 0x68);
    speeds(0, 0, 0x100000, 0x100000);
    Stage1Cam68();
}

/* $005046. Down to $500, the junction. */
void Stage1Cam68(void)
{
    if ((int32_t)rd32(v_planes + 4) < 0x5000000) { bounds_y(); return; }
    junction(0x00506C);
}

/* $005074. Down, then left. */
void Stage1Go6C(void)
{
    wr16(v_cameraStep, 0x6C);
    bounds(0xB80000, 0x2A00000, 0x1880000, 0x3500000);
    wr32(CART_X, 0xFFF00000);
    wr32(CART_Y, 0x100000);
    set_x(0x4000000);
    set_y(0x4000000);
    speeds(0, 0, 0x100000, 0x100000);
    Stage1Cam6C();
}

/* $0050E2 */
void Stage1Cam6C(void)
{
    if ((int32_t)rd32(v_planes + 4) < 0x6000000) { bounds_y(); return; }
    wr16(v_cameraStep, 0x70);
    speeds(0xFFF00000, 0xFFF00000, 0, 0);
    Stage1Cam70();
}

/* $00511A. Left to $200, the junction. */
void Stage1Cam70(void)
{
    if ((int32_t)rd32(v_planes) > 0x2000000) { bounds_x(); return; }
    junction(0x005140);
}

/* $005148. Left, then up. */
void Stage1Go74(void)
{
    wr16(v_cameraStep, 0x74);
    wr32(v_boundsX1, 0xFEB80000);
    wr32(v_boundsX0, 0xFF880000);
    wr32(v_boundsY0, 0xA00000);
    wr32(v_boundsY1, 0x1500000);
    wr32(CART_X, 0xFFF00000);
    wr32(CART_Y, 0xFFF00000);
    set_x(0x2000000);
    set_y(0x6000000);
    speeds(0xFFF00000, 0xFFF00000, 0, 0);
    Stage1Cam74();
}

/* $0051B6. Left to 0. */
void Stage1Cam74(void)
{
    if ((int32_t)rd32(v_planes) > 0) { bounds_x(); return; }
    wr16(v_cameraStep, 0x78);
    speeds(0, 0, 0xFFF00000, 0xFFF00000);
    Stage1Cam78();
}

/* $0051EE. Up to $400, the junction. */
void Stage1Cam78(void)
{
    if ((int32_t)rd32(v_planes + 4) > 0x4000000) { bounds_y(); return; }
    junction(0x005214);
}

/* $00521C. Up, then left. */
void Stage1Go7C(void)
{
    wr16(v_cameraStep, 0x7C);
    wr32(v_boundsX0, 0xB80000);
    wr32(v_boundsX1, 0x1880000);
    wr32(v_boundsY1, 0xFEA00000);
    wr32(v_boundsY0, 0xFF500000);
    wr32(CART_X, 0xFFF00000);
    wr32(CART_Y, 0xFFF00000);
    set_x(0x4000000);
    set_y(0x5000000);
    speeds(0, 0, 0xFFF00000, 0xFFF00000);
    Stage1Cam7C();
}

/* $00528A */
void Stage1Cam7C(void)
{
    if ((int32_t)rd32(v_planes + 4) > 0x3000000) { bounds_y(); return; }
    wr16(v_cameraStep, 0x80);
    speeds(0xFFF00000, 0xFFF00000, 0, 0);
    Stage1Cam70();
}

/* $0052C6. Left, then down. */
void Stage1Go84(void)
{
    wr16(v_cameraStep, 0x84);
    wr32(v_boundsX1, 0xFEB80000);
    wr32(v_boundsX0, 0xFF880000);
    wr32(v_boundsY1, 0xA00000);
    wr32(v_boundsY0, 0x1500000);
    wr32(CART_X, 0xFFF00000);
    wr32(CART_Y, 0x100000);
    set_x(0x2000000);
    set_y(0x3000000);
    speeds(0xFFF00000, 0xFFF00000, 0, 0);
    Stage1Cam84();
}

/* $005334 */
void Stage1Cam84(void)
{
    if ((int32_t)rd32(v_planes) > 0) { bounds_x(); return; }
    wr16(v_cameraStep, 0x88);
    speeds(0, 0, 0x100000, 0x100000);
    Stage1Cam68();
}

/* $005370. Down, then right. */
void Stage1Go8C(void)
{
    wr16(v_cameraStep, 0x8C);
    wr32(v_boundsX1, 0xB80000);
    wr32(v_boundsX0, 0x1880000);
    wr32(v_boundsY0, 0x2A00000);
    wr32(v_boundsY1, 0x3500000);
    wr32(CART_X, 0x100000);
    wr32(CART_Y, 0x100000);
    set_x(0);
    set_y(0x4000000);
    speeds(0, 0, 0x100000, 0x100000);
    Stage1Cam8C();
}

/* $0053DE */
void Stage1Cam8C(void)
{
    if ((int32_t)rd32(v_planes + 4) < 0x6000000) { bounds_y(); return; }
    wr16(v_cameraStep, 0x90);
    speeds(0x100000, 0x100000, 0, 0);
    Stage1Cam60();
}

/* $00541A. Right, then up. */
void Stage1Go94(void)
{
    wr16(v_cameraStep, 0x94);
    bounds(0x2B80000, 0xA00000, 0x3880000, 0x1500000);
    wr32(CART_X, 0x100000);
    wr32(CART_Y, 0xFFF00000);
    set_x(0x2000000);
    set_y(0x6000000);
    speeds(0x100000, 0x100000, 0, 0);
    Stage1Cam94();
}

/* $005488 */
void Stage1Cam94(void)
{
    if ((int32_t)rd32(v_planes) < 0x4000000) { bounds_x(); return; }
    wr16(v_cameraStep, 0x98);
    speeds(0, 0, 0xFFF00000, 0xFFF00000);
    Stage1Cam78();
}

/* $0054C4 */
void Stage1Cam9C(void)
{
}

/* ---- stage 1's background ---- */

/* $005620: plane B's camera at half the camera's x speed, a quarter of its y; plane B with it
 * in y */
static void back_half_quarter(void)
{
    wr32(v_camSpeedX + 8, (uint32_t)((int32_t)rd32(v_camSpeedX) >> 1));
    wr32(v_planeSpeeds + 8, 0);
    uint32_t y = (uint32_t)((int32_t)rd32(v_camSpeedY) >> 2);
    wr32(v_camSpeedY + 8, y);
    wr32(v_planeSpeeds + 0xC, y);
}

/* $0055CE. Background step 0: until x $200. */
void Stage1Back00(void)
{
    if ((int32_t)rd32(v_camX) < 0x2000000) return;
    next_back();
    Stage1Back04();
}

/* $0055E0. Down with the camera to the second camera's y $100. */
void Stage1Back04(void)
{
    if ((int32_t)rd32(v_camY + 8) < 0x1000000) { back_half_quarter(); return; }
    next_back();
    wr32(v_camY + 8, 0x1000000);
    wr32(v_planes + 0xC, 0x1000000);
    Stage1Back08();
}

/* $005600. Then plane B still. */
void Stage1Back08(void)
{
    wr32(v_camSpeedX + 8, (uint32_t)((int32_t)rd32(v_camSpeedX) >> 1));
    wr32(v_planeSpeeds + 8, 0);
    wr32(v_camSpeedY + 8, (uint32_t)((int32_t)rd32(v_camSpeedY) >> 2));
    wr32(v_planeSpeeds + 0xC, 0);
}

/* $00561E */
void Stage1Back0C(void)
{
}

/* ---- shared: a chunk's tiles to the VRAM by the DMA queue, the players put in place ---- */

/* $001AB0. A chunk (4 x 4 tiles, 32 bytes at src) queued for DMA to the VRAM at vram, a row
 * (8 bytes) at a time: four entries of the queue, each the last copied with the next row's
 * address ($80 on) and source (8 bytes on, the carry taken through its three bytes). The
 * address keeps bit 13 and the low 12 (plane A's $1000 bytes wrap). */
void ChunkDma(uint32_t src, uint16_t vram)
{
    uint16_t q = rd16(v_dmaQueue);
    q = (uint16_t)(q - 2); wr16(q, 0x83);
    q = (uint16_t)(q - 2); wr16(q, (uint16_t)((vram & 0x2000) | (vram & 0xFFF) | 0x4000));
    uint32_t s = (src & 0xFFFFFF) >> 1;
    q = (uint16_t)(q - 2); wr16(q, (uint16_t)(0x9500 | (s & 0xFF)));
    q = (uint16_t)(q - 2); wr16(q, (uint16_t)(0x9600 | ((s >> 8) & 0xFF)));
    q = (uint16_t)(q - 2); wr16(q, (uint16_t)(0x9700 | ((s >> 16) & 0xFF)));
    q = (uint16_t)(q - 2); wr16(q, 0x8F02);
    q = (uint16_t)(q - 4); wr32(q, 0x94009304);
    for (int row = 0; row < 3; row++) {
        uint32_t v = rd32((uint16_t)(q + 12));
        q = (uint16_t)(q - 4); wr32(q, v);
        wr16(q, (uint16_t)(rd16(q) + 0x80));
        v = rd32((uint16_t)(q + 12));
        q = (uint16_t)(q - 4); wr32(q, v);
        uint16_t w = rd16((uint16_t)(q + 14));
        q = (uint16_t)(q - 2); wr16(q, w);
        uint8_t lo = rd8((uint16_t)(q + 5));
        wr8((uint16_t)(q + 5), (uint8_t)(lo + 4));
        if (lo >= 0xFC) {
            uint8_t mid = (uint8_t)(rd8((uint16_t)(q + 3)) + 1);
            wr8((uint16_t)(q + 3), mid);
            if (!mid) wr8((uint16_t)(q + 1), (uint8_t)(rd8((uint16_t)(q + 1)) + 1));
        }
        q = (uint16_t)(q - 2); wr16(q, 0x8F02);
        q = (uint16_t)(q - 4); wr32(q, 0x94009304);
    }
    wr16(v_dmaQueue, q);
}

/* $001AB8. The same, the chunk first copied into plane A's buffer where that VRAM address is
 * (the buffer's half by plane A's y); none for VRAM $E000 on. */
void PlaneAChunk(uint32_t src, uint16_t vram)
{
    if (vram < 0xE000) {
        uint16_t at = (uint16_t)((((rd16(v_planes + 4) << 4) & 0x1000) + vram) & 0x1FFF);
        for (uint16_t k = 0; k < 4; k++) {
            wr32((uint16_t)(at + 0x80 * k), bus_rd32(src + 8 * k));
            wr32((uint16_t)(at + 0x80 * k + 4), bus_rd32(src + 8 * k + 4));
        }
    }
    ChunkDma(src, vram);
}

/* $00F0BC. A player put at x, y in a state, still. */
void PlayerPlace(uint16_t obj, uint16_t x, uint16_t y, uint16_t state)
{
    wr16(obj + O_X, x);
    wr16(obj + O_Y, y);
    wr16(obj + O_STATE, state);
    wr16(obj + O_X + 2, 0);
    wr16(obj + O_Y + 2, 0);
    wr32(obj + P_VX, 0);
    wr32(obj + P_VY, 0);
    wr16(obj + 6, 0);
    wr8(obj + 0x22, 0);
}

/* $00F074. Player 1 (16 to the right when both play). */
void Player1Place(uint16_t x, uint16_t y, uint16_t state)
{
    if (!bit8(v_players, 0)) return;
    PlayerPlace(v_player1, x, y, state);
    if (rd8(v_players) == 3) wr16(v_player1 + O_X, (uint16_t)(rd16(v_player1 + O_X) + 0x10));
}

/* $00F098. Player 2 (16 to the left when both play). */
void Player2Place(uint16_t x, uint16_t y, uint16_t state)
{
    if (!bit8(v_players, 1)) return;
    PlayerPlace(v_player2, x, y, state);
    if (rd8(v_players) == 3) wr16(v_player2 + O_X, (uint16_t)(rd16(v_player2 + O_X) - 0x10));
}

/* $00F070 */
void PlayersPlace(uint16_t x, uint16_t y, uint16_t state)
{
    Player2Place(x, y, state);
    Player1Place(x, y, state);
}

/* $00427E. Both held for the stage's scene. */
void PlayersStageHold(void)
{
    set8(v_player1 + P_STAGE_HOLD, 7);
    set8(v_player2 + P_STAGE_HOLD, 7);
}

/* $00419A. Plane B and the second camera at these speeds. */
void BackSpeed(uint32_t x, uint32_t y)
{
    wr32(v_camSpeedX + 8, x);
    wr32(v_planeSpeeds + 8, x);
    wr32(v_camSpeedY + 8, y);
    wr32(v_planeSpeeds + 0xC, y);
}

/* ---- stage 2, the battleship: the ship sinks and rises (plane A's columns: the first four the
 * ship's bow at its own y), then the climb up its side, the deck, the bridge ---- */

#define STAGE2_CAMERA 0x00590Au
#define STAGE2_BACK   0x00673Cu
#define BOW_Y      0xA994        /* long: the bow's y (the first four columns' scroll) */
#define SHIP_Y     0xA9A4        /* long: the rest's */
#define BOW_SPEED  0xA99C
#define BOW_PULL   0xA9B4        /* taken off the bow's speed each frame */
#define SHIP_SPEED 0xA9AC        /* the ship's y speed (the camera's too), bobbing round */
#define BOB_MIDDLE 0xA9B0
#define BOB_DIR    0xA98C        /* 0 slowing, 4 speeding up */
#define RUMBLE_SPRITES 0xA988
#define RUMBLE_PLANE   0xA98A
#define BOW_COLUMN 0xA9B8        /* the bow's last drawn column (y & $1E0) */
#define STAGE2_SIGNAL 0xA9C0     /* the camera's script tells the background's to go on */
#define CAUGHT1 0xA980           /* bytes: a player fallen off the top is put back at y $40 */
#define CAUGHT2 0xA981
#define BACK_SPEED_X 0xA9D0      /* plane B's speeds (set by the stage's objects) */
#define BACK_SPEED_Y 0xA9D4
#define BANDS 0xA9E0             /* four longs: the sky's bands' x */
#define INPUT_C 0x0C             /* PlayerInputC */

/* $0058F4 */
void Stage2Frame(void)
{
    Stage2Camera();
    call68k(step_routine(STAGE2_BACK, rd16(v_backStep)));
}

/* $0058FC */
void Stage2Camera(void)
{
    call68k(step_routine(STAGE2_CAMERA, rd16(v_cameraStep)));
}

/* $005E3C. The rumble's two counters, down one every 64 and 128 frames, to the shakes. */
void Stage2Rumble(void)
{
    uint16_t f = rd16(v_gameFrames) & 0x7F;
    if (!f && rd16(RUMBLE_PLANE)) wr16(RUMBLE_PLANE, (uint16_t)(rd16(RUMBLE_PLANE) - 1));
    f &= 0x3F;
    if (!f && rd16(RUMBLE_SPRITES)) wr16(RUMBLE_SPRITES, (uint16_t)(rd16(RUMBLE_SPRITES) - 1));
    wr16(v_shakeSprites, rd16(RUMBLE_SPRITES));
    wr16(v_shakePlane, rd16(RUMBLE_PLANE));
}

/* $005F66. The ship's speed bobbing: down $100 a frame to the middle less $4000, then up to the
 * middle plus $4000. */
void Stage2Bob(void)
{
    int32_t mid = (int32_t)rd32(BOB_MIDDLE);
    if (!rd16(BOB_DIR)) {
        int32_t lim = mid - 0x4000;
        wr32(SHIP_SPEED, rd32(SHIP_SPEED) - 0x100);
        if (lim < (int32_t)rd32(SHIP_SPEED)) return;
        wr32(SHIP_SPEED, (uint32_t)lim);
        wr16(BOB_DIR, 4);
        return;
    }
    int32_t lim = mid + 0x4000;
    wr32(SHIP_SPEED, rd32(SHIP_SPEED) + 0x100);
    if (lim > (int32_t)rd32(SHIP_SPEED)) return;
    wr32(SHIP_SPEED, (uint32_t)lim);
    wr16(BOB_DIR, 0);
}

/* $005FB4. The bow's speed less the pull, -16 at least. */
void Stage2BowPull(void)
{
    int32_t v = (int32_t)(rd32(BOW_SPEED) - rd32(BOW_PULL));
    if (v <= (int32_t)0xFFF00000) v = (int32_t)0xFFF00000;
    wr32(BOW_SPEED, (uint32_t)v);
}

/* $005FD2. A new column of the bow (every 32 pixels of its y): two chunks into plane A. */
void Stage2BowColumn(void)
{
    uint16_t c = rd16(BOW_Y) & 0x1E0;
    if (c == rd16(BOW_COLUMN)) return;
    wr16(BOW_COLUMN, c);
    c >>= 4;
    uint32_t src = 0xFFFF0000u | rom16(0x00600C + (c & 2));
    uint16_t vram = rom16(0x006010 + c);
    PlaneAChunk(src, vram);
    PlaneAChunk(0xFFFF0000u | (uint16_t)(src + 0x20), (uint16_t)(vram + 8));
}

/* $006030. The same, the bow's plain chunk. */
void Stage2BowColumnPlain(void)
{
    uint16_t c = rd16(BOW_Y) & 0x1E0;
    if (c == rd16(BOW_COLUMN)) return;
    wr16(BOW_COLUMN, c);
    c >>= 4;
    uint16_t vram = rom16(0x006010 + c);
    PlaneAChunk(0xFFFF4000u, vram);
    PlaneAChunk(0xFFFF4000u, (uint16_t)(vram + 8));
}

/* four chunks along from src, to vram 8 bytes apart */
static void chunk_row(uint32_t src, uint16_t vram)
{
    for (int k = 0; k < 4; k++) {
        ChunkDma(src, vram);
        vram = (uint16_t)(vram + 8);
        src = 0xFFFF0000u | (uint16_t)(src + 0x20);
    }
}

/* $006060. While the camera's y is $2C0-$39F, the lights' frames (one row each other frame). */
void Stage2Lights(void)
{
    int16_t y = (int16_t)rd16(v_camY);
    if (y >= 0x3A0 || y < 0x2C0) return;
    uint16_t f = rd16(v_gameFrames);
    if (!(f & 1)) chunk_row(0xFFFF0000u | rom16(0x0060F2 + (f & 2)), 0xC810);
    else chunk_row(0xFFFF0000u | rom16(0x0060F6 + (f & 2)), 0xCA10);
}

/* a chunk's frame (of two) by DMA while y is in lo..hi */
static void chunk_in(int16_t lo, int16_t hi, uint32_t table, uint16_t vram, uint16_t frame, uint16_t y)
{
    if ((int16_t)y < lo || (int16_t)y >= hi) return;
    ChunkDma(0xFFFF0000u | rom16(table + frame), vram);
}

/* the side's animated chunks, each while the camera's y is in its range */
/* $0061E2 */
void Stage2SideA(uint16_t vram, uint16_t frame, uint16_t y) { chunk_in(0x240, 0x320, 0x0061FE, vram, frame, y); }
/* $0061C2 */
void Stage2SideB(uint16_t vram, uint16_t frame, uint16_t y) { chunk_in(0x260, 0x340, 0x0061DE, vram, frame, y); }
/* $0061A2 */
void Stage2SideC(uint16_t vram, uint16_t frame, uint16_t y) { chunk_in(0x280, 0x360, 0x0061BE, vram, frame, y); }
/* $006182 */
void Stage2SideD(uint16_t vram, uint16_t frame, uint16_t y) { chunk_in(0x2A0, 0x380, 0x00619E, vram, frame, y); }
/* $006162 */
void Stage2SideE(uint16_t vram, uint16_t frame, uint16_t y) { chunk_in(0x2C0, 0x3A0, 0x00617E, vram, frame, y); }

/* $0060FA. Left of x $1080, the side's six animated chunks down a column. */
void Stage2Side(void)
{
    if ((int16_t)rd16(v_camX) >= 0x1080) return;
    uint16_t vram = 0xC008, frame = rd16(v_gameFrames) & 2, y = rd16(v_camY);
    Stage2SideA(vram, frame, y);
    vram = (uint16_t)(vram + 0x200);
    Stage2SideB(vram, frame, y);
    vram = (uint16_t)(vram + 0x200);
    Stage2SideC(vram, frame, y);
    vram = (uint16_t)(vram + 0x200);
    Stage2SideD(vram, frame, y);
    vram = (uint16_t)(vram + 0x200);
    Stage2SideE(vram, frame, y);
    vram = (uint16_t)(vram + 0x200);
    chunk_in(0x2E0, 0x3C0, 0x00615E, vram, frame, y);
}

/* the deck's animated chunks, each while the camera's x is in its range */
/* $006376 */
void Stage2DeckA(uint16_t vram, uint16_t frame, uint16_t x) { chunk_in(0x1380, 0x14E0, 0x006392, vram, frame, x); }
/* $006356 */
void Stage2DeckB(uint16_t vram, uint16_t frame, uint16_t x) { chunk_in(0x13A0, 0x1500, 0x006372, vram, frame, x); }
/* $006336 */
void Stage2DeckC(uint16_t vram, uint16_t frame, uint16_t x) { chunk_in(0x13C0, 0x1520, 0x006352, vram, frame, x); }
/* $006316 */
void Stage2DeckD(uint16_t vram, uint16_t frame, uint16_t x) { chunk_in(0x13E0, 0x1540, 0x006332, vram, frame, x); }
/* $0062F6 */
void Stage2DeckE(uint16_t vram, uint16_t frame, uint16_t x) { chunk_in(0x1400, 0x1560, 0x006312, vram, frame, x); }
/* $0062D6 */
void Stage2DeckF(uint16_t vram, uint16_t frame, uint16_t x) { chunk_in(0x1420, 0x1580, 0x0062F2, vram, frame, x); }
/* $0062B6 */
void Stage2DeckG(uint16_t vram, uint16_t frame, uint16_t x) { chunk_in(0x1440, 0x15A0, 0x0062D2, vram, frame, x); }
/* $006296 */
void Stage2DeckH(uint16_t vram, uint16_t frame, uint16_t x) { chunk_in(0x1460, 0x15C0, 0x0062B2, vram, frame, x); }
/* $006276 */
void Stage2DeckI(uint16_t vram, uint16_t frame, uint16_t x) { chunk_in(0x1480, 0x15E0, 0x006292, vram, frame, x); }

/* $006202: the deck's ten, along a row (rows $C430 and $C830 on alternate frames) */
static void deck_row(void)
{
    uint16_t f = rd16(v_gameFrames);
    uint16_t vram = (f & 1) ? 0xC830 : 0xC430, frame = f & 2, x = rd16(v_camX);
    void (*const deck[])(uint16_t, uint16_t, uint16_t) = {
        Stage2DeckA, Stage2DeckB, Stage2DeckC, Stage2DeckD, Stage2DeckE,
        Stage2DeckF, Stage2DeckG, Stage2DeckH, Stage2DeckI,
    };
    for (int k = 0; k < 9; k++) {
        deck[k](vram, frame, x);
        vram = (uint16_t)(vram + 8);
    }
    chunk_in(0x14A0, 0x1600, 0x006272, vram, frame, x);
}

/* $006396. The per-column scroll: the bow's four columns at its y (rounded up), the rest at the
 * ship's, both less the sprites' shake. */
void Stage2ColumnScroll(void)
{
    uint16_t bow = rd16(BOW_Y);
    if (rd16(BOW_Y + 2)) bow++;
    bow = (uint16_t)(bow - rd16(v_cameraY));
    uint16_t ship = (uint16_t)(rd16(SHIP_Y) - rd16(v_cameraY));
    for (uint16_t k = 0; k < 20; k++) wr16((uint16_t)(VSCROLL_RAM + 4 * k), k < 4 ? bow : ship);
}

/* a player fallen off the top (or in state $64) put back at y $40, for as long as its flag is
 * set */
static void catch_player(uint16_t obj, uint16_t flag)
{
    if (!rd8(flag)) {
        if (!(rd16(obj + O_Y) & 0x8000) && rd16(obj + O_STATE) != 0x64) return;
        wr8(flag, 1);
    }
    wr16(obj + O_Y, 0x40);
}

/* $006442 */
void Stage2Catch2(void)
{
    if (bit8(v_players, 1)) catch_player(v_player2, CAUGHT2);
}

/* $006402. Once plane A is below $2C0. */
void Stage2Catch(void)
{
    if ((int16_t)rd16(v_planes + 4) <= 0x2C0) return;
    Stage2Catch2();
    if (bit8(v_players, 0)) catch_player(v_player1, CAUGHT1);
}

/* $006474. The players no longer held between the top and bottom limits. */
void PlayersUnlimitY(void)
{
    if (bit8(v_players, 0)) clr8(v_player1 + P_LIMITS, 0);
    if (bit8(v_players, 1)) clr8(v_player2 + P_LIMITS, 0);
}

/* $006496. And held again. */
void PlayersLimitY(void)
{
    if (bit8(v_players, 0)) set8(v_player1 + P_LIMITS, 0);
    if (bit8(v_players, 1)) set8(v_player2 + P_LIMITS, 0);
}

/* $005E76. While a scene holds either, both press C (PlayerInputC). */
void Stage2SceneInput(void)
{
    if (!((rd16(v_player1 + P_STAGE_HOLD) | rd16(v_player2 + P_STAGE_HOLD)) & 0x8000)) return;
    if (bit8(v_players, 0)) wr16(v_player1 + P_INPUT, INPUT_C);
    if (bit8(v_players, 1)) wr16(v_player2 + P_INPUT, INPUT_C);
}

/* $005EA4. A player in state $64 put at x $120. */
void Stage2StateX(void)
{
    if (bit8(v_players, 0) && rd16(v_player1 + O_STATE) == 0x64) wr16(v_player1 + O_X, 0x120);
    if (bit8(v_players, 1) && rd16(v_player2 + O_STATE) == 0x64) wr16(v_player2 + O_X, 0x120);
}

/* a player carried with the level while it stands on a floor (and +$22 bit 3 is clear) */
static void pin_on_floor(uint16_t obj)
{
    if (rd16(obj + P_FLOOR) && !bit8(obj + 0x22, 3)) {
        wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) | 0x100);
        return;
    }
    wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) & 0xFEFF);
}

/* $005F0A */
void Player2PinOnFloor(void)
{
    if (bit8(v_players, 1)) pin_on_floor(v_player2);
}

/* the camera's y speed and plane A's after the players, below y $120 */
static void follow_down_120(int plane)
{
    uint16_t x = 0, y = 0;
    PlayersCentre(&x, &y);
    y = (uint16_t)(y - 0x120);
    if ((int16_t)y <= 0) y = 0;
    wr16(v_camSpeedY, y);
    if (plane) wr16(v_planeSpeeds + 4, y);
}

/* the ship's move: the bow and the rest by their speeds, the camera with the ship */
static void ship_move(void)
{
    wr32(SHIP_Y, rd32(SHIP_Y) + rd32(SHIP_SPEED));
    wr32(BOW_Y, rd32(BOW_Y) + rd32(BOW_SPEED));
    wr32(v_planeSpeeds + 4, rd32(SHIP_SPEED));
    wr32(v_camSpeedY, rd32(SHIP_SPEED));
}

/* $005932. Step 0: sinking (speeding up to $C000, then bobbing) to plane A's y $580. */
void Stage2Cam00(void)
{
    if ((int32_t)rd32(v_planes + 4) < 0x5800000) {
        Stage2ColumnScroll();
        Stage2Rumble();
        wr32(SHIP_SPEED, rd32(SHIP_SPEED) + 0x100);
        if ((int32_t)rd32(SHIP_SPEED) >= 0xC000) {
            wr32(BOB_MIDDLE, 0xC000);
            Stage2Bob();
        }
        wr32(SHIP_Y, rd32(SHIP_Y) + rd32(SHIP_SPEED));
        wr32(v_planeSpeeds + 4, rd32(SHIP_SPEED));
        wr32(v_camSpeedY, rd32(SHIP_SPEED));
        return;
    }
    next_step();
    wr16(STAGE2_SIGNAL, 1);
    PlayersUnpin();
    wr32(BOW_SPEED, 0xFFFFC000);
    wr32(SHIP_SPEED, rd32(SHIP_SPEED) + 0xFFFFC000);
    wr32(BOB_MIDDLE, rd32(BOB_MIDDLE) + 0xFFFFC000);
    wr32(BOW_PULL, 0x18);
    Stage2Cam04();
}

/* $0059A4. Rising, the bow first, to $300. */
void Stage2Cam04(void)
{
    if ((int32_t)rd32(v_planes + 4) <= 0x3000000) {
        next_step();
        wr32(BOB_MIDDLE, 0);
        wr32(v_camY, rd32(SHIP_Y));
        wr16(v_stageEvent, 0x410);
        Stage2Cam08();
        return;
    }
    Stage2BowColumn();
    Stage2ColumnScroll();
    Stage2Lights();
    Stage2BowPull();
    wr32(BOB_MIDDLE, rd32(BOB_MIDDLE) - 0x40);
    if ((int32_t)rd32(BOB_MIDDLE) <= (int32_t)0xFFFFC000) wr32(BOB_MIDDLE, 0xFFFFC000);
    Stage2Bob();
    ship_move();
}

/* $005A16. $410 frames more, the bow's pull growing to $400. */
void Stage2Cam08(void)
{
    wr16(v_stageEvent, (uint16_t)(rd16(v_stageEvent) - 1));
    if (!rd16(v_stageEvent)) {
        next_step();
        wr16(STAGE2_SIGNAL, 1);
        wr16(v_stageEvent, (uint16_t)(rd16(BOW_Y) - 0x200));
        Stage2Cam0C();
        return;
    }
    Stage2BowColumn();
    Stage2ColumnScroll();
    Stage2Lights();
    wr32(BOW_PULL, rd32(BOW_PULL) + 1);
    if ((int32_t)rd32(BOW_PULL) >= 0x400) wr32(BOW_PULL, 0x400);
    Stage2BowPull();
    Stage2Bob();
    ship_move();
}

/* $005A82. Until the bow is $200 above where it was: then the ship whole again (the columns'
 * scroll off), x $600. */
void Stage2Cam0C(void)
{
    if ((int16_t)rd16(BOW_Y) <= (int16_t)rd16(v_stageEvent)) {
        next_step();
        wr8(SCROLL_MODE, 0);
        wr8(v_planeFlagsA, 0);
        wr16(v_battleship, 0);
        wr32(v_planes, 0x6000000);
        wr32(v_camX, rd32(v_planes));
        wr16(v_stageEvent, 0x400);
        Stage2Cam10();
        return;
    }
    Stage2BowColumnPlain();
    Stage2ColumnScroll();
    Stage2Lights();
    Stage2BowPull();
    Stage2Bob();
    ship_move();
}

/* $005AE4. $400 frames of bobbing. */
void Stage2Cam10(void)
{
    wr16(v_stageEvent, (uint16_t)(rd16(v_stageEvent) - 1));
    if (!rd16(v_stageEvent)) {
        next_step();
        wr16(STAGE2_SIGNAL, 1);
        PlayersUnlimitY();
        wr8(CAUGHT1, 0);
        wr8(CAUGHT2, 0);
        QueueSound(0xDA);
        Stage2Cam14();
        return;
    }
    Stage2Lights();
    Stage2Bob();
    wr32(v_planeSpeeds + 4, rd32(SHIP_SPEED));
    wr32(v_camSpeedY, rd32(SHIP_SPEED));
}

/* $005B22. Down, faster to 7, to $700: the next part's tiles, the camera at $1000, $600. */
void Stage2Cam14(void)
{
    if ((int32_t)rd32(v_planes + 4) >= 0x7000000) {
        next_step();
        wr32(v_planes + 4, 0x7000000);
        uint32_t list = 0x009234;
        NextJob(&list);
        wr16(v_planes, 0x1000);
        wr16(v_planes + 4, 0x600);
        wr32(v_camX, rd32(v_planes));
        wr32(v_camY, rd32(v_planes + 4));
        CameraStop();
        Stage2Cam18();
        return;
    }
    Stage2Catch();
    Stage2Lights();
    wr32(v_camSpeedY, rd32(v_camSpeedY) + 0x400);
    wr32(v_planeSpeeds + 4, rd32(v_planeSpeeds + 4) + 0x400);
    if ((int32_t)rd32(v_camSpeedY) >= 0x70000) {
        wr32(v_camSpeedY, 0x70000);
        wr32(v_planeSpeeds + 4, 0x70000);
    }
}

/* $005B96. When the tiles are in, up the side. */
void Stage2Cam18(void)
{
    if (rd16(v_jobKind)) { Stage2Catch(); return; }
    next_step();
    CameraSpeed(0, 0xFFF9C000);
    QueueSound(0xDA);
    Stage2Cam1C();
}

/* $005BB8. Up (slowing) to $220. */
void Stage2Cam1C(void)
{
    if ((int32_t)rd32(v_planes + 4) > 0x2200000) {
        Stage2Catch();
        Stage2Side();
        wr32(v_camSpeedY, rd32(v_camSpeedY) + 0x400);
        wr32(v_planeSpeeds + 4, rd32(v_planeSpeeds + 4) + 0x400);
        return;
    }
    next_step();
    set8(v_cameraHold, 7);
    clr8(v_cameraHold, 0);
    CameraStop();
    set_y(0x2200000);
    uint32_t list = 0x009266;
    NextJob(&list);
    PlayersLimitY();
    PlayersPin();
    Stage2Cam20();
}

/* $005C16. The fight, its tiles in. */
void Stage2Cam20(void)
{
    if (rd16(v_jobKind) || (rd8(v_cameraHold) & 0x80)) { Stage2Side(); return; }
    next_step();
    uint32_t list = 0x009278;
    NextJob(&list);
    CameraStop();
    Stage2Cam24();
}

/* $005C3E. The checkpoint. */
void Stage2Cam24(void)
{
    if (rd16(v_jobKind)) { Stage2Side(); return; }
    next_step();
    wr16(v_checkpoint, 4);
    wr16(v_checkpoints + 4, rd16(v_checkpoint));
    CameraStop();
    wr32(v_planes, rd32(v_camX));
    wr32(v_planes, rd32(v_planes) - 0x400000);
    Stage2Cam28();
}

/* $005C68. Along the deck to x $1340. */
void Stage2Cam28(void)
{
    if ((int32_t)rd32(v_camX) < 0x13400000) {
        CameraFollowRight();
        Stage2Side();
        return;
    }
    next_step();
    wr32(v_camX, 0x13400000);
    wr32(v_planes, 0x13000000);
    set8(v_cameraHold, 7);
    clr8(v_cameraHold, 0);
    CameraStop();
    uint32_t list = 0x009282;
    NextJob(&list);
    Stage2Cam2C();
}

/* $005CAC */
void Stage2Cam2C(void)
{
    if ((rd8(v_cameraHold) & 0x80) || rd16(v_jobKind)) return;
    next_step();
    Stage2Cam30();
}

/* $005CC2. To x $1540: the object at $FFA800 (type $284), the camera on at 2. */
void Stage2Cam30(void)
{
    if ((int32_t)rd32(v_camX) < 0x15400000) {
        CameraFollowRight();
        deck_row();
        return;
    }
    next_step();
    wr32(v_camX, 0x15400000);
    wr32(v_planes, 0x15000000);
    wr16(0xA800 + O_TYPE, 0x284);
    CameraSpeed(0x20000, 0);
    Stage2Cam34();
}

/* $005CFC. To x $1680: the bridge's fight (the per-line scroll). */
void Stage2Cam34(void)
{
    if ((int32_t)rd32(v_camX) < 0x16800000) { deck_row(); return; }
    next_step();
    wr32(v_camX, 0x16800000);
    wr32(v_planes, 0x16400000);
    set8(v_cameraHold, 7);
    clr8(v_cameraHold, 0);
    CameraStop();
    wr8(SCROLL_MODE, 3);
    wr16(v_stageEvent, 0);
    Stage2Cam38();
}

/* $005D36. Until the stage says (the players pressing C while a scene holds them, carried with
 * the level on floors); then the escape: the players put in place, a new part of the map. */
void Stage2Cam38(void)
{
    if (!rd16(v_stageEvent)) {
        Stage2SceneInput();
        Stage2StateX();
        Player2PinOnFloor();
        if (bit8(v_players, 0)) pin_on_floor(v_player1);
        return;
    }
    next_step();
    wr16(STAGE2_SIGNAL, 1);
    PlayersPin();
    PlayersStageHold();
    PlayersPlace(0x120, 0x100, 0);
    wr32(v_speedLimitY, 0xE0000);
    wr8(v_planeFlagsA, 0x30);
    set_x(0xA000000);
    set_y(0x1000000);
    CameraStop();
    clr8(v_cameraHold, 7);
    QueueSound(0xD8);
    Stage2Cam3C();
}

/* $005DB0. Down after the players to $600. */
void Stage2Cam3C(void)
{
    if ((int32_t)rd32(v_planes + 4) < 0x6000000) { follow_down_120(1); return; }
    next_step();
    wr32(v_planeSpeeds + 4, 0);
    Stage2Cam40();
}

/* $005DC4. Once plane B is at $800, until the camera crosses a $200 line. */
void Stage2Cam40(void)
{
    if ((int32_t)rd32(v_planes + 0xC) >= 0x8000000) {
        uint32_t y = rd32(v_camY);
        if (((y - rd32(v_camSpeedY)) ^ y) & 0x2000000) {
            next_step();
            Stage2Cam44();
            return;
        }
    }
    follow_down_120(0);
}

/* $005DEC. To $720: the end. */
void Stage2Cam44(void)
{
    if ((int32_t)rd32(v_planes + 4) < 0x7200000) { follow_down_120(1); return; }
    next_step();
    wr16(STAGE2_SIGNAL, 1);
    set_y(0x7200000);
    CameraStop();
    wr16(v_stageEvent, 0x80);
    QueueSound(0xAB);
    Stage2Cam48();
}

/* $005E26. $81 frames, then the stage is over. */
void Stage2Cam48(void)
{
    wr16(v_stageEvent, (uint16_t)(rd16(v_stageEvent) - 1));
    if (!(rd16(v_stageEvent) & 0x8000)) return;
    next_step();
    set8(v_stageClear, 7);
    Stage2Cam4C();
}

/* $005E3A */
void Stage2Cam4C(void)
{
}

/* ---- stage 2's background ---- */

/* $006A06. The sky's bands start where the second camera is (the first at 0). */
void Stage2BandsStart(void)
{
    uint32_t x = rd32(v_camX + 8);
    wr32(BANDS, 0);
    wr32(BANDS + 4, x);
    wr32(BANDS + 8, x);
    wr32(BANDS + 12, x);
}

/* $006AA0. A band of the background (map lines top to bottom) scrolled by `scroll` where it is
 * on the screen (from..to are the screen's map lines): plane B's word of each line in it, or with
 * the per-cell scroll each 8 lines' (from and to taken down to 8s). */
void Stage2Band(uint16_t top, uint16_t bottom, uint16_t scroll, uint16_t from, uint16_t to, uint32_t table)
{
    int lines = bit8(SCROLL_MODE, 0);
    if (!lines) {
        from &= 0xFFF8;
        to &= 0xFFF8;
    }
    if ((int16_t)top >= (int16_t)to || (int16_t)bottom <= (int16_t)from) return;
    uint32_t at;
    uint16_t n;
    if ((int16_t)top < (int16_t)from) {
        n = (uint16_t)(bottom - from);
        at = table + 2;
    } else if ((int16_t)bottom > (int16_t)to) {
        n = (uint16_t)(to - top);
        at = table + 2 + (uint32_t)(int16_t)(uint16_t)((top - from) << 2);
    } else {
        n = (uint16_t)(bottom - top);
        at = table + 2 + (uint32_t)(int16_t)(uint16_t)((top - from) << 2);
    }
    uint16_t v = (uint16_t)-scroll;
    if (lines) {
        for (uint16_t k = (uint16_t)(n - 1);; k--) {
            bus_wr16(at, v);
            at += 4;
            if (k == 0) break;
        }
    } else {
        for (uint16_t k = (uint16_t)(((int16_t)n >> 3) - 1);; k--) {
            bus_wr16(at, v);
            at += 0x20;
            if (k == 0) break;
        }
    }
}

/* $006A20. The sky's four bands, each faster (half the camera's speed and 2, 3, 4 more). */
void Stage2Bands(void)
{
    uint32_t table = rd32(v_hscrollSrc);
    uint16_t from = (uint16_t)((rd32(v_camY + 8) + rd32(v_camSpeedY + 8)) >> 16);
    uint16_t to = (uint16_t)(from + 0xE0);
    Stage2Band(0x240, 0x280, rd16(BANDS), from, to, table);
    Stage2Band(0x280, 0x2A0, rd16(BANDS + 4), from, to, table);
    Stage2Band(0x2A0, 0x2C8, rd16(BANDS + 8), from, to, table);
    Stage2Band(0x2C8, 0x500, rd16(BANDS + 12), from, to, table);
    uint32_t v = (uint32_t)((int32_t)rd32(v_camSpeedX) >> 1);
    wr32(BANDS + 4, rd32(BANDS + 4) + v);
    wr32(BANDS + 8, rd32(BANDS + 8) + v);
    wr32(BANDS + 12, rd32(BANDS + 12) + v);
    wr32(BANDS + 4, rd32(BANDS + 4) + 0x20000);
    wr32(BANDS + 8, rd32(BANDS + 8) + 0x30000);
    wr32(BANDS + 12, rd32(BANDS + 12) + 0x40000);
}

/* $0069EA: the second camera and plane B at half the bow's speed */
static void back_with_bow(void)
{
    uint32_t v = (uint32_t)((int32_t)rd32(BOW_SPEED) >> 1);
    wr32(v_planeSpeeds + 0xC, v);
    wr32(v_camSpeedY + 8, v);
}

/* $0069FA: the second camera only */
static void back_camera_with_bow(void)
{
    wr32(v_camSpeedY + 8, (uint32_t)((int32_t)rd32(BOW_SPEED) >> 1));
}

/* the second camera and plane B at half the camera's y speed */
static void back_half_y(void)
{
    uint32_t v = (uint32_t)((int32_t)rd32(v_camSpeedY) >> 1);
    wr32(v_camSpeedY + 8, v);
    wr32(v_planeSpeeds + 0xC, v);
}

/* plane B by the speeds the stage's objects set */
static void back_by_objects(int plane_x)
{
    wr32(v_camSpeedX + 8, rd32(BACK_SPEED_X));
    if (plane_x) wr32(v_planeSpeeds + 8, rd32(BACK_SPEED_X));
    wr32(v_camSpeedY + 8, rd32(BACK_SPEED_Y));
    wr32(v_planeSpeeds + 0xC, rd32(BACK_SPEED_Y));
}

/* $006758. Background step 0: until the camera's script signals. */
void Stage2Back00(void)
{
    if (!rd16(STAGE2_SIGNAL)) return;
    next_back();
    BackStop();
    Stage2Back04();
}

/* $00676A. Plane B up with the bow to $680: the next tiles. */
void Stage2Back04(void)
{
    if ((int32_t)rd32(v_planes + 0xC) > 0x6800000) { back_with_bow(); return; }
    next_back();
    wr32(v_planes + 0xC, 0x6800000);
    uint32_t list = 0x00922A;
    NextJob(&list);
    wr32(v_planeSpeeds + 0xC, 0);
    wr16(STAGE2_SIGNAL, 0);
    Stage2Back08();
}

/* the second camera above y $180 / $480 is put $200 down; 1 when it is */
static int back_wrap(uint32_t at)
{
    if ((int32_t)rd32(v_camY + 8) > (int32_t)at) return 0;
    wr16(v_camY + 8, (uint16_t)(rd16(v_camY + 8) + 0x200));
    return 1;
}

/* $006796. The second camera round its loop until the tiles are in and the camera signals. */
void Stage2Back08(void)
{
    if (!back_wrap(0x4800000) || rd16(v_jobKind) || !rd16(STAGE2_SIGNAL)) { back_camera_with_bow(); return; }
    next_back();
    wr32(v_camY + 8, 0x6800000);
    Stage2Back0C();
}

/* $0067C8. Plane B up to $380. */
void Stage2Back0C(void)
{
    if ((int32_t)rd32(v_planes + 0xC) > 0x3800000) { back_with_bow(); return; }
    next_back();
    wr32(v_planes + 0xC, 0x3800000);
    wr32(v_planeSpeeds + 0xC, 0);
    wr16(STAGE2_SIGNAL, 0);
    Stage2Back10();
}

/* $0067E8. Round the loop until the camera signals. */
void Stage2Back10(void)
{
    if (!back_wrap(0x1800000) || !rd16(STAGE2_SIGNAL)) { back_camera_with_bow(); return; }
    next_back();
    wr32(v_planeSpeeds + 0xC, 0);
    Stage2Back14();
}

/* $00680E. The second camera speeding up (x to 4, y to -1) round the loop; at y -1: the sky's
 * bands. */
void Stage2Back14(void)
{
    if ((int32_t)rd32(v_camY + 8) <= 0x1800000) {
        wr16(v_camY + 8, (uint16_t)(rd16(v_camY + 8) + 0x200));
        if (rd32(v_camSpeedY + 8) == 0xFFFF0000) {
            next_back();
            wr32(v_camY + 8, 0x3800000);
            BackSpeed(0, 0xFFFF0000);
            wr8(SCROLL_MODE, 3);
            wr8(v_planeFlagsB, 0x80);
            Stage2BandsStart();
            Stage2Back18();
            return;
        }
    }
    int32_t v = (int32_t)(rd32(v_camSpeedX + 8) + 0x400);
    if (v >= 0x40000) v = 0x40000;
    wr32(v_camSpeedX + 8, (uint32_t)v);
    v = (int32_t)(rd32(v_camSpeedY + 8) + 0x400);
    if (v >= (int32_t)0xFFFF0000) v = (int32_t)0xFFFF0000;
    wr32(v_camSpeedY + 8, (uint32_t)v);
}

/* $00688E. Up to $240 with the bands; then by cells. */
void Stage2Back18(void)
{
    if ((int32_t)rd32(v_camY + 8) > 0x2400000) { Stage2Bands(); return; }
    next_back();
    wr32(v_camY + 8, 0x2400000);
    wr32(v_planes + 0xC, 0x2400000);
    wr8(SCROLL_MODE, 2);
    wr8(v_planeFlagsB, 0x80);
    wr16(STAGE2_SIGNAL, 0);
    BackStop();
    Stage2Back1C();
}

/* $0068C2. The bands, plane B by the objects, until the camera signals: plane B at x $1400. */
void Stage2Back1C(void)
{
    if (!rd16(STAGE2_SIGNAL)) {
        Stage2Bands();
        back_by_objects(0);
        return;
    }
    next_back();
    wr8(v_planeFlagsB, 0);
    wr32(v_planes + 8, 0x14000000);
    wr32(v_camX + 8, 0x14000000);
    wr32(v_planes + 0xC, 0);
    wr32(v_camY + 8, 0);
    BackStop();
    wr16(STAGE2_SIGNAL, 0);
    Stage2Back20();
}

/* $006914. Then x $200. */
void Stage2Back20(void)
{
    if (!rd16(STAGE2_SIGNAL)) { back_by_objects(1); return; }
    next_back();
    wr32(v_planes + 8, 0x2000000);
    wr32(v_camX + 8, 0x2000000);
    wr32(v_planes + 0xC, 0);
    wr32(v_camY + 8, 0);
    BackStop();
    wr16(STAGE2_SIGNAL, 0);
    Stage2Back24();
}

/* $006962 */
void Stage2Back24(void)
{
    if (!rd16(STAGE2_SIGNAL)) { back_by_objects(0); return; }
    next_back();
    BackStop();
    Stage2Back28();
}

/* $006986. Down to $600: the end's tiles, two collision cells. */
void Stage2Back28(void)
{
    if ((int32_t)rd32(v_camY + 8) < 0x6000000) { back_half_y(); return; }
    next_back();
    uint32_t list = 0x0092F8;
    NextJob(&list);
    wr8(0x794D, 0x40);
    wr8(0x794E, 0x40);
    wr16(STAGE2_SIGNAL, 0);
    Stage2Back2C();
}

/* $0069C6 */
void Stage2Back2C(void)
{
    if (!rd16(STAGE2_SIGNAL)) { back_half_y(); return; }
    next_back();
    BackStop();
    Stage2Back30();
}

/* $0069E6 */
void Stage2Back30(void)
{
}

/* $0069E8 */
void Stage2Back34(void)
{
}

/* $008EE8. The camera and plane A at ax, ay, the second camera and plane B at bx, by (pixels). */
void CameraPlace(uint16_t ax, uint16_t ay, uint16_t bx, uint16_t by)
{
    wr32(v_planes, (uint32_t)ax << 16);
    wr32(v_planes + 4, (uint32_t)ay << 16);
    wr32(v_planes + 8, (uint32_t)bx << 16);
    wr32(v_planes + 0xC, (uint32_t)by << 16);
    wr32(v_camX, (uint32_t)ax << 16);
    wr32(v_camY, (uint32_t)ay << 16);
    wr32(v_camX + 8, (uint32_t)bx << 16);
    wr32(v_camY + 8, (uint32_t)by << 16);
}

/* ---- stage 3, the dice palace: two fights, then a fade to the board (its own map) ---- */

#define STAGE3_CAMERA 0x006D32u
#define STAGE3_BACK   0x006EBAu
#define STAGE3_SIGNAL 0xA9C0

/* $006D1C */
void Stage3Frame(void)
{
    Stage3Camera();
    call68k(step_routine(STAGE3_BACK, rd16(v_backStep)));
}

/* $006D24 */
void Stage3Camera(void)
{
    call68k(step_routine(STAGE3_CAMERA, rd16(v_cameraStep)));
}

/* $006D42. Step 0: to x $380, the fight. */
void Stage3Cam00(void)
{
    if ((int32_t)rd32(v_camX) < 0x3800000) { CameraFollowRight(); return; }
    next_step();
    hold_x(0x3800000);
    Stage3Cam04();
}

/* $006D72 */
void Stage3Cam04(void)
{
    if (rd8(v_cameraHold) & 0x80) return;
    next_step();
    wr8(v_planeFlagsA, 0);
    Stage3Cam08();
}

/* $006D84. To x $9C0, the fight. */
void Stage3Cam08(void)
{
    if ((int32_t)rd32(v_camX) < 0x9C00000) { CameraFollowRight(); return; }
    next_step();
    hold_x(0x9C00000);
    Stage3Cam0C();
}

/* $006DB4. Then the players at the door (x $180 on, y $E0 on): the fade out. */
void Stage3Cam0C(void)
{
    if (rd8(v_cameraHold) & 0x80) return;
    uint16_t x = 0, y = 0;
    PlayersCentre(&x, &y);
    if ((int16_t)x < 0x180 || (int16_t)y < 0xE0) return;
    next_step();
    wr16(v_fadeSpeed, 0x200);
    wr16(v_fadeLevel, 0);
    Stage3Cam10();
}

/* $006DE0. Faded: the checkpoint, the board's tiles and palette, the players and the camera put
 * there, both planes redrawn, the boss ($260), the fade in. */
void Stage3Cam10(void)
{
    if (rd16(v_fadeSpeed)) return;
    next_step();
    wr16(v_checkpoint, 4);
    wr16(v_checkpoints + 6, rd16(v_checkpoint));
    RunCommands(0x00935C);
    LoadPalettes(0x00969C);
    PlayersPlace(0xC0, 0x140, 0);
    CameraPlace(0x200, 0x100, 0x200, 0x100);
    CameraStop();
    PlaneARedraw();
    PlaneBRedraw();
    wr16(STAGE3_SIGNAL, 1);
    wr8(SCROLL_MODE, 0);
    wr8(v_planeFlagsA, 0xC0);
    wr16(v_limitTop, 0x98);
    wr16(v_boss + O_TYPE, 0x260);
    set8(VDP_REG(1) + 1, 6);
    wr8(v_displayOn, 0x80);
    wr16(v_fadeSpeed, 0xFE00);
    wr16(v_fadeLevel, 0x1000);
    Stage3Cam14();
}

/* $006E7E. Faded in. */
void Stage3Cam14(void)
{
    if (rd16(v_fadeSpeed)) return;
    next_step();
    wr8(v_sceneSignal, 0);
    set8(VDP_REG(1) + 1, 6);
    wr8(v_displayOn, 0x80);
    Stage3Cam18();
}

/* $006E9C. When the stage signals: the players' scenes (stage 0's code). */
void Stage3Cam18(void)
{
    uint8_t s = rd8(v_sceneSignal);
    if (!s || (s & 0x80)) return;
    boss_scenes();
}

/* $006EAA */
void Stage3Cam1C(void)
{
}

/* $006EC0. Background step 0: plane B's strips (the bottom four 8 times as fast) until the board:
 * then plane B by cells. */
void Stage3Back00(void)
{
    if (rd16(STAGE3_SIGNAL)) {
        next_back();
        wr8(v_planeFlagsB, 0xC0);
        BackStop();
        Stage3Back04();
        return;
    }
    wr32(v_camSpeedX + 8, (uint32_t)((int32_t)rd32(v_camSpeedX) >> 2));
    uint32_t x = rd32(v_camX + 8);
    uint16_t far = (uint16_t)-(uint16_t)(x >> 16), near = (uint16_t)-(uint16_t)((x << 3) >> 16);
    for (uint16_t k = 0; k < 28; k++) wr16((uint16_t)(HSCROLL_RAM + 2 + 0x20 * k), k < 24 ? far : near);
}

/* $006ED6 */
void Stage3Back04(void)
{
}

/* $006ED8 */
void Stage3Back08(void)
{
}

/* ---- stage 4, the base: plane A (the train) moves at the players' speed times a factor
 * ($800-$1000 of $1000, eased up and down by turns), the camera with it; the strips below scroll
 * by what the camera has moved ---- */

#define STAGE4_CAMERA 0x0071C8u
#define STAGE4_BACK   0x00768Eu
#define TRAIN_FACTOR 0xA98E
#define TRACK_X      0xA990      /* long: the strips' x (the camera's moves added up) */
#define ALARM_TIME   0xA9CC
#define STAGE4_SIGNAL 0xA9C0

/* $00715C */
void Stage4Frame(void)
{
    int b = (rd16(v_gameFrames) & 2) != 0;
    wr16(v_palette + 0x22, b ? 0xEEE : 0xAEE);
    wr16(v_paletteTarget + 0x22, b ? 0xEEE : 0xAEE);
    wr16(v_palette + 0x24, b ? 0x4EE : 0x4CE);
    wr16(v_paletteTarget + 0x24, b ? 0x4EE : 0x4CE);
    wr16(v_palette + 0x26, b ? 0x2CE : 0x2AE);
    wr16(v_paletteTarget + 0x26, b ? 0x2CE : 0x2AE);
    Stage4Camera();
    call68k(step_routine(STAGE4_BACK, rd16(v_backStep)));
}

/* $0071BA */
void Stage4Camera(void)
{
    call68k(step_routine(STAGE4_CAMERA, rd16(v_cameraStep)));
}

/* $007590: the per-strip scroll: 20 strips by plane A, the last 8 by the track; the track on by
 * the camera's speed */
static void train_strips(void)
{
    uint16_t a = (uint16_t)-rd16(v_planes), t = (uint16_t)-rd16(TRACK_X);
    wr32(TRACK_X, rd32(TRACK_X) + rd32(v_camSpeedX));
    for (uint16_t k = 0; k < 28; k++) wr16((uint16_t)(HSCROLL_RAM + 0x20 * k), k < 20 ? a : t);
}

static uint16_t train_ahead(void)
{
    uint16_t y = 0, x = players_ahead(&y);
    ClampFour(&x, &y);
    return x;
}

/* $007558. The camera's speed after the players (2 by itself with v_cameraHold bit 0), plane A's
 * that times the factor, the camera where plane A is. */
void Stage4Follow(void)
{
    uint16_t x = bit8(v_cameraHold, 0) ? 2 : train_ahead();
    wr16(v_camSpeedX, x);
    wr32(v_planeSpeeds, (uint32_t)((int32_t)(int16_t)x * (int16_t)rd16(TRAIN_FACTOR)) << 4);
    wr32(v_camX, rd32(v_planes));
    train_strips();
}

/* $007616: the factor down $40 a frame to $800 (straight to $800 from below) */
static void train_slower(void)
{
    uint16_t f = rd16(TRAIN_FACTOR);
    if (f < 0x800) { wr16(TRAIN_FACTOR, 0x800); return; }
    f = (uint16_t)(f - 0x40);
    wr16(TRAIN_FACTOR, f);
    if (f <= 0x800) wr16(TRAIN_FACTOR, 0x800);
}

/* $007636: up $40 a frame to $1000 */
static void train_faster(void)
{
    uint16_t f = rd16(TRAIN_FACTOR);
    if (f > 0x1000) { wr16(TRAIN_FACTOR, 0x1000); return; }
    f = (uint16_t)(f + 0x40);
    wr16(TRAIN_FACTOR, f);
    if (f >= 0x1000) wr16(TRAIN_FACTOR, 0x1000);
}

/* the camera held where plane A reaches x (the camera kept as far behind as it was) */
static void train_hold(uint32_t x)
{
    set8(v_cameraHold, 7);
    clr8(v_cameraHold, 0);
    CameraStop();
    wr32(v_camX, rd32(v_camX) - (rd32(v_planes) - x));
    wr32(v_planes, x);
}

/* a stretch: on to x, easing the factor down or up */
static int train_to(uint32_t x, int faster)
{
    if ((int32_t)rd32(v_planes) >= (int32_t)x) return 1;
    Stage4Follow();
    if (faster) train_faster();
    else train_slower();
    return 0;
}

/* $0071EE. Step 0: to $4C0, slowing. */
void Stage4Cam00(void)
{
    if (!train_to(0x4C00000, 0)) return;
    next_step();
    Stage4Cam04();
}

/* $007206. To $758, faster: a fight. */
void Stage4Cam04(void)
{
    if (!train_to(0x7580000, 1)) return;
    next_step();
    train_hold(0x7580000);
    Stage4Cam08();
}

/* $007244 */
void Stage4Cam08(void)
{
    if (rd8(v_cameraHold) & 0x80) return;
    next_step();
    Stage4Cam0C();
}

/* $007252. To $980, faster. */
void Stage4Cam0C(void)
{
    if (!train_to(0x9800000, 1)) return;
    next_step();
    Stage4Cam10();
}

/* $00726A. To $B84, slowing: a fight. */
void Stage4Cam10(void)
{
    if (!train_to(0xB840000, 0)) return;
    next_step();
    train_hold(0xB840000);
    Stage4Cam14();
}

/* $0072A8 */
void Stage4Cam14(void)
{
    if (rd8(v_cameraHold) & 0x80) return;
    next_step();
    Stage4Cam18();
}

/* $0072B6. To $C40, slowing. */
void Stage4Cam18(void)
{
    if (!train_to(0xC400000, 0)) return;
    next_step();
    Stage4Cam1C();
}

/* $0072CE. To $1000, faster: a fight. */
void Stage4Cam1C(void)
{
    if (!train_to(0x10000000, 1)) return;
    next_step();
    train_hold(0x10000000);
    Stage4Cam20();
}

/* $00730C */
void Stage4Cam20(void)
{
    if (rd8(v_cameraHold) & 0x80) return;
    next_step();
    Stage4Cam24();
}

/* $00731A. To $1100, faster. */
void Stage4Cam24(void)
{
    if (!train_to(0x11000000, 1)) return;
    next_step();
    Stage4Cam28();
}

/* $007332. To $1500, slowing: the checkpoint, the next tiles, $7950 on cleared, the factor
 * $400. */
void Stage4Cam28(void)
{
    if (!train_to(0x15000000, 0)) return;
    next_step();
    wr16(v_checkpoint, 4);
    wr16(v_checkpoints + 8, rd16(v_checkpoint));
    uint32_t list = 0x00942A;
    NextJob(&list);
    Stage4ClearCells();
    wr16(TRAIN_FACTOR, 0x400);
    CameraStop();
    Stage4Cam2C();
}

/* $00751E. The collision's 176 bytes at $7950 cleared. */
void Stage4ClearCells(void)
{
    for (uint16_t i = 0; i < 0xB0; i += 4) wr32((uint16_t)(0x7950 + i), 0);
}

/* $0080D0. The collision's 768 bytes at $7800 cleared. */
void Stage4ClearCollision(void)
{
    for (uint16_t i = 0; i < 0x300; i += 4) wr32((uint16_t)(0x7800 + i), 0);
}

/* $007370. While the tiles load, the camera only. */
void Stage4Cam2C(void)
{
    if (rd16(v_jobKind)) {
        wr16(v_camSpeedX, train_ahead());
        wr32(v_camX, rd32(v_planes));
        train_strips();
        return;
    }
    next_step();
    Stage4Cam30();
}

/* $00737C. To $1780: a fight. */
void Stage4Cam30(void)
{
    if ((int32_t)rd32(v_planes) < 0x17800000) { Stage4Follow(); return; }
    next_step();
    train_hold(0x17800000);
    Stage4Cam34();
}

/* $0073B2 */
void Stage4Cam34(void)
{
    if (rd8(v_cameraHold) & 0x80) return;
    next_step();
    Stage4Cam38();
}

/* $0074B8: the players' scenes (player 1's $1C, player 2's $20) */
static void end_scenes(void)
{
    set8(v_sceneSignal, 7);
    wr16(v_sceneDone, 0);
    uint8_t p = rd8(v_players);
    if (p == 3) {
        wr16(v_player1 + P_INPUT, 0x1C);
        wr16(v_player2 + P_INPUT, 0x20);
    } else if (p & 1) {
        wr16(v_player1 + P_INPUT, 0x1C);
    } else if (p & 2) {
        wr16(v_player2 + P_INPUT, 0x1C);
    }
}

/* $0073C0. When the stage signals, the scenes; else to $1800: the boss's tiles, the players in
 * front. */
void Stage4Cam38(void)
{
    uint8_t s = rd8(v_sceneSignal);
    if (s && !(s & 0x80)) { end_scenes(); return; }
    if ((int32_t)rd32(v_planes) < 0x18000000) { Stage4Follow(); return; }
    next_step();
    set8(v_cameraHold, 7);
    clr8(v_cameraHold, 0);
    CameraStop();
    set_x(0x18000000);
    uint32_t list = 0x00944C;
    NextJob(&list);
    PlayersInFront();
    wr16(STAGE4_SIGNAL, 1);
    CameraStop();
    wr16(v_stageEvent, 0);
    wr8(v_sceneSignal, 0);
    Stage4Cam3C();
}

/* $00741E. The boss's tiles in, the stage's signal, the fight over: two objects ($324), the
 * collision cleared, the players free, down at 8. */
void Stage4Cam3C(void)
{
    if (rd16(v_jobKind) || !rd16(v_stageEvent) || (rd8(v_cameraHold) & 0x80)) return;
    next_step();
    wr16(0xC800 + O_TYPE, 0x324);
    wr16(0xC800 + O_X, 0xE0);
    wr16(0xC860 + O_TYPE, 0x324);
    wr16(0xC860 + O_X, 0x180);
    Stage4ClearCollision();
    PlayersUnpin();
    wr16(v_planeSpeeds + 4, 8);
    Stage4Cam40();
}

/* $007462. Plane A down to $200, then the camera down at 1/2. */
void Stage4Cam40(void)
{
    if ((int32_t)rd32(v_planes + 4) < 0x2000000) return;
    next_step();
    CameraSpeed(0, 0x8000);
    wr16(ALARM_TIME, 0x200);
    Stage4Cam44();
}

/* $007486. Shaking (an alarm every 8 frames for $200) down to $360: the stage is over. */
void Stage4Cam44(void)
{
    if ((int32_t)rd32(v_planes + 4) >= 0x3600000) {
        next_step();
        set8(v_stageClear, 7);
        QueueSound(0xF6);
        Stage4Cam48();
        return;
    }
    wr16(v_shakeSprites, 4);
    wr16(v_shakePlane, 4);
    uint16_t t = rd16(ALARM_TIME);
    if (t & 0x8000) return;
    t = (uint16_t)(t - 1);
    wr16(ALARM_TIME, t);
    if (!(t & 7)) QueueSound(0xDA);
}

/* $0074B6 */
void Stage4Cam48(void)
{
}

/* $007758: the per-strip scroll's plane B words: 16 strips still, 5 by a sixteenth of the second
 * camera, 7 by it */
static void base_strips(void)
{
    uint32_t x = rd32(v_camX + 8);
    uint16_t far = (uint16_t)-(uint16_t)((uint32_t)((int32_t)x >> 4) >> 16), near = (uint16_t)-(uint16_t)(x >> 16);
    for (uint16_t k = 0; k < 28; k++) wr16((uint16_t)(HSCROLL_RAM + 2 + 0x20 * k), k < 16 ? 0 : k < 21 ? far : near);
}

/* $00773E: the second camera at 1.5 times the camera's x speed (plane B still), then the
 * strips */
static void base_back(void)
{
    uint32_t v = rd32(v_camSpeedX);
    v += (uint32_t)((int32_t)v >> 1);
    wr32(v_camSpeedX + 8, v);
    wr32(v_planeSpeeds + 8, 0);
    wr32(v_camSpeedY + 8, 0);
    wr32(v_planeSpeeds + 0xC, 0);
    base_strips();
}

/* $00769A. Background step 0: from x $1400, until the second camera crosses a $200 line. */
void Stage4Back00(void)
{
    if ((int32_t)rd32(v_planes) >= 0x14000000
        && (((rd32(v_camX + 8) & 0x1FFFFFF) - rd32(v_camSpeedX + 8)) & 0x2000000)) {
        next_back();
        wr32(v_planes + 8, rd32(v_planes + 8) & 0xFE000000);
        Stage4Back04();
        return;
    }
    base_back();
}

/* $0076CC. Plane B with it to $200. */
void Stage4Back04(void)
{
    if ((int32_t)rd32(v_planes + 8) < 0x2000000) {
        uint32_t v = rd32(v_camSpeedX);
        v += (uint32_t)((int32_t)v >> 1);
        wr32(v_camSpeedX + 8, v);
        wr32(v_planeSpeeds + 8, v);
        wr32(v_planeSpeeds + 0xC, 0);
        wr32(v_camSpeedY + 8, 0);
        base_strips();
        return;
    }
    next_back();
    wr32(v_planes + 8, 0x2000000);
    wr16(STAGE4_SIGNAL, 0);
    Stage4Back08();
}

/* $0076E8. Until the camera's script signals: then plane B down at 8 (its strips plane A's). */
void Stage4Back08(void)
{
    if (!rd16(STAGE4_SIGNAL)) { base_back(); return; }
    next_back();
    wr32(v_planeSpeeds + 0xC, 0x80000);
    Stage4StripsLikeA();
    Stage4Back0C();
}

/* $0077DA. The strips' plane B words: none for the first 20, plane A's for the last 8. */
void Stage4StripsLikeA(void)
{
    for (uint16_t k = 0; k < 20; k++) wr16((uint16_t)(HSCROLL_RAM + 2 + 0x20 * k), 0);
    for (uint16_t k = 20; k < 28; k++) wr16((uint16_t)(HSCROLL_RAM + 2 + 0x20 * k), rd16((uint16_t)(HSCROLL_RAM + 0x20 * k)));
}

/* $007700. Plane B to $100: the stage's signal. */
void Stage4Back0C(void)
{
    if ((int32_t)rd32(v_planes + 0xC) < 0x1000000) return;
    next_back();
    BackStop();
    wr16(v_stageEvent, 1);
    Stage4Back10();
}

/* $00771C */
void Stage4Back10(void)
{
}

/* $00771E */
void Stage4Back14(void)
{
}

/* ---- shared helpers of the later stages ---- */

/* $002D50. The effects' and shots' slots ($BFC0, $780 bytes) cleared. */
void EffectsClear(void)
{
    for (uint16_t i = 0; i < 0x780; i += 4) wr32((uint16_t)(0xBFC0 + i), 0);
}

/* $008E62. The players at x, y (both: 16 either side of x). */
void PlayersAt(uint16_t x, uint16_t y)
{
    uint8_t p = rd8(v_players);
    if (p == 3) {
        wr16(v_player1 + O_X, (uint16_t)(x + 0x10));
        wr16(v_player2 + O_X, (uint16_t)(x + 0x10 - 0x20));
        wr16(v_player1 + O_Y, y);
        wr16(v_player2 + O_Y, y);
        return;
    }
    uint16_t obj = (p & 1) ? v_player1 : (p & 2) ? v_player2 : 0;
    if (!obj) return;
    wr16(obj + O_X, x);
    wr16(obj + O_Y, y);
}

/* $0041E0. The players' byte +$20 set. */
void PlayersSet20(uint8_t v)
{
    if (bit8(v_players, 0)) wr8(v_player1 + 0x20, v);
    if (bit8(v_players, 1)) wr8(v_player2 + 0x20, v);
}

/* ---- stage 5, space: the shooter (the camera drifting with the ships, a star field), a fade to
 * the base's corridor (its own map: the ships become players on foot again), the boss ---- */

#define STAGE5_CAMERA 0x007A50u
#define STAGE5_BACK   0x0080F8u
#define DRIFT_DIR    0xA98E      /* 0 drifting down to 16 below the middle, 4 up to 16 above */
#define DRIFT_MIDDLE 0xA994      /* long: the camera's middle y */
#define DRIFT_SPEED  0xA9A4      /* long: the middle's own speed */
#define STAGE5_SIGNAL 0xA9C0     /* byte */
#define STAR_TIME    0xA9CE
#define STARS_X      0xAA60      /* 200 longs: the stars' x (a line each) */
#define STARS_SPEED  0xAE60      /* 200 longs: their speeds */

/* $007F54. The camera's y speed: the middle's, plus a drift that swings it 16 pixels either side
 * of the middle (eased: 2, 4, 6, 8 ... by how far it is). */
void Stage5Drift(void)
{
    int up = rd16(DRIFT_DIR) != 0;
    for (;;) {
        if (!up) {
            wr16(DRIFT_DIR, 0);
            int16_t d = (int16_t)(rd16(v_camY) - rd16(DRIFT_MIDDLE));
            if (d >= 0x10) {
                wr16(DRIFT_DIR, 4);
                up = 1;
                continue;
            }
            d = (int16_t)(d + 0x10);
            if (d < 0) d = 0;
            wr32(v_camSpeedY, rd32(DRIFT_SPEED) + rom32(0x007F8E + (uint32_t)(d & 0x1C)));
            return;
        }
        int16_t d = (int16_t)(rd16(DRIFT_MIDDLE) - rd16(v_camY));
        if (d >= 0x10) {
            up = 0;
            continue;
        }
        d = (int16_t)(d + 0x10);
        if (d < 0) d = 0;
        wr32(v_camSpeedY, rd32(DRIFT_SPEED) - rom32(0x007F8E + (uint32_t)(d & 0x1C)));
        return;
    }
}

/* $007A3A */
void Stage5Frame(void)
{
    Stage5Camera();
    call68k(step_routine(STAGE5_BACK, rd16(v_backStep)));
}

/* $007A42 */
void Stage5Camera(void)
{
    call68k(step_routine(STAGE5_CAMERA, rd16(v_cameraStep)));
}

/* the ships' wait: until the tiles are in and the fight is over */
static int fighting(void)
{
    return rd16(v_jobKind) || (rd8(v_cameraHold) & 0x80);
}

/* the camera held at x (plane A at px) for a fight, with the tiles from list */
static void space_hold(uint32_t x, uint32_t px, uint32_t list)
{
    next_step();
    wr32(v_camX, x);
    wr32(v_planes, px);
    if (list) NextJob(&list);
    wr8(v_cameraHold, 0x80);
    CameraStop();
}

/* $007A72. Step 0: the camera against star 8's speed (a quarter) until the stage says. */
void Stage5Cam00(void)
{
    if (!rd8(v_stageEvent)) {
        uint32_t v = (uint32_t)((int32_t)(0u - rd32(0xAE80)) >> 2);
        wr32(v_camSpeedX, v);
        wr32(v_camX, rd32(v_camX) - v);
        return;
    }
    next_step();
    PlayersInFront();
    wr32(v_planes, rd32(v_planes) - 0x400000);
    CameraSpeed(0x4000, 0);
    wr32(DRIFT_MIDDLE, rd32(v_planes + 4));
    wr32(DRIFT_SPEED, 0);
    Stage5Cam04();
}

/* $007AB2. Right to $A80: a fight. */
void Stage5Cam04(void)
{
    if ((int32_t)rd32(v_camX) < 0xA800000) { Stage5Drift(); return; }
    space_hold(0xA800000, 0xA400000, 0x009586);
    Stage5Cam08();
}

/* $007AE8 */
void Stage5Cam08(void)
{
    if (fighting()) { Stage5Drift(); return; }
    next_step();
    CameraSpeed(0x4000, 0);
    Stage5Cam0C();
}

/* $007B08. To $CC0, then until y is $560: down. */
void Stage5Cam0C(void)
{
    if ((int32_t)rd32(v_camX) < 0xCC00000) { Stage5Drift(); return; }
    set_x(0xCC00000);
    CameraStop();
    if (rd16(v_camY) != 0x560) { Stage5Drift(); return; }
    next_step();
    wr32(v_camY, 0x5600000);
    wr32(v_planes + 4, 0x5800000);
    CameraSpeed(0, 0x8000);
    wr32(DRIFT_MIDDLE, rd32(v_camY));
    wr32(DRIFT_SPEED, 0x8000);
    Stage5Cam10();
}

/* $007B60. Down to $700, then left. */
void Stage5Cam10(void)
{
    if ((int32_t)rd32(v_camY) < 0x7000000) {
        Stage5Drift();
        wr32(v_planeSpeeds + 4, rd32(v_camSpeedY));
        wr32(DRIFT_MIDDLE, rd32(DRIFT_MIDDLE) + 0x8000);
        return;
    }
    next_step();
    set_y(0x7000000);
    wr32(v_planes, rd32(v_camX));
    wr32(v_planes, rd32(v_planes) + 0x600000);
    CameraSpeed(0xFFFFC000, 0);
    wr32(DRIFT_MIDDLE, rd32(v_planes + 4));
    wr32(DRIFT_SPEED, 0);
    Stage5Cam14();
}

/* $007BB8. Left to $AC0: a fight. */
void Stage5Cam14(void)
{
    if ((int32_t)rd32(v_camX) > 0xAC00000) { Stage5Drift(); return; }
    space_hold(0xAC00000, 0xB200000, 0x00956C);
    Stage5Cam18();
}

/* $007BEE */
void Stage5Cam18(void)
{
    if (fighting()) { Stage5Drift(); return; }
    next_step();
    CameraSpeed(0xFFFFC000, 0);
    Stage5Cam1C();
}

/* $007C0E. To $800, then until y is $700: up. */
void Stage5Cam1C(void)
{
    if ((int32_t)rd32(v_camX) > 0x8000000) { Stage5Drift(); return; }
    set_x(0x8000000);
    CameraStop();
    if (rd16(v_camY) != 0x700) { Stage5Drift(); return; }
    next_step();
    set_y(0x7000000);
    CameraSpeed(0, 0xFFFF8000);
    wr32(DRIFT_MIDDLE, rd32(v_planes + 4));
    wr32(DRIFT_SPEED, 0xFFFF8000);
    Stage5Cam20();
}

/* $007C66. Up to $610, then right. */
void Stage5Cam20(void)
{
    if ((int32_t)rd32(v_planes + 4) > 0x6100000) {
        Stage5Drift();
        wr32(v_planeSpeeds + 4, rd32(v_camSpeedY));
        wr32(DRIFT_MIDDLE, rd32(DRIFT_MIDDLE) - 0x8000);
        return;
    }
    next_step();
    wr32(v_planes + 4, 0x6100000);
    wr32(v_camY, 0x6100000);
    wr32(v_planes, rd32(v_camX));
    wr32(v_planes, rd32(v_planes) - 0x400000);
    CameraSpeed(0xC000, 0);
    wr32(DRIFT_MIDDLE, rd32(v_planes + 4));
    wr32(DRIFT_SPEED, 0);
    Stage5Cam24();
}

/* $007CBE. Right to $A80: a fight. */
void Stage5Cam24(void)
{
    if ((int32_t)rd32(v_camX) < 0xA800000) { Stage5Drift(); return; }
    space_hold(0xA800000, 0xA400000, 0x009586);
    Stage5Cam28();
}

/* $007CF4 */
void Stage5Cam28(void)
{
    if (fighting()) { Stage5Drift(); return; }
    next_step();
    CameraSpeed(0x4000, 0);
    Stage5Cam2C();
}

/* $007D14. To $E80: put back to $A80 (the same stretch again), the last fight. */
void Stage5Cam2C(void)
{
    if ((int32_t)rd32(v_camX) < 0xE800000) { Stage5Drift(); return; }
    space_hold(0xA800000, 0xA400000, 0);
    Stage5Cam30();
}

/* $007D3E. The fight over: a fade out; faded: the checkpoint, the corridor (its tiles, palette,
 * the players on foot, both planes), the limits, the boss ($440), the music, the fade in. */
void Stage5Cam30(void)
{
    if ((rd8(v_cameraHold) & 0x80) || rd16(v_fadeSpeed)) { Stage5Drift(); return; }
    if (!rd16(v_fadeLevel)) {
        QueueSound(0xF6);
        wr16(v_fadeSpeed, 0x200);
        wr16(v_fadeLevel, 0);
        Stage5Drift();
        return;
    }
    next_step();
    wr16(v_checkpoint, 4);
    wr16(v_checkpoints + 0xA, rd16(v_checkpoint));
    wr8(STAGE5_SIGNAL, 1);
    EffectsClear();
    RunCommands(0x0094A8);
    LoadPalettes(0x0096BC);
    wr16(v_playerMode, 0);
    PlayersInit();
    PlayersPlace(0xC0, 0xE0, 0);
    CameraPlace(0x200, 0x100, 0x200, 0x100);
    PlaneARedraw();
    PlaneBRedraw();
    CameraStop();
    BackStop();
    wr8(v_cameraHold, 0);
    wr8(SCROLL_MODE, 0);
    wr8(v_planeFlagsA, 0);
    wr16(v_deathLine, 0x180);
    wr16(v_limitLeft, 0x8F);
    wr16(v_limitRight, 0x1B0);
    wr16(v_limitTop, 0x40);
    wr16(v_limitBottom, 0x1C0);
    wr32(v_speedLimitX, 0x70000);
    wr32(v_speedLimitY, 0x70000);
    wr16(v_boss + O_TYPE, 0x440);
    wr16(v_boss + O_STATE, 0);
    wr8(v_soundNext, 0x93);
    wr16(v_soundDelay, 0x28);
    set8(VDP_REG(1) + 1, 6);
    wr8(v_displayOn, 0x80);
    wr16(v_fadeSpeed, 0xFE00);
    wr16(v_fadeLevel, 0x1000);
    Stage5Cam34();
}

/* $007E52. On foot to $700. */
void Stage5Cam34(void)
{
    if ((int32_t)rd32(v_planes) < 0x7000000) { CameraFollowRight(); return; }
    next_step();
    set_x(0x7000000);
    CameraStop();
    Stage5Cam38();
}

/* $007E76. The players at the far end (x $1A0 on, y $E0 on): a fade out; faded: a pause the
 * 68k spends in a loop (~21 frames), the boss's room, its music. */
void Stage5Cam38(void)
{
    if (rd16(v_fadeSpeed)) return;
    if (!rd16(v_fadeLevel)) {
        uint16_t x = 0, y = 0;
        PlayersCentre(&x, &y);
        if ((int16_t)x < 0x1A0 || (int16_t)y < 0xE0) return;
        QueueSound(0xF6);
        wr16(v_fadeSpeed, 0x100);
        wr16(v_fadeLevel, 0);
        return;
    }
    next_step();
    cpu_spin(0x41u * 0x4002u);
    LoadPalettes(0x0096CC);
    wr16(v_camX, 0xC00);
    wr16(v_planes, 0xC80);
    wr16(v_camY, 0x100);
    wr16(v_planes + 4, 0x100);
    CameraStop();
    PlayersAt(0xA0, 0x140);
    PlaneAFill();
    PlaneADma();
    QueueSound(0xF9);
    wr8(v_soundNext, 0x92);
    wr16(v_soundDelay, 0x80);
    wr16(v_boss + O_TYPE, 0x41C);
    wr16(v_boss + O_STATE, 0);
    PlayersInFront();
    PlayersSet20(4);
    wr8(STAGE5_SIGNAL, 1);
    set8(VDP_REG(1) + 1, 6);
    wr8(v_displayOn, 0x80);
    wr16(v_fadeSpeed, 0xFF00);
    wr16(v_fadeLevel, 0x1000);
    Stage5Cam3C();
}

/* $007F50 */
void Stage5Cam3C(void)
{
}

/* $007F52 */
void Stage5Cam40(void)
{
}

/* ---- stage 5's background: a star field of 200 lines, each its own x and speed ---- */

/* $0081AE: each star on by its speed, its line's plane B scroll */
static void stars_move(void)
{
    for (uint16_t i = 0; i < 200; i++) {
        uint32_t x = rd32((uint16_t)(STARS_X + 4 * i)) + rd32((uint16_t)(STARS_SPEED + 4 * i));
        wr32((uint16_t)(STARS_X + 4 * i), x);
        wr16((uint16_t)(HSCROLL_RAM + 0x62 + 4 * i), (uint16_t)(x >> 16));
    }
}

/* $008534. The stars' tiles (frame i, 0-$C by 4) by DMA to VRAM $20. */
void Stage5Tiles(uint16_t i)
{
    uint16_t q = rd16(v_dmaQueue), vram = 0x20;
    QueueDmaBlock(&q, rom32(0x00854C + (uint32_t)(int16_t)i), &vram);
    wr16(v_dmaQueue, q);
}

/* $008106. Background step 0: the stars, for the timer. */
void Stage5Back00(void)
{
    wr16(STAR_TIME, (uint16_t)(rd16(STAR_TIME) - 1));
    if (!(rd16(STAR_TIME) & 0x8000)) { stars_move(); return; }
    next_back();
    wr16(STAR_TIME, 0);
    wr16(0xA98C, 8);
    Stage5Back04();
}

/* $00811C. $100 frames of the stars speeding up (by the table at $8334), their tiles stepping on
 * every 16 frames for the first 64. */
void Stage5Back04(void)
{
    uint16_t t = rd16(STAR_TIME);
    if ((int16_t)t >= 0x100) {
        next_back();
        wr16(STAR_TIME, 0);
        wr16(0xA98C, 0xC);
        Stage5Back08();
        return;
    }
    wr16(STAR_TIME, (uint16_t)(t + 1));
    t = rd16(STAR_TIME);
    if ((int16_t)t < 0x40 && !(t & 0xF)) Stage5Tiles((uint16_t)((int16_t)t >> 2));
    for (uint16_t i = 0; i < 200; i++) {
        uint32_t v = (uint32_t)(int32_t)(int16_t)rom16(0x008334 + 2 * i) + rd32((uint16_t)(STARS_SPEED + 4 * i));
        wr32((uint16_t)(STARS_SPEED + 4 * i), v);
        uint32_t x = v + rd32((uint16_t)(STARS_X + 4 * i));
        wr32((uint16_t)(STARS_X + 4 * i), x);
        wr16((uint16_t)(HSCROLL_RAM + 0x62 + 4 * i), (uint16_t)(x >> 16));
    }
}

/* $008140. $80 frames more: the tiles back down, a star in each 32 given a new speed each frame
 * (by the frame), then the stage's signal. */
void Stage5Back08(void)
{
    if ((int16_t)rd16(STAR_TIME) >= 0x80) {
        next_back();
        wr8(v_stageEvent, 1);
        wr8(STAGE5_SIGNAL, 0);
        Stage5Back0C();
        return;
    }
    wr16(STAR_TIME, (uint16_t)(rd16(STAR_TIME) + 1));
    uint16_t t = rd16(STAR_TIME);
    if ((int16_t)t < 0x20 && !(t & 7)) Stage5Tiles((uint16_t)(0xC - ((int16_t)t >> 1)));
    uint16_t pick = rom16(0x0082F4 + (rd16(v_gameFrames) & 0x3E));
    for (uint16_t i = 0; i < 200; i++) {
        uint16_t n = (uint16_t)(0xC7 - i);
        if ((n & 0x1F) == pick) wr32((uint16_t)(STARS_SPEED + 4 * i), rom32(0x0081F8 + 4 * (n & 0xF)));
        uint32_t x = rd32((uint16_t)(STARS_X + 4 * i)) + rd32((uint16_t)(STARS_SPEED + 4 * i));
        wr32((uint16_t)(STARS_X + 4 * i), x);
        wr16((uint16_t)(HSCROLL_RAM + 0x62 + 4 * i), (uint16_t)(x >> 16));
    }
}

/* $008160. The stars until the camera's script signals (the fade to the corridor). */
void Stage5Back0C(void)
{
    if (!rd8(STAGE5_SIGNAL)) { stars_move(); return; }
    next_back();
    wr8(v_planeFlagsB, 0);
    wr8(STAGE5_SIGNAL, 0);
    Stage5Back10();
}

/* $008174. At the next signal (the boss's room): plane B there, drawn. */
void Stage5Back10(void)
{
    if (!rd8(STAGE5_SIGNAL)) return;
    next_back();
    wr16(v_camX + 8, 0xC00);
    wr16(v_planes + 8, 0xC00);
    wr16(v_camY + 8, 0x100);
    wr16(v_planes + 0xC, 0x100);
    BackStop();
    PlaneBFill();
    PlaneBDma();
    Stage5Back14();
}

/* $0081AA */
void Stage5Back14(void)
{
}

/* $0081AC */
void Stage5Back18(void)
{
}

/* ---- stage 6, the final battle: along the Empire's ship (fights), a lift up, the gate, a scene,
 * a fade to the last room ---- */

#define STAGE6_CAMERA 0x008744u
#define LIFT_SPEED 0xA994        /* long: the camera's y speed on the lift */

/* $00872E. (Its background script does nothing.) */
void Stage6Frame(void)
{
    Stage6Camera();
}

/* $008736 */
void Stage6Camera(void)
{
    call68k(step_routine(STAGE6_CAMERA, rd16(v_cameraStep)));
}

/* $008D52. Two colours of a palette and its fade target (pal, target: RAM addresses) cycling
 * every 8 frames. */
void Stage6Lamps(uint32_t pal, uint32_t target)
{
    uint16_t i = (uint16_t)(((int16_t)rd16(v_gameFrames) >> 3) & 0xE);
    bus_wr16(pal + 0x36, rom16(0x008D7E + i));
    bus_wr16(target + 0x36, rom16(0x008D7E + i));
    bus_wr16(pal + 0x38, rom16(0x008D8E + i));
    bus_wr16(target + 0x38, rom16(0x008D8E + i));
}

/* $008CF8: the lamps, and three colours blinking every other frame */
static void ship_lights(void)
{
    Stage6Lamps(RAM_BASE | v_palette, RAM_BASE | v_paletteTarget);
    uint16_t i = rd16(v_gameFrames) & 2;
    for (uint16_t k = 0; k < 3; k++) {
        uint16_t c = rom16(0x008D46 + 4 * k + i);
        wr16((uint16_t)(v_palette + 0x3A + 2 * k), c);
        wr16((uint16_t)(v_paletteTarget + 0x3A + 2 * k), c);
    }
}

/* $008CB4. The camera up after the players (above y $110, 4 at most; 2 by itself with
 * v_cameraHold bit 0). */
void CameraFollowUp(void)
{
    uint16_t y;
    if (bit8(v_cameraHold, 0)) {
        y = 0xFFFE;
    } else {
        uint16_t x = 0;
        y = 0;
        PlayersCentre(&x, &y);
        y = (uint16_t)(y - 0x110);
        if (!(y & 0x8000)) y = 0;
        ClampFour(&x, &y);
    }
    wr16(v_planeSpeeds, 0);
    wr16(v_camSpeedX, 0);
    wr16(v_planeSpeeds + 4, y);
    wr16(v_camSpeedY, y);
}

/* $008B58. The gate's chunks: six to plane B's VRAM, six into plane A. */
void Stage6Gate(void)
{
    for (uint16_t k = 0; k < 6; k++) ChunkDma(0xFFFF3A00u + 0x20 * k, (uint16_t)(0xEA10 + 8 * k));
    for (uint16_t k = 0; k < 6; k++) PlaneAChunk(0xFFFF53C0u + 0x20 * k, (uint16_t)(0xCA50 + 8 * k));
}

/* $008C06. And open. */
void Stage6GateOpen(void)
{
    for (uint16_t k = 0; k < 6; k++) PlaneAChunk(0xFFFF5760u + 0x20 * k, (uint16_t)(0xCA50 + 8 * k));
    for (uint16_t k = 0; k < 6; k++) ChunkDma(0xFFFF3580u + 0x20 * k, (uint16_t)(0xEA10 + 8 * k));
}

/* $008AFC. The players become the scenes' players ($5BC, $5C0) for scene s. */
void PlayersToScene(uint16_t s)
{
    uint8_t p = rd8(v_players);
    if (p & 1) {
        wr16(v_player1 + O_TYPE, 0x5BC);
        wr16(v_player1 + 0x48, s);
        wr16(v_player1 + O_STATE, 0);
    }
    if (p & 2) {
        wr16(v_player2 + O_TYPE, 0x5C0);
        wr16(v_player2 + 0x48, s);
        wr16(v_player2 + O_STATE, 0);
    }
}

/* $008B2E. And back to their characters. */
void PlayersCharacters(void)
{
    uint8_t p = rd8(v_players);
    if (p & 1) PlayerCharacter(v_player1);
    if (p & 2) PlayerCharacter(v_player2);
}

/* a stretch right to x, then held for a fight */
static int ship_to(uint32_t x, int hold)
{
    if ((int32_t)rd32(v_planes) < (int32_t)x) {
        CameraFollowRight();
        ship_lights();
        return 0;
    }
    next_step();
    if (hold) {
        set8(v_cameraHold, 7);
        clr8(v_cameraHold, 0);
        CameraStop();
    }
    set_x(x);
    return 1;
}

/* the fight's wait */
static int ship_fight(void)
{
    if (rd8(v_cameraHold) & 0x80) { ship_lights(); return 0; }
    next_step();
    return 1;
}

/* $00876A. Step 0: to $350, a fight. */
void Stage6Cam00(void) { if (ship_to(0x3500000, 1)) Stage6Cam04(); }

/* $0087A2 */
void Stage6Cam04(void) { if (ship_fight()) Stage6Cam08(); }

/* $0087AE. To $8C0. */
void Stage6Cam08(void) { if (ship_to(0x8C00000, 0)) Stage6Cam0C(); }

/* $0087D6. Up after the players to y $200. */
void Stage6Cam0C(void)
{
    if ((int32_t)rd32(v_planes + 4) > 0x2000000) {
        CameraFollowUp();
        ship_lights();
        return;
    }
    next_step();
    set_y(0x2000000);
    Stage6Cam10();
}

/* $0087FE. To $AD0, a fight. */
void Stage6Cam10(void) { if (ship_to(0xAD00000, 1)) Stage6Cam14(); }

/* $008836 */
void Stage6Cam14(void) { if (ship_fight()) Stage6Cam18(); }

/* $008842. To $1000, a fight. */
void Stage6Cam18(void) { if (ship_to(0x10000000, 1)) Stage6Cam1C(); }

/* $00887A */
void Stage6Cam1C(void) { if (ship_fight()) Stage6Cam20(); }

/* $008886. To $1300: the lift (its speed 1 down), the gate shut, the players free. */
void Stage6Cam20(void)
{
    if (!ship_to(0x13000000, 1)) return;
    wr32(LIFT_SPEED, 0x10000);
    Stage6Gate();
    PlayersUnpin();
    Stage6Cam24();
}

/* $0088CE. The lift's fight, then the camera's y wrapped to $200 lines. */
void Stage6Cam24(void)
{
    if (rd8(v_cameraHold) & 0x80) {
        wr32(v_camSpeedY, rd32(LIFT_SPEED));
        ship_lights();
        return;
    }
    wr32(v_camY, rd32(v_camY) & 0x1FFFFFF);
    next_step();
    Stage6Cam28();
}

/* $0088EC. Down to $200: the gate open, the players carried again. */
void Stage6Cam28(void)
{
    if ((int32_t)rd32(v_camY) < 0x2000000) {
        wr32(v_camSpeedY, rd32(LIFT_SPEED));
        ship_lights();
        return;
    }
    Stage6GateOpen();
    PlayersPin();
    next_step();
    wr32(v_camY, 0x2000000);
    Stage6Cam2C();
}

/* $008916. To $1450, a fight. */
void Stage6Cam2C(void) { if (ship_to(0x14500000, 1)) Stage6Cam30(); }

/* $00894E. The players at the gate (x $198 on, y $100 on): both cameras off up-left. */
void Stage6Cam30(void)
{
    if (rd8(v_cameraHold) & 0x80) { ship_lights(); return; }
    uint16_t x = 0, y = 0;
    PlayersCentre(&x, &y);
    if ((int16_t)x < 0x198 || (int16_t)y < 0x100) { ship_lights(); return; }
    next_step();
    wr32(v_camSpeedX, 0xFFFF0000);
    wr32(v_planeSpeeds, 0xFFFF0000);
    wr32(v_camSpeedX + 8, 0xFFFF0000);
    wr32(v_planeSpeeds + 8, 0xFFFF0000);
    wr32(v_camSpeedY, 0x10000);
    wr32(v_planeSpeeds + 4, 0x10000);
    wr32(v_camSpeedY + 8, 0x10000);
    wr32(v_planeSpeeds + 0xC, 0x10000);
    wr16(v_limitRight, 0x240);
    wr16(v_limitLeft, 0);
    Stage6Cam34();
}

/* $0089AA. Back to $1430: the scene ($C). */
void Stage6Cam34(void)
{
    if ((int32_t)rd32(v_planes) > 0x14300000) return;
    next_step();
    set8(v_cameraHold, 7);
    clr8(v_cameraHold, 0);
    CameraStop();
    BackStop();
    wr8(v_sceneSignal, 1);
    PlayersToScene(0xC);
    Stage6Cam38();
}

/* $0089DE. After it, right at 1. */
void Stage6Cam38(void)
{
    if (rd8(v_cameraHold) & 0x80) return;
    next_step();
    wr32(v_camSpeedX, 0x10000);
    wr32(v_planeSpeeds, 0x10000);
    wr32(v_camSpeedX + 8, 0x10000);
    wr32(v_planeSpeeds + 8, 0x10000);
    Stage6Cam3C();
}

/* $008A02. To x $1470. */
void Stage6Cam3C(void)
{
    if ((int32_t)rd32(v_camX) < 0x14700000) return;
    next_step();
    set8(v_cameraHold, 7);
    clr8(v_cameraHold, 0);
    CameraStop();
    BackStop();
    Stage6Cam40();
}

/* $008A28. The fight over: a fade out; faded: the checkpoint, the last room (tiles, palette, the
 * players themselves again, both planes), the limits, the boss ($5B4), its music, the fade in. */
void Stage6Cam40(void)
{
    if (rd16(v_fadeSpeed)) return;
    if (!rd16(v_fadeLevel)) {
        if (rd8(v_cameraHold) & 0x80) return;
        QueueSound(0xF6);
        wr16(v_fadeSpeed, 0x200);
        wr16(v_fadeLevel, 0);
        return;
    }
    next_step();
    wr16(v_checkpoint, 4);
    wr16(v_checkpoints + 0xC, rd16(v_checkpoint));
    RunCommands(0x00960A);
    LoadPalettes(0x0096FC);
    PlayersCharacters();
    PlayersPlace(0xC0, 0x140, 0);
    CameraPlace(0x1A00, 0x200, 0x800, 0x100);
    CameraStop();
    PlaneARedraw();
    PlaneBRedraw();
    PlayersInFront();
    wr16(v_limitLeft, 0x9F);
    wr16(v_limitRight, 0x1A0);
    wr16(v_boss + O_TYPE, 0x5B4);
    wr16(v_boss + O_STATE, 0);
    wr8(v_soundNext, 0x94);
    wr16(v_soundDelay, 0x20);
    set8(VDP_REG(1) + 1, 6);
    wr8(v_displayOn, 0x80);
    wr16(v_fadeSpeed, 0xFE00);
    wr16(v_fadeLevel, 0x1000);
    Stage6Cam44();
}

/* $008AF6. The last room: two colours cycling every 4 frames. */
void Stage6Cam44(void)
{
    uint16_t i = (uint16_t)(((int16_t)rd16(v_gameFrames) >> 2) & 0xE);
    wr16(v_palette + 0x3C, rom16(0x008DCE + i));
    wr16(v_paletteTarget + 0x3C, rom16(0x008DCE + i));
    wr16(v_palette + 0x3E, rom16(0x008DDE + i));
    wr16(v_paletteTarget + 0x3E, rom16(0x008DDE + i));
}

/* $008AFA */
void Stage6Cam48(void)
{
}

/* ---- the stages' set-ups (StageStart's table at $390E): tiles, palettes, the players, the camera
 * and planes, the script's steps, the limits, the level's object list, the music. From a
 * checkpoint (v_checkpoints' word for the stage) the camera starts further on. ---- */

/* $01B7EA. On a Japanese console, more tiles. */
void StageTilesJapan(void)
{
    if (rd8(v_consoleVersion) & 0x80) return;
    RunCommands(0x01B9DA);
}

/* $00EFA0. The player's vitality: by the stages cleared (100-180), or once all four are, by the
 * stage. */
void PlayerVitality(uint16_t obj)
{
    uint8_t c = rd8(v_stagesCleared);
    if ((int8_t)c >= 0xF) {
        wr16(obj + O_VITALITY, rom16(0x00EFE8 + (uint32_t)(int16_t)((int16_t)rd16(v_stage4) >> 1)));
        return;
    }
    wr16(obj + O_VITALITY, rom16(0x00EFC8 + 2 * (c & 0xF)));
}

/* $00EF94 */
void PlayersVitality(void)
{
    PlayerVitality(v_player1);
    PlayerVitality(v_player2);
}

/* $02A13C. Three colours from colours into line 3 (and its fade target); for stage $18 three
 * more into line 2. */
void StageColoursCopy(uint32_t colours, uint16_t stage)
{
    for (uint16_t k = 0; k < 3; k++) {
        uint16_t c = bus_rd16(colours + 2 * k);
        wr16((uint16_t)(v_palette + 0x72 + 2 * k), c);
        wr16((uint16_t)(v_paletteTarget + 0x72 + 2 * k), c);
    }
    if (stage != 0x18) return;
    for (uint16_t k = 0; k < 3; k++) {
        uint16_t c = rom16(0x02A1AA + 2 * k);
        wr16((uint16_t)(v_palette + 0x52 + 2 * k), c);
        wr16((uint16_t)(v_paletteTarget + 0x52 + 2 * k), c);
    }
}

/* $02A132 */
void StageColoursFor(uint16_t stage)
{
    StageColoursCopy(rom32(0x02A17C + (uint32_t)(int16_t)stage), stage);
}

/* $02A12E. The stage's three colours of line 3. */
void StageColours(void)
{
    StageColoursFor(rd16(v_stage4));
}

/* $02A1DA. Three colours into line 2. */
void StageColoursCopy2(uint32_t colours)
{
    for (uint16_t k = 0; k < 3; k++) {
        uint16_t c = bus_rd16(colours + 2 * k);
        wr16((uint16_t)(v_palette + 0x52 + 2 * k), c);
        wr16((uint16_t)(v_paletteTarget + 0x52 + 2 * k), c);
    }
}

/* $02A1C8. And line 2's. */
void StageColours2(void)
{
    StageColours();
    StageColoursCopy2(rom32(0x02A1F4 + (uint32_t)(int16_t)rd16(v_stage4)));
}

/* $01ACDE. The level's object list from its start (for a checkpoint's, the set-up's own). */
void StageLayout(void)
{
    wr32(v_layout, rom32(0x01ADE2 + (uint32_t)(int16_t)rd16(v_stage4)));
}

/* $01ABAC. The first free slot from $C800 below the boss's ($C980): Z set when there is one. */
int SlotFreeLow(uint16_t *slot)
{
    uint16_t a = 0xC800;
    while (rd16(a)) {
        a = (uint16_t)(a + 0x60);
        if (a >= 0xC980) { *slot = a; return 0; }
    }
    *slot = a;
    return 1;
}

/* $033A96. An object $1F0 at $1C0, $110 ($8000 at +6), in a free slot if any. */
void Spawn1F0(void)
{
    uint16_t a;
    if (!SlotFreeLow(&a)) return;
    wr16(a + O_TYPE, 0x1F0);
    wr16(a + 6, 0x8000);
    wr16(a + O_X, 0x1C0);
    wr16(a + O_Y, 0x110);
}

/* $03351C. The mine's director ($1C8) in the boss's slot at its checkpoint step ($44). */
void Stage1DirectorAtCheckpoint(void)
{
    wr16(v_boss + O_TYPE, 0x1C8);
    wr16(v_boss + O_STATE, 0x44);
    Spawn1F0();
    wr16(v_boss + 0x48, 0x40);
}

/* $0064B8. The battleship's eight objects $290 ($DB80 on, every $60, numbered by 4 at +$48). */
void Stage2Objects(void)
{
    for (uint16_t k = 0; k < 8; k++) wr16((uint16_t)(0xDE20 - 0x60 * k), 0x290);
    for (uint16_t k = 0; k < 8; k++) wr16((uint16_t)(0xDE68 - 0x60 * k), (uint16_t)(4 * k));
}

/* $0081D0. The stars: at 0, their speeds from the table at $81F8. */
void StarsStart(void)
{
    for (uint16_t i = 0; i < 200; i++) {
        wr32((uint16_t)(STARS_X + 4 * i), 0);
        wr32((uint16_t)(STARS_SPEED + 4 * i), rom32(0x0081F8 + 4 * ((0xC7 - i) & 0xF)));
    }
}

/* the common start: the stage's tiles (cmds), palettes, the players (mode) placed at x, y */
static void stage_load(uint32_t first, int japan, uint32_t cmds, uint32_t pal)
{
    RunCommands(first);
    if (japan) StageTilesJapan();
    RunCommands(cmds);
    LoadPalettes(pal);
}

static void players_start(uint16_t mode, uint16_t x, uint16_t y)
{
    wr16(v_playerMode, mode);
    PlayersInit();
    PlayersVitality();
    PlayersPlace(x, y, 0);
}

static void limits(uint16_t death, uint16_t left, uint16_t right, uint16_t top, uint16_t bottom)
{
    wr16(v_deathLine, death);
    wr16(v_limitLeft, left);
    wr16(v_limitRight, right);
    wr16(v_limitTop, top);
    wr16(v_limitBottom, bottom);
}

static void speed_limits(uint32_t v)
{
    wr32(v_speedLimitX, v);
    wr32(v_speedLimitY, v);
}

static void steps(uint16_t camera, uint16_t back)
{
    wr16(v_cameraStep, camera);
    wr16(v_backStep, back);
}

/* the stage's music after $12 frames (not in the demo, mode $14), the music stopped first */
static void music(int stop, uint8_t id, uint16_t delay)
{
    if (rd16(v_mode) == 0x14) return;
    if (stop) QueueSound(0xF9);
    wr8(v_soundNext, id);
    wr16(v_soundDelay, delay);
}

/* $003942. The ruins. */
void Stage0Start(void)
{
    uint16_t c = rd16(v_checkpoints);
    wr16(v_checkpoint, c);
    if (!c) {
        stage_load(0x008FC0, 1, 0x00901A, 0x00966C);
        players_start(0, 0xC0, 0x160);
        CameraPlace(0, 0x700, 0x100, 0x200);
        PlaneARedraw();
        PlaneBRedraw();
        speed_limits(0x70000);
        steps(0, 0);
        limits(0x180, 0x8F, 0x1B0, 0x40, 0x1C0);
        wr32(v_layout, 0x01AE0A);
        StageColours2();
        music(1, 0x81, 0x12);
        return;
    }
    RunCommands(0x008FC0);
    StageTilesJapan();
    RunCommands(0x00901A);
    RunCommands(0x009094);
    RunCommands(0x00909E);
    LoadPalettes(0x00966C);
    wr16(v_playerMode, 0);
    PlayersInit();
    PlayersVitality();
    PlayersPlace(0xC0, 0x160, 0);
    PlayersInFront();
    CameraPlace(0x1100, 0x320, 0x100, 0x48);
    wr16(v_camX + 8, 0x540);
    PlaneARedraw();
    PlaneBRedraw();
    speed_limits(0x70000);
    steps(0x30, 8);
    wr16(v_limitLeft, 0x8F);
    wr16(v_limitRight, 0x1B0);
    wr16(v_limitTop, 0x40);
    wr16(v_limitBottom, 0x1C0);
    wr16(v_deathLine, 0x180);
    wr8(v_cameraHold, 0x80);
    wr8(SCROLL_MODE, 3);
    wr8(v_planeFlagsA, 0x80);
    wr8(v_planeFlagsB, 0x80);
    BackLayersStart();
    wr32(v_layout, 0x01AE3E);
    StageLayout();
    StageColours2();
    music(0, 0x81, 0x12);
}

/* $0045A6. The mine. */
void Stage1Start(void)
{
    uint16_t c = rd16(v_checkpoints + 2);
    wr16(v_checkpoint, c);
    if (!c) {
        stage_load(0x008FC0, 1, 0x0090CA, 0x00967C);
        wr16(v_palette + 0x2E, 2);
        wr16(v_paletteTarget + 0x2E, 2);
        players_start(4, 0xC0, 0x150);
        PlayersInFront();
        CameraPlace(0, 0, 0x100, 0xFFF8);
        PlaneARedraw();
        PlaneBRedraw();
        speed_limits(0x70000);
        steps(0, 0);
        wr8(v_planeFlagsA, 0x30);
        limits(0, 0x8F, 0x1B0, 0x98, 0x160);
        wr16(v_boundsX0, 0x120);
        wr16(v_boundsY0, 0x150);
        wr16(v_boundsX1, 0x880);
        wr32(v_boundsY1, 0);
        wr32(CART_X, 0x20000);
        wr32(CART_Y, 0x40000);
        CameraSpeed(0x20000, 0);
        BackStop();
        wr16(v_boss + O_TYPE, 0x1C8);
        wr32(v_layout, 0x01AFDE);
        StageColours2();
        music(0, 0x85, 0x12);
        return;
    }
    stage_load(0x008FC0, 1, 0x0090CA, 0x00967C);
    players_start(4, 0x120, 0xF0);
    PlayersInFront();
    CameraPlace(0x200, 0x300, 0x100, 0x100);
    PlaneARedraw();
    PlaneBRedraw();
    speed_limits(0x70000);
    steps(0x4C, 8);
    wr8(v_planeFlagsA, 0x30);
    limits(0, 0x8F, 0x1B0, 0x98, 0x160);
    wr32(v_boundsX0, 0);
    wr32(v_boundsX1, 0);
    wr16(v_boundsY0, 0xA0);
    wr16(v_boundsY1, 0x150);
    wr16(v_boundsY0 + 2, 0);
    wr16(v_boundsY1 + 2, 0);
    wr32(CART_X, 0x100000);
    wr16(v_planes, 0x200);
    wr16(v_camX, 0x200);
    wr16(v_planes + 4, 0x300);
    wr16(v_camY, 0x300);
    wr16(v_camSpeedX, 0x10);
    wr16(v_planeSpeeds, 0);
    wr16(v_camSpeedY, 0);
    wr16(v_planeSpeeds + 4, 0);
    wr32(v_layout, 0x01AFDE);
    Stage1DirectorAtCheckpoint();
    StageColours2();
    music(0, 0x85, 0x12);
}

/* $0056AA. The battleship. */
void Stage2Start(void)
{
    uint16_t c = rd16(v_checkpoints + 4);
    wr16(v_checkpoint, c);
    if (!c) {
        stage_load(0x008FC0, 1, 0x00912E, 0x00968C);
        players_start(0, 0xA0, 0x100);
        CameraPlace(0x200, 0x4E0, 0x200, 0xA00);
        PlaneARedraw();
        PlaneBRedraw();
        speed_limits(0x70000);
        steps(0, 0);
        wr8(SCROLL_MODE, 4);
        wr8(v_planeFlagsA, 0x40);
        limits(0x180, 0x8F, 0x1B0, 0x40, 0x1C0);
        PlayersUnpin();
        wr16(v_battleship, 1);
        wr16(BOB_DIR, 0);
        wr16(STAGE2_SIGNAL, 0);
        CameraStop();
        BackStop();
        wr32(0xA990, rd32(v_planes));
        wr32(BOW_Y, rd32(v_planes + 4));
        wr32(BOW_COLUMN, rd32(v_planes + 4));
        wr32(0xA9A0, rd32(v_planes));
        wr32(SHIP_Y, rd32(v_planes + 4));
        wr32(0xA998, 0);
        wr32(BOW_SPEED, 0);
        wr32(0xA9A8, 0);
        wr32(SHIP_SPEED, 0);
        Stage2Objects();
        wr16(RUMBLE_SPRITES, 4);
        wr16(RUMBLE_PLANE, 2);
        wr16(0xA800 + O_TYPE, 0x2A4);
        wr32(v_layout, 0x01AFE6);
        StageColours2();
        music(0, 0x9A, 0x12);
        return;
    }
    stage_load(0x008FC0, 1, 0x0091B0, 0x00968C);
    players_start(0, 0xE0, 0x100);
    CameraPlace(0x1000, 0x220, 0x200, 0x240);
    PlaneARedraw();
    PlaneBRedraw();
    speed_limits(0x70000);
    steps(0x24, 0x1C);
    wr8(SCROLL_MODE, 2);
    wr8(v_planeFlagsA, 0);
    wr8(v_planeFlagsB, 0x80);
    Stage2BandsStart();
    limits(0x180, 0x8F, 0x1B0, 0x40, 0x1C0);
    CameraStop();
    BackStop();
    wr32(v_camY + 8, 0x2400000);
    wr32(v_planes + 0xC, 0x2400000);
    wr16(STAGE2_SIGNAL, 0);
    wr16(0xA800 + O_TYPE, 0x2C0);
    wr32(v_layout, 0x01AFE6);
    StageColours2();
    music(0, 0x9A, 0x12);
}

/* $006B46. The dice palace. */
void Stage3Start(void)
{
    uint16_t c = rd16(v_checkpoints + 6);
    wr16(v_checkpoint, c);
    if (!c) {
        stage_load(0x008FC0, 1, 0x009302, 0x00969C);
        players_start(0, 0xC0, 0x120);
        CameraPlace(0, 0, 0x100, 0);
        PlaneARedraw();
        PlaneBRedraw();
        speed_limits(0x70000);
        steps(0, 0);
        wr8(SCROLL_MODE, 2);
        wr8(v_planeFlagsB, 0x80);
        limits(0x180, 0x8F, 0x1B0, 0x40, 0x1C0);
        wr16(STAGE3_SIGNAL, 0);
        wr32(v_layout, 0x01B08E);
        StageColours2();
        music(0, 0x8A, 0x12);
        return;
    }
    stage_load(0x008FC0, 1, 0x00935C, 0x00969C);
    players_start(0, 0xC0, 0x120);
    CameraPlace(0x200, 0x100, 0x200, 0x100);
    PlaneARedraw();
    PlaneBRedraw();
    speed_limits(0x70000);
    steps(0x18, 8);
    wr8(v_planeFlagsA, 0xC0);
    wr8(v_planeFlagsB, 0xC0);
    wr16(v_limitLeft, 0x8F);
    wr16(v_limitRight, 0x1B0);
    wr16(v_limitTop, 0x98);
    wr16(v_limitBottom, 0x1C0);
    wr16(v_deathLine, 0x180);
    wr16(0xA98E, 4);
    wr32(v_layout, 0x01B09E);
    StageLayout();
    wr16(v_boss + O_TYPE, 0x260);
    StageColours2();
    music(0, 0x8A, 0x12);
}

/* $006F66. The base (no checkpoint start of its own). */
void Stage4Start(void)
{
    wr16(v_checkpoint, rd16(v_checkpoints + 8));
    stage_load(0x008FC0, 1, 0x0093C8, 0x0096AC);
    players_start(0, 0xC0, 0x120);
    CameraPlace(0, 0, 0, 0);
    PlaneARedraw();
    PlaneBRedraw();
    speed_limits(0x70000);
    steps(0, 0);
    wr8(SCROLL_MODE, 2);
    wr8(v_planeFlagsA, 0x80);
    wr8(v_planeFlagsB, 0x80);
    wr16(v_limitLeft, 0x8F);
    wr16(v_limitRight, 0x1B0);
    wr16(v_limitTop, 0x40);
    wr16(v_limitBottom, 0x1C0);
    wr16(v_deathLine, 0x180);
    wr16(TRAIN_FACTOR, 0x800);
    wr32(TRACK_X, rd32(v_planes));
    CameraStop();
    wr32(v_layout, 0x01B106);
    StageColours2();
    music(0, 0x89, 0x12);
}

/* $00785C. Space: the ships ($C), the stars. */
void Stage5Start(void)
{
    uint16_t c = rd16(v_checkpoints + 0xA);
    wr16(v_checkpoint, c);
    if (!c) {
        stage_load(0x008F86, 0, 0x0094EA, 0x0096DC);
        players_start(0xC, 0x120, 0xF0);
        CameraPlace(0x780, 0x560, 0x200, 0x400);
        PlaneARedraw();
        PlaneBRedraw();
        speed_limits(0x100000);
        steps(0, 0);
        wr8(SCROLL_MODE, 3);
        wr8(v_planeFlagsB, 0x80);
        limits(0x180, 0x8F, 0x1B0, 0x98, 0x160);
        CameraStop();
        wr16(STAR_TIME, 0x100);
        StarsStart();
        wr16(0xA98C, 4);
        wr32(v_layout, 0x01B45A);
        StageColours();
        wr16(v_boss + O_TYPE, 0x444);
        wr16(v_boss + O_STATE, 0);
        music(0, 0x8D, 0x12);
        return;
    }
    stage_load(0x008FC0, 1, 0x009456, 0x0096BC);
    players_start(0, 0xC0, 0xE0);
    CameraPlace(0x200, 0x100, 0x200, 0x100);
    PlaneARedraw();
    PlaneBRedraw();
    steps(0x34, 0x10);
    CameraStop();
    BackStop();
    limits(0x180, 0x8F, 0x1B0, 0x40, 0x1C0);
    speed_limits(0x70000);
    wr32(v_layout, 0x01B45A);
    StageColours();
    wr16(v_boss + O_TYPE, 0x440);
    wr16(v_boss + O_STATE, 0);
    music(0, 0x93, 0x28);
}

/* $00855C. The final battle. */
void Stage6Start(void)
{
    uint16_t c = rd16(v_checkpoints + 0xC);
    wr16(v_checkpoint, c);
    if (!c) {
        stage_load(0x008FC0, 1, 0x009598, 0x0096EC);
        players_start(0, 0xC0, 0xE0);
        CameraPlace(0x200, 0x400, 0x200, 0x100);
        PlaneARedraw();
        PlaneBRedraw();
        limits(0x180, 0x8F, 0x1B0, 0x80, 0x1C0);
        speed_limits(0x70000);
        steps(0, 0);
        CameraStop();
        BackStop();
        wr16(v_boss + O_TYPE, 0x398);
        wr16(v_boss + O_STATE, 0);
        wr32(v_layout, 0x01B45A);
        StageColours();
        music(0, 0x98, 0x12);
        return;
    }
    stage_load(0x008FC0, 1, 0x00960A, 0x0096FC);
    players_start(0, 0xC0, 0xE0);
    CameraPlace(0x1A00, 0x200, 0x800, 0x100);
    PlaneARedraw();
    PlaneBRedraw();
    limits(0x180, 0x9F, 0x1A0, 0x80, 0x1C0);
    speed_limits(0x70000);
    steps(0x44, 0);
    CameraStop();
    BackStop();
    wr32(v_layout, 0x01B45A);
    StageColours();
    PlayersInFront();
    wr16(v_boss + O_TYPE, 0x5B4);
    wr16(v_boss + O_STATE, 0);
    music(0, 0x94, 0x80);
}
