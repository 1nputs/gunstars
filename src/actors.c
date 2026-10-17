/* The story scenes' actors: characters that stand in a pose and change it on a timer. Each type
 * is run by its scene number (+$48: a new scene restarts it) through a table of one entry per
 * scene. */
#include "game.h"

#define A_SCENE      0x48
#define A_SCENE_SEEN 0x4A
#define A_TIME       0x4C

/* a new scene restarts the actor; its routine for the scene */
static void by_scene(uint16_t obj, uint32_t table)
{
    uint16_t s = rd16(obj + A_SCENE);
    if (s != rd16(obj + A_SCENE_SEEN)) {
        wr16(obj + A_SCENE_SEEN, s);
        wr16(obj + O_STATE, 0);
    }
    call_object(rom32(table + (uint32_t)(int16_t)s), obj);
}

/* an actor set up: shown at x, y with its sprite word, +$DA and first pose, $200 frames to go */
static void actor_start(uint16_t obj, uint16_t sprite, uint16_t da, uint16_t x, uint16_t y, uint32_t pose)
{
    wr16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0xED00);
    wr16(obj + 0x0E, sprite);
    wr16(obj + 0xDA, da);
    wr8(obj + 0x20, 0x80);
    wr16(obj + O_X, x);
    wr16(obj + O_Y, y);
    wr32(obj + 0x08, pose);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + A_TIME, 0x200);
}

static void pose(uint16_t obj, uint32_t p)
{
    wr32(obj + 0x08, p);
    wr16(obj + 0x0C, 0);
}

/* $02191E */
void ActorA(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) actor_start(obj, 0x8EC0, 0xD800, 0x180, 0xF72C, 0x0CFF98);
    wr16(obj + A_TIME, (uint16_t)(rd16(obj + A_TIME) - 1));
    uint16_t t = rd16(obj + A_TIME);
    if (!t) pose(obj, 0x0CFF2C);
    else if (t == 0x40) pose(obj, 0x0D0314);
    else if (t == 0x80) pose(obj, 0x0D027C);
}

/* $021900. Type $644. */
void ActorTypeA(uint16_t obj)
{
    by_scene(obj, 0x02191A);
}

/* $021FBE */
void ActorB(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) actor_start(obj, 0x8580, 0xB000, 0xB0, 0xD8, 0x0C00DA);
    wr16(obj + A_TIME, (uint16_t)(rd16(obj + A_TIME) - 1));
    uint16_t t = rd16(obj + A_TIME);
    if (!t) pose(obj, 0x0C00DA);
    else if (t == 0x80) pose(obj, 0x0C01F6);
    else if (t == 0x100) pose(obj, 0x0C007E);
    else if (t == 0x180) pose(obj, 0x0C0022);
}

/* $021FA0. Type $61C. */
void ActorTypeB(uint16_t obj)
{
    by_scene(obj, 0x021FBA);
}

/* a still figure: shown with these flags, sprite word, $20 and mappings at x, y */
static void figure(uint16_t obj, uint16_t flags, uint16_t sprite, uint8_t b20, uint16_t x, uint16_t y, uint32_t map)
{
    wr16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, flags);
    wr16(obj + 0x0E, sprite);
    wr8(obj + 0x20, b20);
    wr16(obj + O_X, x);
    wr16(obj + O_Y, y);
    wr32(obj + 0x08, map);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
}

/* $020D30. A banner's set-up (still, not touching, its first frame). */
void BannerStart(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0xE100);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr8(obj + 0x21, 0);
    wr16(obj + 0x0E, 0x400);
    wr32(obj + 0x08, 0x0E21B4);
    wr16(obj + 0x0C, 0);
    wr8(obj + 0x20, 0xA0);
    wr16(obj + O_STATE, 4);
}

/* $020D60 */
void BannerWait(uint16_t obj)
{
    (void)obj;
}

/* $020D62. Its second frame. */
void BannerFlip(uint16_t obj)
{
    wr32(obj + 0x08, 0x0E21CC);
    wr16(obj + 0x0C, 0);
    wr16(obj + O_STATE, 0xC);
}

/* $020D76 */
void BannerDone(uint16_t obj)
{
    (void)obj;
}

