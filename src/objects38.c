/* Objects of the stages (the types from $38464 to $3F774): the figures next to type $BC, the
 * timers' digits, the shaking ceiling's machine, Black's dice palace (type $280), the debris
 * and pieces of the bosses, the seven-form boss (type $284), and the small types after it. */
#include "game.h"

/* ---- shared helpers ---- */

/* loc_03D2BC: the slot made a puff (type $100, its list $565F6), thrown in a random one of 16
 * directions (the sine tables at $2977C, $297FC, times 8) */
static void puff_thrown(uint16_t slot)
{
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x0565F6);
    uint16_t k = rd16(v_randomSeed) & 0x1E0;
    int32_t vy = (int16_t)rom16(0x02977C + k), vx = (int16_t)rom16(0x0297FC + k);
    wr32(slot + 0x1C, (uint32_t)vy << 3);
    wr32(slot + 0x18, (uint32_t)vx << 3);
}

/* $03D2A6. A puff (type $100) at the object, thrown in a random direction. */
void DebrisPuff(uint16_t obj)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    puff_thrown(slot);
}

/* $03D342. Every 8th frame a spark (type $1DC, its list $566BC) at a random place up to 8 from
 * (x, y). */
void SparksAt(uint16_t x, uint16_t y)
{
    if (rd16(v_gameFrames) & 7) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x1DC);
    wr32(slot + 0x48, 0x0566BC);
    wr16(slot + O_X, (uint16_t)((rd8(v_randomSeed) & 0xF) + (uint16_t)(x - 8)));
    wr16(slot + O_Y, (uint16_t)((rd8(0xFF09) & 0xF) + (uint16_t)(y - 8)));
}

/* $03D33A. The same at the object in slot $DE80. */
void SparksAtDE80(void)
{
    SparksAt(rd16(0xDE90), rd16(0xDE94));
}

/* $043906. A group of count+1 objects from this one placed by their offsets (+$48, +$4C) from
 * the leader's (+$50) place; the leader stays. */
void GroupPlace(uint16_t obj, uint16_t count)
{
    uint16_t lead = rd16(obj + 0x50);
    uint16_t x = (uint16_t)(rd16(lead + O_X) - rd16(lead + 0x48));
    uint16_t y = (uint16_t)(rd16(lead + O_Y) - rd16(lead + 0x4C));
    uint16_t a = obj;
    for (;;) {
        if (a != rd16(obj + 0x50)) {
            wr16(a + O_X, (uint16_t)(x + rd16(a + 0x48)));
            wr16(a + O_Y, (uint16_t)(y + rd16(a + 0x4C)));
        }
        a = (uint16_t)(a + 0x60);
        if (count-- == 0) break;
    }
}

/* ---- the timers: type $31C counting down M:SS:FF (frames by 2, a tick each second, at 0 its
 * state 8 and +$4E 1), type $458 counting up, type $330 two digits; their digits' tiles queued
 * at +$48 (as VRAM), drawn at +$50, +$52 through the sprite list at $A100 (only on the frames
 * $A200 & 3 says, the 2-digit one always) ---- */

/* $039278: a digit (`d` & $F) of the sprite list */
static void timer_digit(uint16_t obj, uint16_t d)
{
    d = (uint16_t)((d & 0x0F) << 2);
    wr16(0xA104, (uint16_t)((rd16(obj + 0x48) >> 5) + 0x8000 + d + 1));
    DrawSpriteList(0xFFFFA100);
}

static void timer_list(uint16_t obj)
{
    wr16(0xA106, rd16(obj + 0x50));
    wr16(0xA100, rd16(obj + 0x52));
    wr16(0xA102, 0x500);
    wr16(0xA108, 0xFFFF);
}

/* $0391E6: M:SS:FF's digits, the colons and the frame */
static void timer_digits(uint16_t obj)
{
    timer_digit(obj, rd8(obj + 0x4B));
    wr16(0xA106, (uint16_t)(rd16(0xA106) + 0x1C));
    timer_digit(obj, (uint16_t)(rd8(obj + 0x4C) >> 4));
    wr16(0xA106, (uint16_t)(rd16(0xA106) + 0x10));
    timer_digit(obj, rd8(obj + 0x4C));
    wr16(0xA106, (uint16_t)(rd16(0xA106) + 0x1C));
    timer_digit(obj, (uint16_t)(rd8(obj + 0x4D) >> 4));
    wr16(0xA106, (uint16_t)(rd16(0xA106) + 0x10));
    timer_digit(obj, rd8(obj + 0x4D));
    wr16(0xA106, (uint16_t)(rd16(obj + 0x50) + (rd16(obj + O_TYPE) == 0x458 ? 0x20 : 0x10)));
    wr16(0xA102, 0);
    wr16(0xA104, (uint16_t)((rd16(obj + 0x48) >> 5) + 0x8000));
    DrawSpriteList(0xFFFFA100);
    wr16(0xA106, (uint16_t)(rd16(0xA106) + 0x2C));
    DrawSpriteList(0xFFFFA100);
    wr16(0xA106, (uint16_t)(rd16(0xA106) + 4));
    DrawSpriteList(0xFFFFA100);
}

/* $0391CA */
static void timer_draw(uint16_t obj)
{
    timer_list(obj);
    timer_digits(obj);
}

/* $03910C: no message showing: the digits' tiles (a block at $D4D76, ten at $D4F00) queued */
void TimerStart(uint16_t obj)
{
    if (rd16(0xF720)) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x4E, 0);
    uint16_t vram = rd16(obj + 0x48), q = rd16(0xF70C);
    QueueDmaBlock(&q, 0x0D4D76, &vram);
    for (uint32_t src = 0x0D4F00, k = 0; k < 10; k++, src += 0x82) QueueDmaBlock(&q, src, &vram);
    wr16(0xF70C, q);
}

/* $039166 */
void TimerDown(uint16_t obj)
{
    uint8_t f = rd8(obj + 0x4D), s = rd8(obj + 0x4C), m = rd8(obj + 0x4B);
    int x = 0;
    f = bcd_sub8(f, 2, &x);
    if (x) {
        QueueSound(0xAD);
        x = 0;
        s = bcd_sub8(s, 1, &x);
        if (x) {
            s = bcd_sub8(s, 0x39, &x);
            x = 0;
            m = bcd_sub8(m, 1, &x);
            if (x) {
                wr16(obj + O_STATE, 8);
                wr16(obj + 0x4E, 1);
                f = s = m = 0;
            }
        }
    }
    wr8(obj + 0x4D, f);
    wr8(obj + 0x4C, s);
    wr8(obj + 0x4B, m);
    timer_draw(obj);
}

/* $03915A: run out: blinking */
void TimerOut(uint16_t obj)
{
    if (bit8(v_gameFrames + 1, 3)) timer_draw(obj);
}

