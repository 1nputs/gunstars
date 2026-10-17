/* Object types from $28000 to $2FFFF: small effects and pieces of a set-piece fight that runs
 * in fixed screen coordinates (types $30 to $3C, $1EC to $25C), a pad-driven test object ($378),
 * an empty type ($44, $9C, $A0, $AC); the type $1F8 and the type $380 with their steps. */
#include "game.h"

#define SINE     0x02977Cu       /* 256 words of a sine (a turn is $200 bytes); the cosine at +$80 */
#define COSINE   0x0297FCu
#define LEVEL    0xA20C          /* the difficulty's table index (a word, 0 to $E) */
#define FRAMES_LO (v_gameFrames + 1)    /* the frame counter's low byte */

static void next_state(uint16_t obj, uint16_t by)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + by));
}

/* subq.w #1 on a word: 1 when it went below 0 */
static int count_down(uint16_t field)
{
    wr16(field, (uint16_t)(rd16(field) - 1));
    return (rd16(field) & 0x8000) != 0;
}

static void remove_me(uint16_t obj) { wr16(obj + O_FLAGS, 0x1000); }

/* a table word sign-extended to a long, shifted left (ext.l, asl.l) */
static uint32_t ext_shift(uint16_t v, int by) { return (uint32_t)(int32_t)(int16_t)v << by; }
static uint16_t sine(uint16_t a) { return rom16(SINE + (uint32_t)(int16_t)a); }
static uint16_t cosine(uint16_t a) { return rom16(COSINE + (uint32_t)(int16_t)a); }

/* a look from three word tables (+$E, +8, +$A) at the same offset */
static void look3(uint16_t obj, uint32_t sprites, uint32_t maps, uint32_t boxes, uint16_t k)
{
    wr16(obj + 0x0E, rom16(sprites + (uint32_t)(int16_t)k));
    wr16(obj + 0x08, rom16(maps + (uint32_t)(int16_t)k));
    wr16(obj + 0x0A, rom16(boxes + (uint32_t)(int16_t)k));
}

/* ---- type $30: a sprite shown on the frames the mask (+$4A) leaves, for +$48 frames ---- */

/* $028016. Type $30: its look, then blinking by the frame count and +$4A until +$48 runs out. */
void MaskBlinker(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_FLAGS, 0x8E00);
        wr16(obj + 0x0E, 0x4260);
        wr16(obj + 0x08, 0xF00);
        wr16(obj + 0x0A, 0xF0F0);
        next_state(obj, 1);
        return;
    }
    wr16(obj + O_FLAGS, (rd16(v_gameFrames) & rd16(obj + 0x4A)) ? 0xE00 : 0x8E00);
    if (count_down(obj + 0x48)) remove_me(obj);
}

/* ---- type $34: a pair of satellites (type $10) circling it, as it moves right, then left ---- */

/* $028076. Type $34. First: moving right at 4, two type $10s in free slots from $C980 (their
 * addresses kept as longs at +$48 and +$4C; none: it removes itself). Then: right until x $1A0,
 * slowing to the left until x $70, when all three go. Each frame the two circle it (radius by
 * the sine >> 10), each turned and flipped by its side of the circle. */
void SatellitePair(uint16_t obj)
{
    uint16_t s = rd16(obj + O_STATE);
    if (!s) {
        wr16(obj + O_FLAGS, 0xC00);
        wr32(obj + 0x18, 0x40000);
        next_state(obj, 1);
        uint16_t a = 0, t;
        if (!SlotFreeFromC980(&a, &t)) { remove_me(obj); return; }
        wr32(obj + 0x48, 0xFFFF0000u | a);
        a = (uint16_t)(a + 0x60);
        if (!SlotFreeFrom(&a, &t)) { remove_me(obj); return; }
        wr32(obj + 0x4C, 0xFFFF0000u | a);
        uint16_t first = (uint16_t)rd32(obj + 0x48);
        wr16(first + O_TYPE, 0x10);
        wr16(first + O_FLAGS, 0xCC00);
        wr16(first + 0x48, 0xA00);
        wr32(first + 0x08, 0x0E145C);
        wr16(a + O_TYPE, 0x10);
        wr16(a + O_FLAGS, 0xCC00);
        wr16(a + 0x48, 0x2200);
        wr32(a + 0x08, 0x0E145C);
        wr16(obj + 0x26, 6);
        wr32(obj + 0x2C, 0xF40BF40C);
        wr8(obj + 0x21, 0x10);
        return;
    }
    if (s == 2) {
        wr32(obj + 0x18, rd32(obj + 0x18) - 0x8000);
        if ((int16_t)(rd16(obj + O_X) - 0x70) < 0) {
            wr16((uint16_t)rd32(obj + 0x48) + O_FLAGS, 0x1000);
            wr16((uint16_t)rd32(obj + 0x4C) + O_FLAGS, 0x1000);
            remove_me(obj);
            return;
        }
    } else if ((int16_t)(rd16(obj + O_X) - 0x1A0) >= 0) {
        next_state(obj, 1);
    }
    uint16_t d6 = (uint16_t)((rd16(obj + 0x56) + 0x18) & 0x1FE);
    wr16(obj + 0x56, d6);
    uint16_t sat = (uint16_t)rd32(obj + 0x48);
    uint32_t looks = 0x027916;
    uint16_t d4 = 0x80;
    for (int k = 0; k < 2; k++) {
        d6 = (uint16_t)((d6 + 0x80) & 0x1FE);
        uint16_t d5 = (uint16_t)((d6 - d4) & 0x1FE);
        if ((int16_t)(d5 - 0x100) < 0) wr16(sat + 0x0E, rd16(sat + 0x48));
        else wr16(sat + 0x0E, rd16(sat + 0x48) ^ 0x1800);
        d5 = (uint16_t)(((int16_t)d5 >> 3) & 0x1C);
        uint16_t d0 = sine(d6), d1 = cosine(d6);
        wr32(sat + 0x08, rom32(looks + d5));
        d0 = (uint16_t)((int16_t)d0 >> 10);
        d1 = (uint16_t)((int16_t)d1 >> 10);
        wr16(sat + O_Y, (uint16_t)(d0 + rd16(obj + O_Y)));
        wr16(sat + O_X, (uint16_t)(d1 + rd16(obj + O_X)));
        d4 = 0x180;
        looks = 0x0279D6;
        sat = (uint16_t)rd32(obj + 0x4C);
    }
}

/* ---- type $38: a falling piece that bursts at y $140 ---- */

/* $0281E6. Type $38. Falls (gravity $8000) to y $140, then becomes a puff (type $100) thrown up
 * and left, with sound $A6. */
