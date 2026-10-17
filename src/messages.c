/* The stages' messages ($38882, every frame of mode $38): text typed out as sprites from a
 * script, a player's vitality shown, a bonus counted into a score, the warp's ring of letters,
 * flashing notices, arrows. Its step is $FF80C2 (an offset into the table at $388BE), its
 * sprites a list at $FF8200 ($FFFF: none); B on either pad (once a skip is allowed) hurries it. */
#include "game.h"

#define STEP     0x80C2
#define SPEED    0x80C4          /* frames a letter */
#define COUNT    0x80C6
#define SCRIPT   0x80C8          /* long: the next text's pointer in the script */
#define PUT      0x80CC          /* where the next letter's sprite goes in the list */
#define TEXT     0x80CE          /* long: the text being typed (the bonus, BCD, when counting) */
#define AT_X     0x80D2
#define AT_Y     0x80D4
#define WHO      0x80D6          /* the player it is about (a slot), or a multiplier */
#define HELD     0x80E0          /* the pads (both), cleared while $80E4 holds input off */
#define PRESSED  0x80E2
#define SKIP     0x80EA          /* B was pressed: hurry */
#define LIST     0x8200
#define LETTER   0x87B4          /* a letter's tile: this plus its code */
#define DIGIT0   0x87B5

static void draw(void) { DrawSpriteList(0xFFFF8200); }

static void end(void)
{
    wr16(STEP, 0);
    wr16(LIST, 0xFFFF);
}

static int paused(void) { return (rd8(v_pauseFlags) & 0x80) != 0; }

/* B on either pad, a skip allowed */
static int hurry(void)
{
    return rd16(SKIP) && ((rd8(HELD) | rd8(HELD + 1)) & 0x10);
}

/* a sprite into a list at `at`: y, its size word, its tile, x */
static uint16_t sprite(uint16_t at, uint16_t y, uint16_t size, uint16_t tile, uint16_t x)
{
    wr16(at, y);
    wr16((uint16_t)(at + 2), size);
    wr16((uint16_t)(at + 4), tile);
    wr16((uint16_t)(at + 6), x);
    return (uint16_t)(at + 8);
}

/* $038EF8. A text's lines (each: x, y, letters, $80 the next line, $FF the end) as sprites from
 * `list` on (not ended). */
void MessageLayout(uint32_t *text, uint16_t *list, uint16_t *y, uint16_t *x)
{
    for (;;) {
        *x = (uint16_t)(bus_rd8((*text)++) + 0x80);
        *y = (uint16_t)(bus_rd8((*text)++) + 0x80);
        for (;;) {
            uint16_t c = bus_rd8((*text)++);
            if (c == 0x80) break;
            if (c == 0xFF) return;
            *list = sprite(*list, *y, 0, (uint16_t)(c + LETTER), *x);
            *x = (uint16_t)(*x + 8);
        }
    }
}

/* $038EF2. The same from the list's start. */
void MessageLayoutTop(uint32_t *text, uint16_t *list, uint16_t *y, uint16_t *x)
{
    *list = LIST;
    MessageLayout(text, list, y, x);
}

/* loc_038EA0: a text shown whole */
static void show_text(uint32_t text)
{
    uint16_t list, y, x;
    MessageLayoutTop(&text, &list, &y, &x);
    wr16(list, 0xFFFF);
    draw();
}

/* $038926. Step 0: none. */
void MessageNone(void)
{
}

/* $038928. Step 8: the script's next text: its place and first wait. */
void MessageStart(void)
{
    wr16(STEP, (uint16_t)(rd16(STEP) + 4));
    wr16(PUT, LIST);
    wr16(SPEED, 2);
    wr16(LIST, 0xFFFF);
    uint32_t s = rd32(SCRIPT);
    wr32(TEXT, bus_rd32(s));
    wr32(SCRIPT, s + 4);
    uint32_t t = rd32(TEXT);
    wr16(AT_X, (uint16_t)(bus_rd8(t) + 0x80));
    wr16(AT_Y, (uint16_t)(bus_rd8(t + 1) + 0x80));
    wr16(COUNT, bus_rd8(t + 2));
    wr32(TEXT, t + 3);
    wr16(SKIP, 0);
}

