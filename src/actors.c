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

/* ---- type $620: a scene's set (figures in their places) ---- */

/* $0220AC. One already placed: a pose at $40 frames left, another at the end. */
void SetA1(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 4);
        wr16(obj + O_FLAGS, 0xED00);
        wr16(obj + 0x0E, 0x8300);
        wr8(obj + 0x20, 0x80);
        wr16(obj + 0x0C, 0);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr16(obj + A_TIME, 0x200);
    }
    wr16(obj + A_TIME, (uint16_t)(rd16(obj + A_TIME) - 1));
    if (!rd16(obj + A_TIME)) pose(obj, 0x0DFC4E);
    else if (rd16(obj + A_TIME) == 0x40) pose(obj, 0x0DFC6E);
}

/* $02210E */
void SetA2(uint16_t obj) { if (!rd16(obj + O_STATE)) figure(obj, 0xED00, 0x8480, 0, 0x100, 0x118, 0x0DF4FC); }
/* $022150 */
void SetA3(uint16_t obj) { if (!rd16(obj + O_STATE)) figure(obj, 0xED00, 0x480, 0x40, 0x180, 0xB8, 0x0DFC8E); }
/* $022192 */
void SetA4(uint16_t obj) { if (!rd16(obj + O_STATE)) figure(obj, 0xED00, 0xC80, 0x40, 0x198, 0xF0, 0x0DF4FC); }
/* $0221D4 */
void SetA5(uint16_t obj) { if (!rd16(obj + O_STATE)) figure(obj, 0xED00, 0xC80, 0x40, 0x1B0, 0xF0, 0x0DF4FC); }
/* $022216 */
void SetA6(uint16_t obj) { if (!rd16(obj + O_STATE)) figure(obj, 0xED00, 0xC80, 0x40, 0xC8, 0x120, 0x0E0164); }
/* $022258 */
void SetA7(uint16_t obj) { if (!rd16(obj + O_STATE)) figure(obj, 0xED00, 0xC80, 0x40, 0xE8, 0x120, 0x0E022C); }
/* $02229A */
void SetA8(uint16_t obj) { if (!rd16(obj + O_STATE)) figure(obj, 0xED00, 0xC80, 0x40, 0xCC, 0xC8, 0x0E002A); }
/* $0222DC */
void SetA9(uint16_t obj) { if (!rd16(obj + O_STATE)) figure(obj, 0xED00, 0xC80, 0x40, 0xF0, 0xE8, 0x0E00CC); }

/* $02231E. Drifting left, wrapping round from x $60 to $1E0. */
void SetA10(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 4);
        wr16(obj + O_FLAGS, 0xED00);
        wr16(obj + 0x0E, 0x8B00);
        wr8(obj + 0x20, 0x80);
        pose(obj, 0x0DF2D0);
        wr32(obj + 0x18, 0xFFFEC000);
        wr32(obj + 0x1C, 0);
    }
    if ((int16_t)rd16(obj + O_X) <= 0x60) wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) + 0x180));
}

/* $02206A. Type $620. */
void SetTypeA(uint16_t obj)
{
    by_scene(obj, 0x022084);
}

/* ---- type $624: a cloud carrying a figure (type $628 in the next slot) ---- */

/* $022402. Bobbing: 16 frames down, 16 up. */
void CloudBob(uint16_t obj)
{
    uint16_t n = (uint16_t)(rd16(obj + 0x4E) + 1);
    wr16(obj + 0x4E, n);
    wr32(obj + 0x1C, (n & 0x10) ? 0xFFFFC000 : 0x4000);
}

/* $022386. Right at 2, wrapping round from $1E0; the rider kept on it. */
void Cloud(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, 4);
        wr16(obj + O_FLAGS, 0xEC00);
        wr16(obj + 0x0E, 0x300);
        wr8(obj + 0x20, 0xE0);
        wr16(obj + 0x0C, 0);
        wr32(obj + 0x18, 0x20000);
        wr32(obj + 0x1C, 0);
        wr16(obj + 0x4E, 0);
        uint16_t rider = (uint16_t)(obj + 0x60);
        wr16(obj + 0x50, rider);
        wr16(rider + O_TYPE, 0x628);
        wr16(rider + O_STATE, 0);
        wr16(rider + 0x48, 0);
    }
    if ((int16_t)rd16(obj + O_X) >= 0x1E0) wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) - 0x180));
    CloudBob(obj);
    uint16_t rider = rd16(obj + 0x50);
    wr16(rider + O_X, (uint16_t)(rd16(obj + O_X) - 4));
    wr16(rider + O_Y, (uint16_t)(rd16(obj + O_Y) - 0x10));
}

