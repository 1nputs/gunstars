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