/* $038978. Step 4: the text stays its time (B ends it). */
void MessageWait(void)
{
    if (paused()) {
        draw();
        return;
    }
    if (!hurry()) {
        wr16(COUNT, (uint16_t)(rd16(COUNT) - 1));
        if (!(rd16(COUNT) & 0x8000)) {
            draw();
            return;
        }
    }
    end();
}

/* $0389C2. Step $C: typing, a letter every few frames (B: at once): a 0 is a space, $FF the
 * end (then step 4), another with bit 7 the script's next text (its place; $FE first: a new
 * page). */
void MessageType(void)
{
    if (paused()) {
        draw();
        return;
    }
    if (hurry()) wr16(COUNT, 0);
    wr16(COUNT, (uint16_t)(rd16(COUNT) - 1));
    if (!(rd16(COUNT) & 0x8000)) {
        draw();
        return;
    }
    wr16(COUNT, rd16(SPEED));
    QueueSound(0xAD);
    uint16_t at = rd16(PUT);
    uint32_t t = rd32(TEXT);
    wr16(PUT, (uint16_t)(rd16(PUT) + 8));
    uint16_t y = rd16(AT_Y), x = rd16(AT_X), c;
    while (!(c = bus_rd8(t++))) {
        x = (uint16_t)(x + 8);
        wr16(AT_X, (uint16_t)(rd16(AT_X) + 8));
    }
    if (c & 0x80) {
        if (c == 0xFF) {
            wr16(STEP, 4);
            wr16(COUNT, 0x80);
            draw();
            return;
        }
        c &= 0x7F;
        uint32_t s = rd32(SCRIPT);
        wr32(TEXT, bus_rd32(s));
        wr32(SCRIPT, s + 4);
        t = rd32(TEXT);
        uint16_t px = bus_rd8(t++);
        if (px == 0xFE) {
            wr16(PUT, LIST);
            px = bus_rd8(t++);
        }
        wr16(AT_X, (uint16_t)(px + 0x80));
        wr16(AT_Y, (uint16_t)(bus_rd8(t++) + 0x80));
        wr16(COUNT, bus_rd8(t++));
    } else {
        wr16(AT_X, (uint16_t)(rd16(AT_X) + 8));
    }
    wr32(TEXT, t);
    at = sprite(at, y, 0, (uint16_t)(c + LETTER), x);
    wr16(at, 0xFFFF);
    draw();
}

/* $0389AC. Step $1C: blinking for its time. */
void MessageBlink(void)
{
    wr16(COUNT, (uint16_t)(rd16(COUNT) - 1));
    if (rd16(COUNT) & 0x8000) {
        end();
        return;
    }
    if (rd16(COUNT) & 4) draw();
}

/* loc_038B4C: the player is out: its notice ($3A325) blinking */
static void vitality_out(void)
{
    wr16(STEP, 0x1C);
    wr16(COUNT, 0x60);
    show_text(0x03A325);
}

/* $038AB0. Step $10: the "vitality" notice ($3A331). */
void MessageVitality(void)
{
    wr16(STEP, (uint16_t)(rd16(STEP) + 4));
    if (rd16(rd16(WHO) + O_VITALITY) & 0x8000) {
        vitality_out();
        return;
    }
    wr16(COUNT, 0x40);
    show_text(0x03A331);
}

/* the four digits' sprites ($340) */
static void vitality_digits(void)
{
    uint16_t at = LIST;
    for (uint16_t x = 0x110; x < 0x130; x = (uint16_t)(x + 8)) at = sprite(at, 0xA0, 0, 0x340, x);
    wr16(at, 0xFFFF);
}

/* $038AD0. Step $14: blinking, then the digits. */
void MessageVitalityBlink(void)
{
    if (rd16(rd16(WHO) + O_VITALITY) & 0x8000) {
        vitality_out();
        return;
    }
    wr16(COUNT, (uint16_t)(rd16(COUNT) - 1));
    if (rd16(COUNT) & 0x8000) {
        wr16(STEP, (uint16_t)(rd16(STEP) + 4));
        vitality_digits();
        return;
    }
    if (rd16(COUNT) & 4) draw();
}

/* the vitality plus 1 in the four digits' tiles from $821C back (leading zeros '0') */
static void vitality_number(uint16_t v)
{
    uint16_t at = 0x821C;
    for (int k = 0; k < 4; k++) {
        uint16_t q = (uint16_t)(v / 10), r = (uint16_t)(v % 10);
        wr16(at, (uint16_t)(r + DIGIT0));
        if (!q) break;
        at = (uint16_t)(at - 8);
        v = q;
    }
    draw();
}

