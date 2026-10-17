/* Sprites. Each frame BuildSprites steps every shown object's animation and writes its sprites
 * into the table at v_sprites (80 at most): the players', the object at $FFA800's, then the
 * objects listed at v_drawList. A sprite goes into one of 64 priority buckets (the object's
 * +$20, by 4): v_spriteBuckets keeps, for each bucket, its first sprite's number and a pointer
 * just past its last one, and the link fields chain each bucket's sprites; at the end the
 * buckets are chained in order after the sprites reserved at the table's head.
 *
 * An object's fields used here: +2 its drawing flags (bit 7 a mapping, else one sprite; bit 6
 * animated; bit 5 the static kind of mapping), +3 bit 7 fixed on screen (no camera), +8 the
 * animation script (or a single sprite's size), +9 the delay before the next animation, +$A and
 * +$B the single sprite's offsets, +$C and +$D the script's timer and frame, +$E the tile word
 * (flip and palette bits, and the base tile), +$10 x, +$14 y, +$20 the priority, +$DA the VRAM
 * address the frame's tiles are sent to, +$DC the last frame drawn, +$E0 the tiles' list. */
#include "game.h"

#define SPRITES_MAX 80
#define BUCKETS 64
#define S_COUNT    0xBE00                /* sprites written */
#define S_FREE     0xBE01                /* 80 less them */
#define S_NEXT     0xBE02                /* where the next goes */
#define S_BUCKETS  0xBE04                /* per bucket: its first sprite (a word), past its last */
#define CAMERA_Y   v_cameraY
#define FLIP_V     0x1000
#define FLIP_H     0x0800

/* the sprites written so far: their number and where the next one goes (the 68k's d4, a3) */
typedef struct { uint16_t count; uint16_t at; } Builder;

static uint16_t bucket_of(uint16_t obj) { return (uint16_t)(S_BUCKETS + 2 + (rd8((uint16_t)(obj + 0x20)) & 0xFC)); }

/* the bucket's last sprite (just before the pointer) links to the one about to be written */
static void link_into(uint16_t bucket, uint8_t count)
{
    wr8((uint16_t)(rd16(bucket) - 5), count);
}

static int16_t camera_y(uint16_t obj) { return (rd8((uint16_t)(obj + 3)) & 0x80) ? 0 : (int16_t)rd16(CAMERA_Y); }

/* $001E6E. A new table: the buckets empty (each pointing into itself, so a first link lands
 * in its first-sprite word), the reserved sprites (v_reservedSprites, at least 1) kept. */
void SpritesStart(void)
{
    for (int k = 0; k < BUCKETS; k++) wr32((uint16_t)(S_BUCKETS + 4 * k), (uint32_t)(S_BUCKETS + 6 + 4 * k));
    uint16_t reserved = rd16(v_reservedSprites);
    if (!reserved) reserved = 1;
    wr8(S_COUNT, (uint8_t)reserved);
    wr16(S_NEXT, (uint16_t)(v_sprites + 8 * reserved));
    wr8(v_spritesStarted, 1);
}

/* the buckets chained in order after the reserved sprites, the last link 0 */
static void sprites_finish(void)
{
    wr8(S_FREE, (uint8_t)(SPRITES_MAX - rd8(S_COUNT)));
    uint16_t reserved = rd16(v_reservedSprites);
    if (!reserved) reserved = 1;
    uint16_t last = (uint16_t)(v_sprites + 8 * reserved);
    for (int k = 0; k < BUCKETS; k++) {
        uint16_t first = rd16((uint16_t)(S_BUCKETS + 4 * k));
        if (!first) continue;
        wr8((uint16_t)(last - 5), (uint8_t)first);
        last = rd16((uint16_t)(S_BUCKETS + 4 * k + 2));
    }
    wr8((uint16_t)(last - 5), 0);
    wr8(v_spritesStarted, 0);
}