/* $0390EA */
void Timer(uint16_t obj)
{
    if (!(rd8(0xA200) & 3)) return;
    call_object(rom32(0x039100 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $039346: the count-up's hours digit, then the rest */
static void clock_draw(uint16_t obj)
{
    timer_list(obj);
    timer_digit(obj, (uint16_t)(rd8(obj + 0x4B) >> 4));
    wr16(0xA106, (uint16_t)(rd16(0xA106) + 0x10));
    timer_digits(obj);
}

/* $0392BC */
void ClockStart(uint16_t obj)
{
    TimerStart(obj);
    wr16(obj + 0x4A, 0);
    wr16(obj + 0x4C, 0);
}

/* $0392DC: counting up (seconds to 60, then the minutes; at 99 stopped as the timer's end) */
void ClockUp(uint16_t obj)
{
    uint8_t f = rd8(obj + 0x4D), s = rd8(obj + 0x4C), m = rd8(obj + 0x4B);
    int x = 0;
    f = bcd_add8(f, 2, &x);
    if (x) {
        QueueSound(0xAD);
        x = 0;
        s = bcd_add8(s, 1, &x);
        if ((int8_t)(s - 0x60) >= 0) {
            s = 0;
            x = 0;
            m = bcd_add8(m, 1, &x);
            if (x) {
                m = 0;
                wr16(obj + O_STATE, 8);
                wr16(obj + 0x4E, 1);
                f = s = m = 0;
            }
        }
    }
    wr8(obj + 0x4D, f);
    wr8(obj + 0x4C, s);
    wr8(obj + 0x4B, m);
    clock_draw(obj);
}

/* $0392CC */
void ClockOut(uint16_t obj)
{
    if (bit8(v_gameFrames + 1, 3)) clock_draw(obj);
}

/* $0392D8 */
void ClockShow(uint16_t obj)
{
    clock_draw(obj);
}

/* $039296 */
void Clock(uint16_t obj)
{
    if (!(rd8(0xA200) & 3)) return;
    call_object(rom32(0x0392AC + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $039418: two digits of +$4A */
static void counter_draw(uint16_t obj)
{
    timer_list(obj);
    timer_digit(obj, (uint16_t)((int16_t)rd16(obj + 0x4A) >> 4));
    wr16(0xA106, (uint16_t)(rd16(0xA106) + 0x12));
    timer_digit(obj, rd16(obj + 0x4A));
}

/* $039390 */
void CounterStart(uint16_t obj)
{
    wr16(obj + 0x4C, 0x60);
    TimerStart(obj);
}

/* $03939A: shown $60 frames; then counting up from 0 (down from more) each $32 frames */
void CounterWait(uint16_t obj)
{
    wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) - 1));
    if ((int16_t)rd16(obj + 0x4C) < 0) {
        wr16(obj + 0x4E, 0x31);
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + (rd16(obj + 0x4A) ? 8 : 4)));
    }
    counter_draw(obj);
}

/* $0393C0: up to 10 (then shown $40 again) */
void CounterUp(uint16_t obj)
{
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 1));
    if ((int16_t)rd16(obj + 0x4E) < 0) {
        wr16(obj + 0x4E, 0x31);
        uint16_t v = rd16(obj + 0x4A);
        int x = 0;
        v = (uint16_t)((v & 0xFF00) | bcd_add8((uint8_t)v, 1, &x));
        wr16(obj + 0x4A, v);
        if (v == 0x10) {
            wr16(obj + O_STATE, 4);
            wr16(obj + 0x4C, 0x40);
        }
    }
    counter_draw(obj);
}

/* $0393EA: down to 0 (then shown $40 again) */
void CounterDown(uint16_t obj)
{
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 1));
    if ((int16_t)rd16(obj + 0x4E) < 0) {
        wr16(obj + 0x4E, 0x31);
        uint16_t v = rd16(obj + 0x4A);
        int x = 0;
        v = (uint16_t)((v & 0xFF00) | bcd_sub8((uint8_t)v, 1, &x));
        wr16(obj + 0x4A, v);
        if (!v) {
            wr16(obj + O_STATE, 4);
            wr16(obj + 0x4C, 0x40);
        }
    }
    counter_draw(obj);
}

/* $039376 */
void Counter(uint16_t obj)
{
    call_object(rom32(0x039380 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $258: a splinter flung along its angle (+$48), blinking; struck or down at $140, a
 * puff ($100) drifting ---- */

/* $03AB84 */
void Splinter(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + O_FLAGS, 0x8D00);
        wr16(obj + 0x0E, 0x04B5);
        wr16(obj + 0x08, 0x0500);
        wr16(obj + 0x0A, 0xF8F8);
        wr8(obj + 0x21, 0x50);
        wr32(obj + 0x2C, 0xFD03FD03);
        wr16(obj + 0x26, 4);
        uint32_t a = (uint32_t)(int16_t)rd16(obj + 0x48);
        wr32(obj + 0x1C, (uint32_t)(int32_t)(int16_t)rom16(0x02977C + a) << 5);
        wr32(obj + 0x18, (uint32_t)(int32_t)(int16_t)rom16(0x0297FC + a) << 5);
        return;
    }
    if (bit8(obj + 0x22, 4) || bit8(obj + 0x22, 6) || (int16_t)(rd16(obj + O_Y) - 0x140) >= 0) {
        wr16(obj + O_TYPE, 0x100);
        wr32(obj + 0x48, 0x0565D4);
        wr16(obj + O_STATE, 0);
        wr8(obj + 0x21, 0);
        wr32(obj + 0x18, (uint32_t)((int32_t)rd32(obj + 0x18) >> 3));
        wr32(obj + 0x1C, 0xFFFF0000);
        return;
    }
    if (bit8(v_gameFrames + 1, 0)) {
        wr16(obj + 0x0E, 0x04B5);
        wr16(obj + 0x08, 0x0500);
        wr16(obj + 0x0A, 0xF8F8);
    } else {
        wr16(obj + 0x0E, 0x6481);
        wr16(obj + 0x08, 0);
        wr16(obj + 0x0A, 0xFCFC);
    }
}

/* ---- type $4E4: a beacon coming down; below $130 the weight ($4E8 in $DEE0) dropped once ---- */

/* $03DB94 */
void Beacon(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + O_FLAGS, 0xC100);
        wr16(obj + 0x0E, 0x2000);
        wr32(obj + 0x08, 0x0E097E);
        wr8(obj + 0x20, 0);
        wr16(obj + O_X, 0x150);
        wr16(obj + O_Y, 0xF44);
        return;
    }
    if ((int16_t)(rd16(obj + O_Y) - 0x130) < 0 && rd16(0xDEE0) != 0x4E8) {
        wr16(0xDEE0 + O_TYPE, 0x4E8);
        wr16(0xDEE0 + O_STATE, 0);
    }
    wr32(obj + 0x08, bit8(v_gameFrames + 1, 4) ? 0x0E0996 : 0x0E097E);
}

/* ---- type $4E8: a weight dropped at $120, sparking, bouncing on $12C till it settles ($A028
 * bit 1) ---- */

/* $03DC28 */
void WeightStart(uint16_t obj)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, 0x8C00);
    wr16(obj + 0x0E, 0x468B);
    wr16(obj + O_X, 0x120);
    wr16(obj + O_Y, 0x80);
    wr32(obj + 0x1C, 0x40000);
}

/* $03DC4E */
void WeightFall(uint16_t obj)
{
    uint32_t v = rd32(obj + 0x1C) + 0x3000;
    wr32(obj + 0x1C, v);
    if ((v & 0x80000000u) || (int16_t)(rd16(obj + O_Y) - 0x12C) < 0) return;
    wr16(obj + O_Y, 0x12C);
    if ((int32_t)(v - 0x6000) < 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr32(obj + 0x1C, 0);
        wr8(0xA028, (uint8_t)(rd8(0xA028) | 2));
        return;
    }
    wr32(obj + 0x1C, (uint32_t)((int32_t)(0u - v) >> 1));
    QueueSound(0xDC);
}

