/* The engine's frame, the parts every mode with a stage in it runs (the story, the title, the
 * demos, the stages): the camera and the planes' scrolls move, the objects move with their
 * speeds (and with the camera when they belong to the level), the players held to their speed
 * limits, the draw list started and counted, the screen's shake. */
#include "game.h"


#define O_VX 0x18
#define O_VY 0x1C
#define O_MOVE 0x02              /* the flags' high byte: bit 3 moves in x, bit 2 in y, bit 0 with
                                    the camera (fixed to the level) */
#define P_HITS 0x47
#define P_FLOOR 0x82
#define P_WALL 0x84
#define P_STATUS 0x23
#define P_PUSH 0x88              /* longs: a player's push from what it stands on */

/* $00A630. The camera and the background planes by their speeds; the map's scroll from the
 * camera ($80 from its middle). */
void CameraMove(void)
{
    if (rd8(v_pauseFlags) & 0x80) return;
    for (uint16_t i = 0; i < 0x10; i += 4)
        wr32((uint16_t)(v_planes + i), rd32((uint16_t)(v_planes + i)) + rd32((uint16_t)(v_planeSpeeds + i)));
    wr32(v_camX, rd32(v_camX) + rd32(v_camSpeedX));
    wr16(v_mapX, (uint16_t)(rd16(v_camX) - 0x80));
    wr32(v_camY, rd32(v_camY) + rd32(v_camSpeedY));
    wr16(v_mapY, (uint16_t)(rd16(v_camY) - 0x80));
    wr32(v_camX + 8, rd32(v_camX + 8) + rd32(v_camSpeedX + 8));
    wr32(v_camY + 8, rd32(v_camY + 8) + rd32(v_camSpeedY + 8));
}

/* an object's speeds added (the flags' bits), and the camera's move taken off one fixed to the
 * level */
static void move(uint16_t obj, uint8_t f, int with_camera, uint16_t dx, uint16_t dy)
{
    if (f & 8) wr32(obj + O_X, rd32(obj + O_X) + rd32(obj + O_VX));
    if (f & 4) wr32(obj + O_Y, rd32(obj + O_Y) + rd32(obj + O_VY));
    if (with_camera && (f & 1)) {
        wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) - dx));
        wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - dy));
    }
}

/* $01F6F6. Everything moved: the players (their speeds held to the limits, then what they stand
 * on and the camera), the object at $FFA800, the drawn objects. When the camera has not moved a
 * pixel nothing follows it. */
void ObjectsMove(void)
{
    if (rd8(v_pauseFlags) & 0x80) return;
    uint16_t dx = (uint16_t)((uint16_t)((rd32(v_camX) + rd32(v_camSpeedX)) >> 16) - rd16(v_camX));
    uint16_t dy = (uint16_t)((uint16_t)((rd32(v_camY) + rd32(v_camSpeedY)) >> 16) - rd16(v_camY));
    int camera = dx || dy;
    for (uint16_t p = v_player1;; p = v_player2) {
        uint8_t f = rd8(p + O_MOVE);
        if (f) {
            PlayerMoveLimited(p, f);
            if (camera) PlayerMoveWithFloor(p, f, dx, dy);
        }
        if (p == v_player2) break;
    }
    uint8_t f = rd8(0xA800 + O_MOVE);
    if (f) move(0xA800, f, camera, dx, dy);
    uint16_t n = rd16(v_drawCount);
    for (uint16_t i = 0; i < n; i++) {
        uint16_t obj = rd16((uint16_t)(v_drawList + 2 * i));
        f = rd8(obj + O_MOVE);
        if (f) move(obj, f, camera, dx, dy);
    }
}

/* $01F862. A player's speeds, each held within v_speedLimitX / Y, added; its floor and walls
 * forgotten (the collision finds them again). */
void PlayerMoveLimited(uint16_t obj, uint8_t f)
{
    wr8(obj + P_HITS, 0);
    wr16(obj + P_WALL, 0);
    wr16(obj + P_FLOOR, 0);
    if (f & 8) {
        int32_t v = (int32_t)rd32(obj + O_VX), lim = (int32_t)rd32(v_speedLimitX);
        if (v >= lim) v = lim;
        else if (v <= (int32_t)(0u - (uint32_t)lim)) v = (int32_t)(0u - (uint32_t)lim);
        wr32(obj + O_X, rd32(obj + O_X) + (uint32_t)v);
    }
    if (f & 4) {
        int32_t v = (int32_t)rd32(obj + O_VY), lim = (int32_t)rd32(v_speedLimitY);
        if (v >= lim) v = lim;
        else if (v <= (int32_t)(0u - (uint32_t)lim)) v = (int32_t)(0u - (uint32_t)lim);
        wr32(obj + O_Y, rd32(obj + O_Y) + (uint32_t)v);
    }
}

/* $01F8C0. A player fixed to the level: what it stands on's push, the camera's move off (not
 * the frame it was put down: P_STATUS bit 0). */
void PlayerMoveWithFloor(uint16_t obj, uint8_t f, uint16_t dx, uint16_t dy)
{
    if (bit8(obj + P_STATUS, 0)) { clr8(obj + P_STATUS, 0); return; }
    if (!(f & 1)) return;
    wr32(obj + O_X, rd32(obj + O_X) + rd32(obj + P_PUSH));
    wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) - dx));
    wr32(obj + O_Y, rd32(obj + O_Y) + rd32(obj + P_PUSH + 4));
    wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - dy));
}

/* $01F65E. The draw list begins (not while paused). */
void DrawListStart(void)
{
    if (rd8(v_pauseFlags) & 0x80) return;
    wr16(v_drawPtr, v_drawList);
}

/* $01F66E. The draw list's count. */
void DrawListEnd(void)
{
    if (rd8(v_pauseFlags) & 0x80) return;
    wr16(v_drawCount, (uint16_t)((uint16_t)(rd16(v_drawPtr) - v_drawList) >> 1));
}

/* $01F914. The screen's shake: v_shakeSprites and v_shakePlane count down, and the offsets
 * (the sprites' v_cameraY, v_planeShake) are a quarter of them, the other way on odd frames. */
void ScreenShake(void)
{
    if (rd8(v_pauseFlags) & 0x80) return;
    int16_t a = (int16_t)rd16(v_shakeSprites), b = (int16_t)rd16(v_shakePlane);
    if (a) wr16(v_shakeSprites, (uint16_t)(a - 1));
    if (b) wr16(v_shakePlane, (uint16_t)(b - 1));
    if (rd16(v_gameFrames) & 1) {
        a = (int16_t)-a;
        b = (int16_t)-b;
    }
    wr16(v_cameraY, (uint16_t)(a >> 2));
    wr16(v_planeShake, (uint16_t)(b >> 2));
}
