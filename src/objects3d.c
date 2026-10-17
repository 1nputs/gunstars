/* The battleship's deck fight (type $284, from $3D4B2): the deck rising beside the ship, tilting
 * and swaying ($9800 its height, $9804 its sway, $9810 its tilt, $9828/$982C the lower deck's,
 * each 16.16), the lines of planes A and B scrolled to draw it; the boss on foot ($380) brought
 * up on it, then the brawler of the deck ($56: its moves), held on the slope; beaten: fallen
 * off, the screen faded, the stage's next part. */
#include "game.h"

#define DECK_Y     0x9800
#define DECK_SWAY  0x9804
#define DECK_TILT  0x9810

static uint16_t sway(void) { return rd16(DECK_SWAY); }

/* ---- the deck's motion ---- */

/* $03F0E6: the deck bobbing $8000 a frame between `low` and `high` ($981E its way) */
static void deck_bob(uint16_t high, uint16_t low)
{
    if (!rd16(0x981E)) {
        wr32(DECK_Y, rd32(DECK_Y) + 0x8000);
        if ((int16_t)(high - rd16(DECK_Y)) >= 0) return;
        wr16(0x981E, 2);
        return;
    }
    wr32(DECK_Y, rd32(DECK_Y) - 0x8000);
    if ((int16_t)(low - rd16(DECK_Y)) < 0) return;
    wr16(0x981E, 0);
}

/* $03F11E: the deck swaying $2000 a frame between -2 and 2 ($981C its way) */
static void deck_sway(void)
{
    if (!rd16(0x981C)) {
        uint32_t v = rd32(DECK_SWAY) + 0x2000;
        wr32(DECK_SWAY, v);
        if ((v & 0x80000000u) || (int16_t)(rd16(DECK_SWAY) - 2) < 0) return;
        wr16(0x981C, 2);
        return;
    }
    uint32_t v = rd32(DECK_SWAY) - 0x2000;
    wr32(DECK_SWAY, v);
    if (!(v & 0x80000000u) || (int16_t)(rd16(DECK_SWAY) - 0xFFFE) >= 0) return;
    wr16(0x981C, 0);
}

/* $03F0D4 / $03F0DE: bobbing 1-$11 */
static void deck_motion(void)
{
    deck_bob(0x11, 1);
    deck_sway();
}

/* $03F09E: bobbing $20-$30 */
static void deck_motion_high(void)
{
    deck_bob(0x30, 0x20);
    deck_sway();
}

/* $03F0B0: bobbing -$20 to -$10 */
static void deck_motion_low(void)
{
    deck_bob(0xFFF0, 0xFFE0);
    deck_sway();
}

/* $03F078: bobbing, the planes' vertical and horizontal scroll at it */
static void deck_motion_scroll(void)
{
    deck_motion();
    uint16_t v = (uint16_t)(rd16(DECK_Y) + 0x20);
    wr16(0xEC00, v);
    wr16(0xEC50, v);
    wr16(0xE400, (uint16_t)(sway() + 0x180));
}

/* $03F16C, $03F1A8: the lower deck's bob ($9828: 0-$10) and sway ($982C: -4 to 4) */
static void deck_lower_motion(void)
{
    if (!rd16(0x9832)) {
        wr32(0x9828, rd32(0x9828) + 0x3E00);
        if ((int16_t)(rd16(0x9828) - 0x10) >= 0) wr16(0x9832, 2);
    } else {
        wr32(0x9828, rd32(0x9828) - 0x3E00);
        if ((int16_t)rd16(0x9828) < 0) wr16(0x9832, 0);
    }
    if (!rd16(0x9830)) {
        uint32_t v = rd32(0x982C) + 0x4000;
        wr32(0x982C, v);
        if (!(v & 0x80000000u) && (int16_t)(rd16(0x982C) - 4) >= 0) wr16(0x9830, 2);
    } else {
        uint32_t v = rd32(0x982C) - 0x4000;
        wr32(0x982C, v);
        if ((v & 0x80000000u) && (int16_t)(rd16(0x982C) - 0xFFFC) < 0) wr16(0x9830, 0);
    }
}

/* $03F3DC: the tilt carried into the sway and the lower deck */
static void deck_tilt_apply(void)
{
    uint32_t t = rd32(DECK_TILT) << 1;
    wr32(DECK_SWAY, rd32(DECK_SWAY) - t);
    t <<= 3;
    wr32(0x982C, rd32(0x982C) + t);
}

/* $03F356: the next way of tilting ($9840) by chance, its time ($9842) */
static void deck_tilt_choose(void)
{
    uint16_t r = rd16(v_randomSeed), m = r & 0x0C;
    wr16(0x9840, m);
    if (m == 0) wr16(0x9842, (uint16_t)((r & 0x3F) + 0x80));
    else if (m == 4) wr16(0x9842, (uint16_t)((r & 0x7F) + 0x100));
    else if (m == 8) wr16(0x983E, bit8(DECK_SWAY, 7) ? 1 : 0);
    else {
        wr16(0x9840, 4);
        wr16(0x9842, (uint16_t)((r & 0x7F) + 0x100));
    }
    deck_tilt_apply();
}

/* $03F3D6: too far over: righting */
static void deck_tilt_right(void)
{
    wr16(0x9840, 0x10);
    deck_tilt_apply();
}

/* the tilt up (toward $B000) or down (toward -$B000) by `by` */
static void deck_tilt_up(uint32_t by)
{
    if ((int32_t)(rd32(DECK_TILT) - 0xB000) < 0) wr32(DECK_TILT, rd32(DECK_TILT) + by);
    deck_tilt_apply();
}

static void deck_tilt_down(uint32_t by)
{
    if ((int32_t)(rd32(DECK_TILT) - 0xFFFF5000u) >= 0) wr32(DECK_TILT, rd32(DECK_TILT) - by);
    deck_tilt_apply();
}

/* $03F20E: low, levelling out ($200 a frame) */
void DeckTiltLevel(void)
{
    deck_motion_low();
    wr16(0x9842, (uint16_t)(rd16(0x9842) - 1));
    if ((int16_t)rd16(0x9842) < 0) { deck_tilt_choose(); return; }
    wr32(DECK_TILT, rd32(DECK_TILT) & 0xFFFFFE00u);
    uint32_t t = rd32(DECK_TILT);
    if (t) wr32(DECK_TILT, t & 0x80000000u ? t + 0x200 : t - 0x200);
    deck_tilt_apply();
}

/* $03F24A: rocking (its way by chance each $20 frames), within sway +-$50 */
void DeckTiltRock(void)
{
    deck_motion();
    wr16(0x9842, (uint16_t)(rd16(0x9842) - 1));
    if ((int16_t)rd16(0x9842) < 0) { deck_tilt_choose(); return; }
    if ((int16_t)(sway() - 0x50) >= 0 || (int16_t)(sway() - 0xFFB0) < 0) { deck_tilt_right(); return; }
    if (!(rd16(v_gameFrames) & 0x1F)) wr16(0x983E, (uint16_t)(rd16(v_randomSeed) & 1));
    if (!rd16(0x983E)) deck_tilt_up(0xC00);
    else deck_tilt_down(0xC00);
}

/* $03F2BA: low, leaning one way to the sway's end ($983E) */
void DeckTiltLean(void)
{
    deck_motion_low();
    if (!rd16(0x983E)) {
        if ((int16_t)(sway() - 0xFFC0) < 0) { deck_tilt_right(); return; }
        deck_tilt_up(0x200);
    } else {
        if ((int16_t)(sway() - 0x40) >= 0) { deck_tilt_right(); return; }
        deck_tilt_down(0x200);
    }
}

/* $03F30A: righting toward the middle (+-$20): there, a new way */
void DeckTiltCentre(void)
{
    deck_motion();
    uint16_t s = sway();
    if ((int16_t)(s - 0x20) >= 0) deck_tilt_up(0x200);
    else if ((int16_t)(s - 0xFFE0) >= 0) deck_tilt_choose();
    else deck_tilt_down(0x200);
}

/* $03F1EC: the lower deck moving, the tilt by its way */
static void deck_tilt(void)
{
    deck_lower_motion();
    call68k(rom32(0x03F1FA + (uint32_t)(int16_t)rd16(0x9840)));
}

/* ---- the planes' lines drawing the deck ---- */