/* $03DC9C */
void WeightRest(uint16_t obj)
{
    (void)obj;
}

/* $03DC04 */
void Weight(uint16_t obj)
{
    SparksAt(rd16(obj + O_X), rd16(obj + O_Y));
    call_object(rom32(0x03DC1C + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $28C: creeping right a pixel each $40 frames up to $90 ---- */

/* $03F564 */
void Creeper(uint16_t obj)
{
    if ((int16_t)(rd16(obj + O_X) - 0x90) >= 0 || (rd16(v_gameFrames) & 0x3F)) return;
    wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) + 1));
}

/* ---- type $288: an afterimage for 7 frames (while $983C), carried by $9834/$9838, shown on
 * alternate frames by its slot ---- */

/* $03F5EA */
static void afterimage_blink(uint16_t obj)
{
    int odd = bit8(v_gameFrames + 1, 0);
    if (obj & 0x20) wr16(obj + O_FLAGS, odd ? 0xC000 : 0);
    else wr16(obj + O_FLAGS, odd ? 0 : 0xC000);
}

/* $03F592 */
void Afterimage(uint16_t obj)
{
    if (!rd16(0x983C)) { wr16(obj + O_FLAGS, 0x1000); return; }
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 1));
        wr8(obj + 0x20, 0x80);
        wr16(obj + 0x48, 7);
        wr16(obj + O_FLAGS, 0xC000);
        wr32(obj + 0x08, 0x0E0930);
        wr16(obj + 0x0C, 0);
    }
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) < 0) { wr16(obj + O_FLAGS, 0x1000); return; }
    afterimage_blink(obj);
    wr32(obj + O_X, rd32(obj + O_X) + rd32(0x9834));
    wr32(obj + O_Y, rd32(obj + O_Y) + rd32(0x9838));
}

/* ---- types $298 and $294: embers thrown out in a random direction from `radius` round
 * ($100 - $9800, $120 + $9804), slowing, drifting back with $9834/$9838 ---- */

/* $03F6CE */
void EmberPlace(uint16_t obj, uint16_t radius)
{
    uint16_t a = rd16(v_randomSeed) & 0x1F0;
    uint16_t s = rom16(0x02977C + a), c = rom16(0x0297FC + a);
    wr16(obj + 0x50, s);
    wr16(obj + 0x52, c);
    wr32(obj + 0x1C, (uint32_t)(int32_t)(int16_t)s << 5);
    wr32(obj + 0x18, (uint32_t)(int32_t)(int16_t)c << 5);
    uint16_t y = (uint16_t)(((uint32_t)((int32_t)(int16_t)(uint16_t)~s * (int16_t)radius) << 2) >> 16);
    uint16_t x = (uint16_t)(((uint32_t)((int32_t)(int16_t)(uint16_t)~c * (int16_t)radius) << 2) >> 16);
    wr16(obj + O_Y, (uint16_t)(y + 0x100 - rd16(0x9800)));
    wr16(obj + O_X, (uint16_t)(x + 0x120 + rd16(0x9804)));
}

/* $03F734 */
void EmberDrift(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) - (uint32_t)(int32_t)(int16_t)rd16(obj + 0x50));
    wr32(obj + 0x18, rd32(obj + 0x18) - (uint32_t)(int32_t)(int16_t)rd16(obj + 0x52));
    wr32(obj + 0x18, rd32(obj + 0x18) - (uint32_t)((int32_t)rd32(0x9834) >> 3));
    wr32(obj + 0x1C, rd32(obj + 0x1C) - (uint32_t)((int32_t)rd32(0x9838) >> 3));
}

/* $03F65A: animated ($563EE), $A850 round */
void EmberRing(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        EmberPlace(obj, rd16(0xA850));
        wr32(obj + 0x48, 0x0563EE);
        AnimList(obj);
        wr16(obj + O_FLAGS, 0x8C00);
        return;
    }
    EmberDrift(obj);
    AnimList(obj);
}

/* $03F68A: $20 frames, $B0 round */
void Fleck(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 1));
        wr16(obj + O_FLAGS, 0x8C00);
        wr16(obj + 0x0E, 0xE7FD);
        wr16(obj + 0x08, 0);
        wr16(obj + 0x0A, 0xFCFC);
        wr16(obj + 0x48, 0x20);
        EmberPlace(obj, 0xB0);
        return;
    }
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) >= 0) { EmberDrift(obj); return; }
    wr16(obj + O_FLAGS, 0x1000);
}

/* ---- type $254: a furnace in plane A (+6: its place, 0 the boss's own), its doors' tiles
 * ($3AA0A by +6) flickering, breathing flame (splinters, $258) in a sweep; soldiers ($48) from
 * the sides; the boss's own rolls to and fro; beaten: shaking, debris, its picture broken ---- */

/* $03A906 / loc_03A910: its doors (the frame's bit 4 or the caller's flag: their other look) */
static void furnace_doors(uint16_t obj, int lit)
{
    uint16_t at = rom16(0x03AA0A + (uint32_t)(int16_t)rd16(obj + 0x06));
    PlaneAChunk(lit ? 0xFFFF4AE0 : 0xFFFF4AC0, at);
    PlaneAChunk(lit ? 0xFFFF4BC0 : 0xFFFF4BA0, (uint16_t)(at + 0x200));
}

/* $03A906 */
static void furnace_draw(uint16_t obj)
{
    furnace_doors(obj, bit8(v_gameFrames + 1, 4));
}

/* $03A8D6: (the caller's btst its Z) open, or as furnace_draw's dark look */
static void furnace_mouth(uint16_t obj, int set)
{
    if (set) {
        furnace_doors(obj, 0);
        return;
    }
    uint16_t at = rom16(0x03AA0A + (uint32_t)(int16_t)rd16(obj + 0x06));
    PlaneAChunk(0xFFFF4AA0, at);
    PlaneAChunk(0xFFFF4B80, (uint16_t)(at + 0x200));
}

/* $03AB44: every $40 frames a soldier ($48) at a side */
static void furnace_soldier(void)
{
    if (rd16(v_gameFrames) & 0x3F) return;
    uint16_t slot = 0xCC80, t;
    if (!SlotFreeBelowCE(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x48);
    wr16(slot + 0x06, 0);
    wr16(slot + O_X, (rd16(v_gameFrames) & 0xC0) ? 0x1C0 : 0x80);
}

/* $03AB3C: the boss's own only */
static void furnace_soldier_boss(uint16_t obj)
{
    if (!rd16(obj + 0x06)) furnace_soldier();
}

/* $03AADA: the lines' scroll with it */
static void furnace_scroll(uint16_t obj)
{
    uint16_t v = (uint16_t)(rd16(obj + O_X) + 0xFB20);
    for (uint16_t a = 0xE400, k = 0; k < 19; k++, a = (uint16_t)(a + 0x20)) wr16(a, v);
}

/* $03AA2A: a splinter of flame ($258) at its mouth, its angle sweeping $A6-$E6, and a puff */
static void furnace_flame(uint16_t obj)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x258);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, (uint16_t)(rd16(obj + O_Y) + 0x42));
    if (!rd16(obj + 0x4C)) {
        wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) + 8));
        if ((int16_t)(rd16(obj + 0x4A) - 0x40) >= 0) {
            wr16(obj + 0x4A, 0x40);
            wr16(obj + 0x4C, 2);
        }
    } else {
        wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) - 8));
        if ((int16_t)rd16(obj + 0x4A) < 0) {
            wr16(obj + 0x4A, 0);
            wr16(obj + 0x4C, 0);
        }
    }
    wr16(slot + 0x48, (uint16_t)((rd16(obj + 0x4A) + 0xA6) & 0x1FE));
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x0565F6);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, (uint16_t)(rd16(obj + O_Y) + 0x42));
}