void FallBurst(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_FLAGS, 0xCE00);
        wr16(obj + 0x0E, 0x200);
        wr32(obj + 0x08, 0x0E145C);
        next_state(obj, 1);
        wr16(obj + 0x26, 8);
        wr32(obj + 0x2C, 0xF40BF40C);
        wr8(obj + 0x21, 0x10);
        return;
    }
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x8000);
    if ((int16_t)(rd16(obj + O_Y) - 0x140) < 0) return;
    wr8(obj + 0x21, 0);
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x05639C);
    wr16(obj + O_STATE, 0);
    wr32(obj + 0x1C, 0xFFFC0000);
    wr32(obj + 0x18, 0xFFFA0000);
    QueueSound(0xA6);
}

/* ---- type $3C: a piece drifting until x $C0 (moving left) or $180 (right) ---- */

/* $028262. Type $3C. A sprite of four by the frame count; gone past x $C0 or $180. */
void Drifter(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next_state(obj, 1);
        wr16(obj + O_FLAGS, 0x8E00);
        wr16(obj + 0x0E, rom16(0x0282AA + (rd16(v_gameFrames) & 6)));
        wr16(obj + 0x08, 0);
        wr16(obj + 0x0A, 0xFCFC);
        wr16(obj + 0x26, 1);
        wr32(obj + 0x2C, 0xFE01FE02);
        wr8(obj + 0x21, 0x10);
        return;
    }
    if (rd16(obj + 0x18) & 0x8000) {
        if ((int16_t)(rd16(obj + O_X) - 0xC0) >= 0) return;
    } else {
        if ((int16_t)(rd16(obj + O_X) - 0x180) < 0) return;
    }
    remove_me(obj);
}

/* ---- type $24C: blinking for $10 frames ---- */

/* $0282DA. Type $24C. Shown on odd frames, hidden on even ones, for $11 even frames. */
void BlinkOut(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next_state(obj, 4);
        wr16(obj + 0x48, 0x10);
        return;
    }
    if (bit8(FRAMES_LO, 0)) { set8(obj + O_FLAGS, 7); return; }
    if (count_down(obj + 0x48)) { remove_me(obj); return; }
    clr8(obj + O_FLAGS, 7);
}

/* ---- type $1EC: a piece swaying across, animated at a random rate ---- */

/* $028318. Type $1EC. One of four animations at random (the table's +$50 word, 0 for each,
 * steps it at a random rate of 1 to 4); its x speed pulled $1000 a frame against its way. */
void Swayer(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next_state(obj, 1);
        uint16_t d0 = rd16(v_randomSeed) & 0xC;
        wr32(obj + 0x4C, rom32(0x0283B4 + d0));
        wr32(obj + 0x08, rom32(0x0283B4 + d0));
        d0 = (uint16_t)(d0 >> 1);
        wr16(obj + 0x50, rom16(0x0283C4 + d0));
        wr16(obj + 0x0E, 0x200);
        wr16(obj + O_FLAGS, 0x4E00);
        wr16(obj + 0x48, (uint16_t)((rd8(v_randomSeed) & 3) + 1));
        wr8(obj + 0x20, 0xC0);
        return;
    }
    wr16(obj + O_FLAGS, 0xCE00);
    if (!rd16(obj + 0x50)) {
        uint32_t list = rd32(obj + 0x4C);
        uint16_t d0 = (uint16_t)(rd16(obj + 0x52) + rd16(obj + 0x48));
        wr16(obj + 0x52, d0);
        wr32(obj + 0x08, bus_rd32(list + (d0 & 0x1C)));
    }
    if (bit8(obj + 0x18, 7)) wr32(obj + 0x18, rd32(obj + 0x18) + 0x1000);
    else wr32(obj + 0x18, rd32(obj + 0x18) - 0x1000);
}

/* ---- type $204: spinning, spraying shots (type $208) ---- */

/* $0283CC. Type $204. For $39 frames: turning (four looks by the frame count), and every 4th
 * frame sound $B6 and a $208 at its angle (+$56) at speed 1/8 of the sine, its look by the
 * angle's sixteenth. */
void Sprayer(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next_state(obj, 1);
        wr16(obj + O_FLAGS, 0x8000);
        wr16(obj + 0x0E, 0x4B5);
        wr16(obj + 0x08, 0x500);
        wr16(obj + 0x0A, 0xF8F8);
        wr8(obj + 0x20, 0);
        wr16(obj + 0x48, 0x38);
        return;
    }
    if (count_down(obj + 0x48)) { remove_me(obj); return; }
    look3(obj, 0x0284B4, 0x0284BC, 0x0284C4, rd16(v_gameFrames) & 6);
    if (rd16(v_gameFrames) & 3) return;
    QueueSound(0xB6);
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x208);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    uint16_t d0 = rd16(obj + 0x56);
    wr32(slot + 0x1C, ext_shift(sine(d0), 5));
    wr32(slot + 0x18, ext_shift(cosine(d0), 5));
    uint32_t a1 = 0x05647C + (uint32_t)(((int16_t)(uint16_t)(d0 + 8) >> 1) & 0x78);
    wr16(slot + 0x0E, rom16(a1));
    wr16(slot + 0x08, rom16(a1 + 2));
    wr16(slot + 0x0A, rom16(a1 + 4));
}

/* ---- type $208: a shot bouncing in the box x $90-$1B0, y $B0-$140 ---- */

/* loc_0285E4: the bounces spent: stopped, harmless, 9 frames of flashing */
static void bouncer_stop(uint16_t obj)
{
    next_state(obj, 1);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x48, 8);
    wr8(obj + 0x21, 0);
}

/* $028504. Type $208. Off the box's top or bottom: back by its x speed, turned upside down, its
 * y speed reversed; then off its left or right: back by its y speed, turned round, its x speed
 * reversed. Each bounce spends one of 3 + the difficulty's half; spent, it flashes (the
 * sprayer's looks) for 9 frames and goes. */