/* $03EC8E: plane A's lines (every $20 of $E460-$E77F): the deck's slope from its height */
static void deck_lines_a(void)
{
    uint32_t a = ((uint32_t)(int32_t)(int16_t)(uint16_t)-rd16(DECK_Y) << 2) + 0xFFFFE640u;
    a &= 0xFFFFFFE0u;
    uint16_t a0 = (uint16_t)a, a1 = a0;
    uint32_t d1 = rd32(DECK_TILT);
    uint32_t d0 = rd32(DECK_SWAY) + 0x1800000;
    do {
        d0 += d1;
        wr16(a0, (uint16_t)(d0 >> 16));
        a0 = (uint16_t)(a0 + 0x20);
    } while ((int16_t)(a0 - 0xE780) < 0);
    d0 = rd32(DECK_SWAY) + 0x1800000;
    do {
        wr16(a1, (uint16_t)(d0 >> 16));
        a1 = (uint16_t)(a1 - 0x20);
        d0 -= d1;
    } while ((int16_t)(a1 - 0xE460) >= 0);
}

/* $03ECEE: the columns' vertical scroll ($EC00-$EC4F) along the slope */
static void deck_columns(void)
{
    uint32_t a = ((uint32_t)((int32_t)(int16_t)sway() >> 2) & 0xFFFFFFFCu) + 0xFFFFEC28u;
    uint16_t a0 = (uint16_t)a, a1 = a0;
    uint32_t d1 = rd32(DECK_TILT) << 1;
    uint32_t d0 = rd32(DECK_Y) + 0x200000;
    do {
        wr16(a0, (uint16_t)(d0 >> 16));
        a0 = (uint16_t)(a0 + 4);
        d0 += d1;
    } while ((int16_t)(a0 - 0xEC50) < 0);
    d0 = rd32(DECK_Y) + 0x200000;
    do {
        wr16(a1, (uint16_t)(d0 >> 16));
        a1 = (uint16_t)(a1 - 4);
        d0 -= d1;
    } while ((int16_t)(a1 - 0xEBFE) >= 0);
}

/* $03EC7A */
static void deck_draw_upper(void)
{
    deck_columns();
    deck_lines_a();
}

/* $03ED6A, $03ED4A: the lower deck's columns ($EC02-$EC4E, plane B) and lines ($E782 up) */
static void deck_draw_lower(void)
{
    uint32_t d0 = rd32(0x9828) + 0x3C0000;
    uint32_t d1 = rd32(DECK_TILT) << 2;
    if (bit8(DECK_TILT, 7)) {
        for (uint16_t a = 0xEC02, k = 0; k < 20; k++, a = (uint16_t)(a + 4)) {
            d0 -= d1;
            wr16(a, (uint16_t)(d0 >> 16));
        }
    } else {
        for (uint16_t a = 0xEC4E, k = 0; k < 20; k++, a = (uint16_t)(a - 4)) {
            wr16(a, (uint16_t)(d0 >> 16));
            d0 += d1;
        }
    }
    d0 = rd32(0x982C);
    for (uint16_t a = 0xE782, k = 0; k < 26; k++, a = (uint16_t)(a - 0x20)) {
        wr16(a, (uint16_t)(d0 >> 16));
        d0 += d1;
    }
}

/* $03D6B4: the lines below the deck with plane A's sway, those below them with the camera */
static void deck_lines_flat(void)
{
    wr8(v_planeFlagsA, (uint8_t)(rd8(v_planeFlagsA) | 0x80));
    uint16_t a = 0xE460, v = (uint16_t)(sway() - 0x80);
    for (int k = 0; k <= 0xA8; k++, a = (uint16_t)(a + 4)) wr16(a, v);
    v = (uint16_t)-rd16(v_camX);
    for (int k = 0; k <= 0x20; k++, a = (uint16_t)(a + 4)) wr16(a, v);
}

/* $03EBF4: the deck's edge tiles (`count`+1 of them, $C428 on), flickering */
static void deck_edge(uint16_t count)
{
    uint16_t vram = 0xC428;
    uint32_t src = bit8(v_gameFrames + 1, 1) ? 0xFFFF5900 : 0xFFFF5800;
    do {
        ChunkDma(src, vram);
        src += 0x20;
        vram = (uint16_t)(vram + 8);
    } while (count-- != 0);
}

/* $03EBF0 */
static void deck_edge_all(void)
{
    deck_edge(7);
}

/* $03EBCC: as much of the edge as plane A's scroll ($E400) shows */
static void deck_edge_rising(void)
{
    uint16_t at = rd16(0xE400), from = 0xEA00, count = 7;
    for (int k = 0; k < 8; k++) {
        if ((int16_t)(at - from) < 0) break;
        from = (uint16_t)(from + 0x20);
        count = (uint16_t)(count - 1);
    }
    deck_edge(count);
}

/* $03EC26: the edge's tiles cleared */
static void deck_edge_clear(void)
{
    for (uint16_t vram = 0xC428, k = 0; k < 8; k++, vram = (uint16_t)(vram + 8)) ChunkDma(0xFFFF4000, vram);
}

/* $03EC42, $03EC54: the lower deck's tile (at $E618 its own look while the sway is -$5A-$E1) */
static void deck_lower_tile_fixed(void)
{
    PlaneAChunk(0xFFFF3000, 0xE658);
}

static void deck_lower_tile(void)
{
    int16_t s = (int16_t)rd16(0x982C);
    if ((int16_t)(s - (int16_t)0xFFA6) < 0 || (int16_t)(s - 0xE2) >= 0) PlaneAChunk(0xFFFF3000, 0xE618);
    else PlaneAChunk(0xFFFF3020, 0xE618);
}

/* $03F3EE: the H-interrupt's line counts by the columns' scroll */
static void deck_hint_lines(void)
{
    int16_t d0 = (int16_t)((rd16(0xEC00) & 0x1FF) - 0x20);
    if (d0 < 0) d0 = 0;
    uint16_t d1 = (uint16_t)(0xBF - d0);
    wr16(0x801C, (uint16_t)(d1 | 0x8A00));
    wr16(0x801E, (uint16_t)((uint16_t)d0 | 0x8A00));
}

/* $03DCC2: the background's two planes' y wobbling */
static void deck_wobble(void)
{
    wr16(0x9860, (uint16_t)(rd16(0x9860) + 1));
    uint16_t d = (uint16_t)((rd16(0x9860) & 0x78) >> 3);
    if (bit8(0x9861, 7)) d = (uint16_t)((d ^ 0x0F) + 1);
    d = (uint16_t)(d + 0x210);
    wr16(0xA90C, d);
    wr16(0xA92C, d);
}

/* ---- things standing on the deck ---- */

/* $03EF6C: `obj` at x on the slope (its y +$14, 16.16) */
static void deck_stand(uint16_t obj, uint16_t x)
{
    wr16(obj + O_X, x);
    int16_t d0 = (int16_t)(x - 0x120 - sway());
    int16_t t = (int16_t)(uint16_t)((int32_t)rd32(DECK_TILT) >> 4);
    uint32_t p = (uint32_t)((int32_t)d0 * t) << 1;
    wr32(obj + O_Y, 0x1140000u - rd32(DECK_Y) - p);
}

/* $03EF44: a player's marker carried with the deck, kept within $A0-$1A0 of the sway */
static void deck_marker(uint16_t mark, uint16_t player, uint32_t dx, uint32_t dy)
{
    wr32(mark + 0x18, dx);
    wr32(mark + 0x1C, dy);
    uint16_t x = rd16(player + O_X);
    uint16_t hi = (uint16_t)(0x1A0 + sway()), lo = (uint16_t)(0xA0 + sway());
    if ((int16_t)(x - hi) >= 0) x = hi;
    else if ((int16_t)(x - lo) < 0) x = lo;
    deck_stand(mark, x);
}

/* $03EEB6: a thrown thing carried along: off the deck's span, gone (1) */
static int deck_carry(uint16_t thing, uint32_t dx)
{
    wr32(thing + O_X, rd32(thing + O_X) + dx);
    uint16_t x = rd16(thing + O_X);
    if ((int16_t)(x - (uint16_t)(0x1A0 + sway())) >= 0 || (int16_t)(x - (uint16_t)(0xA0 + sway())) < 0) {
        wr16(thing + O_FLAGS, 0x1000);
        return 1;
    }
    deck_stand(thing, x);
    return 0;
}

/* $03EEEC: every 8th frame (`tick` 0) a blast ($104) off the thing */
static void deck_carry_blast(uint16_t thing, uint16_t tick)
{
    if (tick) return;
    QueueSound(0xCA);
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x104);
    wr32(slot + 0x48, 0x05663A);
    wr16(slot + O_X, rd16(thing + O_X));
    wr16(slot + O_Y, rd16(thing + O_Y));
    wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) - 4));
    wr32(slot + 0x1C, 0xFFFA0000);
    wr8(slot + 0x21, 0x10);
    wr32(slot + 0x2C, 0xF808F808);
    wr16(slot + 0x26, 0x0E);
}