/* the ten tiles of a broken picture from `at` on */
static void furnace_rubble(uint16_t at)
{
    for (uint32_t t = 0x03AA0E, k = 0; k < 10; k++, t += 2) {
        at = (uint16_t)(at + rom16(t));
        PlaneAChunk(0xFFFF4000, at);
    }
}

/* $03A5C4: its count by the difficulty; the boss's own waiting for the camera */
void FurnaceStart(uint16_t obj)
{
    wr8(obj + 0x21, 0x20);
    wr32(obj + 0x28, 0xD828E038);
    wr16(obj + 0x48, rom16(0x03AACA + (uint32_t)(int16_t)rd16(0xA20C)));
    if (!rd16(obj + 0x06)) {
        wr16(obj + O_STATE, 0x18);
        wr16(obj + O_FLAGS, 0x0900);
        wr16(obj + O_VITALITY, 0x7FFF);
        wr8(v_cameraHold, (uint8_t)(rd8(v_cameraHold) | 1));
        return;
    }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + O_FLAGS, 0x0100);
    wr16(obj + O_VITALITY, 0x12C);
}

/* $03A616: gone off the left ($60); its count out (past $A0): opening */
void FurnaceWait(uint16_t obj)
{
    if ((int16_t)(rd16(obj + O_X) - 0x60) < 0) { wr16(obj + O_FLAGS, 0x1000); return; }
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) >= 0 || (int16_t)(rd16(obj + O_X) - 0xA0) < 0) {
        furnace_draw(obj);
        return;
    }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x48, 0x40);
}

/* $03A64E: opening (the doors on the frames' bit 3) */
void FurnaceOpen(uint16_t obj)
{
    furnace_soldier_boss(obj);
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) < 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + 0x48, 0x90);
        wr16(obj + 0x4A, 0);
        wr16(obj + 0x4C, 0);
        return;
    }
    furnace_mouth(obj, bit8(v_gameFrames + 1, 3));
}

/* $03A644 */
void FurnaceOpenOn(uint16_t obj)
{
    if ((int16_t)(rd16(obj + O_X) - 0x60) < 0) { wr16(obj + O_FLAGS, 0x1000); return; }
    FurnaceOpen(obj);
}

/* $03A688: breathing flame for $90 frames (a splinter each 4th, a roar), then waiting again (the
 * boss's own: rolling) */
void FurnaceBreathe(uint16_t obj)
{
    furnace_soldier_boss(obj);
    wr16(v_shakeSprites, 4);
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) < 0) {
        wr16(obj + 0x48, rom16(0x03AACA + (uint32_t)(int16_t)rd16(0xA20C)));
        if (!rd16(obj + 0x06)) {
            wr16(obj + O_STATE, 0x1C);
            wr16(obj + 0x48, 0x20);
        } else {
            wr16(obj + O_STATE, 4);
        }
        return;
    }
    furnace_mouth(obj, bit8(v_gameFrames + 1, 2));
    if (rd16(v_gameFrames) & 3) return;
    furnace_flame(obj);
    QueueSound(0xCB);
}

/* $03A67E */
void FurnaceBreatheOn(uint16_t obj)
{
    if ((int16_t)(rd16(obj + O_X) - 0x60) < 0) { wr16(obj + O_FLAGS, 0x1000); return; }
    FurnaceBreathe(obj);
}

/* $03A6EE: beaten: shaking $40 frames, debris thrown, its place flickering; then broken */
void FurnaceBeaten(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 0x0F)) QueueSound(0xC5);
    if (!rd16(obj + 0x06)) furnace_scroll(obj);
    wr16(v_shakeSprites, 7);
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) < 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        furnace_rubble((uint16_t)(rom16(0x03AA0A + (uint32_t)(int16_t)rd16(obj + 0x06)) - 0x208));
        return;
    }
    wr16(obj + O_X, bit8(v_gameFrames + 1, 0) ? rd16(obj + 0x5C) : 0x40);
    furnace_draw(obj);
    DebrisEmit(obj);
}

/* $03A99E: its picture broken */
static void furnace_broken(uint16_t obj)
{
    uint16_t at = rom16(0x03AA0A + (uint32_t)(int16_t)rd16(obj + 0x06));
    uint32_t src = rd16(obj + 0x06) ? 0xFFFF4E00 : 0xFFFF4000;
    PlaneAChunk(src, (uint16_t)(at + 0x600));
    PlaneAChunk(src, (uint16_t)(at + 0x608));
    furnace_rubble((uint16_t)(at + 0x1F8));
}

/* $03A74A: gone (the boss's own: the camera let go; else the lamps' colours out) */
void FurnaceGone(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x1000);
    furnace_broken(obj);
    if (!rd16(obj + 0x06)) {
        wr8(v_cameraHold, (uint8_t)(rd8(v_cameraHold) & ~0x80));
        wr16(0x80C2, 0x58);
        return;
    }
    wr32(0x6284, 0);
    wr32(0x628C, 0);
    wr32(0x6294, 0);
    wr32(0x629C, 0);
    wr8(0x62A5, 0xB2);
    wr8(0x62A6, 0xB2);
    wr8(0x62C0, 0);
    wr8(0x62C8, 0);
    wr8(0x62D0, 0);
    wr8(0x62D8, 0);
}

/* $03A76E: the boss's own: the camera stopped, the fight ($370 its vitality) */
void FurnaceBossWait(uint16_t obj)
{
    wr16(obj + O_VITALITY, 0x7FFF);
    if (!bit8(v_cameraHold, 7)) { furnace_draw(obj); return; }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x48, 0x40);
    wr16(obj + O_VITALITY, 0x370);
    wr16(0x80C2, 0x10);
    wr16(0x80D6, 0xC9E0);
    wr8(v_planeFlagsA, (uint8_t)(rd8(v_planeFlagsA) | 0x80));
    uint16_t v = (uint16_t)-rd16(0xA900);
    for (uint16_t a = 0xE400, k = 0; k < 28; k++, a = (uint16_t)(a + 0x20)) wr16(a, v);
}

/* $03A7BC: open a while; then rolling toward the middle (or away, by chance) */
void FurnaceBossOpen(uint16_t obj)
{
    furnace_soldier();
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if ((int16_t)rd16(obj + 0x48) >= 0) {
        furnace_mouth(obj, bit8(v_gameFrames + 1, 3));
        return;
    }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x48, (uint16_t)((rd16(v_gameFrames) & 0x7F) + 0x10));
    int left;
    if (bit8(0xFF09, 0)) left = (int16_t)(rd16(obj + O_X) - 0x170) < 0;
    else left = (int16_t)(rd16(obj + O_X) - 0x100) < 0;
    wr16(obj + 0x4A, left ? 2 : 0);
}