void BoxBouncer(uint16_t obj)
{
    uint16_t s = rd16(obj + O_STATE);
    if (!s) {
        next_state(obj, 1);
        wr16(obj + O_FLAGS, 0x8C00);
        wr8(obj + 0x20, 0);
        wr16(obj + 0x26, 0xA);
        wr8(obj + 0x21, 0x10);
        wr32(obj + 0x2C, 0xFC03FC04);
        wr16(obj + 0x48, (uint16_t)(((int16_t)rd16(LEVEL) >> 1) + 3));
        return;
    }
    if (s == 2) {
        uint16_t k = rd16(obj + 0x48) & 6;
        look3(obj, 0x0284B4, 0x0284BC, 0x0284C4, k);
        if (count_down(obj + 0x48)) remove_me(obj);
        return;
    }
    int off;
    if (bit8(obj + 0x1C, 7)) off = (int16_t)(rd16(obj + O_Y) - 0xB0) < 0;
    else off = (int16_t)(rd16(obj + O_Y) - 0x140) >= 0;
    if (off) {
        wr32(obj + O_X, rd32(obj + O_X) + rd32(obj + 0x18));
        wr16(obj + 0x0E, rd16(obj + 0x0E) ^ 0x1000);
        wr32(obj + 0x1C, ~rd32(obj + 0x1C) + 1);
        if (count_down(obj + 0x48)) { bouncer_stop(obj); return; }
    }
    if (bit8(obj + 0x18, 7)) {
        if ((int16_t)(rd16(obj + O_X) - 0x90) >= 0) return;
    } else {
        if ((int16_t)(rd16(obj + O_X) - 0x1B0) < 0) return;
    }
    wr32(obj + O_Y, rd32(obj + O_Y) + rd32(obj + 0x1C));
    wr16(obj + 0x0E, rd16(obj + 0x0E) ^ 0x800);
    wr32(obj + 0x18, ~rd32(obj + 0x18) + 1);
    if (count_down(obj + 0x48)) bouncer_stop(obj);
}

/* ---- type $230: a cluster of eight parts (the next eight slots) that open and close round
 * it, following the target player; it fires $234s from the part at $D040 and aims the parts at
 * $CFE0 and $D040 at it ---- */

#define CLUSTER_PARTS 8

/* loc_0289C4: the angle from the part at $CFE0 to it (a 256th of a turn, times 2) into +$56 of
 * the parts at $CFE0 and $D040 */
static void cluster_aim(uint16_t obj)
{
    uint16_t dx = (uint16_t)(rd16(obj + O_X) - rd16(0xCFF0));
    uint32_t d1 = (uint16_t)(rd16(obj + O_Y) - rd16(0xCFF4));
    uint16_t d2 = Atan2((int16_t)dx, &d1);
    d2 = (uint16_t)(((int16_t)d2 >> 7) & 0x1FE);
    wr16(0xD036, d2);
    wr16(0xD096, d2);
}

/* loc_028A94: the cluster and its parts go */
static void cluster_remove(uint16_t obj)
{
    uint16_t a = obj;
    wr16(obj + O_FLAGS, 0x1000);
    for (int k = 0; k < CLUSTER_PARTS; k++) {
        a = (uint16_t)(a + 0x60);
        wr16(a + O_FLAGS, 0x1000);
    }
}

/* $028838. Its timer (+$48): a time by the difficulty and a random part to $1F. */
void ClusterTimer(uint16_t obj)
{
    wr16(obj + 0x48, rom16(0x028856 + (uint32_t)(int16_t)rd16(LEVEL)));
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) + (rd16(v_randomSeed) & 0x1F)));
}

/* the next spread (+$4E): wide ($38) or close ($12) by the next bit of the pattern (+$4A,
 * rotated right) */
static void cluster_next_spread(uint16_t obj)
{
    uint16_t v = rd16(obj + 0x4A);
    wr16(obj + 0x4A, (uint16_t)(v >> 1 | v << 15));
    wr16(obj + 0x4E, (v & 1) ? 0x38 : 0x12);
}

/* $028664. Step 0: in the middle of the screen, the pattern of spreads by the difficulty's mask
 * of a random word, 4 to 7 rounds (+$4C); its eight parts made (type $10) with their looks. */
void ClusterStart(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x800);
    wr16(obj + O_Y, 0xC0);
    wr16(obj + O_X, 0x120);
    wr32(obj + 0x18, 0);
    wr16(obj + 0x4A, rd16(v_randomSeed));
    wr16(obj + 0x4A, rd16(obj + 0x4A) & rom16(0x02872E + (uint32_t)(int16_t)rd16(LEVEL)));
    wr16(obj + 0x4C, (uint16_t)((rd8(v_randomSeed) & 3) + 4));
    uint16_t a = obj;
    for (uint16_t k = 0; k < CLUSTER_PARTS; k++) {
        a = (uint16_t)(a + 0x60);
        wr16(a + O_TYPE, 0x10);
        wr16(a + O_FLAGS, 0x8000);
        wr16(a + 0x0E, rom16(0x0286FE + 2u * k));
        wr16(a + 0x08, rom16(0x02870E + 2u * k));
        wr16(a + 0x0A, rom16(0x02871E + 2u * k));
    }
    ClusterTimer(obj);
    next_state(obj, 4);
    wr16(obj + 0x50, 0x12);
    cluster_next_spread(obj);
}

/* $028866. Its x speed: pushed back inside x $88-$1B8, else toward the target player up to a
 * speed by the difficulty (the code after the compare, a random turn every $20 frames, is never
 * reached: bpl and bmi take every case). */
void ClusterSteer(uint16_t obj)
{
    if ((int16_t)(rd16(obj + O_X) - 0x88) < 0) {
        wr32(obj + 0x18, rd32(obj + 0x18) + 0x8000);
        return;
    }
    if ((int16_t)(rd16(obj + O_X) - 0x1B8) >= 0) {
        wr32(obj + 0x18, rd32(obj + 0x18) - 0x8000);
        return;
    }
    uint16_t target = PlayerTarget();
    uint32_t d1 = rom32(0x0288F4 + (uint32_t)(int16_t)(uint16_t)(rd16(v_difficulty) << 1));
    if ((int16_t)(rd16(target + O_X) - rd16(obj + O_X)) >= 0) {
        if ((int32_t)(d1 - rd32(obj + 0x18)) < 0) return;
        wr32(obj + 0x18, rd32(obj + 0x18) + 0x8000);
        return;
    }
    d1 = ~d1;
    if ((int32_t)(d1 - rd32(obj + 0x18)) >= 0) return;
    wr32(obj + 0x18, rd32(obj + 0x18) - 0x8000);
}

/* $028904. The spread (+$50) two nearer the wanted one (+$4E); the parts placed round it: above,
 * below, left, right at half the spread, the four corners at the whole. */