/* $03EE7E: the thing it threw (+$4C bit 10, in $9858) carried and blasting */
static void deck_thrown(uint16_t obj, uint32_t dx)
{
    if (!bit8(obj + 0x4C, 2)) return;
    uint16_t tick = rd16(v_gameFrames) & 7;
    uint16_t thing = rd16(0x9858);
    if (thing) {
        deck_carry_blast(thing, tick);
        if (!deck_carry(thing, dx)) return;
        wr16(0x9858, 0);
    }
    wr8(obj + 0x4C, (uint8_t)(rd8(obj + 0x4C) & ~4));
}

/* $03EFA2: the brawler on the slope (+$48 its floor); held (+$4C bit 8) swaying about the
 * middle; else carried (+$4D bit 1), kept on the deck (+$4D bit 0: at its edge) */
static void deck_brawler(uint16_t obj, uint32_t dx)
{
    uint16_t x;
    uint32_t base;
    if (bit8(obj + 0x4C, 0)) {
        x = rd16(obj + O_X);
        wr8(obj + 0x4D, (uint8_t)(rd8(obj + 0x4D) & ~1));
        if (bit8(obj + 0x4C, 1)) {
            uint16_t c = rom16(0x0297FC + ((uint16_t)(rd16(v_gameFrames) << 5) & 0x1FE));
            x = (uint16_t)((uint16_t)((int16_t)c >> 7) + 0x120 + sway());
            wr16(obj + O_X, x);
        }
        base = 0xD00000;
    } else {
        if (bit8(obj + 0x4D, 1)) wr32(obj + O_X, rd32(obj + O_X) + dx);
        wr8(obj + 0x4D, (uint8_t)(rd8(obj + 0x4D) & ~1));
        x = rd16(obj + O_X);
        if ((int16_t)(x - (uint16_t)(0x1A0 + sway())) >= 0 || (int16_t)(x - (uint16_t)(0xA0 + sway())) < 0)
            wr8(obj + 0x4D, (uint8_t)(rd8(obj + 0x4D) | 1));
        base = 0x1100000;
    }
    int16_t d0 = (int16_t)(x - 0x120 - sway());
    int16_t t = (int16_t)(uint16_t)((int32_t)rd32(DECK_TILT) >> 4);
    uint32_t p = (uint32_t)((int32_t)d0 * t) << 1;
    wr32(obj + 0x48, base - rd32(DECK_Y) - p + 0xFFEC0000u);
}

/* $03EE10: the deck's motion this frame ($9834, $9838) carrying the players' markers ($9814,
 * $9816), the brawler and what it threw */
static void deck_track(uint16_t obj)
{
    uint32_t dx = rd32(DECK_SWAY) - rd32(0x980C);
    wr32(0x9834, dx);
    wr32(0x980C, rd32(DECK_SWAY));
    uint32_t dy = rd32(0x9808) - rd32(DECK_Y);
    wr32(0x9838, dy);
    wr32(0x9808, rd32(DECK_Y));
    if (bit8(v_players, 0)) deck_marker(rd16(0x9814), 0xA400, dx, dy);
    else wr16(rd16(0x9814) + O_Y, 0);
    if (bit8(v_players, 1)) {
        deck_marker(rd16(0x9816), 0xA600, dx, dy);
        deck_thrown(obj, dx);
    } else {
        wr16(rd16(0x9816) + O_Y, 0);
    }
    deck_brawler(obj, dx);
    deck_thrown(obj, dx);
}

/* ---- the brawler of the deck: its moves by +$56 ($3DE50); +$4C/+$4D its state bits (+$4D
 * bit 0: at the deck's edge, bit 6: right of the middle), +$40 the player it holds ---- */

static void fight_step(uint16_t obj, uint16_t to)
{
    wr16(obj + 0x56, to);
}

static void fight_next(uint16_t obj, uint16_t by)
{
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) + by));
}

static void look(uint16_t obj, uint32_t map)
{
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x08, map);
}

static void on_floor(uint16_t obj)
{
    wr16(obj + O_Y, rd16(obj + 0x48));
}

static int count_down(uint16_t obj, uint16_t at)
{
    wr16(obj + at, (uint16_t)(rd16(obj + at) - 1));
    return (int16_t)rd16(obj + at) < 0;
}

static void flag4C(uint16_t obj, uint16_t set, uint16_t keep)
{
    wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) | set));
    wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) & keep));
}

/* $03E9CA: which half of the deck it is on (+$4D bit 6: the right) */
static void fight_side(uint16_t obj)
{
    if ((int16_t)((uint16_t)(rd16(obj + O_X) - sway()) - 0x120) >= 0) wr8(obj + 0x4D, (uint8_t)(rd8(obj + 0x4D) | 0x40));
    else wr8(obj + 0x4D, (uint8_t)(rd8(obj + 0x4D) & ~0x40));
}

/* $03E9F4: put back inside the deck's edge on its side */
static void fight_inside(uint16_t obj)
{
    fight_side(obj);
    wr16(obj + O_X, (uint16_t)((bit8(obj + 0x4D, 6) ? 0x19E : 0xA2) + sway()));
}

/* $03E9EA: at the edge: back inside */
static void fight_edge(uint16_t obj)
{
    if (bit8(obj + 0x4D, 0)) fight_inside(obj);
}

/* loc_03EA1E: fallen off the deck: down past $180, its fall ($1C) */
static void fight_fell(uint16_t obj)
{
    if (bit8(obj + 0x1C, 7) || (int16_t)(rd16(obj + O_Y) - 0x180) < 0) return;
    fight_step(obj, 0x1C);
    wr16(obj + 0x50, 0x20);
    wr16(obj + 0x5A, 0x7FFF);
    wr16(obj + 0x5C, 8);
    wr8(obj + 0x23, (uint8_t)(rd8(obj + 0x23) | 4));
}

/* $03EB5C: a count of `n` plus the difficulty's */
static void fight_count(uint16_t obj, uint16_t n)
{
    wr16(obj + 0x50, (uint16_t)(n + rom16(0x03EB72 + (uint32_t)(int16_t)rd16(0xA20C))));
}

/* $03EA88: its grab box */
static void fight_grab_box(uint16_t obj)
{
    wr8(obj + 0x21, 0x23);
    wr32(obj + 0x2C, 0xEC0CE818);
    wr16(obj + 0x40, 0);
}

/* $03EAD6 */
static void fight_body_box(uint16_t obj)
{
    wr8(obj + 0x21, 0x22);
}

/* $03EA9E: a player caught (+$40): held still (the Z flag: 1) */
static int fight_caught(uint16_t obj)
{
    uint16_t p = rd16(obj + 0x40);
    if (p != 0xA400 && p != 0xA600) return 0;
    wr8(p + 0x23, (uint8_t)(rd8(p + 0x23) | 2));
    wr32(p + 0x18, 0);
    wr32(p + 0x1C, 0);
    return 1;
}

/* $03EADE: the held player moved along `table` (count, dx, dy; a negative count ends): 1 while
 * held (the Z flag) */
static int fight_hold(uint16_t obj, uint32_t table)
{
    uint16_t p = rd16(obj + 0x40);
    if (p & 0x8000) wr8(p + 0x23, (uint8_t)(rd8(p + 0x23) | 2));   /* (0: the ROM's, unwritten) */
    if (!count_down(obj, 0x50)) return 1;
    uint16_t i = rd16(obj + 0x52);
    wr16(obj + 0x50, rom16(table + (uint32_t)(int16_t)i));
    if (rd16(obj + 0x50) & 0x8000) return 0;
    uint16_t dx = rom16(table + (uint32_t)(int16_t)(uint16_t)(i + 2));
    uint16_t dy = rom16(table + (uint32_t)(int16_t)(uint16_t)(i + 4));
    if (bit8(obj + 0x0E, 3)) dx = (uint16_t)-dx;
    wr16(p + O_X, (uint16_t)(dx + rd16(obj + O_X)));
    wr16(p + O_Y, (uint16_t)(dy + rd16(obj + O_Y)));
    wr16(obj + 0x52, (uint16_t)(rd16(obj + 0x52) + 6));
    return 1;
}

/* the held player let go, flung (vy, vx toward its facing) */
static void fight_fling(uint16_t obj, uint32_t vy, uint32_t vx)
{
    fight_body_box(obj);
    wr8(obj + 0x21, (uint8_t)(rd8(obj + 0x21) & ~1));
    uint16_t p = rd16(obj + 0x40);
    wr16(obj + 0x40, 0);
    wr8(obj + 0x22, 0);
    wr8(p + 0x23, (uint8_t)(rd8(p + 0x23) & ~2));
    wr8(p + 0x23, (uint8_t)(rd8(p + 0x23) | 0x10));
    wr32(p + 0x1C, vy);
    wr32(p + 0x18, bit8(obj + 0x0E, 3) ? (uint32_t)-vx : vx);
}