/* $03A80A: rolling ($1000 a frame up to 2) till its count or the edge ($E0 / $190): then a jolt */
void FurnaceBossRoll(uint16_t obj)
{
    furnace_soldier();
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    int stop = (int16_t)rd16(obj + 0x48) < 0;
    if (!stop) {
        if (!rd16(obj + 0x4A)) {
            if ((int32_t)(rd32(obj + 0x18) - 0xFFFE0000u) >= 0) wr32(obj + 0x18, rd32(obj + 0x18) - 0x1000);
            stop = (int16_t)(rd16(obj + O_X) - 0xE0) < 0;
        } else {
            if ((int32_t)(rd32(obj + 0x18) - 0x20000) < 0) wr32(obj + 0x18, rd32(obj + 0x18) + 0x1000);
            stop = (int16_t)(rd16(obj + O_X) - 0x190) >= 0;
        }
    }
    if (stop) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr32(obj + 0x18, 0);
        wr16(obj + 0x48, rd16(obj + O_X));
        wr16(obj + 0x4A, 8);
        wr16(obj + 0x50, 0xFFFF);
        return;
    }
    furnace_draw(obj);
    furnace_scroll(obj);
}

/* $03A886: the jolt: shaking about where it stopped 8 frames */
void FurnaceBossJolt(uint16_t obj)
{
    furnace_soldier();
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) + 1));
    furnace_scroll(obj);
    furnace_mouth(obj, bit8(v_gameFrames + 1, 0));
    if (bit8(obj + 0x51, 1)) { wr16(obj + O_X, rd16(obj + 0x48)); return; }
    wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) - 1));
    if ((int16_t)rd16(obj + 0x4A) < 0) {
        wr16(obj + O_X, rd16(obj + 0x48));
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + 0x48, 0x40);
        return;
    }
    wr16(obj + O_X, (uint16_t)(rd16(obj + 0x48) + rd16(obj + 0x4A)));
}

/* $03A53C: beaten: shaking (state $10), debris (puffs, $100) round it */
void Furnace(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) & 0x8000) {
        wr16(obj + O_STATE, 0x10);
        wr16(obj + O_VITALITY, 0);
        wr8(obj + 0x21, 0);
        wr16(obj + 0x48, 0x40);
        wr16(obj + 0x4A, 0);
        wr16(obj + 0x4C, 0x1F);
        wr16(obj + 0x4E, 0x1F);
        wr16(obj + 0x50, 0);
        wr16(obj + 0x52, 0xFFFD);
        wr32(obj + 0x54, 0x0563EE);
        wr16(obj + 0x58, 0x100);
        wr16(obj + 0x5A, 0);
        wr16(obj + 0x5C, rd16(obj + O_X));
    }
    call_object(rom32(0x03A594 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $2A8: a boss of 18 parts ($C980 its body; its arms' links at +$48/+$4C, the two
 * chains from $CA28 and $CA88; its hands $CC80 and $CEC0, their claws $CF20-$CFE0) reaching
 * for its player, stamping, its claws sparking ($2AC); beaten: 68,040 points, falling away ---- */

static void clawer_looks(uint32_t look)
{
    wr32(0xCF28, look);
    wr16(0xCF2C, 0);
    wr32(0xCF88, look);
    wr16(0xCF8C, 0);
    wr32(0xCFE8, look);
    wr16(0xCFEC, 0);
}

static void clawer_claws_flip(int on)
{
    static const uint16_t c[3] = { 0xCF2E, 0xCF8E, 0xCFEE };
    for (int k = 0; k < 3; k++) wr16(c[k], on ? (uint16_t)(rd16(c[k]) | 0x800) : (uint16_t)(rd16(c[k]) & 0xF7FF));
}

/* $040264: the hands moving with it */
static void clawer_hands_speed(uint16_t obj)
{
    wr32(0xCC98, rd32(obj + 0x18));
    wr32(0xCED8, rd32(obj + 0x18));
    wr32(0xCC9C, rd32(obj + 0x1C));
    wr32(0xCEDC, rd32(obj + 0x1C));
}

/* $04017C */
static void clawer_arms_set(uint16_t obj)
{
    wr16(obj + 0x56, rd16(obj + 0x54));
    wr16(obj + 0x52, rd16(obj + 0x58));
}

/* $04018A: its player's side: worn down (below $3E8), reaching; else leaping at it (or not) */
static void clawer_aim(uint16_t obj)
{
    uint16_t px = rd16(PlayerTarget() + O_X);
    if ((int16_t)(rd16(obj + O_VITALITY) - 0x3E8) < 0) {
        wr16(obj + O_STATE, 0x24);
        wr32(0xCF28, 0x0E00CC);
        wr16(0xCF2C, 0);
        wr32(0xCF88, 0x0DFC4E);
        wr16(0xCF8C, 0);
        wr32(0xCFE8, 0x0DFC8E);
        wr16(0xCFEC, 0);
        return;
    }
    if ((int16_t)(px - rd16(0xC9F0)) < 0) {
        if ((int16_t)(rd16(obj + O_X) - 0x80) < 0) return;
        wr32(obj + 0x18, 0xFFFF4000);
        clawer_claws_flip(1);
    } else if ((int16_t)(px - rd16(0xCA50)) >= 0) {
        wr32(obj + 0x18, 0xC000);
        clawer_claws_flip(0);
    } else {
        if (rd16(v_randomSeed) & 1) return;
        wr16(obj + O_STATE, 0x14);
        return;
    }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr32(obj + 0x1C, 0xFFF90000);
    wr32(0xCF28, 0x0E002A);
    wr16(0xCF2C, 0);
    wr32(0xCF88, 0x0E002A);
    wr16(0xCFEC, 0);
    clawer_hands_speed(obj);
}

/* $0400CE: crouching (to -$18, the claws open, then its aim); or rising back */
static void clawer_crouch(uint16_t obj)
{
    if (!rd16(obj + 0x5A)) {
        if ((int16_t)(rd16(obj + 0x58) - 6) < 0) wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) + 1));
        wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) - 1));
        if ((int16_t)(rd16(obj + 0x54) - 0xFFE8) < 0) {
            wr16(obj + 0x54, 0xFFE8);
            wr16(obj + 0x5A, 2);
            clawer_looks(0x0E002A);
            clawer_aim(obj);
        }
    } else {
        if ((int16_t)rd16(obj + 0x58) >= 0) wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 1));
        wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) + 1));
        if (!(rd16(obj + 0x54) & 0x8000)) {
            wr16(obj + 0x54, 0);
            wr16(obj + 0x5A, 0);
            clawer_looks(0x0DFF4E);
        }
    }
    clawer_arms_set(obj);
}

/* $040088: standing up */
static void clawer_stand(uint16_t obj)
{
    if ((int16_t)rd16(obj + 0x58) >= 0) wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 1));
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) + 4));
    if (!(rd16(obj + 0x54) & 0x8000)) wr16(obj + 0x54, 0);
    clawer_arms_set(obj);
}