/* $038B22. Step $18: the player's vitality (plus 1) shown. */
void MessageVitalityCount(void)
{
    for (uint16_t a = 0x8204; a <= 0x821C; a = (uint16_t)(a + 8)) wr16(a, DIGIT0);
    int16_t v = (int16_t)(rd16(rd16(WHO) + O_VITALITY) + 1);
    if (v <= 0) {
        vitality_out();
        return;
    }
    vitality_number((uint16_t)v);
}

/* $038BA0. Step $20: the digits (the player out: nothing). */
void MessageVitalityStart(void)
{
    if (rd16(rd16(WHO) + O_VITALITY) & 0x8000) {
        end();
        return;
    }
    wr16(STEP, (uint16_t)(rd16(STEP) + 4));
    vitality_digits();
    MessageVitalityShow();
}

/* $038BE8. Step $24: the vitality shown (the player out: nothing). */
void MessageVitalityShow(void)
{
    for (uint16_t a = 0x8204; a <= 0x821C; a = (uint16_t)(a + 8)) wr16(a, DIGIT0);
    int16_t v = (int16_t)(rd16(rd16(WHO) + O_VITALITY) + 1);
    if (v <= 0) {
        wr16(STEP, 0);
        return;
    }
    vitality_number((uint16_t)v);
}

/* $038E6E. Step $28: the script's text shown (blinking by its speed's mask) for its time. */
void MessageShow(void)
{
    if (!paused()) {
        wr16(COUNT, (uint16_t)(rd16(COUNT) - 1));
        if (rd16(COUNT) & 0x8000) {
            end();
            return;
        }
    }
    if (rd16(COUNT) & rd16(SPEED)) return;
    show_text(rd32(SCRIPT));
}

/* the bonus's text, and its eight BCD digits after it */
static void bonus_show(void)
{
    uint32_t text = rd32(SCRIPT);
    uint16_t list, y, x;
    MessageLayoutTop(&text, &list, &y, &x);
    uint32_t n = rd32(TEXT);
    x = (uint16_t)(x + 0x10);
    for (int k = 0; k < 8; k++) {
        list = sprite(list, y, 0, (uint16_t)(((n >> 28) & 0xF) + DIGIT0), x);
        n <<= 4;
        x = (uint16_t)(x + 8);
    }
    wr16(list, 0xFFFF);
    draw();
}

/* $038C44. Step $3C: the bonus shown $100 frames. */
void MessageBonusStart(void)
{
    wr16(STEP, (uint16_t)(rd16(STEP) + 4));
    wr16(COUNT, 0x100);
    MessageBonus();
}

/* $038C4E. Step $40: the bonus shown. */
void MessageBonus(void)
{
    if (!paused()) {
        wr16(COUNT, (uint16_t)(rd16(COUNT) - 1));
        if (rd16(COUNT) & 0x8000) wr16(STEP, (uint16_t)(rd16(STEP) + 4));
    }
    bonus_show();
}

/* $038E42. Points (BCD) onto the score ending at `end` (as AddBcd). */
void AddBcdAt(uint16_t end_at, uint32_t points)
{
    AddBcd(end_at, points);
}

/* $038CAC. Step $44: the bonus counted into the player's score (+$80D6; 0: both players), a
 * unit of its lowest digit a frame. */
void MessageBonusCount(void)
{
    if (paused()) {
        bonus_show();
        return;
    }
    if (!rd32(TEXT)) {
        wr16(STEP, (uint16_t)(rd16(STEP) + 4));
        wr16(COUNT, 0x40);
        bonus_show();
        return;
    }
    uint16_t score;
    if (rd16(WHO) == v_player2) {
        set8(v_player2 + 0x81, 1);
        score = v_player2 + 0x69;
    } else {
        set8(v_player1 + 0x81, 1);
        score = v_player1 + 0x69;
    }
    QueueSound(0xAD);
    static const uint32_t unit[8] = { 1, 0x10, 0x100, 0x1000, 0x10000, 0x100000, 0x1000000, 0 };
    uint32_t add = 0;
    int k;
    for (k = 0; k < 8; k++) {
        uint16_t b = (uint16_t)(TEXT + 3 - k / 2);
        uint8_t mask = (k & 1) ? 0xF0 : 0x0F;
        if (rd8(b) & mask) {
            wr8(b, (uint8_t)(rd8(b) - ((k & 1) ? 0x10 : 1)));
            add = unit[k];
            break;
        }
    }
    if (k == 8) {
        wr16(STEP, (uint16_t)(rd16(STEP) + 4));
        wr16(COUNT, 0x40);
    }
    AddBcdAt(score, add);
    if (!rd16(WHO)) {
        set8(v_player2 + 0x81, 1);
        AddBcdAt(v_player2 + 0x69, add);
    }
    bonus_show();
}