/* $022368. Type $624. */
void CloudType(uint16_t obj)
{
    by_scene(obj, 0x022382);
}

/* $022446. The rider. */
void CloudRider(uint16_t obj)
{
    if (rd16(obj + O_STATE)) return;
    wr16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0xED00);
    wr16(obj + 0x0E, 0x6D6);
    wr8(obj + 0x20, 0xE0);
    pose(obj, 0x0E06E4);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
}

/* $022428. Type $628. */
void CloudRiderType(uint16_t obj)
{
    by_scene(obj, 0x022442);
}

/* ---- type $5C8 ---- */

/* $02249A. Two poses, at $C0 frames left and at the end. */
void FigureF(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        figure(obj, 0xEC00, 0x8E8C, 0x80, 0x128, 0x114, 0x0E49E6);
        wr16(obj + A_TIME, 0x240);
    }
    wr16(obj + A_TIME, (uint16_t)(rd16(obj + A_TIME) - 1));
    if (!rd16(obj + A_TIME)) pose(obj, 0x0E4A3C);
    else if (rd16(obj + A_TIME) == 0xC0) pose(obj, 0x0E4A00);
}

/* $02247C. Type $5C8. */
void FigureTypeF(uint16_t obj)
{
    by_scene(obj, 0x022496);
}

/* ---- type $640 ---- */

/* $022A90 */
void FigureG1(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) figure(obj, 0xEC00, 0x8C80, 0x80, 0x140, 0xD0, 0x0E5FD8);
}

/* $022AEC. The ending's: in from the right at the scene's step 8 ... */
void FigureG2a(uint16_t obj)
{
    figure(obj, 0xEC00, 0x8CE0, 0x80, 0x1E0, 0xE8, 0x0E61C0);
    FigureG2b(obj);
}

/* $022B24 */
void FigureG2b(uint16_t obj)
{
    if ((int16_t)rd16(v_sceneStep) < 8) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr32(obj + 0x18, 0xFFFFC000);
    FigureG2c(obj);
}

/* $022B3C. ... stopping at x $170 ... */
void FigureG2c(uint16_t obj)
{
    if ((int16_t)rd16(obj + O_X) > 0x170) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr32(obj + 0x18, 0);
    FigureG2d(obj);
}

/* $022B50. ... on at step $C ... */
void FigureG2d(uint16_t obj)
{
    if ((int16_t)rd16(v_sceneStep) < 0xC) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr32(obj + 0x18, 0xFFFFC000);
    FigureG2e(obj);
}

/* $022B68 */
void FigureG2e(uint16_t obj)
{
    if ((int16_t)rd16(v_sceneStep) < 0x14) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    FigureG2f(obj);
}

/* $022B78. ... slowing, flipping every 8 frames, gone at step $18. */
void FigureG2f(uint16_t obj)
{
    if ((int16_t)rd16(v_sceneStep) >= 0x18) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    wr32(obj + 0x18, (uint32_t)((int32_t)rd32(obj + 0x18) >> 1));
    wr32(obj + 0x1C, 0);
    if (bit8(v_gameFrames + 1, 3)) set8(obj + 0x0E, 3);
    else clr8(obj + 0x0E, 3);
}

/* $022AD2 */
void FigureG2(uint16_t obj)
{
    uint32_t at = 0x022AE0 + (uint32_t)(int16_t)((int16_t)rd16(obj + O_STATE) >> 1);
    call_object(at + (uint32_t)(int16_t)rom16(at), obj);
}

/* $022A6E. Type $640. */
void FigureTypeG(uint16_t obj)
{
    by_scene(obj, 0x022A88);
}