void ClusterSpread(uint16_t obj)
{
    uint16_t d0 = rd16(obj + 0x50);
    if (d0 != rd16(obj + 0x4E)) {
        if ((int16_t)(d0 - rd16(obj + 0x4E)) >= 0) wr16(obj + 0x50, (uint16_t)(d0 - 2));
        else wr16(obj + 0x50, (uint16_t)(d0 + 2));
    }
    d0 = rd16(obj + 0x50);
    uint16_t d4 = (uint16_t)((int16_t)d0 >> 1);
    uint16_t x = rd16(obj + O_X), y = rd16(obj + O_Y);
    static const int8_t place[CLUSTER_PARTS][2] = {   /* x, y: -2/+2 the whole, -1/+1 the half */
        {0, -1}, {0, 1}, {-1, 0}, {1, 0}, {-2, -2}, {2, -2}, {-2, 2}, {2, 2}
    };
    uint16_t a = obj;
    for (int k = 0; k < CLUSTER_PARTS; k++) {
        a = (uint16_t)(a + 0x60);
        int px = place[k][0], py = place[k][1];
        uint16_t ox = px == 0 ? 0 : (px == 1 || px == -1) ? d4 : d0;
        uint16_t oy = py == 0 ? 0 : (py == 1 || py == -1) ? d4 : d0;
        wr16(a + O_X, (uint16_t)(px < 0 ? x - ox : x + ox));
        wr16(a + O_Y, (uint16_t)(py < 0 ? y - oy : y + oy));
    }
}

/* $0289EA. The parts shown or hidden by bit 1 of its timer. */
void ClusterBlink(uint16_t obj)
{
    uint16_t a = obj;
    int show = !(rd16(obj + 0x48) & 2);
    for (int k = 0; k < CLUSTER_PARTS; k++) {
        a = (uint16_t)(a + 0x60);
        if (show) wr16(a + O_FLAGS, rd16(a + O_FLAGS) | 0x8000);
        else wr16(a + O_FLAGS, rd16(a + O_FLAGS) & 0x7FFF);
    }
}

/* $028A1C. The first four parts' looks: one set on odd frames, another on even ones. */
void ClusterLooks(uint16_t obj)
{
    uint32_t k = bit8(FRAMES_LO, 0) ? 0 : 8;
    uint16_t a = obj;
    for (uint32_t n = 0; n < 4; n++) {
        a = (uint16_t)(a + 0x60);
        wr16(a + 0x0E, rom16(0x028A64 + k + 2 * n));
        wr16(a + 0x08, rom16(0x028A74 + k + 2 * n));
        wr16(a + 0x0A, rom16(0x028A84 + k + 2 * n));
    }
}

/* loc_028AB8: a $234 from the part at $D040, at speed 1/4 of the sine along its angle; +$58 the
 * kind (2 or 0); its slot in +$52 */
static void cluster_fire(uint16_t obj, uint16_t kind)
{
    uint16_t slot, t, d1 = 0;
    uint32_t a1 = 0;
    if (!SlotFreeOrPuff(&slot, &t, &d1, &a1)) return;
    wr16(slot + O_TYPE, 0x234);
    wr16(slot + O_X, rd16(0xD050));
    wr16(slot + O_Y, rd16(0xD054));
    wr16(slot + O_STATE, 0);
    wr16(obj + 0x52, slot);
    uint16_t d0 = rd16(0xD036);
    wr32(slot + 0x1C, ext_shift(sine(d0), 6));
    wr32(slot + 0x18, ext_shift(cosine(d0), 6));
    wr16(slot + 0x58, kind);
}

/* $028AB0. A $234 of kind 2 (it bursts where it lands). */
void ClusterFireBurst(uint16_t obj)
{
    cluster_fire(obj, 2);
}

/* $028AB6. A $234 of kind 0. */
void ClusterFire(uint16_t obj)
{
    cluster_fire(obj, 0);
}

/* $02873E. Step 4: steering for its timer, then stopped with $10 frames to go. */
void ClusterMove(uint16_t obj)
{
    ClusterSteer(obj);
    if (count_down(obj + 0x48)) {
        next_state(obj, 4);
        wr16(obj + 0x48, 0x10);
        wr32(obj + 0x18, 0);
    }
    ClusterSpread(obj);
    ClusterLooks(obj);
    cluster_aim(obj);
}

/* $028768. Step 8: blinking $10 frames; then sound $C3 and, spread wide, a bursting $234 and
 * step $C, or step $10 (that frame not aimed). */
void ClusterWarn(uint16_t obj)
{
    ClusterBlink(obj);
    ClusterSpread(obj);
    ClusterLooks(obj);
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (rd16(obj + 0x48)) { cluster_aim(obj); return; }
    wr16(obj + 0x48, 0x20);
    if (rd16(obj + 0x4E) != 0x38) {
        next_state(obj, 8);
        QueueSound(0xC3);
        return;
    }
    next_state(obj, 4);
    ClusterFireBurst(obj);
    QueueSound(0xC3);
}

/* $0287B2. Step $C: $21 frames, then closing (+$4E 0) at step $14. */
void ClusterAfterBurst(uint16_t obj)
{
    if (count_down(obj + 0x48)) {
        next_state(obj, 8);
        wr16(obj + 0x4E, 0);
    }
    ClusterSpread(obj);
    ClusterLooks(obj);
    cluster_aim(obj);
}

/* $0287D0. Step $10: a $234 every frame for $21 frames, then closing at step $14. */
void ClusterSpray(uint16_t obj)
{
    if (count_down(obj + 0x48)) {
        next_state(obj, 4);
        wr16(obj + 0x4E, 0);
    }
    ClusterFire(obj);
    ClusterSpread(obj);
    ClusterLooks(obj);
    cluster_aim(obj);
}

/* $0287F2. Step $14: closed, a round spent: none left, it and its parts go; else moving again
 * (step 4) with a new timer and spread. */
void ClusterClose(uint16_t obj)
{
    if (rd16(obj + 0x50) == rd16(obj + 0x4E)) {
        if (count_down(obj + 0x4C)) { cluster_remove(obj); return; }
        wr16(obj + O_STATE, 4);
        ClusterTimer(obj);
        cluster_next_spread(obj);
    }
    ClusterSteer(obj);
    ClusterSpread(obj);
    ClusterLooks(obj);
    cluster_aim(obj);
}