/* $03FF6E: the links of its arms and legs from its pose: the floor's line below the camera */
static uint16_t clawer_pose(uint16_t obj)
{
    wr16(obj + 0x48, 0);
    wr16(obj + 0x4C, 0);
    wr16(0xCA28, 0xFFE4);
    wr16(0xCA2C, 0);
    wr16(0xCA88, 0x1C);
    wr16(0xCA8C, 0);
    uint16_t d = (uint16_t)(rd16(0xCA28) - 0x0C + rd16(obj + 0x4A));
    wr16(0xCAE8, d);
    d = (uint16_t)(d - 0x10);
    for (uint16_t a = 0xCB48; a <= 0xCCC8; a = (uint16_t)(a + 0x60)) wr16(a, d);
    d = (uint16_t)(rd16(0xCA2C) + rd16(obj + 0x58));
    wr16(0xCAEC, d);
    wr16(0xCB4C, d);
    d = (uint16_t)(d + 0x10);
    wr16(0xCBAC, d);
    d = (uint16_t)(d + 0x20 + rd16(obj + 0x54));
    wr16(0xCC0C, d);
    d = (uint16_t)(d + 0x20 + rd16(obj + 0x54));
    wr16(0xCC6C, d);
    wr16(0xCCCC, (uint16_t)(d + 0x18));
    d = (uint16_t)(rd16(0xCA88) + 0x0C + rd16(obj + 0x4E));
    wr16(0xCD28, d);
    d = (uint16_t)(d + 0x10);
    for (uint16_t a = 0xCD88; a <= 0xCF08; a = (uint16_t)(a + 0x60)) wr16(a, d);
    d = (uint16_t)(rd16(0xCA8C) + rd16(obj + 0x52));
    wr16(0xCD2C, d);
    wr16(0xCD8C, d);
    d = (uint16_t)(d + 0x10);
    wr16(0xCDEC, d);
    d = (uint16_t)(d + 0x20 + rd16(obj + 0x56));
    wr16(0xCE4C, d);
    d = (uint16_t)(d + 0x20 + rd16(obj + 0x56));
    wr16(0xCEAC, d);
    wr16(0xCF0C, (uint16_t)(d + 0x18));
    uint16_t x = rd16(obj + 0x48), y = (uint16_t)(rd16(obj + 0x4C) - 0x10);
    wr16(0xCF6C, y);
    y = (uint16_t)(y + 4);
    wr16(0xCFCC, y);
    wr16(0xD02C, y);
    wr16(0xCF68, x);
    wr16(0xCFC8, (uint16_t)(x - 0x18));
    wr16(0xD028, (uint16_t)(x - 0x18 + 0x30));
    return BelowCamera220(0x140);
}

/* $04031A: every 4th frame a spark ($2AC) off `from`, thrown up in a fan */
static void clawer_spark(uint16_t from)
{
    if (rd16(v_gameFrames) & 3) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x2AC);
    wr16(slot + O_X, rd16(from + O_X));
    wr16(slot + O_Y, rd16(from + O_Y));
    uint16_t a = rom16(0x040384 + (((uint16_t)((int16_t)rd16(v_gameFrames) >> 1)) & 0x0E));
    wr32(slot + 0x1C, (uint32_t)(int32_t)(int16_t)rom16(0x02977C + a) << 4);
    wr32(slot + 0x18, (uint32_t)(int32_t)(int16_t)rom16(0x0297FC + a) << 4);
    QueueSound(0xB6);
}

/* $040A74: its face's colours flashing ($C9E0, its hands, $CA40) */
static void clawer_flash(void)
{
    wr16(0xC9E0 + 0x0E, rom16(0x040A6A + (rd16(v_gameFrames) & 2)));
    uint16_t c = rd16(0xC9EE);
    wr16(0xCA4E, c);
    wr16(0xCC8E, c);
    wr16(0xCECE, c);
}

/* $040394 / $0403B2: an arm out / in a pixel each other frame */
static void clawer_reach_out(uint16_t obj)
{
    if (bit8(v_gameFrames + 1, 0) || rd16(obj + 0x4A) == 0xFFF4) return;
    wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) - 1));
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) + 1));
}

static void clawer_reach_in(uint16_t obj)
{
    if (bit8(v_gameFrames + 1, 0) || rd16(obj + 0x4A) == 2) return;
    wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) + 1));
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) - 1));
}

/* $04040C: a hand's lift (`at`: its link's y) rising faster; past -$20, the stamp's wait */
static void clawer_lift(uint16_t obj, uint16_t at)
{
    if ((int16_t)(rd16(at) - 0xFFE0) < 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + 0x5E, rom16(0x03FE0E + (uint32_t)(int16_t)rd16(0xA20C)));
        return;
    }
    wr32(0xCA3C, rd32(0xCA3C) + 0x2000);
    wr16(at, (uint16_t)(rd16(at) - rd16(0xCA3C)));
}

/* $0403DC: a hand coming down 4 a frame: landed, a thump */
static void clawer_stamp(uint16_t obj, uint16_t at)
{
    wr16(at, (uint16_t)(rd16(at) + 4));
    if ((int16_t)rd16(at) < 0) return;
    wr16(at, 0);
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x5E, 0x10);
    QueueSound(0xA6);
}

/* $04043A: its crouch shaking with its count */
static void clawer_shake(uint16_t obj)
{
    wr32(0xCA3C, 0);
    uint16_t d = (uint16_t)((int16_t)rd16(obj + 0x5C) >> 1);
    if (d) {
        if (!bit8(obj + 0x5D, 3)) d = (uint16_t)((d ^ 3) + 1);
        d |= 0xFFFC;
    }
    wr16(obj + 0x54, d);
    wr16(obj + 0x56, d);
}

/* $03FE1E: its parts' looks, sizes ($3FED8, $3FEEA, $3FEFC) */
static void clawer_parts(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x8C00);
    for (uint16_t a = obj, k = 0; k < 18; k++, a = (uint16_t)(a + 0x60)) {
        if (k) {
            wr16(a + O_TYPE, 0x10);
            wr16(a + O_FLAGS, 0x8000);
        }
        wr8(a + 0x20, rom8(0x03FEEA + k));
        uint32_t t = 0x03FEFC + rom8(0x03FED8 + k);
        wr16(a + 0x0E, rom16(t));
        wr16(a + 0x08, rom16(t + 2));
        wr16(a + 0x0A, rom16(t + 4));
    }
    wr16(0xCF22, 0xE000);
    wr32(0xCF28, 0x0E002A);
    wr16(0xCF2C, 0);
    wr16(0xCF82, 0xE000);
    wr32(0xCF88, 0x0E002A);
    wr16(0xCF8C, 0);
    wr16(0xCF82, 0xE000);
    wr32(0xCF88, 0x0E002A);
    wr16(0xCF8C, 0);
    wr16(0xCFE2, 0xE000);
    wr32(0xCFE8, 0x0E002A);
    wr16(0xCFEC, 0);
    wr8(0xCD4E, (uint8_t)(rd8(0xCD4E) | 8));
    wr16(0xCD4A, 0xF6F2);
}

/* $03FF20: its vitality $7F8, its and its hands' hit boxes */
static void clawer_boxes(uint16_t obj)
{
    wr16(obj + O_VITALITY, 0x7F8);
    wr8(obj + 0x21, 0x20);
    wr32(obj + 0x28, 0xF010D030);
    wr8(0xCCA1, 0x24);
    wr8(0xCEE1, 0x24);
    wr32(0xCCAC, 0xF410F010);
    wr32(0xCEEC, 0xF410F010);
    wr32(0xCCA8, 0xE818E818);
    wr32(0xCEE8, 0xE818E818);
    wr8(0xCCC6, 0x8F);
    wr8(0xCF06, 0x8F);
}