/* $03F466: a part ($10) at its x */
static uint16_t fight_part(uint16_t obj)
{
    uint16_t slot = 0, t = 0, d1 = 0;
    uint32_t a1 = 0;
    SlotFreeOrPuff(&slot, &t, &d1, &a1);
    wr16(slot + O_TYPE, 0x10);
    wr16(slot + O_FLAGS, 0x0800);
    wr16(slot + O_X, rd16(obj + O_X));
    return slot;
}

/* $03F432: a thing thrown ($9858) */
static void fight_throw_thing(uint16_t obj)
{
    wr32(0x9856, 0);
    uint16_t t = fight_part(obj);
    if (!t) return;
    wr16(0x9858, t);
    wr32(t + 0x18, bit8(obj + 0x0E, 3) ? 0xFFFE8000 : 0x18000);
}

/* $03F47E: five bombs ($2CC) thrown out in a fan */
static void fight_bombs(uint16_t obj)
{
    QueueSound(0xCE);
    uint16_t a = 0x20;
    for (int k = 0; k < 5; k++, a = (uint16_t)(a + 0x30)) {
        uint16_t slot, t;
        if (!SlotFreeHigh(&slot, &t)) return;
        wr16(slot + O_TYPE, 0x2CC);
        wr32(slot + 0x48, 0x05663A);
        wr16(slot + O_X, rd16(obj + O_X));
        wr16(slot + O_Y, rd16(obj + O_Y));
        wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) + 0x10));
        uint16_t s = rom16(0x02977C + a), c = rom16(0x0297FC + a);
        wr32(slot + 0x1C, (uint32_t)(int32_t)(int16_t)s << 5);
        wr32(slot + 0x18, (uint32_t)(int32_t)(int16_t)c << 3);
        wr32(slot + 0x5C, (uint32_t)(int32_t)(int16_t)(uint16_t)-s);
        wr32(slot + 0x58, (uint32_t)(int32_t)(int16_t)(uint16_t)-c);
        wr8(slot + 0x21, 0x10);
        wr32(slot + 0x2C, 0xF808F808);
        wr16(slot + 0x26, 9);
    }
}

/* $03E854: what next, by its player's distance (and the frame's bit 10: near or far measures) */
static void fight_decide(uint16_t obj)
{
    uint16_t dx, dy, p;
    uint16_t d = NearestPlayer(obj, &dx, &dy, &p);
    int close;
    if (bit8(v_gameFrames, 2)) {
        if ((int16_t)(d - 0x78) >= 0) {
            if (rd16(v_randomSeed) & 7) return;
            fight_step(obj, 0x10);
            wr32(obj + 0x18, 0);
            wr16(obj + 0x58, 8);
            fight_count(obj, 0x12);
            look(obj, 0x0C007E);
            return;
        }
        if ((int16_t)(d - 0x18) >= 0) {
            if (rd16(v_gameFrames) & 0x7F) return;
            fight_step(obj, 0x34);
            wr16(obj + 0x4C, 0x22);
            wr32(obj + 0x1C, 0xFFFA0000);
            return;
        }
        close = 1;
    } else {
        if ((int16_t)(d - 0x90) >= 0) {
            if (!rd16(v_difficulty) || bit8(obj + 0x4C, 2)) return;
            fight_step(obj, 0x58);
            look(obj, 0x0BFF5E);
            wr16(obj + 0x50, 0x20);
            wr16(obj + 0x58, 8);
            wr32(obj + 0x18, 0);
            return;
        }
        if ((int16_t)(d - 0x24) >= 0) {
            if (rd16(v_gameFrames) & 0x3F) return;
            fight_step(obj, 0x48);
            wr32(obj + 0x18, 0);
            fight_count(obj, 0x10);
            wr16(obj + 0x58, 8);
            look(obj, 0x0C0022);
            fight_side(obj);
            wr32(obj + 0x18, bit8(obj + 0x4D, 6) ? 0xFFFE8000 : 0x18000);
            return;
        }
        close = 1;
    }
    if (close) {
        if ((int16_t)dy < 0 || (int16_t)(dy - 0x10) < 0) {
            fight_grab_box(obj);
            fight_step(obj, 0x24);
            wr16(obj + 0x50, 0x20);
            return;
        }
        if (bit8(obj + 0x4C, 2)) return;
        fight_step(obj, 0x5C);
        look(obj, 0x0BFD56);
        wr16(obj + 0x50, 0x20);
        wr16(obj + 0x58, 8);
        wr32(obj + 0x18, 0);
    }
}

/* $03DEC0: the fight ($95F its vitality) */
void DeckFightStart(uint16_t obj)
{
    fight_next(obj, 4);
    wr8(obj + 0x20, 0x10);
    wr16(obj + 0x4C, 0x22);
    look(obj, 0x0C0022);
    wr16(obj + 0x50, 0x28);
    wr16(obj + 0x5C, 7);
    wr8(obj + 0x21, 0x22);
    wr8(obj + 0x23, 0x20);
    wr32(obj + 0x2C, 0xEC08F010);
    wr32(obj + 0x28, 0xF410F010);
    wr16(obj + O_VITALITY, 0x95F);
    wr16(0x80C2, 0x10);
    wr16(0x80D6, 0xA800);
}

/* $03DF1A: standing, then facing its player */
void DeckFightStand(uint16_t obj)
{
    fight_edge(obj);
    wr8(obj + 0x21, 0x22);
    wr32(obj + 0x18, 0);
    flag4C(obj, 0x22, 0xFCFF);
    on_floor(obj);
    if (!count_down(obj, 0x50)) return;
    uint16_t dx, dy, p;
    NearestPlayer(obj, &dx, &dy, &p);
    if (dx & 0x8000) wr16(obj + 0x0E, (uint16_t)(rd16(obj + 0x0E) | 0x800));
    else wr16(obj + 0x0E, rd16(obj + 0x0E) & 0xF7FF);
    fight_next(obj, 4);
    look(obj, 0x0BFB62);
}

/* $03DF76: walking (at an end, standing again), deciding */
void DeckFightWalk(uint16_t obj)
{
    on_floor(obj);
    wr32(obj + 0x18, bit8(obj + 0x0E, 3) ? 0xFFFEF000 : 0x11000);
    if (bit8(obj + 0x4D, 2) || bit8(obj + 0x4D, 3)) {
        wr16(obj + 0x50, 0);
    } else if (bit8(obj + 0x4D, 0)) {
        fight_inside(obj);
        wr16(obj + 0x50, 0x40);
    } else {
        fight_decide(obj);
        return;
    }
    fight_next(obj, (uint16_t)-4);
    look(obj, 0x0BFB06);
}

/* $03DFF0: staggered a while */
void DeckFightStagger(uint16_t obj)
{
    on_floor(obj);
    wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 1));
    if (rd16(obj + 0x58) && !(rd16(obj + 0x58) & 0x8000)) return;
    fight_step(obj, 4);
    wr16(obj + 0x50, 0xFFFF);
    look(obj, 0x0BFB06);
}

/* $03E066: winding up; then charging at its player (its afterimages on) */
void DeckFightWindUp(uint16_t obj)
{
    on_floor(obj);
    if (!count_down(obj, 0x50)) return;
    QueueSound(0xB9);
    fight_next(obj, 4);
    wr16(obj + 0x26, 0x12);
    wr8(obj + 0x21, 0x30);
    wr32(obj + 0x2C, 0xF208F40C);
    wr16(obj + 0x50, 0x10);
    look(obj, 0x0BFBF6);
    wr16(0x983C, 1);
    uint16_t dx, dy, p;
    NearestPlayer(obj, &dx, &dy, &p);
    if (dx & 0x8000) {
        wr16(obj + 0x0E, (uint16_t)(rd16(obj + 0x0E) | 0x800));
        wr32(obj + 0x18, 0xFFFA0000);
        return;
    }
    wr16(obj + 0x0E, rd16(obj + 0x0E) & 0xF7FF);
    wr32(obj + 0x18, 0x60000);
}

/* $03E0FC: stopped: standing again */
static void fight_stop(uint16_t obj)
{
    wr16(0x983C, 0);
    wr16(obj + 0x50, 0x40);
    wr32(obj + 0x18, 0);
    wr8(obj + 0x21, 0x22);
    fight_step(obj, 4);
    wr16(obj + 0x58, 0);
    look(obj, 0x0BFB06);
    fight_edge(obj);
}

/* $03E0DC: charging till its count is out and it hits something (+$4C bits 0, 2, 3) */
void DeckFightCharge(uint16_t obj)
{
    on_floor(obj);
    if (!count_down(obj, 0x50) || !(rd16(obj + 0x4C) & 0x0D)) {
        AfterImage(obj);
        return;
    }
    fight_stop(obj);
}