/* $028642. Type $230. */
void Cluster(uint16_t obj)
{
    call_object(rom32(0x02864C + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $234: the cluster's shot: flies to y $C0; kind 0 stops there for a while, kind 2
 * bursts in rings of puffs ($1E4) ---- */

/* $028B24. Step 0: harmless, animated (the list at $566DE). */
void ClusterShotStart(uint16_t obj)
{
    next_state(obj, 4);
    wr8(obj + 0x21, 0);
    wr32(obj + 0x2C, 0);
    wr16(obj + O_FLAGS, 0x8C00);
    wr32(obj + 0x48, 0x0566DE);
    wr16(obj + 0x4C, 0);
}

/* $028B4C. Step 4: animated while below y $C0; there, harmful, kind 0: still, shown, step 8;
 * kind 2: hidden at the x of the object at $DCA0, a puff, sound $CF, and step $C. */
void ClusterShotFly(uint16_t obj)
{
    if ((int16_t)(rd16(obj + O_Y) - 0xC0) >= 0) { AnimLoop(obj); return; }
    wr8(obj + 0x21, 0x10);
    wr32(obj + 0x2C, 0xF40CF40C);
    if (!rd16(obj + 0x58)) {
        next_state(obj, 4);
        wr16(obj + O_Y, 0xC0);
        wr16(obj + O_FLAGS, 0x8000);
        wr16(obj + 0x26, 8);
        return;
    }
    QueueSound(0xCF);
    next_state(obj, 8);
    wr16(obj + O_Y, 0xC0);
    wr16(obj + O_X, rd16(0xDCB0));
    wr16(obj + O_FLAGS, 0);
    wr16(obj + 0x48, 6);
    wr16(obj + 0x54, 6);
    wr16(obj + 0x26, 0x14);
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x1E4);
    wr32(slot + 0x48, 0x05663A);
    wr16(slot + 0x54, 0x8000);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
}

/* $028BE6. Step 8: gone. */
void ClusterShotEnd(uint16_t obj)
{
    remove_me(obj);
}

/* $028D2A. A puff (type $1E4, its animation in d3) at (d1, d2) from it. */
void ClusterShotPuff(uint16_t obj, uint16_t dx, uint16_t dy, uint32_t list)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x1E4);
    wr32(slot + 0x48, list);
    wr16(slot + 0x54, 0x8000);
    wr16(slot + O_X, (uint16_t)(rd16(obj + O_X) + dx));
    wr16(slot + O_Y, (uint16_t)(rd16(obj + O_Y) + dy));
}

/* $028BEE. Step $C: every 7 frames a ring of four puffs, growing ($20 below/right/above/left,
 * then $10 and $20 at the corners: its box grows with it), sound $D2 at the second; then gone.
 * The last corner puff is placed at (d1 - 1, 1 - d2): the 68k flips d1 with eori but forgets
 * its addq (it adds to d2 instead). */
void ClusterShotBurst(uint16_t obj)
{
    if (!count_down(obj + 0x48)) return;
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) - 2));
    if (rd16(obj + 0x54) & 0x8000) { remove_me(obj); return; }
    if (rd16(obj + 0x54) == 4) QueueSound(0xD2);
    wr16(obj + 0x48, 6);
    uint16_t d0 = rd16(obj + 0x54);
    uint16_t d1 = rom16(0x028D1E + (uint32_t)(int16_t)d0), d2 = d1;
    wr16(obj + 0x2C, rom16(0x028D24 + (uint32_t)(int16_t)d0));
    wr16(obj + 0x2E, rd16(obj + 0x2C));
    uint32_t d3 = 0x05663A;
    if (!(d1 & 1)) {
        ClusterShotPuff(obj, d1, d2, d3);
        d1 = (uint16_t)(~d1 + 1);
        ClusterShotPuff(obj, d1, d2, d3);
        d2 = (uint16_t)(~d2 + 1);
        ClusterShotPuff(obj, d1, d2, d3);
        d1 = (uint16_t)~d1;
        d2 = (uint16_t)(d2 + 1);
        ClusterShotPuff(obj, d1, d2, d3);
        return;
    }
    d1 = (uint16_t)(d1 & ~1u);
    uint16_t slot, t;
    for (int k = 0; k < 4; k++) {
        if (!SlotFreeHigh(&slot, &t)) return;
        wr16(slot + O_TYPE, 0x1E4);
        wr32(slot + 0x48, d3);
        wr16(slot + 0x54, 0x8000);
        uint16_t x = rd16(obj + O_X), y = rd16(obj + O_Y);
        switch (k) {
        case 0: wr16(slot + O_X, x); wr16(slot + O_Y, (uint16_t)(y + d1)); break;
        case 1: wr16(slot + O_Y, y); wr16(slot + O_X, (uint16_t)(x + d1)); break;
        case 2: wr16(slot + O_X, x); wr16(slot + O_Y, (uint16_t)(y - d1)); break;
        default: wr16(slot + O_Y, y); wr16(slot + O_X, (uint16_t)(x - d1)); break;
        }
    }
}