/* $038E24. Step $48: the bonus shown a while, then gone. */
void MessageBonusDone(void)
{
    if (!paused()) {
        wr16(COUNT, (uint16_t)(rd16(COUNT) - 1));
        if (rd16(COUNT) & 0x8000) {
            end();
            return;
        }
    }
    bonus_show();
}

/* $00369E. The palettes at their full level ($800A $E000). */
void PalettesFull(void)
{
    wr16(0x800A, 0xE000);
    AdjustPalettes();
}

/* $03985C. Tiles DMA'd from the list of sources at `list` ($FFFFFFFF its end), to VRAM $400 on. */
void QueueDmaList(uint32_t list)
{
    uint16_t q = rd16(v_dmaQueue), vram = 0x400;
    for (;;) {
        uint32_t src = bus_rd32(list);
        list += 4;
        if (src == 0xFFFFFFFFu) break;
        QueueDmaBlock(&q, src, &vram);
    }
    wr16(v_dmaQueue, q);
}

/* a sprite at $A100 (size $500) on the ring: angle +$80D2, radius +$80D4 times +$80D6, centre
 * (x $120, y `cy`), the angle `a` */
static void ring_at(uint16_t a, uint16_t cy)
{
    int32_t r = (int32_t)(int16_t)rd16(AT_Y) * (int16_t)rd16(WHO);
    int16_t rw = (int16_t)r;
    uint16_t y = (uint16_t)(((uint32_t)((int32_t)(int16_t)rom16(0x02977C + a) * rw) << 2) >> 16);
    uint16_t x = (uint16_t)(((uint32_t)((int32_t)(int16_t)rom16(0x0297FC + a) * rw) << 2) >> 16);
    wr16(0xA106, (uint16_t)(x + 0x120));
    wr16(0xA100, (uint16_t)(y + cy));
    DrawSpriteList(0xFFFFA100);
}

/* $0396A4 */
void RingLetter(void)
{
    ring_at(rd16(AT_X), 0xE8);
}

/* $0396E6. The other way round, higher. */
void RingLetterBack(void)
{
    ring_at((uint16_t)(-rd16(AT_X) & 0x1FE), 0xD0);
}

/* $03958E. The warp's ring of letters (their tiles from $398C0), at multiples -5 to 5 of the
 * radius; on the last stage a second ring. */
void MessageRing(void)
{
    static const uint16_t tiles[10] = { 0x8020, 0x8024, 0x8028, 0x802C, 0x8030, 0x8034, 0x8038, 0x8030, 0x8028, 0x803C };
    static const uint16_t mult[10] = { 0xFFFB, 0xFFFC, 0xFFFD, 0xFFFE, 0xFFFF, 1, 2, 3, 4, 5 };
    QueueDmaList(0x0398C0);
    wr16(0xA102, 0x500);
    wr16(0xA108, 0xFFFF);
    for (int k = 0; k < 10; k++) {
        wr16(0xA104, tiles[k]);
        wr16(WHO, mult[k]);
        RingLetter();
    }
    if (rd16(v_stage4) != 0x18) return;
    static const uint16_t tiles2[5] = { 0x8040, 0x8044, 0x8048, 0x8028, 0x8038 };
    static const uint16_t mult2[5] = { 0xFFFE, 0xFFFF, 0, 1, 2 };
    for (int k = 0; k < 5; k++) {
        wr16(0xA104, tiles2[k]);
        wr16(WHO, mult2[k]);
        RingLetterBack();
    }
}