/* $03E01E: falling to its floor */
void DeckFightFall(uint16_t obj)
{
    uint16_t y = rd16(obj + O_Y);
    uint32_t v = rd32(obj + 0x1C) + 0x4000;
    wr32(obj + 0x1C, v);
    if (v & 0x80000000u) {
        if ((int16_t)(y - rd16(obj + 0x48)) >= 0) wr16(obj + O_Y, rd16(obj + 0x48));
        return;
    }
    if ((int16_t)(y - rd16(obj + 0x48)) < 0) return;
    if (bit8(obj + 0x4D, 0)) { fight_fell(obj); return; }
    on_floor(obj);
    fight_step(obj, 4);
    wr32(obj + 0x1C, 0);
}

/* $03E134: fallen off: a while, then thrown back up from below ($28 off its vitality) */
void DeckFightFallen(uint16_t obj)
{
    if (!count_down(obj, 0x50)) return;
    fight_next(obj, 4);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0xFFF60000);
    wr16(obj + O_Y, 0x180);
    wr16(obj + O_X, 0x120);
    look(obj, 0x0BFC2E);
    wr8(obj + 0x21, 0x22);
    wr16(obj + O_VITALITY, (uint16_t)(rd16(obj + O_VITALITY) - 0x28));
    QueueSound(0xE1);
}

/* $03E182: up and over onto the deck, steering to its middle */
void DeckFightClimb(uint16_t obj)
{
    uint16_t y = rd16(obj + O_Y);
    uint32_t v = rd32(obj + 0x1C) + 0x4000;
    wr32(obj + 0x1C, v);
    if (v & 0x80000000u) {
        fight_side(obj);
        if (bit8(obj + 0x4D, 6)) {
            wr16(obj + 0x0E, (uint16_t)(rd16(obj + 0x0E) | 0x800));
            wr32(obj + 0x18, rd32(obj + 0x18) - 0x1000);
        } else {
            wr16(obj + 0x0E, rd16(obj + 0x0E) & 0xF7FF);
            wr32(obj + 0x18, rd32(obj + 0x18) + 0x1000);
        }
        return;
    }
    if ((int16_t)(y - rd16(obj + 0x48)) < 0) return;
    if (bit8(obj + 0x4D, 0)) { fight_fell(obj); return; }
    on_floor(obj);
    wr32(obj + 0x1C, 0);
    fight_step(obj, 4);
    wr16(obj + 0x58, 0);
    wr16(obj + 0x5A, 0x3C);
}

/* $03E1F4: reaching to grab: a player caught: a throw ($28) or, one time in four, a slam ($2C) */
void DeckFightGrab(uint16_t obj)
{
    if (rd16(obj + 0x4C) & 0x0D) { fight_stop(obj); return; }
    on_floor(obj);
    if (count_down(obj, 0x50)) {
        fight_step(obj, 4);
        fight_body_box(obj);
        return;
    }
    if (!fight_caught(obj)) return;
    QueueSound(0xE1);
    wr8(obj + 0x21, 0);
    wr32(obj + 0x18, 0);
    wr16(obj + 0x50, 0);
    wr16(obj + 0x52, 0);
    if ((int16_t)((rd16(v_gameFrames) & 7) - 6) >= 0) {
        fight_step(obj, 0x2C);
        look(obj, 0x0BFDE6);
        return;
    }
    fight_step(obj, 0x28);
    look(obj, 0x0BFE76);
}

/* $03E280: the throw ($3EB3A): then the player flung */
void DeckFightThrow(uint16_t obj)
{
    on_floor(obj);
    if (fight_hold(obj, 0x03EB3A)) return;
    fight_next(obj, 8);
    wr16(obj + 0x50, 0x20);
    fight_fling(obj, 0xFFFCC000, 0x8000);
}

/* $03E2EA: the slam ($3EB4E): flung harder */
void DeckFightSlam(uint16_t obj)
{
    on_floor(obj);
    if (fight_hold(obj, 0x03EB4E)) return;
    fight_next(obj, 4);
    wr16(obj + 0x50, 0x20);
    fight_fling(obj, 0xFFF98000, 0x40000);
}

/* $03E354: a pause, carried on the deck */
void DeckFightPause(uint16_t obj)
{
    on_floor(obj);
    wr8(obj + 0x4D, (uint8_t)(rd8(obj + 0x4D) | 2));
    wr8(obj + 0x4D, (uint8_t)(rd8(obj + 0x4D) | 0x20));
    wr8(obj + 0x4C, (uint8_t)(rd8(obj + 0x4C) & ~1));
    wr8(obj + 0x4C, (uint8_t)(rd8(obj + 0x4C) & ~2));
    wr32(obj + 0x18, 0);
    if (!count_down(obj, 0x50)) return;
    fight_step(obj, 4);
    wr16(obj + O_FLAGS, 0xEC00);
    look(obj, 0x0BFB06);
}

/* $03E39E: jumping: at the top, mostly a dive kick */
void DeckFightJump(uint16_t obj)
{
    uint32_t v = rd32(obj + 0x1C) + 0x4000;
    wr32(obj + 0x1C, v);
    if (v & 0x80000000u) return;
    fight_next(obj, 4);
    if (!(rd16(v_randomSeed) & 3)) return;
    QueueSound(0xE1);
    wr8(obj + 0x21, 0x32);
    wr32(obj + 0x2C, 0xF40CFC0E);
    wr16(obj + 0x26, 0x0A);
    wr16(obj + 0x58, 8);
    look(obj, 0x0BFC66);
    if (!bit8(obj + 0x0E, 3)) wr32(obj + 0x18, rd32(obj + 0x18) + 0x20000);
    else wr32(obj + 0x18, rd32(obj + 0x18) - 0x20000);
}

/* $03E40A: coming down: landed (or off the deck) */
void DeckFightLand(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    uint16_t d = (uint16_t)(rd16(obj + O_Y) - rd16(obj + 0x48));
    if ((int16_t)d < 0) return;
    if ((int16_t)(d - 0x0C) >= 0 || bit8(obj + 0x4D, 0)) { fight_fell(obj); return; }
    on_floor(obj);
    wr32(obj + 0x1C, 0);
    fight_count(obj, 8);
    fight_step(obj, 0x30);
    wr16(obj + 0x58, 0);
    wr8(obj + 0x21, 0x22);
    wr16(obj + O_FLAGS, 0xCC00);
    wr32(obj + 0x08, 0x0BFF2E);
}

/* $03E71A: beaten: what it threw gone, still, flashing */
void DeckFightBeaten(uint16_t obj)
{
    if (rd16(0x9856)) wr16(rd16(0x9856) + O_FLAGS, 0x1000);
    if (rd16(0x9858)) wr16(rd16(0x9858) + O_FLAGS, 0x1000);
    fight_next(obj, 4);
    wr16(obj + 0x5A, 0xFFFF);
    wr16(obj + 0x4C, 0);
    wr8(obj + 0x21, 0);
    wr16(obj + O_VITALITY, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x0C, 0);
    wr16(obj + O_FLAGS, 0xEC00);
    wr16(obj + 0x50, 0x10);
    wr16(obj + 0x52, rd16(obj + O_Y));
    wr32(obj + 0x08, bit8(obj + 0x4D, 0) ? 0x0BFCC6 : 0x0BFC96);
}

/* $03E792: shaking, flashing; then thrown up off the deck */
void DeckFightDying(uint16_t obj)
{
    if (count_down(obj, 0x50)) {
        QueueSound(0xA3);
        fight_next(obj, 4);
        wr16(obj + O_FLAGS, 0xCC00);
        wr32(obj + 0x08, 0x0BFEDE);
        wr32(obj + 0x1C, 0xFFF90000);
        wr32(obj + 0x18, (int16_t)(rd16(obj + O_X) - 0x120) >= 0 ? 0xFFFE0000 : 0x20000);
        return;
    }
    uint16_t y = rd16(obj + 0x52);
    if (!bit8(obj + 0x51, 2)) y = (uint16_t)(y - 4);
    wr16(obj + O_Y, y);
    if (!bit8(obj + 0x51, 0)) wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) & 0x7FFF);
    else wr16(obj + O_FLAGS, (uint16_t)(rd16(obj + O_FLAGS) | 0x8000));
}

/* $03E80A: falling away, smoking: below $180, the deck's end (the boss's next step) */
void DeckFightFallAway(uint16_t obj)
{
    PuffTrail(obj);
    uint32_t v = rd32(obj + 0x1C) + 0x4400;
    wr32(obj + 0x1C, v);
    if (v & 0x80000000u) return;
    wr8(obj + 0x0E, (uint8_t)(rd8(obj + 0x0E) | 0x10));
    if ((int16_t)(rd16(obj + O_Y) - 0x180) < 0) return;
    wr16(obj + O_FLAGS, 0);
    wr16(obj + 0x4E, 0x60);
    wr16(obj + 0x50, 0x18);
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(0x80C2, 0x2C);
    wr8(0xA4C0, (uint8_t)(rd8(0xA4C0) | 0x80));
    wr8(0xA6C0, (uint8_t)(rd8(0xA6C0) | 0x80));
}