/* $028B0A. Type $234. */
void ClusterShot(uint16_t obj)
{
    call_object(rom32(0x028B14 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $238: an orb held on its owner (+$58) for $81 frames, then thrown along the
 * owner's angle until it leaves x $C0-$180, y $A0-$170 ---- */

/* $028DA6. Step 0: harmless, animated, $80 frames to hold. */
void OrbStart(uint16_t obj)
{
    next_state(obj, 4);
    wr8(obj + 0x21, 0);
    wr32(obj + 0x2C, 0xFE02FE02);
    wr16(obj + 0x26, 8);
    wr16(obj + O_FLAGS, 0x8C00);
    wr32(obj + 0x48, 0x0566DE);
    wr16(obj + 0x4C, 0);
    wr16(obj + 0x4E, 0x80);
    wr8(obj + 0x20, 0);
}

/* $028E74. Its speed: 1/16 of the sine along the owner's angle (+$56), the angle of the object
 * at $C980, and the owner's +$50, less $60. */
void OrbAim(uint16_t obj)
{
    uint16_t owner = rd16(obj + 0x58);
    uint16_t d0 = (uint16_t)(rd16(owner + 0x56) + rd16(0xC9D6));
    d0 = (uint16_t)(((uint16_t)(d0 + rd16(owner + 0x50)) - 0x60) & 0x1FE);
    wr32(obj + 0x1C, ext_shift(sine(d0), 4));
    wr32(obj + 0x18, ext_shift(cosine(d0), 4));
}

/* $028DE0. Step 4: on the owner, flickering (+$20); held out, harmful, thrown, sound $C3. */
void OrbHeld(uint16_t obj)
{
    wr8(obj + 0x20, bit8(FRAMES_LO, 0) ? 0x70 : 0);
    uint16_t owner = rd16(obj + 0x58);
    wr16(obj + O_X, rd16(owner + O_X));
    wr16(obj + O_Y, rd16(owner + O_Y));
    if (count_down(obj + 0x4E)) {
        next_state(obj, 4);
        wr8(obj + 0x21, 0x10);
        OrbAim(obj);
        QueueSound(0xC3);
    }
    AnimLoop(obj);
}

/* $028E30. Step 8: flying; out of x $C0-$180: stopped, harmless, $11 frames (step $C); out of
 * y $A0-$170: gone. */
void OrbFly(uint16_t obj)
{
    if ((int16_t)(rd16(obj + O_X) - 0xC0) < 0 || (int16_t)(rd16(obj + O_X) - 0x180) >= 0) {
        next_state(obj, 4);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr16(obj + 0x4E, 0x10);
        wr8(obj + 0x21, 0);
        return;
    }
    if ((int16_t)(rd16(obj + O_Y) - 0xA0) < 0 || (int16_t)(rd16(obj + O_Y) - 0x170) >= 0) {
        remove_me(obj);
        return;
    }
    AnimLoop(obj);
}

/* $028E5E. Step $C: animated for its frames, then gone. */
void OrbFade(uint16_t obj)
{
    if (count_down(obj + 0x4E)) { remove_me(obj); return; }
    AnimLoop(obj);
}

/* $028D8C. Type $238. */
void Orb(uint16_t obj)
{
    call_object(rom32(0x028D96 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- types $23C, $240, $244, $248: launchers that ride beside one of two objects ($D160 at
 * y $B0, $CD40 at y $140, by a random bit), then cross the screen right, accelerating; shot
 * down (+$22 bit 4) they become a puff ($1E4) ---- */

/* loc_02932E: shot down: a puff thrown right, sound $C5 */
static void launcher_shot(uint16_t obj)
{
    wr32(obj + 0x18, 0x40000);
    wr16(obj + O_TYPE, 0x1E4);
    wr16(obj + 0x54, 0x8800);
    wr32(obj + 0x48, 0x0563EE);
    wr16(obj + O_STATE, 0);
    wr8(obj + 0x21, 0);
    QueueSound(0xC5);
}

/* loc_029270: beside its carrier (+$58): its y, $18 right of its x */
static void launcher_ride(uint16_t obj)
{
    uint16_t c = rd16(obj + 0x58);
    wr16(obj + O_Y, rd16(c + O_Y));
    wr16(obj + O_X, (uint16_t)(rd16(c + O_X) + 0x18));
}

/* loc_029256: riding, blinking every 8 frames */
static void launcher_ride_blink(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 8)) { wr16(obj + O_FLAGS, 0x4800); return; }
    wr16(obj + O_FLAGS, 0xC800);
    launcher_ride(obj);
}

/* $029288. Its x speed $2000 more a frame, to 3. */
void LauncherSpeed(uint16_t obj)
{
    if ((int32_t)(rd32(obj + 0x18) - 0x30000) < 0) wr32(obj + 0x18, rd32(obj + 0x18) + 0x2000);
}

/* $02929E. Gone at x $1C4. */
void LauncherOut(uint16_t obj)
{
    if ((int16_t)(rd16(obj + O_X) - 0x1C4) >= 0) remove_me(obj);
}

/* loc_0292B0: gone at x $1C0 */
static void launcher_out_1c0(uint16_t obj)
{
    if ((int16_t)(rd16(obj + O_X) - 0x1C0) >= 0) remove_me(obj);
}

/* $02935E. Its riding time (+$5C) by the difficulty. */
void LauncherRideTime(uint16_t obj)
{
    wr16(obj + 0x5C, rom16(0x029370 + (uint32_t)(int16_t)rd16(LEVEL)));
}

/* $0291F2. Its carrier by a random bit: $D160 (+$4A 2, its y $B0 kept in +$4C) or $CD40 (+$4A 0,
 * y $140). */
void LauncherCarrier(uint16_t obj)
{
    if (bit8(0xFF09, 0)) {
        wr16(obj + 0x4A, 0);
        wr16(obj + 0x4C, 0x140);
        wr16(obj + 0x58, 0xCD40);
        return;
    }
    wr16(obj + 0x4A, 2);
    wr16(obj + 0x4C, 0xB0);
    wr16(obj + 0x58, 0xD160);
}

/* loc_029224: the same in +$52 and +$50 */
static void launcher_carrier_b(uint16_t obj)
{
    if (bit8(0xFF09, 0)) {
        wr16(obj + 0x52, 0);
        wr16(obj + 0x50, 0x140);
        wr16(obj + 0x58, 0xCD40);
        return;
    }
    wr16(obj + 0x52, 2);
    wr16(obj + 0x50, 0xB0);
    wr16(obj + 0x58, 0xD160);
}

/* $0292C2. Three $240s from slot $D580 on, tied to it, at y speeds 4, 8, $C (up when it is
 * below y $100), its look turned. */
void LauncherFire(uint16_t obj)
{
    uint32_t speeds = 0x029318;
    for (int k = 0; k < 3; k++) {
        uint16_t slot = 0xD580, t;
        if (!SlotFree24(&slot, &t)) return;
        wr16(slot + O_TYPE, 0x240);
        uint32_t v = rom32(speeds);
        speeds += 4;
        if ((int16_t)(rd16(obj + O_Y) - 0x100) >= 0) v = ~v + 1;
        wr32(slot + 0x1C, v);
        wr16(slot + 0x58, obj);
        wr16(slot + O_X, rd16(obj + O_X));
        wr16(slot + O_Y, rd16(obj + O_Y));
        wr16(slot + 0x0E, rd16(obj + 0x0E));
        wr16(slot + 0x0E, rd16(slot + 0x0E) | 0x2000);
    }
}

/* $028EEE. Type $23C step 0: its look, its carrier, the x it fires at by the difficulty, its
 * riding time; turned when it rides the lower carrier. */
void LauncherStart(uint16_t obj)
{
    next_state(obj, 4);
    wr16(obj + O_FLAGS, 0xC800);
    wr16(obj + 0x0E, 0x200);
    wr32(obj + 0x08, 0x0E158E);
    wr8(obj + 0x20, 0);
    LauncherCarrier(obj);
    wr16(obj + 0x4E, rom16(0x028F34 + (uint32_t)(int16_t)rd16(LEVEL)));
    LauncherRideTime(obj);
    if (!rd16(obj + 0x4A)) wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x1000);
}

/* $028F44. Step 4: riding (blinking); then off at its carrier's y, harmful. */
void LauncherRide(uint16_t obj)
{
    if (!count_down(obj + 0x5C)) { launcher_ride_blink(obj); return; }
    next_state(obj, 4);
    wr16(obj + O_Y, rd16(obj + 0x4C));
    wr16(obj + O_FLAGS, 0xC800);
    wr8(obj + 0x21, 0x10);
    wr32(obj + 0x2C, 0xF808F808);
    wr16(obj + 0x26, 6);
}

/* $028F76. Step 8: crossing; at its x, three $240s and sound $A6 (step $C). Shot down (or its
 * $240s were): a puff. */
void LauncherCross(uint16_t obj)
{
    if (bit8(obj + 0x22, 4) || bit8(obj + 0x56, 4)) { launcher_shot(obj); return; }
    if ((int16_t)(rd16(obj + O_X) - rd16(obj + 0x4E)) >= 0) {
        next_state(obj, 4);
        LauncherFire(obj);
        QueueSound(0xA6);
    }
    LauncherSpeed(obj);
    LauncherOut(obj);
}

/* $028FB0. Step $C: crossing on. */
void LauncherAway(uint16_t obj)
{
    if (bit8(obj + 0x22, 4) || bit8(obj + 0x56, 4)) { launcher_shot(obj); return; }
    LauncherSpeed(obj);
    LauncherOut(obj);
}

/* $028ED4. Type $23C. */
void Launcher(uint16_t obj)
{
    call_object(rom32(0x028EDE + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $028FCC. Type $240: a launcher's shot, harmful, shown after 8 frames; on its launcher's x,
 * passing its hits (+$22) to it (+$56); its launcher gone (another type in the slot): 3 right,
 * a puff. Gone at x $1C0. */
void LauncherShot(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next_state(obj, 4);
        wr16(obj + O_FLAGS, 0xC400);
        wr32(obj + 0x08, 0x0E158E);
        wr8(obj + 0x20, 0);
        wr16(obj + 0x48, 7);
        wr8(obj + 0x21, 0x10);
        wr32(obj + 0x2C, 0xF808F808);
        wr16(obj + 0x26, 6);
        return;
    }
    if (count_down(obj + 0x48)) wr16(obj + O_FLAGS, 0xC000);
    uint16_t l = rd16(obj + 0x58);
    wr16(obj + O_X, rd16(l + O_X));
    if (rd16(l + O_TYPE) != 0x23C) {
        wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) + 3));
        launcher_shot(obj);
        return;
    }
    wr8(l + 0x56, (uint8_t)(rd8(l + 0x56) | rd8(obj + 0x22)));
    launcher_out_1c0(obj);
}

/* $029422. Its look: two by the frame's parity, turned on frames whose bit 2 is clear. */
void SmokerLook(uint16_t obj)
{
    if (bit8(FRAMES_LO, 0)) {
        wr16(obj + 0x0E, 0x4C2);
        wr16(obj + 0x08, 0xF00);
        wr16(obj + 0x0A, 0xF0F0);
    } else {
        wr16(obj + 0x0E, 0x4B5);
        wr16(obj + 0x08, 0x500);
        wr16(obj + 0x0A, 0xF8F8);
    }
    if (!bit8(FRAMES_LO, 2)) wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x6000);
}

/* $0293CC. On even frames a puff (type $1E4) from slot $D580 on, within $F of it at random. */
void SmokerPuff(uint16_t obj)
{
    if (rd16(v_gameFrames) & 1) return;
    uint16_t slot = 0xD580, t;
    if (!SlotFree24(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x1E4);
    wr16(slot + 0x54, 0x8000);
    wr32(slot + 0x48, 0x0566A2);
    wr16(slot + O_X, (uint16_t)((rd8(v_randomSeed) & 0x1F) + rd16(obj + O_X) - 0xF));
    wr16(slot + O_Y, (uint16_t)((rd8(0xFF09) & 0x1F) + rd16(obj + O_Y) - 0xF));
}

/* $029380. Its y speed its x speed, up or down by +$52, turned round at y $B3 and $13D. */
void SmokerWeave(uint16_t obj)
{
    uint32_t d1 = rd32(obj + 0x18);
    if (!rd16(obj + 0x52)) {
        wr32(obj + 0x1C, ~d1 + 1);
        if ((int16_t)(rd16(obj + O_Y) - 0xB3) >= 0) return;
        wr16(obj + O_Y, 0xB3);
        wr16(obj + 0x52, 2);
        return;
    }
    wr32(obj + 0x1C, d1);
    if ((int16_t)(rd16(obj + O_Y) - 0x13D) < 0) return;
    wr16(obj + O_Y, 0x13D);
    wr16(obj + 0x52, 0);
}

/* $02904E. Type $244 step 0: animated (the list at $5674A), its riding time and carrier. */
void SmokerStart(uint16_t obj)
{
    next_state(obj, 4);
    wr16(obj + O_FLAGS, 0x8C00);
    wr8(obj + 0x20, 0xB8);
    wr32(obj + 0x48, 0x05674A);
    wr16(obj + 0x4C, 0);
    AnimLoop(obj);
    LauncherRideTime(obj);
    launcher_carrier_b(obj);
}

/* $02907A. Step 4: riding, blinking, smoking; then harmful, sound $C3. */
void SmokerRide(uint16_t obj)
{
    SmokerLook(obj);
    SmokerPuff(obj);
    if (count_down(obj + 0x5C)) {
        next_state(obj, 4);
        wr8(obj + 0x21, 0x10);
        wr32(obj + 0x2C, 0xF808F808);
        wr16(obj + 0x26, 4);
        wr16(obj + O_FLAGS, 0x8C00);
        QueueSound(0xC3);
        return;
    }
    if (!(rd16(v_gameFrames) & 8)) { wr16(obj + O_FLAGS, 0); return; }
    wr16(obj + O_FLAGS, 0x8C00);
    launcher_ride(obj);
}

/* $0290D0. Step 8: crossing, weaving, smoking; shot down: a puff. */
void SmokerCross(uint16_t obj)
{
    SmokerLook(obj);
    if (bit8(obj + 0x22, 4)) { launcher_shot(obj); return; }
    LauncherSpeed(obj);
    SmokerWeave(obj);
    SmokerPuff(obj);
    LauncherOut(obj);
}

/* $029038. Type $244. */
void Smoker(uint16_t obj)
{
    call_object(rom32(0x029042 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* loc_029150: the rider's look and riding time */
static void rider_look(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x8800);
    wr16(obj + 0x0E, 0x6260);
    wr16(obj + 0x08, 0xF00);
    wr16(obj + 0x0A, 0xF0F0);
    wr8(obj + 0x20, 0);
    LauncherRideTime(obj);
}

/* $029108. Type $248 step 0: on the upper carrier ($D160, y $B0), and a second $248 (from slot
 * $D580 on) at step 4 for the lower one. */
void PairRiderStart(uint16_t obj)
{
    next_state(obj, 8);
    wr16(obj + 0x4A, 2);
    wr16(obj + 0x4C, 0xB0);
    wr16(obj + 0x58, 0xD160);
    uint16_t slot = 0xD580, t;
    if (SlotFree24(&slot, &t)) {
        wr16(slot + O_TYPE, 0x248);
        wr16(slot + O_STATE, 4);
    }
    rider_look(obj);
}

/* $02913A. Step 4: on the lower carrier ($CD40, y $140). */
void PairRiderLower(uint16_t obj)
{
    next_state(obj, 4);
    wr16(obj + 0x4A, 0);
    wr16(obj + 0x4C, 0x140);
    wr16(obj + 0x58, 0xCD40);
    rider_look(obj);
}

/* $029172. Step 8: riding, blinking; then off at its carrier's y, harmful, sound $C7. */
void PairRiderRide(uint16_t obj)
{
    if (count_down(obj + 0x5C)) {
        next_state(obj, 4);
        wr8(obj + 0x21, 0x10);
        wr32(obj + 0x2C, 0xF808F808);
        wr16(obj + 0x26, 0xA);
        wr16(obj + O_FLAGS, 0x8800);
        wr16(obj + O_Y, rd16(obj + 0x4C));
        QueueSound(0xC7);
        return;
    }
    if (!(rd16(v_gameFrames) & 8)) { wr16(obj + O_FLAGS, 0); return; }
    wr16(obj + O_FLAGS, 0x8800);
    launcher_ride(obj);
}

/* $0291C6. Step $C: crossing, turned on odd frames; shot down: a puff. */
void PairRiderCross(uint16_t obj)
{
    if (bit8(obj + 0x22, 4)) { launcher_shot(obj); return; }
    LauncherSpeed(obj);
    LauncherOut(obj);
    wr16(obj + 0x0E, bit8(FRAMES_LO, 0) ? 0x6260 : 0x4260);
}

/* $0290EE. Type $248. */
void PairRider(uint16_t obj)
{
    call_object(rom32(0x0290F8 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $25C: a shell flying along its angle (+$56), bursting into six puffs ---- */

/* $029466. Type $25C. First: harmful, a puff ($1E4) at it moving at 1/8 of the sine along its
 * angle, it at the whole sine. Then: gone outside x $80-$240; hit, below y $150 or above $A8
 * it bursts: six puffs at angles from the table at $295C4, at 1/4 of the sine, 2 more to the
 * left, pushed toward y $100 by 1/2. */
void Shell25C(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next_state(obj, 4);
        wr16(obj + O_FLAGS, 0x8C00);
        wr16(obj + 0x0E, 0x64B5);
        wr16(obj + 0x08, 0x500);
        wr16(obj + 0x0A, 0xF8F8);
        wr8(obj + 0x21, 0x10);
        wr32(obj + 0x2C, 0xFC04FC04);
        wr16(obj + 0x26, 4);
        uint16_t d0 = rd16(obj + 0x56);
        uint32_t d1 = ext_shift(sine(d0), 0), d2 = ext_shift(cosine(d0), 0);
        uint16_t slot, t;
        if (!SlotFreeHigh(&slot, &t)) return;
        wr16(slot + O_TYPE, 0x1E4);
        wr32(slot + 0x48, 0x0565D4);
        wr16(slot + 0x54, 0x8C00);
        wr16(slot + O_X, rd16(obj + O_X));
        wr16(slot + O_Y, rd16(obj + O_Y));
        d1 <<= 3;
        d2 <<= 3;
        wr32(slot + 0x1C, d1);
        wr32(slot + 0x18, d2);
        d1 <<= 3;
        d2 <<= 3;
        wr32(obj + 0x1C, d1);
        wr32(obj + 0x18, d2);
        return;
    }
    if ((int16_t)(rd16(obj + O_X) - 0x80) < 0 || (int16_t)(rd16(obj + O_X) - 0x240) >= 0) {
        remove_me(obj);
        return;
    }
    if (!bit8(obj + 0x22, 4) && (int16_t)(rd16(obj + O_Y) - 0x150) < 0
        && (int16_t)(rd16(obj + O_Y) - 0xA8) >= 0) return;
    remove_me(obj);
    wr8(obj + 0x21, 0);
    uint32_t d3 = (int16_t)(rd16(obj + O_Y) - 0x100) >= 0 ? 0xFFFF8000u : 0x8000u;
    for (uint32_t d6 = 0; d6 < 12; d6 += 2) {
        uint16_t slot, t;
        if (!SlotFreeHigh(&slot, &t)) return;
        wr16(slot + O_TYPE, 0x1E4);
        wr16(slot + 0x54, 0x8C00);
        wr32(slot + 0x48, 0x0563EE);
        wr16(slot + O_X, rd16(obj + O_X));
        wr16(slot + O_Y, rd16(obj + O_Y));
        uint16_t a = rom16(0x0295C4 + d6);
        uint32_t d1 = ext_shift(sine(a), 2) + d3;
        uint32_t d2 = ext_shift(cosine(a), 2) - 0x20000;
        wr32(slot + 0x1C, d1);
        wr32(slot + 0x18, d2);
    }
}

/* ---- type $378: a test object moved by pad 2's directions ---- */

/* $02A2C2. Type $378: shown, its look, moved a pixel by each direction held on pad 2 ($FFF707:
 * up, down, left, right), then the walls and the floor. */
void PadMover(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x8C00);
    wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) | 0x8000);
    wr16(obj + 0x0E, 0x84B5);
    wr16(obj + 0x08, 0x500);
    wr16(obj + 0x0A, 0xF8F8);
    if (bit8(0xF707, 0)) wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 1));
    if (bit8(0xF707, 1)) wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) + 1));
    if (bit8(0xF707, 2)) wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) - 1));
    if (bit8(0xF707, 3)) wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) + 1));
    uint16_t d3 = 0, d4 = 0;
    ObjectWalls8(obj, &d3, &d4);
    ObjectFloor16(obj, &d3, &d4);
}

/* ---- types $44, $9C, $A0, $AC: nothing ---- */

/* $02A68A. Types $44, $9C, $A0, $AC: does nothing. */
void ObjectNothing(uint16_t obj)
{
    (void)obj;
}

/* ---- shared helpers ---- */

/* $0560DE. A free slot from *slot (a0) on, to $E000: Z set at one (its type, 0, in d0); none:
 * d0 1, the slot past the last. */
int SlotFreeFrom(uint16_t *slot, uint16_t *type)
{
    uint16_t a = *slot;
    for (;;) {
        *type = rd16(a);
        if (!*type) break;
        a = (uint16_t)(a + 0x60);
        if ((int16_t)(a - 0xE000) >= 0) {
            *slot = a;
            *type = 1;
            return 0;
        }
    }
    *slot = a;
    return 1;
}

/* $0560DA. The same from $C980. */
int SlotFreeFromC980(uint16_t *slot, uint16_t *type)
{
    *slot = 0xC980;
    return SlotFreeFrom(slot, type);
}