/* $038F38. Step $2C: the warp: the game paused for its $168 frames. */
void MessageWarpStart(void)
{
    wr16(STEP, (uint16_t)(rd16(STEP) + 4));
    clr8(v_pauseFlags, 6);
    set8(v_pauseFlags, 7);
    wr16(COUNT, 0x168);
    wr16(TEXT, 0);
    wr16(AT_X, 0);
    wr16(AT_Y, 0x80);
    QueueSound(0x8E);
}

/* $038F6A. Step $30: the ring turning in to radius 0 and out to $14, the palettes darkened and
 * colour 5 white; then the game again. */
void MessageWarp(void)
{
    wr16(COUNT, (uint16_t)(rd16(COUNT) - 1));
    if (rd16(COUNT) & 0x8000) {
        for (uint16_t a = 0x8014; a <= 0x801A; a = (uint16_t)(a + 2)) wr16(a, 0);
        wr16(0x8002, 0xF000);
        PalettesFull();
        set8(v_pauseFlags, 6);
        clr8(v_pauseFlags, 7);
        wr16(STEP, 0);
        return;
    }
    int grow = rd16(TEXT) != 0;
    if (!grow) {
        wr16(AT_X, (uint16_t)((rd16(AT_X) - 8) & 0x1FE));
        wr16(AT_Y, (uint16_t)(rd16(AT_Y) - 4));
        if (rd16(AT_Y) & 0x8000) {
            wr16(AT_X, 0);
            wr16(TEXT, 1);
            grow = 1;
        }
    }
    if (grow && (int16_t)(rd16(AT_Y) - 0x14) < 0) wr16(AT_Y, (uint16_t)(rd16(AT_Y) + 2));
    MessageRing();
    for (uint16_t a = 0x8014; a <= 0x801A; a = (uint16_t)(a + 2)) wr16(a, 0xFFFD);
    wr16(0x8002, 0xF000);
    PalettesFull();
    wr16((uint16_t)(v_palette + 0x0A), 0xEEE);
    wr16((uint16_t)(v_paletteTarget + 0x0A), 0xEEE);
}

/* loc_03972E: the notice's seven letters (their tiles from $39888) */
static void notice(void)
{
    static const uint16_t tiles[7] = { 0x8020, 0x8024, 0x8028, 0x802C, 0x8030, 0x802C, 0x8034 };
    QueueDmaList(0x039888);
    wr16(0xA106, 0xD8);
    wr16(0xA100, 0xE8);
    wr16(0xA102, 0x500);
    wr16(0xA108, 0xFFFF);
    for (int k = 0; k < 7; k++) {
        wr16(0xA104, tiles[k]);
        if (k) wr16(0xA106, (uint16_t)(rd16(0xA106) + 0x14));
        DrawSpriteList(0xFFFFA100);
    }
}

/* $03902A. Step $38: the notice flashing; then step 8. */
void MessageFlash(void)
{
    if (!paused()) {
        wr16(COUNT, (uint16_t)(rd16(COUNT) - 1));
        if (rd16(COUNT) & 0x8000) wr16(STEP, 8);
    }
    if (rd16(COUNT) & 8) return;
    notice();
}

/* $039016. Step $34: the notice, $80 frames, a sound. */
void MessageFlashStart(void)
{
    wr16(STEP, (uint16_t)(rd16(STEP) + 4));
    wr16(COUNT, 0x80);
    QueueSound(0xD9);
    MessageFlash();
}

/* $039052. Step $4C. */
void MessageOutroStart(void)
{
    wr16(STEP, (uint16_t)(rd16(STEP) + 4));
    wr16(COUNT, 0xB0);
}

/* $039070. Step $54: three lines appearing one by one (from $E0, $C0, $A0 frames left); then the
 * bonus (step $40, the text at $3A47F, $65816 points, to +$80D8's player). */
void MessageOutro(void)
{
    if (!paused()) {
        wr16(COUNT, (uint16_t)(rd16(COUNT) - 1));
        if (rd16(COUNT) & 0x8000) {
            wr16(STEP, 0x40);
            wr16(COUNT, 0x40);
            wr32(SCRIPT, 0x03A47F);
            wr32(TEXT, 0x065816);
            wr16(WHO, rd16(0x80D8));
            wr16(LIST, 0xFFFF);
            return;
        }
    }
    int16_t c = (int16_t)rd16(COUNT);
    if (c - 0xE0 >= 0) return;
    uint32_t text = rd32(SCRIPT);
    uint16_t list, y, x;
    MessageLayoutTop(&text, &list, &y, &x);
    if (c - 0xC0 < 0) {
        MessageLayout(&text, &list, &y, &x);
        if (c - 0xA0 < 0) MessageLayout(&text, &list, &y, &x);
    }
    wr16(list, 0xFFFF);
    draw();
}