/* $001C32. The frame's sprites. */
void BuildSprites(void)
{
    if (!rd8(v_spritesStarted)) SpritesStart();
    Builder b = { rd8(S_COUNT), rd16(S_NEXT) };
    DrawPlayerSprites(v_player1, &b.count, &b.at);
    DrawPlayerSprites(v_player2, &b.count, &b.at);
    DrawObjectSprites(0xA800, &b.count, &b.at);
    uint16_t n = rd16(v_drawCount);
    for (uint16_t i = 0; i < n; i++) {
        uint16_t obj = rd16((uint16_t)(v_drawList + 2 * i));
        uint8_t f = rd8((uint16_t)(obj + 2));
        if (!(f & 0x80)) {
            if ((f & 0x40) && (f & 0x20)) Animate(obj, rd32((uint16_t)(obj + 8)));
            continue;
        }
        if (!(f & 0x40)) {
            DrawSprite(obj, &b.count, &b.at);
            continue;
        }
        uint32_t script = rd32((uint16_t)(obj + 8)), map;
        map = (f & 0x20) ? Animate(obj, script) : script;
        DrawMapping(obj, map, &b.count, &b.at);
    }
    wr8(S_COUNT, (uint8_t)b.count);
    wr16(S_NEXT, b.at);
    sprites_finish();
}

/* $001DE4. The animation script at `script` (entries of a word, the offset of the frame's
 * mapping, and a word, its frames; a frame of 0 jumps by the first word): the timer at +$C
 * counts down unless paused (negative: held); returns the frame's mapping. */
uint32_t Animate(uint16_t obj, uint32_t script)
{
    int16_t timer = (int16_t)rd16((uint16_t)(obj + 0xC));
    if (timer >= 0) {
        if (timer == 0) {
            timer = (int16_t)bus_rd16(script + 2);
            wr32((uint16_t)(obj + 8), script);
        } else {
            if (!(rd8(v_pauseFlags) & 0x80)) timer--;
            if (timer == 0) {
                script += 4;
                timer = (int16_t)bus_rd16(script + 2);
                if (!timer) {
                    script += (uint32_t)(int16_t)bus_rd16(script);
                    timer = (int16_t)bus_rd16(script + 2);
                }
                wr32((uint16_t)(obj + 8), script);
            }
        }
        wr16((uint16_t)(obj + 0xC), (uint16_t)timer);
    }
    return script + (uint32_t)(int16_t)bus_rd16(script);
}

/* $001E10. The other kind of script (entries of 4 bytes: the mapping's offset, a byte, the
 * frames): +$C the entry's offset, +$D the frames left (negative: held). The frame shown is
 * the current entry's; when its frames run out the next entry with frames (an entry of 0 frames
 * jumps by its first word) is the next frame's. Returns the mapping. */
uint32_t AnimateFrames(uint16_t obj, uint32_t script)
{
    int8_t left = (int8_t)rd8((uint16_t)(obj + 0xD));
    int16_t at = (int8_t)rd8((uint16_t)(obj + 0xC));
    uint32_t entry = script + (uint32_t)at;
    if (left >= 0) {
        uint8_t frames;
        if (left == 0) frames = bus_rd8(entry + 3);
        else if (rd8(v_pauseFlags) & 0x80) return entry + (uint32_t)(int16_t)bus_rd16(entry);
        else frames = (uint8_t)left;
        frames--;
        wr8((uint16_t)(obj + 0xD), frames);
        if (!frames) {
            uint32_t e = entry + 4;
            at = (int16_t)(at + 4);
            for (;;) {
                uint8_t f = bus_rd8(e + 3);
                if (f) {
                    wr8((uint16_t)(obj + 0xD), f);
                    wr8((uint16_t)(obj + 0xC), (uint8_t)at);
                    break;
                }
                int16_t jump = (int16_t)bus_rd16(e);
                e += (uint32_t)jump;
                at = (int16_t)(at + jump);
            }
        }
    }
    return entry + (uint32_t)(int16_t)bus_rd16(entry);
}

/* $001EE2. One sprite: size (+8), tile word (+$E), at the object's place plus its offsets.
 * Off the table's range (512) it is hidden (y 0); x 0 would mask the line's later sprites, so 1. */