/* $03E4BE: crouching to spring */
void DeckFightCrouch(uint16_t obj)
{
    on_floor(obj);
    wr32(obj + 0x18, 0);
    if (!count_down(obj, 0x50)) return;
    fight_next(obj, 4);
    wr32(obj + 0x1C, 0xFFFA2000);
    flag4C(obj, 0x102, 0xFDDF);
    look(obj, 0x0BFC2E);
}

/* $03E4FC: springing up */
void DeckFightSpring(uint16_t obj)
{
    uint32_t v = rd32(obj + 0x1C) + 0x4000;
    wr32(obj + 0x1C, v);
    if (v & 0x80000000u) return;
    fight_next(obj, 4);
    wr16(obj + O_FLAGS, 0xCC00);
    wr32(obj + 0x08, 0x0BFCFE);
}

/* $03E51C: down onto the deck: spinning $80 frames */
void DeckFightLandSpin(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    if ((int16_t)(rd16(obj + O_Y) - rd16(obj + 0x48)) < 0) return;
    if (bit8(obj + 0x4D, 0)) { fight_fell(obj); return; }
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr8(obj + 0x21, 0x30);
    wr32(obj + 0x2C, 0xF410E020);
    wr16(obj + 0x26, 5);
    flag4C(obj, 0x300, 0xFFDD);
    wr16(obj + 0x50, 0x80);
    fight_next(obj, 4);
}

/* $03E576: spinning (a whirr every other frame); then leaping at its player's x */
void DeckFightSpin(uint16_t obj)
{
    if (count_down(obj, 0x50)) {
        uint16_t p = PlayerTarget();
        wr16(obj + O_X, rd16(p + O_X));
        fight_step(obj, 0x38);
        wr16(obj + O_FLAGS, 0xEC00);
        wr8(obj + 0x21, 0x30);
        wr32(obj + 0x2C, 0xF40CFC0E);
        wr16(obj + 0x26, 0x0A);
        look(obj, 0x0BFC66);
        wr32(obj + 0x1C, 0xFFFE4000);
        flag4C(obj, 0x20, 0xFCFD);
        return;
    }
    if (!bit8(v_gameFrames + 1, 0)) QueueSound(0xCE);
    on_floor(obj);
    wr16(obj + O_FLAGS, 0xCC00);
    uint16_t a = (uint16_t)(rd16(v_gameFrames) << 5) & 0x1FE;
    int right;
    uint32_t map;
    if ((int16_t)(a - 0x180) >= 0) { right = 0; map = 0x0BFCFE; }
    else if ((int16_t)(a - 0x100) >= 0) { right = 0; map = 0x0BFD2E; }
    else if ((int16_t)(a - 0x80) >= 0) { right = 1; map = 0x0BFCFE; }
    else { right = 1; map = 0x0BFD2E; }
    if (right) wr8(obj + 0x0E, (uint8_t)(rd8(obj + 0x0E) | 8));
    else wr8(obj + 0x0E, (uint8_t)(rd8(obj + 0x0E) & ~8));
    wr32(obj + 0x08, map);
}

/* $03E468: a thing thrown at its player */
void DeckFightToss(uint16_t obj)
{
    on_floor(obj);
    if (!count_down(obj, 0x50)) return;
    fight_step(obj, 0x30);
    fight_throw_thing(obj);
    wr8(obj + 0x4C, (uint8_t)(rd8(obj + 0x4C) | 4));
    fight_count(obj, 0x28);
    wr16(obj + 0x58, 0);
}

/* $03E496: bombs thrown out */
void DeckFightBombs(uint16_t obj)
{
    on_floor(obj);
    if (!count_down(obj, 0x50)) return;
    fight_step(obj, 0x30);
    fight_bombs(obj);
    fight_count(obj, 0x1E);
    wr16(obj + 0x58, 0);
}

/* $03E650: its hold broken (+$23 bit 1): knocked back */
void DeckFightBroken(uint16_t obj)
{
    uint8_t b = rd8(obj + 0x23);
    wr8(obj + 0x23, (uint8_t)(b & ~2));
    if (b & 2) return;
    fight_next(obj, 4);
    look(obj, 0x0BFCC6);
}

/* $03E66E: knocked back: landing, bouncing once ($1A4 off its vitality) */
void DeckFightKnocked(uint16_t obj)
{
    if (bit8(obj + 0x18, 7)) wr8(obj + 0x0E, (uint8_t)(rd8(obj + 0x0E) & ~8));
    else wr8(obj + 0x0E, (uint8_t)(rd8(obj + 0x0E) | 8));
    uint32_t v = rd32(obj + 0x1C) + 0x4000;
    wr32(obj + 0x1C, v);
    if (v & 0x80000000u) return;
    uint16_t d = (uint16_t)(rd16(obj + O_Y) - rd16(obj + 0x48));
    if ((int16_t)d < 0) return;
    if ((int16_t)(d - 0x0C) >= 0 || bit8(obj + 0x4D, 0)) { fight_fell(obj); return; }
    on_floor(obj);
    if (!rd16(obj + 0x5A)) {
        wr16(obj + 0x5A, 0x40);
        wr32(obj + 0x1C, 0xFFFD4000);
        wr32(obj + 0x18, (uint32_t)((int32_t)rd32(obj + 0x18) >> 2));
        wr16(obj + O_FLAGS, 0xEC00);
        look(obj, 0x0BFC96);
        wr16(obj + O_VITALITY, (uint16_t)(rd16(obj + O_VITALITY) - 0x1A4));
        return;
    }
    wr16(obj + 0x50, 0x10);
    fight_step(obj, 0x30);
    wr16(obj + 0x58, 0);
    wr8(obj + 0x21, 0x22);
    flag4C(obj, 0x22, 0xFCFF);
}

/* $03DD28: its move: beaten ($3C), its hold broken ($60), hit (staggered, or after 7 hits
 * knocked back: $18), or as it was */
static void fight_move(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) & 0x8000) {
        fight_step(obj, 0x3C);
    } else if (bit8(obj + 0x23, 1)) {
        wr8(obj + 0x23, (uint8_t)(rd8(obj + 0x23) & ~2));
        fight_step(obj, 0x60);
        wr8(obj + 0x21, 0);
        wr16(obj + O_FLAGS, 0xEC00);
        look(obj, 0x0BFCC6);
        flag4C(obj, 0x22, 0xFCFF);
    } else if ((!rd16(obj + 0x58) || (rd16(obj + 0x58) & 0x8000)) && bit8(obj + 0x22, 5)) {
        wr8(obj + 0x21, 0x22);
        wr16(obj + 0x40, 0);
        int knock = bit8(obj + 0x23, 4);
        if (!knock) knock = count_down(obj, 0x5C);
        if (!knock) {
            wr32(obj + 0x18, 0);
            wr16(obj + 0x58, 3);
            fight_step(obj, 0x0C);
            wr16(obj + O_FLAGS, 0xEC00);
            look(obj, 0x0BFC96);
        } else {
            QueueSound(0xEF);
            fight_step(obj, 0x18);
            wr16(obj + O_FLAGS, 0xEC00);
            look(obj, 0x0BFCC6);
            wr16(obj + 0x5A, rom16(0x03DEB8 + (uint32_t)(int16_t)rd16(v_difficulty)));
            wr16(obj + 0x5C, (uint16_t)((rd16(v_gameFrames) & 3) + 7));
            wr32(obj + 0x1C, 0xFFFD0000);
            wr32(obj + 0x18, bit8(obj + 0x0E, 3) ? 0x8000 : 0xFFFF8000);
        }
    }
    call_object(rom32(0x03DE50 + (uint32_t)(int16_t)rd16(obj + 0x56)), obj);
}

/* $03DCEE: its move; flashing while +$5A counts down (+$23 bit 2: unhittable) */
static void fight(uint16_t obj)
{
    fight_move(obj);
    wr8(obj + 0x23, (uint8_t)(rd8(obj + 0x23) & ~4));
    uint16_t t = rd16(obj + 0x5A);
    if (t && !(t & 0x8000)) {
        wr8(obj + 0x23, (uint8_t)(rd8(obj + 0x23) | 4));
        wr16(obj + 0x5A, (uint16_t)(t - 1));
        if (!bit8(obj + 0x5B, 0)) {
            wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) & 0x7FFF);
            return;
        }
    }
    wr16(obj + O_FLAGS, (uint16_t)(rd16(obj + O_FLAGS) | 0x8000));
}

/* ---- the deck boss ---- */