/* $020D16. Type $4F0. */
void BannerA(uint16_t obj)
{
    call_object(rom32(0x020D20 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $020D92. Type $4F4's: the same. */
void BannerBStart(uint16_t obj)
{
    BannerStart(obj);
}

/* $020DC2 */
void BannerBWait(uint16_t obj)
{
    (void)obj;
}

/* $020DC4 */
void BannerBFlip(uint16_t obj)
{
    BannerFlip(obj);
}

/* $020DD8 */
void BannerBDone(uint16_t obj)
{
    (void)obj;
}

/* $020D78. Type $4F4. */
void BannerB(uint16_t obj)
{
    call_object(rom32(0x020D82 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $021C08 */
void FigureC(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) figure(obj, 0xEC00, 0xB00, 0x80, 0x158, 0xE8, 0x0E4AFE);
}

/* $021BEA. Type $5C4. */
void FigureTypeC(uint16_t obj)
{
    by_scene(obj, 0x021C04);
}

/* $021EAA, $021EEE, $021F32: three figures one above another */
void FigureD1(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) figure(obj, 0xCC00, 0x300, 0xA0, 0x130, 0xA2, 0x021F76);
}

/* $021EEE */
void FigureD2(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) figure(obj, 0xCC00, 0x300, 0xA0, 0x130, 0xE2, 0x021F82);
}

/* $021F32 */
void FigureD3(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) figure(obj, 0xCC00, 0x300, 0xA0, 0x130, 0x122, 0x021F88);
}

/* $021E84. Type $5CC. */
void FigureTypeD(uint16_t obj)
{
    by_scene(obj, 0x021E9E);
}

/* ---- type $5D0: a craft over the scenes, the boss's slot ($5D8) following it ---- */

/* $0216F2. Bobbing: up and down at 1/4 for 32 frames each. */
void CraftBob(uint16_t obj)
{
    uint16_t n = (uint16_t)(rd16(obj + 0x4E) + 1);
    wr16(obj + 0x4E, n);
    wr32(obj + 0x1C, (n & 0x20) ? 0xFFFFC000 : 0x4000);
}

/* the boss's slot holds the craft's other half ($5D8; $C9D6 16) */
static void craft_half(uint16_t type)
{
    wr16(v_boss + O_TYPE, type);
    wr16(v_boss + O_STATE, 0);
    wr32(v_boss + 0x18, 0);
    wr32(v_boss + 0x1C, 0);
    wr16(v_boss + 0x56, 0x10);
}

/* the other half at the craft (30 lower) */
static void craft_follow(uint16_t obj)
{
    wr16(v_boss + O_X, rd16(obj + O_X));
    wr16(v_boss + O_Y, (uint16_t)(rd16(obj + O_Y) + 0x1E));
}

static void craft_start(uint16_t obj, uint16_t flags, uint16_t x, uint32_t vx)
{
    wr16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, flags);
    wr16(obj + 0x0E, 0x8500);
    wr8(obj + 0x20, 0x80);
    wr16(obj + O_X, x);
    wr16(obj + O_Y, 0xC0);
    wr32(obj + 0x08, 0x0E1970);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x18, vx);
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x4E, 0);
}

/* $021662. Back and forth ($E0 frames each way), bobbing. */
void CraftA(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        craft_start(obj, 0xEC00, 0xE0, 0x8000);
        wr16(obj + 0x4C, 0xE0);
        craft_half(0x5D8);
    }
    wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) - 1));
    if (!rd16(obj + 0x4C)) {
        wr16(obj + 0x4C, 0xE0);
        wr32(obj + 0x18, 0u - rd32(obj + 0x18));
    }
    CraftBob(obj);
    craft_follow(obj);
}

/* $021718. In at 3 a frame; it stops at the scene's step 4. */
void CraftB(uint16_t obj)
{
    uint16_t s = rd16(obj + O_STATE);
    if (s != 4 && !((uint16_t)(s - 4) & 0x8000)) {
        CraftBob(obj);
        craft_follow(obj);
        return;
    }
    if (s != 4) {
        craft_start(obj, 0xED00, 0xB0, 0x30000);
        craft_half(0x5D8);
    }
    if ((int16_t)rd16(v_sceneStep) >= 4) {
        wr16(obj + O_STATE, 8);
        wr32(obj + 0x18, 0);
    }
    CraftBob(obj);
    craft_follow(obj);
}

/* $0217A2. Parked. */
void CraftC(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) figure(obj, 0xEC00, 0x8500, 0x80, 0xD4, 0xD8, 0x0E1970);
}

/* both halves at the craft */
static void craft_with(uint16_t obj)
{
    wr16(v_boss + O_X, rd16(obj + O_X));
    wr16(v_boss + O_Y, rd16(obj + O_Y));
}

/* $0217FE. The ending's: off the right, the other half $610. */
void CraftD0(uint16_t obj)
{
    wr16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0xC00);
    wr16(obj + O_X, 0x1E0);
    wr16(obj + O_Y, 0xD0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    craft_half(0x610);
    CraftD1(obj);
}

/* $021836. At the scene's step $20, in at 1/4. */
void CraftD1(uint16_t obj)
{
    if ((int16_t)rd16(v_sceneStep) < 0x20) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr32(obj + 0x18, 0xFFFFC000);
    CraftD2(obj);
}

/* $02184E. Stopping at x $180, until step $24. */
void CraftD2(uint16_t obj)
{
    if ((int16_t)rd16(v_sceneStep) >= 0x24) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        CraftD3(obj);
        return;
    }
    if ((int16_t)rd16(obj + O_X) <= 0x180) wr32(obj + 0x18, 0);
    craft_with(obj);
}

/* $021878. +$56 of the other half up to $60, until step $28. */
void CraftD3(uint16_t obj)
{
    if ((int16_t)rd16(v_sceneStep) >= 0x28) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        CraftD4(obj);
        return;
    }
    uint16_t v = rd16(v_boss + 0x56);
    if ((int16_t)v < 0x60) wr16(v_boss + 0x56, (uint16_t)(v + 1));
}