void DrawSprite(uint16_t obj, uint16_t *count, uint16_t *at)
{
    if ((int8_t)*count >= SPRITES_MAX) return;
    uint16_t bucket = bucket_of(obj);
    link_into(bucket, (uint8_t)*count);
    uint16_t y = (uint16_t)((int8_t)rd8((uint16_t)(obj + 0xB)) + rd16((uint16_t)(obj + 0x14)) + camera_y(obj));
    if (y >= 0x200) y = 0;
    wr16(*at, y);
    *count = (uint16_t)((*count & 0xFF00) | (uint8_t)(*count + 1));
    wr16((uint16_t)(*at + 2), (uint16_t)((rd16((uint16_t)(obj + 8)) & 0xFF00) | (uint8_t)*count));
    wr16((uint16_t)(*at + 4), rd16((uint16_t)(obj + 0xE)));
    uint16_t x = (uint16_t)((int8_t)rd8((uint16_t)(obj + 0xA)) + rd16((uint16_t)(obj + 0x10)));
    if (x >= 0x200) wr16(*at, 0);
    x &= 0x1FF;
    if (!x) x = 1;
    wr16((uint16_t)(*at + 6), x);
    *at += 8;
    wr16(bucket, *at);
}

/* A mapping's pieces, as DrawMapping and DrawMappingDma read them: a tile word (bit 15 on the
 * last piece), then the size word (DMA mappings: a long, the tiles' source, whose high word is
 * the size), a y offset byte, an x offset byte; flipped with the object. dma: where the
 * tiles' sources go (0 for none); mask: the tile word's bits the piece keeps. */
static void draw_pieces(uint16_t obj, uint32_t map, uint16_t *count, uint16_t *at, uint16_t *dma, uint16_t mask)
{
    uint16_t attr = rd16((uint16_t)(obj + 0xE)), flags = attr & 0xF800, base = attr & 0x7FF;
    int16_t x0 = (int16_t)rd16((uint16_t)(obj + 0x10)), y0 = (int16_t)(rd16((uint16_t)(obj + 0x14)) + camera_y(obj));
    uint16_t bucket = bucket_of(obj);
    link_into(bucket, (uint8_t)*count);
    if (flags & FLIP_H) x0 = (int16_t)(x0 - 9);
    for (;;) {
        if ((int8_t)*count >= SPRITES_MAX) return;
        uint16_t tile = bus_rd16(map);
        uint16_t size;
        if (dma) {
            uint32_t src = bus_rd32(map + 2);
            wr32(*dma, src);
            *dma += 4;
            size = (uint16_t)(src >> 16);
            map += 6;
            *count = (uint16_t)((size & 0xFF00) | (uint8_t)*count);
        } else {
            size = bus_rd16(map + 2);
            map += 4;
        }
        int16_t y = (int8_t)bus_rd8(map++);
        if (flags & FLIP_V) {
            uint16_t s = dma ? *count : size;
            y = (int16_t)(-y - (((s >> 5) & 0x18) + (dma ? 9 : 8)));
        }
        uint16_t sy = (uint16_t)(y + y0);
        if (sy >= 0x200) sy = 0;
        *count = (uint16_t)((*count & 0xFF00) | (uint8_t)(*count + 1));
        wr16(*at, sy);
        wr16((uint16_t)(*at + 2), dma ? *count : (uint16_t)((size & 0xFF00) | (uint8_t)*count));
        wr16((uint16_t)(*at + 4), (uint16_t)(((tile & mask) ^ flags) + base));
        int16_t x = (int8_t)bus_rd8(map++);
        if (flags & FLIP_H) {
            uint8_t s = dma ? bus_rd8(map - 6) : bus_rd8(map - 4);
            x = (int16_t)(-x - (((uint16_t)(s * 2) & 0x18) - (dma ? 0 : 1)));
        }
        uint16_t sx = (uint16_t)(x + x0);
        if (sx >= 0x200) wr16(*at, 0);
        sx &= 0x1FF;
        if (!sx) sx = 1;
        wr16((uint16_t)(*at + 6), sx);
        *at += 8;
        if (tile & 0x8000) break;
    }
    wr16(bucket, *at);
}