/* $03905E. Step $50: a pause, then the lines. */
void MessageOutroWait(void)
{
    wr16(COUNT, (uint16_t)(rd16(COUNT) - 1));
    if (!(rd16(COUNT) & 0x8000)) return;
    wr16(STEP, (uint16_t)(rd16(STEP) + 4));
    wr16(COUNT, 0x100);
    MessageOutro();
}

/* $03947C. Steps $5C, $64: an arrow (two kinds) blinking with a sound, its time. */
void MessageArrow(void)
{
    if (!paused()) {
        wr16(COUNT, (uint16_t)(rd16(COUNT) - 1));
        if (rd16(COUNT) & 0x8000) {
            wr16(STEP, 0);
            return;
        }
    }
    if (bit8(COUNT + 1, 4)) return;
    if (!(rd16(COUNT) & 0xF)) QueueSound(0xC7);
    wr16(0xA108, 0xFFFF);
    uint16_t x = rd16(AT_X), y = rd16(AT_Y);
    if (rd16(STEP) != 0x64) {
        wr16(0xA106, x);
        wr16(0xA100, y);
        wr16(0xA102, 0x800);
        wr16(0xA104, 0x86AE);
        DrawSpriteList(0xFFFFA100);
        wr16(0xA100, (uint16_t)(rd16(0xA100) + 8));
        wr16(0xA104, 0x96AE);
        DrawSpriteList(0xFFFFA100);
        wr16(0xA100, (uint16_t)(rd16(0xA100) + 8));
        wr16(0xA104, 0x86AB);
        DrawSpriteList(0xFFFFA100);
        return;
    }
    wr16(0xA106, x);
    wr16(0xA100, (uint16_t)(y + 0x10));
    wr16(0xA102, 0x800);
    wr16(0xA104, 0x86AB);
    DrawSpriteList(0xFFFFA100);
    wr16(0xA100, (uint16_t)(rd16(0xA100) - 8));
    wr16(0xA102, 0);
    wr16(0xA104, 0x86B3);
    DrawSpriteList(0xFFFFA100);
    wr16(0xA106, (uint16_t)(rd16(0xA106) + 0x10));
    wr16(0xA104, 0x8EB3);
    DrawSpriteList(0xFFFFA100);
    wr16(0xA106, (uint16_t)(rd16(AT_X) + 8));
    wr16(0xA100, (uint16_t)(rd16(AT_Y) + 0x18));
    wr16(0xA104, 0x86B4);
    DrawSpriteList(0xFFFFA100);
    wr16(0xA100, rd16(AT_Y));
    wr16(0xA102, 0x100);
    wr16(0xA104, 0x86B1);
    DrawSpriteList(0xFFFFA100);
}

/* $039466. Step $58: the arrow down at ($188, $E8). */
void MessageArrowDown(void)
{
    wr16(STEP, (uint16_t)(rd16(STEP) + 4));
    wr16(COUNT, 0x80);
    wr16(AT_X, 0x188);
    wr16(AT_Y, 0xE8);
    MessageArrow();
}

/* $03944C. Step $60: the other arrow at ($114, $A0). */
void MessageArrowOther(void)
{
    wr16(STEP, (uint16_t)(rd16(STEP) + 4));
    wr16(COUNT, 0x80);
    wr16(AT_X, 0x114);
    wr16(AT_Y, 0xA0);
    MessageArrow();
}

/* $038882. Each frame: B noted (a skip), the pads copied (unless held off), the step. */
void Messages(void)
{
    if ((rd8(v_padPressed) | rd8(v_padPressed + 1)) & 0x10) wr16(SKIP, 1);
    wr16(HELD, 0);
    wr16(PRESSED, 0);
    if (!rd16(0x80E4)) {
        wr16(HELD, rd16(v_padHeld));
        wr16(PRESSED, rd16(v_padPressed));
    }
    call68k(rom32(0x0388BE + (uint32_t)(int16_t)rd16(STEP)));
}