/* $02189C. Away left, faster, to x $D0: then an object $63C at $D9A0. */
void CraftD4(uint16_t obj)
{
    if ((int16_t)rd16(obj + O_X) > 0xD0) {
        wr32(obj + 0x18, rd32(obj + 0x18) - 0x600);
        craft_with(obj);
        return;
    }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(0xD9A0 + O_TYPE, 0x63C);
    wr16(0xD9A0 + O_STATE, 0);
    wr16(0xD9A0 + 0x48, 0xC);
    CraftD5(obj);
}

/* $0218D0. Slowing by half every 4 frames; the object at $D9A0 with it. */
void CraftD5(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 3)) wr32(obj + 0x18, (uint32_t)((int32_t)rd32(obj + 0x18) >> 1));
    craft_with(obj);
    wr16(0xD9A0 + O_X, rd16(obj + O_X));
    wr16(0xD9A0 + O_Y, rd16(obj + O_Y));
}

/* $0217E4 */
void CraftD(uint16_t obj)
{
    uint32_t at = 0x0217F2 + (uint32_t)(int16_t)((int16_t)rd16(obj + O_STATE) >> 1);
    call_object(at + (uint32_t)(int16_t)rom16(at), obj);
}

/* $021638. Type $5D0. */
void CraftType(uint16_t obj)
{
    by_scene(obj, 0x021652);
}

/* ---- type $648: falling debris in a scene ---- */

static void debris_start(uint16_t obj, uint16_t x, uint32_t vx)
{
    figure(obj, 0xEC00, 0x8400, 0xD0, x, 0xA0, 0x021BAC);
    wr32(obj + 0x18, vx);
    wr32(obj + 0x1C, 0x3000);
}

/* $0219D8. Falling left, gone at y $110. */
void DebrisA(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) debris_start(obj, 0x150, 0xFFFFE000);
    if ((int16_t)rd16(obj + O_Y) >= 0x110) set8(obj + O_FLAGS, 4);
}

/* $021A34. Falling straight, gone at $120. */
void DebrisB(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) debris_start(obj, 0xE0, 0);
    if ((int16_t)rd16(obj + O_Y) >= 0x120) set8(obj + O_FLAGS, 4);
}

/* $021A90. Hidden $81 frames, then shown with a sound until its animation ends. */
void DebrisC(uint16_t obj)
{
    uint16_t s = rd16(obj + O_STATE);
    if (s == 0) {
        wr16(obj + O_STATE, 4);
        wr16(obj + O_FLAGS, 0);
        wr16(obj + 0x0E, 0x8400);
        wr8(obj + 0x20, 0xD0);
        wr16(obj + O_X, 0x160);
        wr16(obj + O_Y, 0xA0);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr16(obj + 0x4C, 0x80);
        s = 4;
    }
    if (s == 4) {
        wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) - 1));
        if (!(rd16(obj + 0x4C) & 0x8000)) return;
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + O_FLAGS, 0xEC00);
        wr32(obj + 0x08, 0x021BC0);
        wr16(obj + 0x0C, 0);
        QueueSound(0xDC);
    }
    if (rd8(obj + 0x0D) & 0x80) set8(obj + O_FLAGS, 4);
}

/* $0219B2. Type $648. */
void DebrisType(uint16_t obj)
{
    by_scene(obj, 0x0219CC);
}

/* ---- type $64C: figures of a scene ---- */

/* $021C80 */
void FigureE1(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) figure(obj, 0xEC00, 0x340, 0x80, 0x16C, 0xEC, 0x0E576E);
}

/* $021CC2. Pacing between x $170 and $1AC (flipped going left). */
void FigureE2(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        figure(obj, 0xEC00, 0x340, 0x80, 0x1A0, 0xEC, 0x0E578A);
        wr32(obj + 0x18, 0x10000);
    }
    int16_t x = (int16_t)rd16(obj + O_X);
    if (x >= 0x1AC) {
        wr32(obj + 0x18, 0xFFFE0000);
        set8(obj + 0x0E, 3);
        return;
    }
    if (x > 0x170) return;
    wr32(obj + 0x18, 0x20000);
    clr8(obj + 0x0E, 3);
}

/* $021D3A */
void FigureE3(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) figure(obj, 0xEC00, 0xB40, 0x80, 0x1B0, 0x128, 0x0E57BA);
}

/* $021D7C */
void FigureE4(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) figure(obj, 0xEC00, 0xB40, 0x80, 0x160, 0x128, 0x0E57A2);
}

/* $021DBE */
void FigureE5(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) figure(obj, 0xEC00, 0x340, 0x80, 0x150, 0x120, 0x0E57A2);
}

/* $021E00 */
void FigureE6(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) figure(obj, 0xEC00, 0xB40, 0x80, 0x100, 0x130, 0x0E57F4);
}

/* $021E42 */
void FigureE7(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) figure(obj, 0xEC00, 0xB40, 0x80, 0xB0, 0x128, 0x0E5842);
}

/* $021C4A. Type $64C. */
void FigureTypeE(uint16_t obj)
{
    by_scene(obj, 0x021C64);
}