/* $001F50. An object's mapping (pieces of 6 bytes). */
void DrawMapping(uint16_t obj, uint32_t map, uint16_t *count, uint16_t *at)
{
    draw_pieces(obj, map, count, at, 0, 0x7FFF);
}

/* $002010 (a player's: the tile word's low 13 bits) and $0020D6 (the object at $FFA800's: 15
 * bits). A mapping whose pieces carry their tiles' sources (pieces of 8 bytes), listed from dma. */
void DrawMappingDma(uint16_t obj, uint32_t map, uint16_t *count, uint16_t *at, uint16_t *dma)
{
    draw_pieces(obj, map, count, at, dma, 0x1FFF);
}

void DrawMappingDma2(uint16_t obj, uint32_t map, uint16_t *count, uint16_t *at, uint16_t *dma)
{
    draw_pieces(obj, map, count, at, dma, 0x7FFF);
}

/* The frame's tiles to VRAM (+$DA on): a queued DMA for each source the mapping listed. */
static void queue_frame_tiles(uint16_t obj, uint16_t end)
{
    uint16_t list = (uint16_t)(obj + 0xE0);
    if (end == list) return;
    uint16_t q = rd16(v_dmaQueue), vram = rd16((uint16_t)(obj + 0xDA));
    for (; list < end; list += 4) {
        uint32_t src = rd32(list);
        QueueDmaBlock(&q, src, &vram);
    }
    wr16(v_dmaQueue, q);
}

/* $001CBA. A player: hidden (+2 clear), one script frame of static tiles (bit 5), or a frame
 * whose tiles are sent by DMA when it changes. */
void DrawPlayerSprites(uint16_t obj, uint16_t *count, uint16_t *at)
{
    int16_t f = (int16_t)rd16((uint16_t)(obj + 2));
    if (f >= 0) {
        if (f) AnimateFrames(obj, rd32((uint16_t)(obj + 8)));
        return;
    }
    uint32_t script = rd32((uint16_t)(obj + 8));
    if (!script) return;
    uint32_t map = AnimateFrames(obj, script);
    if (f & 0x20) {
        DrawMapping(obj, map, count, at);
        return;
    }
    uint16_t dma = (uint16_t)(obj + 0xE0);
    if (map == rd32((uint16_t)(obj + 0xDC))) {
        DrawMappingDma(obj, map, count, at, &dma);
        return;
    }
    wr32((uint16_t)(obj + 0xDC), map);
    DrawMappingDma(obj, map, count, at, &dma);
    queue_frame_tiles(obj, dma);
}

/* $001D1A. The object at $FFA800: as a player, with the other script kind. */
void DrawObjectSprites(uint16_t obj, uint16_t *count, uint16_t *at)
{
    uint8_t f = rd8((uint16_t)(obj + 2));
    if (!(f & 0x80)) {
        if ((f & 0x40) && (f & 0x20)) Animate(obj, rd32((uint16_t)(obj + 8)));
        return;
    }
    uint32_t map = rd32((uint16_t)(obj + 8));
    if (!map) return;
    if (f & 0x20) map = Animate(obj, rd32((uint16_t)(obj + 8)));
    uint16_t dma = (uint16_t)(obj + 0xE0);
    if (map == rd32((uint16_t)(obj + 0xDC))) {
        DrawMappingDma2(obj, map, count, at, &dma);
        return;
    }
    wr32((uint16_t)(obj + 0xDC), map);
    DrawMappingDma2(obj, map, count, at, &dma);
    queue_frame_tiles(obj, dma);
}

/* $001B86. A DMA of a block (its length in bytes, then the data, at src) to VRAM vram, into the
 * queue at q; vram moves on past it. */