static void clawer_place(uint16_t obj)
{
    GroupPlace(obj, 0x11);
}

/* $03FC6A: its lead part on the floor, the rest placed */
static void clawer_on_floor(uint16_t obj)
{
    uint16_t y = clawer_pose(obj);
    wr16(rd16(obj + 0x50) + O_Y, y);
    clawer_place(obj);
}

/* $03FA08: on its left hand */
static void clawer_on_hand(uint16_t obj)
{
    wr16(obj + 0x50, 0xCC80);
    wr16(0xCC94, clawer_pose(obj));
    clawer_place(obj);
}

/* $03F802 */
void ClawerStart(uint16_t obj)
{
    wr16(obj + O_STATE, 0x6C);
    wr16(obj + 0x5E, 0x40);
}

/* $03F810: a while; then its message */
void ClawerWait(uint16_t obj)
{
    wr16(obj + 0x5E, (uint16_t)(rd16(obj + 0x5E) - 1));
    if ((int16_t)rd16(obj + 0x5E) >= 0) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr8(0x8080, (uint8_t)(rd8(0x8080) | 1));
    wr16(0x80C2, 8);
    wr32(0x80C8, 0x03992C);
}

/* $03F832 */
void ClawerMessage(uint16_t obj)
{
    if (rd16(0x80C2)) return;
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x5E, 0x20);
}

/* $03F848: then in at $220, the fight */
void ClawerArrive(uint16_t obj)
{
    wr16(obj + 0x5E, (uint16_t)(rd16(obj + 0x5E) - 1));
    if ((int16_t)rd16(obj + 0x5E) >= 0) return;
    wr16(obj + O_STATE, 4);
    wr16(0x80C2, 0x10);
    wr16(0x80D6, 0xC980);
    wr16(obj + 0x50, 0xC980);
    wr16(obj + O_X, 0x220);
    wr16(obj + O_Y, 0xC0);
    clawer_parts(obj);
    clawer_boxes(obj);
    clawer_pose(obj);
    clawer_place(obj);
}

/* $03F88C: crouching to spring */
void ClawerCrouch(uint16_t obj)
{
    clawer_crouch(obj);
    clawer_on_hand(obj);
}

/* $03F8A8: leaping: on the way down, its claws closed */
void ClawerLeap(uint16_t obj)
{
    uint32_t v = rd32(obj + 0x1C) + 0x4000;
    wr32(obj + 0x1C, v);
    if (!(v & 0x80000000u)) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        clawer_looks(0x0DFF4E);
    }
    clawer_hands_speed(obj);
    clawer_stand(obj);
    wr16(obj + 0x50, 0xC980);
    clawer_pose(obj);
    clawer_place(obj);
}

/* $03F8FE: landing (on its hand's old line): a thump, crouching again */
void ClawerLand(uint16_t obj)
{
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    wr16(obj + 0x50, 0xC980);
    uint16_t y = (uint16_t)(clawer_pose(obj) - 8);
    if ((int16_t)(y - rd16(0xCC94)) < 0) {
        QueueSound(0xA6);
        wr16(obj + O_STATE, 4);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr16(obj + 0x5A, 0);
        clawer_looks(0x0DF81A);
    }
    clawer_place(obj);
    clawer_hands_speed(obj);
}

/* $03F978: beaten, falling (flickering at its place), arms flailing: gone below $160 */
void ClawerFall(uint16_t obj)
{
    wr16(obj + O_X, bit8(v_gameFrames + 1, 0) ? 0x300 : rd16(obj + 0x5E));
    wr32(obj + 0x1C, rd32(obj + 0x1C) + 0x4000);
    wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) - 4));
    wr16(obj + 0x4E, (uint16_t)(rd16(obj + 0x4E) + 4));
    clawer_stand(obj);
    wr16(obj + 0x50, 0xC980);
    clawer_pose(obj);
    clawer_place(obj);
    if ((int16_t)(rd16(obj + O_Y) - 0x160) < 0) return;
    wr8(v_cameraHold, (uint8_t)(rd8(v_cameraHold) & ~0x80));
    wr16(0x80C2, 0x58);
    wr8(0x8080, 0);
    ObjectsVanishFrom(0xC980, 0x2BC, 0);
}

/* $03F9C6: standing, its claws shut */
void ClawerRise(uint16_t obj)
{
    clawer_stand(obj);
    if (!rd16(obj + 0x54)) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        clawer_looks(0x0DF81A);
        wr16(obj + 0x5E, 0xFFF6);
    }
    clawer_on_hand(obj);
}

/* $03FA60: shuddering ten frames, every other; then its claws glow (a count by the difficulty) */
void ClawerShudder(uint16_t obj)
{
    wr16(obj + 0x54, 0);
    wr16(obj + 0x56, 0);
    if (!bit8(v_gameFrames + 1, 0)) {
        wr16(obj + 0x54, rd16(obj + 0x5E));
        wr16(obj + 0x56, rd16(obj + 0x5E));
        wr16(obj + 0x5E, (uint16_t)(rd16(obj + 0x5E) + 1));
        if (!rd16(obj + 0x5E)) {
            wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
            clawer_looks(0x0DF538);
            wr16(obj + 0x5E, rom16(0x03FACC + (uint32_t)(int16_t)rd16(0xA20C)));
        }
    }
    clawer_on_hand(obj);
}

/* $03FA20 */
void ClawerGlow(uint16_t obj)
{
    wr16(obj + 0x5E, (uint16_t)(rd16(obj + 0x5E) - 1));
    if ((int16_t)rd16(obj + 0x5E) < 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + 0x5E, 0x60);
        wr32(0xCF28, 0x0E00CC);
        wr16(0xCF2C, 0);
        wr32(0xCF88, 0x0E002A);
        wr16(0xCF8C, 0);
        wr32(0xCFE8, 0x0E0108);
        wr16(0xCFEC, 0);
    }
    clawer_on_hand(obj);
}

/* $03FADC: sparking, trembling, $60 frames */
void ClawerSpark(uint16_t obj)
{
    wr16(obj + 0x5E, (uint16_t)(rd16(obj + 0x5E) - 1));
    if ((int16_t)rd16(obj + 0x5E) < 0) wr16(obj + O_STATE, 4);
    wr16(obj + 0x54, bit8(v_gameFrames + 1, 1) ? 0 : 0xFFFC);
    wr16(obj + 0x56, rd16(obj + 0x54));
    clawer_spark(0xC980);
    clawer_on_hand(obj);
}

/* $03FB12: stood up: to its stamping (by its side) */
void ClawerStood(uint16_t obj)
{
    clawer_stand(obj);
    if (rd16(obj + 0x54)) { clawer_on_hand(obj); return; }
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
    wr16(obj + 0x5C, 0xFFFF);
    wr16(obj + O_STATE, (int16_t)(rd16(obj + O_X) - 0x120) >= 0 ? 0x2C : 0x4C);
}

/* $03FB44: which hand to lift: by its place */
void ClawerChoose(uint16_t obj)
{
    clawer_flash();
    wr16(obj + 0x5C, 0x0F);
    if ((int16_t)(rd16(obj + O_X) - 0x160) >= 0) {
        clawer_claws_flip(1);
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        clawer_on_hand(obj);
        return;
    }
    if ((int16_t)(rd16(obj + O_X) - 0x100) >= 0) {
        wr16(obj + O_STATE, rd16(obj + 0x5E));
        return;
    }
    clawer_claws_flip(0);
    wr16(obj + O_STATE, 0x4C);
    clawer_on_hand(obj);
}