/* $03EDCA: a marker ($10) carrying a player's place on the deck */
static uint16_t deck_marker_new(void)
{
    uint16_t slot = 0, t = 0, d1 = 0;
    uint32_t a1 = 0;
    SlotFreeOrPuff(&slot, &t, &d1, &a1);
    wr16(slot + O_TYPE, 0x10);
    wr16(slot + O_FLAGS, 0);
    wr32(slot + 0x2C, 0xFF10F808);
    wr8(slot + 0x21, 4);
    wr8(slot + 0x46, 1);
    return slot;
}

/* $03F530: the creeping hatch ($28C) at $80,$F8 ($9844) */
static void deck_hatch(void)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x28C);
    wr16(slot + O_FLAGS, 0xC080);
    wr16(slot + 0x0E, 0x828E);
    wr32(slot + 0x08, 0x0E0954);
    wr16(slot + O_X, 0x80);
    wr16(slot + O_Y, 0xF8);
    wr16(0x9844, slot);
}

/* $03DC9E: four lifts ($38C) from $CB00 */
static void deck_lifts(void)
{
    for (uint16_t a = 0xCB00, k = 0; k < 4; k++, a = (uint16_t)(a + 0x60)) {
        wr16(a + O_TYPE, 0x38C);
        wr16(a + O_STATE, 0);
        wr16(a + 6, (uint16_t)(k << 1));
    }
}

/* $03F41A: a rumble every 8 frames */
static void deck_rumble(void)
{
    if (!(rd16(v_gameFrames) & 7)) QueueSound(0xC5);
}

/* $03F618: a fleck ($294) each 16 frames */
static void deck_flecks(void)
{
    if (rd16(v_gameFrames) & 0x0F) return;
    uint16_t slot, t;
    if (SlotFreeHigh(&slot, &t)) wr16(slot + O_TYPE, 0x294);
}

/* $03F634: embers ($298), alternate frames with two players */
static void deck_embers(void)
{
    if (rd8(v_players) == 3 && !bit8(v_gameFrames + 1, 0)) return;
    uint16_t slot, t;
    if (SlotFreeHigh(&slot, &t)) wr16(slot + O_TYPE, 0x298);
}

/* $02FB5A: the boss on foot ($380) begun here: its fight's message, vitality $207 */
void Boss380Begin(uint16_t obj)
{
    wr16(0x9C00, 0x48);
    wr16(obj + 0x56, 0x8020);
    wr16(0x80C2, 0x10);
    wr16(0x80D6, obj);
    wr32(obj + 0x08, 0x0CFF98);
    wr16(obj + 0x0C, 0);
    wr16(obj + O_VITALITY, 0x207);
    wr8(obj + 0x20, 0x90);
    wr8(obj + 0x23, 0x20);
    wr32(obj + 0x2C, 0xF808EE12);
    wr32(obj + 0x28, 0xF410F010);
    wr16(obj + 0x26, 0x12);
    wr16(obj + 0x5C, 7);
    wr16(0x9C08, rd16(v_randomSeed));
    wr16(0x9C14, 0);
    wr16(0x9C18, 0);
}

/* the deck drawn: carried things, the edge, its height at the boss's floor */
static void deck_frame(uint16_t obj)
{
    deck_track(obj);
    deck_edge_all();
    on_floor(obj);
}

/* $03D55E: the deck's state cleared, the hatch, the markers, the deck's graphics' job */
void DeckBossStart(uint16_t obj)
{
    deck_hatch();
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x0E, 0x0EC0);
    wr16(obj + O_FLAGS, 0xED00);
    wr16(obj + O_X, 0x280);
    wr32(obj + 0x08, 0x0CFF98);
    wr16(obj + 0x0C, 0);
    wr8(obj + 0x20, 0x80);
    wr16(obj + 0xDA, 0xD800);
    for (uint16_t o = 0x50; o <= 0x56; o = (uint16_t)(o + 2)) wr16(obj + o, 0);
    static const uint16_t longs[11] = { 0x9810, 0x9804, 0x9800, 0x980C, 0x9808, 0x982C, 0x9828,
                                        0x9830, 0x9832, 0x9820, 0x9824 };
    for (int k = 0; k < 11; k++) wr32(longs[k], 0);
    static const uint16_t words[7] = { 0x9818, 0x981A, 0x981C, 0x981E, 0x9856, 0x9858, 0x981E };
    for (int k = 0; k < 7; k++) wr16(words[k], 0);
    wr16(0x9814, deck_marker_new());
    wr16(0x9816, deck_marker_new());
    uint32_t list = 0x03F526;
    NextJob(&list);
}

/* $03D5F6: the camera stopped: the deck's H-interrupt effect, its scroll taken */
void DeckBossWait(uint16_t obj)
{
    if (bit8(v_cameraHold, 7)) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr32(obj + 0x08, 0x0CFF2C);
        wr16(obj + 0x0C, 0);
        wr16(v_hintEffect, 8);
        wr16(v_hintState, 0);
        wr16(0x9846, 0);
        wr16(0x9848, 0x60);
        uint16_t v = rd16(0xE400) & 0x1FF;
        wr16(0x984A, v);
        wr16(0x9820, v);
        v = rd16(0xEC00) & 0xFF;
        wr16(0x984C, v);
        wr16(0x9824, v);
        wr16(0x9818, 0x100);
    }
    wr16(DECK_SWAY, (uint16_t)(rd16(0xE400) - 0xE980));
    deck_track(obj);
    deck_edge_rising();
    on_floor(obj);
}

/* $03D66C: rising $100 frames */
void DeckBossRise(uint16_t obj)
{
    wr16(0x9818, (uint16_t)(rd16(0x9818) - 1));
    if ((int16_t)rd16(0x9818) < 0) {
        wr32(obj + 0x08, 0x0D027C);
        wr16(obj + 0x0C, 0);
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(0x9818, 0x100);
        wr16(obj + O_FLAGS, 0xEC00);
        wr16(obj + 0x4C, 2);
    }
    deck_motion_scroll();
    deck_hint_lines();
    deck_lines_flat();
    deck_frame(obj);
}

/* $03D792 */
static void deck_sailing(uint16_t obj)
{
    deck_motion_scroll();
    deck_hint_lines();
    deck_frame(obj);
    if (rd16(0x9818) == 0xA0) {
        wr32(obj + 0x08, 0x0CFE68);
        wr16(obj + 0x0C, 0);
    }
    if ((int16_t)(rd16(0x9818) - 0xA0) < 0 && !bit8(v_gameFrames + 1, 0))
        wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) + 1));
}

/* $03D6F2: the ship speeding away under it $100 frames: then stopped, the boss on foot up */
void DeckBossSail(uint16_t obj)
{
    wr32(v_planeSpeeds, rd32(v_planeSpeeds) + 0x400);
    wr32(v_camSpeedX, rd32(v_camSpeedX) + 0x400);
    wr32(0x9820, rd32(0x9820) - rd32(v_planeSpeeds));
    wr16(0x984A, rd16(0x9820));
    wr32(0xA9D4, rd32(0xA9D4) - 0x200);
    wr16(0x9818, (uint16_t)(rd16(0x9818) - 1));
    if ((int16_t)rd16(0x9818) >= 0) {
        deck_lines_flat();
        deck_sailing(obj);
        return;
    }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(v_hintEffect, 0);
    wr16(v_hintState, 0);
    wr32(v_planeSpeeds, 0);
    wr32(v_camSpeedX, 0);
    wr32(0xA9D0, 0);
    wr32(0xA9D4, 0);
    wr16(0xA9C0, 1);
    uint32_t list = 0x0092A4;
    NextJob(&list);
    wr8(0xF7E7, 6);
    wr8(v_planeFlagsA, (uint8_t)(rd8(v_planeFlagsA) | 0x80));
    wr8(v_planeFlagsA, (uint8_t)(rd8(v_planeFlagsA) | 0x40));
    deck_draw_upper();
    Boss380Begin(obj);
    deck_lifts();
    deck_sailing(obj);
}

/* the deck held up ($9810 to $6000) while the boss on foot fights */
static void deck_tilt_up_to(void)
{
    if ((int32_t)(rd32(DECK_TILT) - 0x6000) < 0) wr32(DECK_TILT, rd32(DECK_TILT) + 0x400);
}

static void deck_tilt_level_off(void)
{
    if (rd32(DECK_TILT)) wr32(DECK_TILT, rd32(DECK_TILT) - 0x400);
}

/* $03D81E: low, tilting up; the boss on foot's frame */
static void deck_boss380(uint16_t obj)
{
    deck_tilt_up_to();
    deck_motion_low();
    deck_draw_upper();
    deck_track(obj);
    deck_edge_all();
    Boss380(obj);
}