void QueueDmaBlock(uint16_t *q, uint32_t src, uint16_t *vram)
{
    uint16_t bytes = bus_rd16(src);
    src += 2;
    uint16_t v = *vram;
    *q -= 2; wr16(*q, (uint16_t)(0x80 | ((v >> 14) & 3)));
    *q -= 2; wr16(*q, (uint16_t)((v & 0x3FFF) | 0x4000));
    *vram = (uint16_t)(v + bytes);
    uint16_t words = bytes >> 1;
    *q -= 2; wr16(*q, (uint16_t)(0x9300 | (words & 0xFF)));
    *q -= 2; wr16(*q, (uint16_t)(0x9400 | (words >> 8)));
    *q -= 2; wr16(*q, 0x8F02);
    uint32_t s = (src & 0xFFFFFF) >> 1;
    *q -= 2; wr16(*q, (uint16_t)(0x9500 | (s & 0xFF)));
    *q -= 2; wr16(*q, (uint16_t)(0x9600 | (s >> 8 & 0xFF)));
    *q -= 2; wr16(*q, (uint16_t)(0x9700 | (s >> 16 & 0xFF)));
}

/* $00219C. SelectAnimation, the sprite word's top bit (priority) kept. */
void SelectAnimationKeep(uint16_t obj, uint32_t table)
{
    uint8_t delay = rd8((uint16_t)(obj + 9));
    uint16_t e;
    if (!delay) e = rd16((uint16_t)(obj + 0xC));
    else {
        if (delay == 0xFF) return;
        wr8((uint16_t)(obj + 9), (uint8_t)(delay - 1));
        if (delay != 1) return;
        e = (uint16_t)(rd16((uint16_t)(obj + 0xC)) + 6);
    }
    uint32_t script;
    while ((int32_t)(script = bus_rd32(table + (uint32_t)(int16_t)e)) < 0) e = (uint16_t)script;
    wr32((uint16_t)(obj + 8), script);
    uint16_t look = bus_rd16(table + (uint32_t)(int16_t)e + 4);
    wr16((uint16_t)(obj + 0xE), rd16((uint16_t)(obj + 0xE)) & 0x8000);
    wr16((uint16_t)(obj + 0xE), rd16((uint16_t)(obj + 0xE)) ^ look);
    wr16((uint16_t)(obj + 0xC), e);
}

/* $0021E8. The animation from a table of 6-byte entries (the script, the tile word; a negative
 * script is the entry to go to): +$C the entry, +9 frames until the next (0: now, $FF: never). */
void SelectAnimation(uint16_t obj, uint32_t table)
{
    uint8_t delay = rd8((uint16_t)(obj + 9));
    uint16_t e;
    if (!delay) e = rd16((uint16_t)(obj + 0xC));
    else {
        if (delay == 0xFF) return;
        wr8((uint16_t)(obj + 9), (uint8_t)(delay - 1));
        if (delay != 1) return;
        e = (uint16_t)(rd16((uint16_t)(obj + 0xC)) + 6);
    }
    uint32_t script;
    while ((int32_t)(script = bus_rd32(table + (uint32_t)(int16_t)e)) < 0) e = (uint16_t)script;
    wr32((uint16_t)(obj + 8), script);
    wr16((uint16_t)(obj + 0xE), bus_rd16(table + (uint32_t)(int16_t)e + 4));
    wr16((uint16_t)(obj + 0xC), e);
}

/* $00222C. A list of ready sprites (8 bytes each, their link byte the priority; ends at a word
 * $FFFF), each into its bucket. */
void DrawSpriteList(uint32_t list)
{
    uint8_t count = rd8(S_COUNT);
    uint16_t at = rd16(S_NEXT);
    while ((int8_t)count < SPRITES_MAX) {
        uint32_t first = bus_rd32(list), second = bus_rd32(list + 4);
        list += 8;
        uint16_t bucket = (uint16_t)(S_BUCKETS + 2 + ((first & 0xFF) & 0xFC));
        wr32(at, first);
        link_into(bucket, count);
        count++;
        wr8((uint16_t)(at + 3), count);
        wr32((uint16_t)(at + 4), second);
        at += 8;
        wr16(bucket, at);
        if (bus_rd16(list) == 0xFFFF) break;
    }
    wr16(S_NEXT, at);
    wr8(S_COUNT, count);
}