/* $03FBA0: shaking $10 frames, then the right hand ($C9D4) lifted, reaching out */
void ClawerLiftRight(uint16_t obj)
{
    clawer_flash();
    wr16(obj + 0x5C, (uint16_t)(rd16(obj + 0x5C) - 1));
    if ((int16_t)rd16(obj + 0x5C) >= 0) {
        clawer_shake(obj);
    } else {
        clawer_reach_out(obj);
        clawer_lift(obj, 0xC9D4);
        if (rd16(obj + 0x4A) != 0xFFF4) wr16(obj + O_STATE, 0x2C);
    }
    wr16(obj + 0x50, 0xCEC0);
    clawer_on_floor(obj);
}

/* $03FBDC: a pause */
void ClawerPause(uint16_t obj)
{
    wr16(obj + 0x5E, (uint16_t)(rd16(obj + 0x5E) - 1));
    if ((int16_t)rd16(obj + 0x5E) < 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + 0x5E, 0x40);
    }
    clawer_on_floor(obj);
}

/* $03FC04: its left hand grinding (sparks), the body rocking */
void ClawerGrindLeft(uint16_t obj)
{
    int alt = bit8(v_gameFrames + 1, 1);
    wr16(obj + 0x54, alt ? 0xFFE0 : 0xFFE4);
    wr16(obj + 0x56, alt ? 0 : 0xFFFE);
    wr16(obj + 0x58, alt ? 4 : 0);
    wr16(obj + 0x52, alt ? 2 : 4);
    wr16(obj + 0x5E, (uint16_t)(rd16(obj + 0x5E) - 1));
    if ((int16_t)rd16(obj + 0x5E) < 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + 0x54, 0xFFE0);
        wr16(obj + 0x56, 0);
        wr16(obj + 0x58, 0);
        wr16(obj + 0x52, 0);
    }
    clawer_spark(0xCC80);
    clawer_on_floor(obj);
}

/* $03FC80: the right hand stamping down */
void ClawerStampRight(uint16_t obj)
{
    clawer_flash();
    wr16(obj + 0x5C, 0x0F);
    clawer_stamp(obj, 0xC9D4);
    wr16(obj + 0x50, 0xCEC0);
    clawer_on_floor(obj);
}

/* $03FC98: shaking, then the left hand ($C9D6) lifted, reaching in */
void ClawerLiftLeft(uint16_t obj)
{
    clawer_flash();
    wr16(obj + 0x5C, (uint16_t)(rd16(obj + 0x5C) - 1));
    if ((int16_t)rd16(obj + 0x5C) >= 0) {
        clawer_shake(obj);
    } else {
        clawer_reach_in(obj);
        clawer_lift(obj, 0xC9D6);
        if (rd16(obj + 0x4A) != 2) wr16(obj + O_STATE, 0x3C);
    }
    wr16(obj + 0x50, 0xCC80);
    clawer_on_floor(obj);
}

/* $03FCD4: its right hand grinding */
void ClawerGrindRight(uint16_t obj)
{
    int alt = bit8(v_gameFrames + 1, 1);
    wr16(obj + 0x56, alt ? 0xFFE0 : 0xFFE4);
    wr16(obj + 0x54, alt ? 0 : 0xFFFE);
    wr16(obj + 0x58, alt ? 2 : 4);
    wr16(obj + 0x52, alt ? 4 : 0);
    wr16(obj + 0x5E, (uint16_t)(rd16(obj + 0x5E) - 1));
    if ((int16_t)rd16(obj + 0x5E) < 0) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 4));
        wr16(obj + 0x56, 0xFFE0);
        wr16(obj + 0x54, 0);
        wr16(obj + 0x58, 0);
        wr16(obj + 0x52, 0);
    }
    clawer_spark(0xCEC0);
    clawer_on_floor(obj);
}

/* $03FD3A: the left hand stamping down: then choosing again */
void ClawerStampLeft(uint16_t obj)
{
    clawer_flash();
    clawer_stamp(obj, 0xC9D6);
    if (!rd16(obj + 0x56)) {
        wr16(obj + O_STATE, 0x28);
        wr16(obj + 0x5E, 0x2C);
    }
    clawer_on_floor(obj);
}

/* $03FD5C: (the other side) shaking, the left hand lifted reaching out */
void ClawerLiftLeftOut(uint16_t obj)
{
    clawer_flash();
    wr16(obj + 0x5C, (uint16_t)(rd16(obj + 0x5C) - 1));
    if ((int16_t)rd16(obj + 0x5C) >= 0) {
        clawer_shake(obj);
    } else {
        clawer_reach_out(obj);
        clawer_lift(obj, 0xC9D6);
        if (rd16(obj + 0x4A) != 0xFFF4) wr16(obj + O_STATE, 0x4C);
    }
    wr16(obj + 0x50, 0xCC80);
    clawer_on_floor(obj);
}

/* $03FD98 */
void ClawerStampLeftOut(uint16_t obj)
{
    clawer_flash();
    wr16(obj + 0x5C, 0x0F);
    clawer_stamp(obj, 0xC9D6);
    wr16(obj + 0x50, 0xCC80);
    clawer_on_floor(obj);
}

/* $03FDB0: shaking, the right hand lifted reaching in */
void ClawerLiftRightIn(uint16_t obj)
{
    clawer_flash();
    wr16(obj + 0x5C, (uint16_t)(rd16(obj + 0x5C) - 1));
    if ((int16_t)rd16(obj + 0x5C) >= 0) {
        clawer_shake(obj);
    } else {
        clawer_reach_in(obj);
        clawer_lift(obj, 0xC9D4);
        if (rd16(obj + 0x4A) != 2) wr16(obj + O_STATE, 0x5C);
    }
    wr16(obj + 0x50, 0xCEC0);
    clawer_on_floor(obj);
}

/* $03FDEC */
void ClawerStampRightIn(uint16_t obj)
{
    clawer_flash();
    clawer_stamp(obj, 0xC9D4);
    if (!rd16(obj + 0x54)) {
        wr16(obj + O_STATE, 0x28);
        wr16(obj + 0x5E, 0x4C);
    }
    clawer_on_floor(obj);
}

/* $04027E: beaten: 68,040 points, falling, its hands' boxes gone, a blast ($2F4) */
static void clawer_beaten(uint16_t obj)
{
    QueueSound(0xBE);
    AddScore(obj, 0x6804);
    wr16(obj + O_STATE, 0x10);
    wr16(obj + O_VITALITY, 0);
    wr32(obj + 0x1C, 0xFFFC8000);
    wr32(obj + 0x18, 0);
    wr16(obj + O_FLAGS, 0x0400);
    wr16(0xC9E2, 0);
    wr16(0xCA42, 0);
    wr8(obj + 0x21, 0);
    wr8(0xCCA1, 0);
    wr8(0xCEE1, 0);
    wr16(obj + 0x5E, rd16(obj + O_X));
    wr16(0xCF22, 0);
    wr16(0xCF82, 0);
    wr16(0xCFE2, 0);
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x2F4);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
}

/* $03F774 */
void Clawer(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) & 0x8000) clawer_beaten(obj);
    call_object(rom32(0x03F78A + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}