/* $03D7DA: its graphics' job done: the background planes moving */
void DeckBossLoaded(uint16_t obj)
{
    if (!rd16(v_jobKind)) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr32(0xA9D0, 0xFFFF2000);
        wr32(0xA9D4, 0x1C000);
    }
    deck_boss380(obj);
}

/* $03D7FA: the planes rising to $218 */
void DeckBossClimb(uint16_t obj)
{
    wr32(0xA9D4, rd32(0xA9D4) + 0x200);
    if ((int16_t)(rd16(0xEC02) - 0x218) >= 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr32(0xA9D4, 0);
        wr16(0x9860, 0x80);
    }
    deck_boss380(obj);
}

/* $03D848: on to plane A's $ED20, the background wobbling */
void DeckBossOn(uint16_t obj)
{
    if ((int16_t)(rd16(0xE402) - 0xED20) >= 0) wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    deck_tilt_up_to();
    deck_motion_high();
    deck_draw_upper();
    deck_track(obj);
    deck_edge_all();
    deck_wobble();
    Boss380(obj);
}

/* $03D884: the boss on foot's fight, the deck levelling: it beaten (+$56 bit 11), its message */
void DeckBoss380Fight(uint16_t obj)
{
    if (rd32(0xA9D0)) wr32(0xA9D0, rd32(0xA9D0) + 0x400);
    deck_tilt_level_off();
    deck_motion_high();
    deck_draw_upper();
    deck_track(obj);
    deck_edge_all();
    deck_wobble();
    if (!bit8(obj + 0x56, 3)) { Boss380(obj); return; }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, 0xED00);
    for (uint16_t o = 0x50; o <= 0x56; o = (uint16_t)(o + 2)) wr16(obj + o, 0);
    wr16(0x80C2, 0x34);
    wr32(0x80C8, 0x0399AC);
}

static void deck_sink_frame(uint16_t obj, int high)
{
    deck_tilt_level_off();
    if (high) deck_motion_high();
    else deck_motion_low();
    deck_draw_upper();
    deck_frame(obj);
}

/* $03D8FC: the planes sinking to $100: the next graphics' job */
void DeckBossSink(uint16_t obj)
{
    wr32(0xA9D4, rd32(0xA9D4) - 0x1000);
    if ((int16_t)(rd16(0xEC02) - 0x100) < 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr32(0xA9D0, 0);
        wr32(0xA9D4, 0);
        wr16(0xA9C0, 1);
        uint32_t list = 0x0092CE;
        uint16_t kind = rom16(list);
        NextJob(&list);
        /* this call does not keep a5: a job of kind 2 (ClearTilePixels) leaves it at $FFB480,
         * and the rest of the frame works there */
        if (!(kind & 0x8000) && (kind & 2) && !(kind & 4)) reg_a5_left = obj = 0xB480;
    }
    deck_sink_frame(obj, 1);
}

/* $03D960 */
void DeckBossLoaded2(uint16_t obj)
{
    if (!rd16(v_jobKind)) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr32(0xA9D4, 0x8000);
    }
    deck_sink_frame(obj, 0);
}

/* $03D978: rising again to $242 */
void DeckBossRaise(uint16_t obj)
{
    wr32(0xA9D4, rd32(0xA9D4) + 0x500);
    if ((int16_t)(rd16(0xEC02) - 0x242) >= 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr32(0xA9D0, 0);
    }
    deck_sink_frame(obj, 0);
}

/* $03D9C2: settling at $23C: the lower deck on, its graphics' job */
void DeckBossSettle(uint16_t obj)
{
    wr32(0xA9D4, rd32(0xA9D4) - 0xC00);
    if ((int16_t)(rd16(0xEC02) - 0x240) < 0) {
        deck_lower_tile_fixed();
        wr16(0xEC02, 0x23C);
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr32(0xA9D4, 0);
        wr8(v_planeFlagsB, (uint8_t)(rd8(v_planeFlagsB) | 0x80));
        wr8(v_planeFlagsB, (uint8_t)(rd8(v_planeFlagsB) | 0x40));
        deck_draw_lower();
        uint32_t list = 0x03DA3A;
        NextJob(&list);
    }
    deck_sink_frame(obj, 1);
}

static void deck_battle_frame(uint16_t obj)
{
    deck_tilt();
    deck_draw_upper();
    deck_draw_lower();
    deck_track(obj);
    deck_edge_all();
}

/* $03DA44: the brawler's fight on the tilting deck */
void DeckBossBattle(uint16_t obj)
{
    deck_lower_tile();
    deck_flecks();
    deck_battle_frame(obj);
    fight(obj);
}

/* $03DB0C */
static void deck_ending(uint16_t obj)
{
    deck_rumble();
    deck_lower_tile();
    deck_embers();
    deck_battle_frame(obj);
}

/* $03DAEA: fallen: rumbling, embers; then the screen fading */
void DeckBossFallen(uint16_t obj)
{
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 1));
    if ((int16_t)rd16(obj + 0x4E) < 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(0x8002, 0xF000);
        for (uint16_t a = 0x8014; a <= 0x801A; a = (uint16_t)(a + 2)) wr16(a, 0);
    }
    deck_ending(obj);
}

/* $03DA66: fading out; dark: the deck gone (its effects, planes' buffer, markers) */
void DeckBossFade(uint16_t obj)
{
    if (rd16(v_gameFrames) & 3) { deck_ending(obj); return; }
    for (uint16_t a = 0x8014; a <= 0x801A; a = (uint16_t)(a + 2)) wr16(a, (uint16_t)(rd16(a) + 1));
    PalettesFull();
    if ((int16_t)(rd16(0x8014) - 8) < 0) { deck_ending(obj); return; }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr8(0xF7E7, 0);
    wr8(v_planeFlagsA, (uint8_t)(rd8(v_planeFlagsA) & ~0x80));
    wr8(v_planeFlagsA, (uint8_t)(rd8(v_planeFlagsA) & ~0x40));
    wr8(v_planeFlagsB, (uint8_t)(rd8(v_planeFlagsB) & ~0x80));
    wr8(v_planeFlagsB, (uint8_t)(rd8(v_planeFlagsB) & ~0x40));
    wr16(obj + 0x4E, 0x10);
    wr16(obj + 0x50, 0x80);
    ClearVramC000();
    ClearPlaneBuffer();
    wr32(0x9834, 0);
    wr32(0x9838, 0);
    wr32(DECK_SWAY, 0);
    wr32(DECK_Y, 0);
    deck_edge_clear();
    wr16(rd16(0x9814) + O_FLAGS, 0x1000);
    wr16(rd16(0x9816) + O_FLAGS, 0x1000);
    wr16(rd16(0x9844) + O_FLAGS, 0x1000);
}

/* $03DB2C: then the stage's next part (the beacon, $4E4) */
void DeckBossGone(uint16_t obj)
{
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 1));
    if ((int16_t)rd16(obj + 0x4E) < 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(v_stageEvent, 1);
        wr16(0xDE80 + O_TYPE, 0x4E4);
        wr16(0xDE80 + O_STATE, 0);
    }
    deck_rumble();
    deck_embers();
}

/* $03DB52: fading back in, $100 frames */
void DeckBossFadeIn(uint16_t obj)
{
    if (rd16(0x8014) && !bit8(v_gameFrames + 1, 0)) {
        for (uint16_t a = 0x8014; a <= 0x801A; a = (uint16_t)(a + 2)) wr16(a, (uint16_t)(rd16(a) - 1));
        PalettesFull();
    }
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) + 1));
    if ((int16_t)(rd16(obj + 0x50) - 0x100) >= 0) wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    deck_embers();
}

/* $03DB92 */
void DeckBossDone(uint16_t obj)
{
    (void)obj;
}

/* $03D4B2: its step; then +$4D bit 4 when low, at the edge of the deck's middle span ($90-$1B0,
 * +$4D bit 5) pushed back in (+$4D bits 2, 3) */
void DeckBoss(uint16_t obj)
{
    Random();
    reg_a5_left = 0;
    call_object(rom32(0x03D516 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
    if (reg_a5_left) obj = reg_a5_left;
    wr16(obj + 0x4C, rd16(obj + 0x4C) & 0xFFE3);
    if ((int16_t)(rd16(obj + O_Y) - 0x150) >= 0) wr8(obj + 0x4D, (uint8_t)(rd8(obj + 0x4D) | 0x10));
    if (!bit8(obj + 0x4D, 5)) return;
    if ((int16_t)(rd16(obj + O_X) - 0x1B0) >= 0) {
        wr16(obj + O_X, 0x1AE);
        wr8(obj + 0x4D, (uint8_t)(rd8(obj + 0x4D) | 8));
        return;
    }
    if ((int16_t)(rd16(obj + O_X) - 0x90) < 0) {
        wr16(obj + O_X, 0x92);
        wr8(obj + 0x4D, (uint8_t)(rd8(obj + 0x4D) | 4));
    }
}
