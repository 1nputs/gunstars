/* Object types from $48D3C to $4FFFF: stage 5's space shooter (type $444, its director: the
 * colours, the waves spawned as the camera goes, the messages; the ships, turrets, mines, lines
 * of fire and shots of $448-$530) and the first of stage 6's (the final battle's) types
 * ($548-$560, $398). The helpers the space types share (the library at $4BB9E-$4C5FF: the shots
 * at the player, the puffs, the aim) are under "shared helpers" at the end. */
#include "game.h"

static void next(uint16_t obj, uint16_t by) { wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + by)); }
/* subq.w #1 on a word, then bpl: 1 when it is now below 0 */
static int dec(uint16_t a)
{
    wr16(a, (uint16_t)(rd16(a) - 1));
    return (rd16(a) & 0x8000) != 0;
}
static void add32(uint16_t a, uint32_t v) { wr32(a, rd32(a) + v); }
static void add16(uint16_t a, uint16_t v) { wr16(a, (uint16_t)(rd16(a) + v)); }
/* cmpi.w #v,(a) then bmi: 1 when the word is below v (signed) */
static int below(uint16_t a, uint16_t v) { return (int16_t)(rd16(a) - v) < 0; }
/* a word table's entry by a word index (sign-extended) */
static uint16_t tab16(uint32_t table, uint16_t i) { return rom16(table + (uint32_t)(int16_t)i); }
/* the sine and cosine tables ($2977C, $297FC: 256 words) */
#define SINES   0x02977Cu
#define COSINES 0x0297FCu
static int32_t sine(uint16_t i) { return (int16_t)tab16(SINES, i); }
static int32_t cosine(uint16_t i) { return (int16_t)tab16(COSINES, i); }

/* the shared helpers' own (at the end) */
static void shot_along(uint16_t obj, uint16_t type);
static void spread(uint16_t d5, uint16_t d6, uint16_t d7);

#define RANK         0xA20C      /* the difficulty's step (0-$E, by 2) */
#define DRIFT_MIDDLE 0xA994      /* stage 5: the camera's middle y */
#define MSG_STEP     0x80C2      /* the messages' step and script (messages.c) */
#define MSG_SCRIPT   0x80C8
#define MSG_NO_INPUT 0x80E4

/* ---- type $444: stage 5's director (in the boss's slot): the colours cycled, then by its
 * step the messages, the waves of each part of the flight (a list of the planes' x and the
 * event at it, +$4A the next entry), the fights' colours ---- */

/* $048FAE. Colours from src (count + 1 words) to dst and $80 below it (the palette and its
 * copy). */
void SpaceColourCopy(uint32_t src, uint16_t dst, uint16_t count)
{
    do {
        uint16_t c = rom16(src);
        src += 2;
        wr16((uint16_t)(dst - 0x80), c);
        wr16(dst, c);
        dst = (uint16_t)(dst + 2);
    } while ((int16_t)(count = (uint16_t)(count - 1)) != -1);
}

/* $048F78. A colour ($E32A, $E3AA) by the frame from 8 (every 4th frame); four from $E3A2 from
 * 4 sets by the frame. */
void SpaceColours(void)
{
    uint16_t c = tab16(0x048FBA, (uint16_t)(((int16_t)rd16(v_gameFrames) >> 2) & 0xE));
    wr16(0xE32A, c);
    wr16(0xE3AA, c);
    uint16_t d0 = (uint16_t)((uint16_t)(rd16(v_gameFrames) << 3) & 0x18);
    SpaceColourCopy(0x048FCA + d0, 0xE3A2, 3);
}

/* $04903C. The colours at $4904C, or 4 on (d0 bit 3). */
uint32_t SpaceColourTable(uint16_t d0)
{
    return 0x04904Cu + (uint32_t)(d0 & 8);
}

/* $048FEA. Two colours from $E382 flashing every 4th frame, two from $E386 every frame. */
void SpaceColoursFlash(void)
{
    SpaceColourCopy(SpaceColourTable((uint16_t)(rd16(v_gameFrames) << 1)), 0xE382, 1);
    SpaceColourCopy(SpaceColourTable((uint16_t)(rd16(v_gameFrames) << 3)) + 4, 0xE386, 1);
}

/* $049018. The four colours from $E382 steady. */
void SpaceColoursSet(void)
{
    SpaceColourCopy(0x04904C, 0xE382, 3);
}

/* $04905C. +$5C: a sprite word by the frame (4 palettes). */
void SpaceDirSprite(uint16_t obj)
{
    wr16(obj + 0x5C, tab16(0x049074, (uint16_t)((uint16_t)(rd16(v_gameFrames) << 1) & 6)));
}

/* the director's next step, its list from the start */
static void dir_next(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + 0x48, 0);
    wr16(obj + 0x4A, 0);
}

/* $048DA2. Step 0: input off, the first message ($39ACC), the colours' priority bit. */
void SpaceDir00(uint16_t obj)
{
    next(obj, 4);
    wr16(MSG_NO_INPUT, 1);
    wr16(MSG_STEP, 8);
    wr32(MSG_SCRIPT, 0x039ACC);
    wr16(0x808A, 0x8000);
}

/* $048DC2. Until the background's step 4. */
void SpaceDir04(uint16_t obj)
{
    if (!below(v_backStep, 4)) next(obj, 4);
}

/* $048DD2. The colours steady until the background's step 8: then the message at $39ABC. */
void SpaceDir08(uint16_t obj)
{
    SpaceColoursSet();
    if (below(v_backStep, 8)) return;
    dir_next(obj);
    wr16(MSG_STEP, 8);
    wr32(MSG_SCRIPT, 0x039ABC);
}

/* $0493E0. An object of type d4 at the right ($1CF), y d6 below the middle (d5). */
void SpaceSpawnRight(uint16_t type, uint16_t d5, uint16_t d6)
{
    uint16_t slot, t;
    if (!SpawnSlot(&slot, &t)) return;
    wr16(slot + O_TYPE, type);
    wr16(slot + O_Y, (uint16_t)(d5 + d6));
    wr16(slot + O_X, 0x1CF);
    SpaceEvent00();
}

/* $049744. The same with +$48 (d3). */
void SpaceSpawnRightArg(uint16_t arg, uint16_t type, uint16_t d5, uint16_t d6)
{
    uint16_t slot, t;
    if (!SpawnSlot(&slot, &t)) return;
    wr16(slot + O_TYPE, type);
    wr16(slot + O_Y, (uint16_t)(d5 + d6));
    wr16(slot + O_X, 0x1CF);
    wr16(slot + 0x48, arg);
}

/* $04950E. The object at $DE80 made a $4B4 (its step and flags cleared). */
uint16_t SpaceSpawn4B4(void)
{
    wr16(0xDE80 + O_TYPE, 0x4B4);
    wr16(0xDE80 + O_STATE, 0);
    wr16(0xDE80 + O_FLAGS, 0);
    return 0xDE80;
}

/* loc_0496B2: a spawned object's type, x, y, +6 from four words */
static void spawn_fill(uint16_t slot, uint32_t list)
{
    wr16(slot + O_TYPE, rom16(list));
    wr16(slot + O_X, rom16(list + 2));
    wr16(slot + O_Y, rom16(list + 4));
    wr16(slot + 6, rom16(list + 6));
}

/* $04969C. An object from four words (type, x, y, +6) in a free slot from $D400: Z set and the
 * slot. */
int SpaceSpawnHigh(uint32_t list, uint16_t *slot)
{
    uint16_t t;
    if (!SlotFreeHigh(slot, &t)) return 0;
    spawn_fill(*slot, list);
    return 1;
}

/* loc_0496A8: the same in a free slot from $CB00 */
static int spawn_list(uint32_t list, uint16_t *slot)
{
    uint16_t t;
    if (!SpawnSlot(slot, &t)) return 0;
    spawn_fill(*slot, list);
    return 1;
}

/* loc_0494A2: an object $4D0 of kind (+6) k */
static void spawn_4d0(uint16_t k)
{
    uint16_t slot, t;
    if (!SpawnSlot(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x4D0);
    wr16(slot + 6, k);
}

/* loc_0494CA: an object $4D8 of kind k, +$48 d6 */
static void spawn_4d8(uint16_t k, uint16_t d6)
{
    uint16_t slot, t;
    if (!SpawnSlot(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x4D8);
    wr16(slot + 6, k);
    wr16(slot + 0x48, d6);
}

/* an object $4C0 of kind k */
static void spawn_4c0(uint16_t k)
{
    uint16_t slot, t;
    if (!SpawnSlot(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x4C0);
    wr16(slot + 6, k);
}

/* the events (the table at $490B8, by the list's word): d5 is the camera's middle less its y */

/* $049402. Nothing. */
void SpaceEvent00(void)
{
}

/* $04934A. A $448 at $14E. */
void SpaceEvent04(uint16_t d5)
{
    SpaceSpawnRight(0x448, d5, 0x14E);
}

/* $049356. Two $448 at $C0 and $150. */
void SpaceEvent08(uint16_t d5)
{
    SpaceSpawnRight(0x448, d5, 0xC0);
    SpaceSpawnRight(0x448, d5, 0x150);
}

/* $0497A2. The message at $39A98. */
void SpaceEvent0C(void)
{
    wr16(MSG_STEP, 8);
    wr32(MSG_SCRIPT, 0x039A98);
}

/* $0492FE. A $450 at $108. */
void SpaceEvent10(uint16_t d5)
{
    SpaceSpawnRight(0x450, d5, 0x108);
}

/* $04930A. Two $450 at $D8 and $120. */
void SpaceEvent14(uint16_t d5)
{
    SpaceSpawnRight(0x450, d5, 0xD8);
    SpaceSpawnRight(0x450, d5, 0x120);
}

/* $04936E. A $448 at $D8. */
void SpaceEvent18(uint16_t d5)
{
    SpaceSpawnRight(0x448, d5, 0xD8);
}

/* $04937A. Two $448 at $E0 and $130. */
void SpaceEvent1C(uint16_t d5)
{
    SpaceSpawnRight(0x448, d5, 0xE0);
    SpaceSpawnRight(0x448, d5, 0x130);
}

/* $049322. Above the easiest level: a $480 at $C0 (+$48 $80). */
void SpaceEvent20(uint16_t d5)
{
    if (!rd16(v_difficulty)) {
        SpaceEvent00();
        return;
    }
    SpaceSpawnRightArg(0x80, 0x480, d5, 0xC0);
}

/* $04933A. A $480 at $120 (+$48 $40). */
void SpaceEvent24(uint16_t d5)
{
    SpaceSpawnRightArg(0x40, 0x480, d5, 0x120);
}

/* $049392. Above the easiest level two $508 (at $F8, $128; +$48 $400, $500); a $484 at $110. */
void SpaceEvent28(uint16_t d5)
{
    if (rd16(v_difficulty)) {
        SpaceSpawnRightArg(0x400, 0x508, d5, 0xF8);
        SpaceSpawnRightArg(0x500, 0x508, d5, 0x128);
    }
    SpaceSpawnRight(0x484, d5, 0x110);
}

/* $0494E2. The $4B4 at $DE80 (+$48 $400). */
void SpaceEvent2C(void)
{
    uint16_t a = SpaceSpawn4B4();
    wr16(a + 0x48, 0x400);
    wr16(a + 0x4C, 0);
    wr16(a + 0x4E, 0);
}

/* $0495AE. Two $524 from the left (at $D0 and $130), moving right. */
void SpaceEvent30(void)
{
    uint16_t slot;
    if (!SpaceSpawnHigh(0x0495E8, &slot)) return;
    wr32(slot + 0x18, 0xE000);
    wr16(slot + 0x48, 0xC0);
    if (!SpaceSpawnHigh(0x0495F0, &slot)) return;
    wr32(slot + 0x18, 0xE000);
    wr16(slot + 0x48, 0xC2);
}

/* $049404. A $4C0 of kind 4. */
void SpaceEvent34(void)
{
    spawn_4c0(4);
}

/* $04941A. Kind 8. */
void SpaceEvent38(void)
{
    spawn_4c0(8);
}

/* $049470. A $4D0 of kind 4. */
void SpaceEvent3C(void)
{
    spawn_4d0(4);
}

/* $049476. Kind 8. */
void SpaceEvent40(void)
{
    spawn_4d0(8);
}

/* $04947C. Kind 12. */
void SpaceEvent44(void)
{
    spawn_4d0(12);
}

/* $049482. Kind 16. */
void SpaceEvent48(void)
{
    spawn_4d0(16);
}

/* $049488. Kind 20. */
void SpaceEvent4C(void)
{
    spawn_4d0(20);
}

/* $04948E. Kind 24. */
void SpaceEvent50(void)
{
    spawn_4d0(24);
}

/* $049494. Kind 28. */
void SpaceEvent54(void)
{
    spawn_4d0(28);
}

/* $0494B6. A $4D8 of kind 4 (+$48 $4F0). */
void SpaceEvent58(void)
{
    spawn_4d8(4, 0x4F0);
}

/* $0496C4. A $4A4 of kind 4 at the right ($1CF, $100) and two $4D4 of kind $2C below it ($16F)
 * tied to it (+$5C, the type at +$5E), +$5A -50 and 50. */
void SpaceEvent5C(void)
{
    uint16_t slot, t;
    if (!SpawnSlot(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x4A4);
    wr16(slot + 6, 4);
    wr16(slot + O_X, 0x1CF);
    wr16(slot + O_Y, 0x100);
    uint16_t a4 = slot;
    uint16_t d6 = (uint16_t)-50;
    for (int i = 0; i < 2; i++) {
        if (!SlotFreeHigh(&slot, &t)) return;
        wr16(slot + O_TYPE, 0x4D4);
        wr16(slot + 6, 0x2C);
        wr16(slot + O_X, 0x1CF);
        wr16(slot + O_Y, 0x16F);
        wr16(slot + 0x58, 0);
        wr16(slot + 0x5A, d6);
        wr16(slot + 0x5C, a4);
        wr16(slot + 0x5E, 0x4A4);
        d6 = (uint16_t)(d6 + 0x64);
    }
}

/* $049764. A $4DC of kind 1. */
void SpaceEvent60(void)
{
    uint16_t slot, t;
    if (!SpawnSlot(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x4DC);
    wr16(slot + 6, 1);
}

/* $049430. A $4C0 of kind $C. */
void SpaceEvent64(void)
{
    spawn_4c0(0xC);
}

/* $04954C. A $4A4 of kind 4 at the right ($1CF, $100). */
void SpaceEvent68(void)
{
    uint16_t slot;
    spawn_list(0x049556, &slot);
}

/* $04955E. Above the easiest level, a $4A4 of kind $C at the left ($71, $100). */
void SpaceEvent6C(void)
{
    uint16_t slot;
    if (!rd16(v_difficulty)) return;
    spawn_list(0x049570, &slot);
}

/* $0497B2. The message at $39AA4. */
void SpaceEvent70(void)
{
    wr16(MSG_STEP, 8);
    wr32(MSG_SCRIPT, 0x039AA4);
}

/* $0494C0. A $4D8 of kind 4 (+$48 $140). */
void SpaceEvent74(void)
{
    spawn_4d8(4, 0x140);
}

/* $0494F6. The $4B4 at $DE80 (+$48 $800, +$4C -1, +$4E 1). */
void SpaceEvent78(void)
{
    uint16_t a = SpaceSpawn4B4();
    wr16(a + 0x48, 0x800);
    wr16(a + 0x4C, 0xFFFF);
    wr16(a + 0x4E, 1);
}

/* $049520. A $50C (+$48 $400). */
void SpaceEvent7C(void)
{
    uint16_t slot, t;
    if (!SpawnSlot(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x50C);
    wr16(slot + 0x48, 0x400);
    wr16(slot + 0x4A, 0);
}

/* $049726. A $51C at x $40. */
void SpaceEvent80(void)
{
    uint16_t slot, t;
    if (!SpawnSlot(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x51C);
    wr16(slot + O_X, 0x40);
    wr16(slot + 0x4A, 0);
    wr16(slot + 0x4C, 0);
}

/* $0493C2. A $520 in one of the last four slots, at $1C0, $D9 below the middle. */
void SpaceEvent84(uint16_t d5)
{
    uint16_t slot;
    if (!SlotFreeLast(&slot)) return;
    wr16(slot + O_TYPE, 0x520);
    wr16(slot + O_Y, (uint16_t)(d5 + 0xD9));
    wr16(slot + O_X, 0x1C0);
}

/* $049578. A $4A4 of kind $1C at the right ($138). */
void SpaceEvent88(void)
{
    uint16_t slot;
    spawn_list(0x049582, &slot);
}

/* $04958A. A $4A4 of kind $1C at the right ($B8). */
void SpaceEvent8C(void)
{
    uint16_t slot;
    spawn_list(0x049594, &slot);
}

/* $04949A. A $4D0 of kind 32. */
void SpaceEvent90(void)
{
    spawn_4d0(32);
}

/* $04953A. The object at $DFA0 made a $530. */
void SpaceEvent94(void)
{
    wr16(0xDFA0 + O_TYPE, 0x530);
    wr16(0xDFA0 + 0x48, 0);
    wr16(0xDFA0 + 0x4A, 0);
}

/* $049446. The object at $DFA0 made a $4C0 of kind $10. */
void SpaceEvent98(void)
{
    wr16(0xDFA0 + O_TYPE, 0x4C0);
    wr16(0xDFA0 + 6, 0x10);
    wr16(0xDFA0 + O_STATE, 0);
}

/* $0495F8. A $528 from the right ($100), moving left at 2. */
void SpaceEvent9C(void)
{
    uint16_t slot;
    if (!SpaceSpawnHigh(0x04961A, &slot)) return;
    wr16(slot + O_TYPE, 0x528);
    wr32(slot + 0x18, 0xFFFE0000);
    wr16(slot + 0x48, 0x50);
}

/* $04945A. A $4C0 of kind $14. */
void SpaceEventA0(void)
{
    spawn_4c0(0x14);
}

/* loc_049628: a $524 from list, moving left fast */
static void spawn_524_left(uint32_t list)
{
    uint16_t slot;
    if (!SpaceSpawnHigh(list, &slot)) return;
    wr32(slot + 0x18, 0xFFF98000);
    wr16(slot + 0x48, 0x20);
}

/* $049622. A $524 from the right at $D0, fast. */
void SpaceEventA4(void)
{
    spawn_524_left(0x049640);
}

/* $049648. The same at $130. */
void SpaceEventA8(void)
{
    spawn_524_left(0x049652);
}

/* $04965A. The same at $100. */
void SpaceEventAC(void)
{
    spawn_524_left(0x04961A);
}

/* loc_04966A: a $4BC from list, moving right fast */
static void spawn_4bc_right(uint32_t list)
{
    uint16_t slot;
    if (!SpaceSpawnHigh(list, &slot)) return;
    wr32(slot + 0x18, 0x58000);
    wr16(slot + 0x48, 0x1C);
}

/* $049664. A $4BC from the left at $D0. */
void SpaceEventB0(void)
{
    spawn_4bc_right(0x049682);
}

/* $04968A. The same at $130. */
void SpaceEventB4(void)
{
    spawn_4bc_right(0x049694);
}

/* $0492EC. Every object from $C9E0 gone but types $44C and $508. */
void SpaceEventB8(void)
{
    ObjectsVanishFrom(0xC9E0, 0x44C, 0x508);
}

/* $0494A0. A $4D0 of kind 36. */
void SpaceEventBC(void)
{
    spawn_4d0(36);
}

/* $04959C. A $4A4 of kind 8 at the right ($100). */
void SpaceEventC0(void)
{
    uint16_t slot;
    spawn_list(0x0495A6, &slot);
}

/* loc_049782: a $4D4 of kind k at $1C0, $170 */
static void spawn_4d4(uint16_t k)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x4D4);
    wr16(slot + 6, k);
    wr16(slot + O_Y, 0x170);
    wr16(slot + O_X, 0x1C0);
}

/* $049780. A $4D4 of kind 52. */
void SpaceEventC4(void)
{
    spawn_4d4(52);
}

/* $04977A. Kind 60. */
void SpaceEventC8(void)
{
    spawn_4d4(60);
}

/* loc_0490A0: the list's next event (its word after the x) */
static void space_event(uint16_t obj, uint32_t list, uint16_t d1)
{
    d1 = (uint16_t)(d1 + 2);
    add16(obj + 0x4A, 4);
    uint16_t d5 = (uint16_t)(rd16(DRIFT_MIDDLE) - rd16(v_camY));
    uint32_t fn = rom32(0x0490B8 + (uint32_t)(int16_t)rom16(list + (uint32_t)(int16_t)d1));
    switch (fn) {
    case 0x049402: SpaceEvent00(); break;
    case 0x04934A: SpaceEvent04(d5); break;
    case 0x049356: SpaceEvent08(d5); break;
    case 0x0497A2: SpaceEvent0C(); break;
    case 0x0492FE: SpaceEvent10(d5); break;
    case 0x04930A: SpaceEvent14(d5); break;
    case 0x04936E: SpaceEvent18(d5); break;
    case 0x04937A: SpaceEvent1C(d5); break;
    case 0x049322: SpaceEvent20(d5); break;
    case 0x04933A: SpaceEvent24(d5); break;
    case 0x049392: SpaceEvent28(d5); break;
    case 0x0494E2: SpaceEvent2C(); break;
    case 0x0495AE: SpaceEvent30(); break;
    case 0x049404: SpaceEvent34(); break;
    case 0x04941A: SpaceEvent38(); break;
    case 0x049470: SpaceEvent3C(); break;
    case 0x049476: SpaceEvent40(); break;
    case 0x04947C: SpaceEvent44(); break;
    case 0x049482: SpaceEvent48(); break;
    case 0x049488: SpaceEvent4C(); break;
    case 0x04948E: SpaceEvent50(); break;
    case 0x049494: SpaceEvent54(); break;
    case 0x0494B6: SpaceEvent58(); break;
    case 0x0496C4: SpaceEvent5C(); break;
    case 0x049764: SpaceEvent60(); break;
    case 0x049430: SpaceEvent64(); break;
    case 0x04954C: SpaceEvent68(); break;
    case 0x04955E: SpaceEvent6C(); break;
    case 0x0497B2: SpaceEvent70(); break;
    case 0x0494C0: SpaceEvent74(); break;
    case 0x0494F6: SpaceEvent78(); break;
    case 0x049520: SpaceEvent7C(); break;
    case 0x049726: SpaceEvent80(); break;
    case 0x0493C2: SpaceEvent84(d5); break;
    case 0x049578: SpaceEvent88(); break;
    case 0x04958A: SpaceEvent8C(); break;
    case 0x04949A: SpaceEvent90(); break;
    case 0x04953A: SpaceEvent94(); break;
    case 0x049446: SpaceEvent98(); break;
    case 0x0495F8: SpaceEvent9C(); break;
    case 0x04945A: SpaceEventA0(); break;
    case 0x049622: SpaceEventA4(); break;
    case 0x049648: SpaceEventA8(); break;
    case 0x04965A: SpaceEventAC(); break;
    case 0x049664: SpaceEventB0(); break;
    case 0x04968A: SpaceEventB4(); break;
    case 0x0492EC: SpaceEventB8(); break;
    case 0x0494A0: SpaceEventBC(); break;
    case 0x04959C: SpaceEventC0(); break;
    case 0x049780: SpaceEventC4(); break;
    case 0x04977A: SpaceEventC8(); break;
    default: call68k_missing(fn); break;
    }
}

/* loc_04907C: the list's next event once the planes' x reaches its x */
static void events_ahead(uint16_t obj, uint32_t list)
{
    uint16_t d1 = rd16(obj + 0x4A);
    if ((int16_t)(rd16(v_planes) - rom16(list + (uint32_t)(int16_t)d1)) < 0) {
        SpaceEvent00();
        return;
    }
    space_event(obj, list, d1);
}

/* loc_049090: the same going back (the planes' x below the entry's) */
static void events_behind(uint16_t obj, uint32_t list)
{
    uint16_t d1 = rd16(obj + 0x4A);
    if ((int16_t)(rd16(v_planes) - rom16(list + (uint32_t)(int16_t)d1)) >= 0) {
        SpaceEvent00();
        return;
    }
    space_event(obj, list, d1);
}

/* $048DFC. The first part's waves ($49184) until the camera holds for a fight; input back on
 * once the message is done. */
void SpaceDir0C(uint16_t obj)
{
    if (!rd16(MSG_STEP)) wr16(MSG_NO_INPUT, 0);
    if (bit8(v_cameraHold, 7)) {
        dir_next(obj);
        return;
    }
    events_ahead(obj, 0x049184);
}

/* $048E2A. The fight: a $45C at $C9E0. */
void SpaceDir10(uint16_t obj)
{
    next(obj, 4);
    wr16(0xC9E0 + O_TYPE, 0x45C);
    wr16(0xC9E0 + O_STATE, 0);
}

/* $048E3C. Until it is gone: then the camera on, the colours of part 4. */
void SpaceDir14(uint16_t obj)
{
    if (rd16(0xC9E0)) return;
    next(obj, 4);
    clr8(v_cameraHold, 7);
    StageColoursFor(4);
}

/* $048E5A. The waves at $491F0 until the camera's step $10. */
void SpaceDir18(uint16_t obj)
{
    if (!below(v_cameraStep, 0x10)) {
        dir_next(obj);
        return;
    }
    events_ahead(obj, 0x0491F0);
}

/* $048E7C. Until the camera's step $14. */
void SpaceDir1C(uint16_t obj)
{
    if (below(v_cameraStep, 0x14)) return;
    dir_next(obj);
}

/* $048E96. The waves at $49224 (going back) until the camera holds. */
void SpaceDir20(uint16_t obj)
{
    if (bit8(v_cameraHold, 7)) {
        dir_next(obj);
        return;
    }
    events_behind(obj, 0x049224);
}

/* $048EB8. On. */
void SpaceDir24(uint16_t obj)
{
    next(obj, 4);
}

/* $048EBE. The camera on, the colours of part 8. */
void SpaceDir28(uint16_t obj)
{
    next(obj, 4);
    clr8(v_cameraHold, 7);
    StageColoursFor(8);
}

/* $048ED2. The waves at $49240 (going back) until the camera's step $20. */
void SpaceDir2C(uint16_t obj)
{
    if (!below(v_cameraStep, 0x20)) {
        dir_next(obj);
        return;
    }
    events_behind(obj, 0x049240);
}

/* $048EF4. Until the camera's step $20. */
void SpaceDir30(uint16_t obj)
{
    if (below(v_cameraStep, 0x20)) return;
    dir_next(obj);
}

/* $048F0E. The waves at $49270 until the camera holds. */
void SpaceDir34(uint16_t obj)
{
    if (bit8(v_cameraHold, 7)) {
        dir_next(obj);
        return;
    }
    events_ahead(obj, 0x049270);
}

/* $048F30. Nothing. */
void SpaceDir38(uint16_t obj)
{
    (void)obj;
}

/* $048F32. The camera on, the colours of part $C. */
void SpaceDir3C(uint16_t obj)
{
    next(obj, 4);
    clr8(v_cameraHold, 7);
    StageColoursFor(12);
}

/* $048F44. The waves at $49294 until the camera holds. */
void SpaceDir40(uint16_t obj)
{
    if (bit8(v_cameraHold, 7)) {
        dir_next(obj);
        return;
    }
    events_ahead(obj, 0x049294);
}

/* $048F6A. It becomes the base's director (type $488). */
void SpaceDir48(uint16_t obj)
{
    wr16(obj + O_TYPE, 0x488);
    wr16(obj + O_STATE, 0);
    wr16(obj + O_VITALITY, 0);
}

/* $048F66. The same. */
void SpaceDir44(uint16_t obj)
{
    SpaceDir48(obj);
}

/* $048D3C. Type $444: the colours cycled, +$5C's sprite word; then its step. */
void SpaceDirector(uint16_t obj)
{
    SpaceColours();
    SpaceColoursFlash();
    SpaceDirSprite(obj);
    call_object(rom32(0x048D52 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- stage 5's ships and turrets ($448-$4DC): a hit takes their vitality (+$24); below 0
 * they score and become a wreck (type $44C, SpaceWreck), +$5C its look; off the screen they
 * are removed ---- */

/* loc_04C39E: its sprite blinking (every 8 frames) */
static void blink_a6b5(uint16_t obj)
{
    wr16(obj + 0x0E, bit8(v_gameFrames + 1, 3) ? 0xA6BE : 0xA6B5);
}

/* loc_04B35A: +$48 from a table by the difficulty's step */
static void wait_by_rank(uint16_t obj, uint32_t table)
{
    wr16(obj + 0x48, tab16(table, rd16(RANK)));
}

/* $04B354. Its next volley's wait (by the difficulty's step). */
void Space448Reload(uint16_t obj)
{
    wait_by_rank(obj, 0x04B366);
}

/* $04B386. Its volley's shots less one (+$4A, by the difficulty's step). */
void Space448Volley(uint16_t obj)
{
    wr16(obj + 0x4A, tab16(0x04B398, rd16(RANK)));
}

/* $04B2E2. Shot down: points, a wreck. Its set-up; then for $220 frames volleys of shots at the
 * player ($A frames apart), its sprite blinking. */
void Space448Run(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) & 0x8000) {
        AddScore(obj, 0x1210);
        wr16(obj + 0x5C, 0);
        SpaceWreck(obj);
        return;
    }
    if (!rd16(obj + O_STATE)) {
        wr16(obj + 0x4C, 0x220);
        wr16(obj + 0x48, 0x20);
        Space448Volley(obj);
        uint32_t a0 = 0x04C4E6;
        uint16_t d7 = 1;
        ObjectSetupNext(obj, &a0, &d7);
        return;
    }
    if (!(rd16(obj + 0x4C) & 0x8000)) {
        add16(obj + 0x4C, 0xFFFF);
        if (dec(obj + 0x48)) {
            uint16_t slot;
            SpawnTowardPlayer(obj, &slot);
            wr16(obj + 0x48, 0xA);
            if (dec(obj + 0x4A)) {
                Space448Reload(obj);
                Space448Volley(obj);
            }
        }
    }
    blink_a6b5(obj);
}

/* $04B2DA. Type $448: a ship that fires volleys at the player. */
void Space448(uint16_t obj)
{
    Space448Run(obj);
    ObjectRemoveOffscreen(obj);
}

/* $04B282. Type $44C: a wreck: its animation (the burst) while flags bit 11; when it ends, the
 * wreck's look (+$5C: 0, 6, $C) shown until it leaves the screen. */
void Space44C(uint16_t obj)
{
    if (bit8(obj + O_FLAGS, 3)) {
        AnimList(obj);
        if (rd16(obj + O_FLAGS) != 0x1000) return;
        wr16(obj + O_FLAGS, 0x8100);
        uint32_t a = 0x04B2C8 + (uint32_t)(int16_t)rd16(obj + 0x5C);
        wr16(obj + 0x0E, rom16(a));
        wr16(obj + 0x08, rom16(a + 2));
        wr16(obj + 0x0A, rom16(a + 4));
        wr8(obj + 0x20, 0xBC);
    }
    ObjectRemoveOffscreen(obj);
}

/* loc_04B3BA: its gun (+$4E) removed */
static void remove_part(uint16_t obj)
{
    wr16(rd16(obj + 0x4E) + O_FLAGS, 0x1000);
}

/* loc_04B3F8: a gun (in one of the last four slots; none: it is removed) on an arm (+$54
 * long, +$56 its angle) */
static void turret_setup(uint16_t obj)
{
    wr16(obj + 0x4C, 0x180);
    wr16(obj + 0x48, 0x30);
    wr16(obj + 0x54, 0x16);
    wr16(obj + 0x56, 0);
    wr16(obj + 0x48, 0x20);
    uint16_t slot;
    if (!SlotFreeLast(&slot)) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    SpaceArmPart(obj, slot);
    uint32_t a0 = 0x04C4F6;
    uint16_t d7 = 1;
    ObjectSetupNext(obj, &a0, &d7);
}

/* $04B3C6. Shot down: points, a wreck (its gun gone). Its set-up (a gun on an arm); then the
 * arm turns to the player; once out, a shot along it every so often (by the difficulty's
 * step). */
void Space450Run(uint16_t obj)
{
    if (rd16(obj + O_STATE)) {
        if (rd16(obj + O_VITALITY) & 0x8000) {
            AddScore(obj, 0x3090);
            wr16(obj + 0x5C, 6);
            SpaceWreck(obj);
            remove_part(obj);
            return;
        }
        uint16_t d7 = SpaceAim(obj);
        SpaceArm(obj, d7);
        if (SpaceArmExtend(obj)) return;
        if (!dec(obj + 0x48)) return;
        wr16(obj + 0x54, 0x10);
        SpaceShot454(obj);
        wait_by_rank(obj, 0x04B376);
        return;
    }
    turret_setup(obj);
}

/* $04B3A8. Type $450: a turret with a gun on an arm; removed (with its gun) off the screen. */
void Space450(uint16_t obj)
{
    Space450Run(obj);
    ObjectRemoveOffscreen(obj);
    if (rd16(obj + O_FLAGS) != 0x1000) return;
    remove_part(obj);
}

/* loc_04C662: it becomes a small puff (type $100, the animation at $5663A), still */
static void puff_small(uint16_t obj)
{
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x05663A);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + O_STATE, 0);
    wr8(obj + 0x21, 0);
    QueueSound(0xCA);
}

/* $04C628. Type $454: a shot (also the start of type $500): set up; its sprite by the frame;
 * hitting something it becomes a puff (a $500 bursts first). */
void Space454(uint16_t obj)
{
    ObjectRemoveOffscreen(obj);
    if (!rd16(obj + O_STATE)) {
        wr16(obj + 0x56, 0x30);
        uint32_t a0 = 0x04C52A;
        uint16_t d7;
        ObjectSetupAll(obj, &a0, &d7);
        return;
    }
    wr16(obj + 0x08, 0x500);
    wr16(obj + 0x0A, 0xF8F8);
    if (bit8(obj + 0x22, 4)) {
        if (rd16(obj + O_TYPE) == 0x500) SpaceRing(obj);
        puff_small(obj);
        return;
    }
    wr16(obj + 0x0E, tab16(0x0461CA, (uint16_t)((uint16_t)(rd16(v_gameFrames) << 1) & 6)));
}

/* $04C610. Type $500: a shot that bursts into a ring of eight after +$56 frames (or on hitting
 * something). */
void Space500(uint16_t obj)
{
    if (!rd16(obj + O_STATE) || !dec(obj + 0x56)) {
        Space454(obj);
        return;
    }
    SpaceRing(obj);
    puff_small(obj);
}

/* loc_04C6C6: it becomes a puff (type $100, the animation at $565F6), still */
static void puff_hit(uint16_t obj)
{
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x0565F6);
    wr8(obj + 0x21, 0);
    wr16(obj + O_STATE, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    QueueSound(0xCA);
}

/* $04C6A2. Type $460: a shot (its sprite word from $CA38); hitting a player it becomes a puff. */
void Space460(uint16_t obj)
{
    wr16(obj + 0x0E, rd16(0xCA38));
    if (bit8(obj + 0x22, 4) && (rd16(obj + 0x40) == 0xA400 || rd16(obj + 0x40) == 0xA600)) {
        puff_hit(obj);
        return;
    }
    ObjectRemoveOffscreen(obj);
}

/* loc_04C428: the object at a6 made a $460 shot from here at the player */
static void shot460_aimed(uint16_t obj)
{
    uint16_t target = PlayerTarget();
    uint32_t d1 = (uint16_t)(rd16(target + O_Y) - rd16(obj + O_Y));
    uint16_t a = Atan2((int16_t)(rd16(target + O_X) - rd16(obj + O_X)), &d1);
    SpaceShot460(obj, AngleIndex(&a));
}

/* the object at a6: a slot (movea.w) */
static void set_a6(uint16_t slot) { reg_a6 = (uint32_t)(int32_t)(int16_t)slot; }

/* $04C774. Types $464, $46C, $470, $474: a flash ($F frames, its look changing at 7); then a
 * $464 becomes a shot at the player, a $46C a fan of three, a $470 of five (the slots allowing),
 * a $474 a bouncing shot (type $478). */
void Space464(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + O_FLAGS, 0x8000);
        wr16(obj + 0x48, 0xF);
    }
    if (!dec(obj + 0x48)) {
        if (!below(obj + 0x48, 7)) {
            wr16(obj + 0x0E, 0x87FD);
            wr16(obj + 0x08, 0);
            wr16(obj + 0x0A, 0xFCFC);
            return;
        }
        wr16(obj + 0x0E, 0xE7FE);
        wr16(obj + 0x08, 0x100);
        wr16(obj + 0x0A, 0xFCF8);
        return;
    }
    if (rd16(obj + O_TYPE) == 0x464) {
        wr16(obj + 0x08, 0);
        wr16(obj + 0x0A, 0xFCFC);
        set_a6(obj);
        shot460_aimed(obj);
        return;
    }
    if (rd16(obj + O_TYPE) == 0x474) {
        wr16(obj + O_TYPE, 0x478);
        wr16(obj + O_FLAGS, 0x8C00);
        wr16(obj + 0x08, 0x500);
        wr16(obj + 0x0A, 0xF8F8);
        wr8(obj + 0x21, 0x10);
        wr32(obj + 0x2C, 0xFE02FE02);
        wr16(obj + 0x26, 0xB);
        wr16(obj + 0x48, 3);
        uint16_t target = PlayerTarget();
        wr16(obj + 0x18, (uint16_t)(rd16(target + O_X) - rd16(obj + O_X)) & 0x8000 ? 0xFFFF : 1);
        wr16(obj + 0x1C, (uint16_t)(rd16(target + O_Y) - rd16(obj + O_Y)) & 0x8000 ? 0xFFFF : 1);
        return;
    }
    uint16_t target = PlayerTarget();
    uint32_t d1 = (uint16_t)(rd16(target + O_Y) - rd16(obj + O_Y));
    uint16_t a = Atan2((int16_t)(rd16(target + O_X) - rd16(obj + O_X)), &d1);
    uint16_t d6 = AngleIndex(&a), d0 = d6, slot, t;
    if (SlotFreeHigh(&slot, &t)) {
        set_a6(slot);
        d0 = (uint16_t)(d0 - 0x20) & 0x1FE;
        SpaceShot460(obj, d0);
        if (SlotFreeHigh(&slot, &t)) {
            set_a6(slot);
            d0 = (uint16_t)(d0 + 0x40) & 0x1FE;
            SpaceShot460(obj, d0);
            if (rd16(obj + O_TYPE) != 0x46C && SlotFreeMid(&slot, &t)) {
                set_a6(slot);
                d0 = (uint16_t)(d0 + 0x20) & 0x1FE;
                SpaceShot460(obj, d0);
                if (SlotFreeMid(&slot, &t)) {
                    set_a6(slot);
                    d0 = (uint16_t)(d0 - 0x80) & 0x1FE;
                    SpaceShot460(obj, d0);
                }
            }
        }
    }
    wr16(obj + 0x08, 0);
    wr16(obj + 0x0A, 0xFCFC);
    set_a6(obj);
    SpaceShot460(obj, d6);
}

/* $04CAEE. Its speed down (+$1C) eased toward the player's y (a0): $1000 a frame to 1. */
void Space47CSteer(uint16_t obj, uint16_t target)
{
    if ((int16_t)(rd16(target + O_Y) - rd16(obj + O_Y)) >= 0) {
        if ((rd16(obj + 0x1C) & 0x8000) || (int32_t)(rd32(obj + 0x1C) - 0x10000u) < 0)
            add32(obj + 0x1C, 0x1000);
        return;
    }
    if (!(rd16(obj + 0x1C) & 0x8000) || (int32_t)(rd32(obj + 0x1C) - 0xFFFF0000u) >= 0)
        add32(obj + 0x1C, (uint32_t)-0x1000);
}

/* $04C9C6. Type $47C: a flier ($60 frames chasing the player, eased to 2.5 across, then on its
 * way), leaving a puff behind every 8th frame; shot down (or hitting something) a puff. */
void Space47C(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + 0x48, 0x60);
        uint32_t a0 = 0x04C550;
        uint16_t d7;
        ObjectSetupAll(obj, &a0, &d7);
        return;
    }
    if (bit8(obj + 0x22, 4) || (rd16(obj + O_VITALITY) & 0x8000)) {
        if (!bit8(obj + 0x22, 4)) AddScore(obj, 2);
        SpaceShotIfHard(obj);
        wr16(obj + O_TYPE, 0x100);
        wr32(obj + 0x48, 0x0563EE);
        wr16(obj + O_STATE, 0);
        wr8(obj + 0x21, 0);
        QueueSound(0xCE);
        return;
    }
    int left;
    if (dec(obj + 0x48)) {
        left = bit8(obj + 0x18, 7);
    } else {
        uint16_t target = PlayerTarget();
        Space47CSteer(obj, target);
        left = (int16_t)(rd16(target + O_X) - rd16(obj + O_X)) < 0;
    }
    if (left) {
        set8(obj + 0x0E, 3);
        if (!(rd16(obj + 0x18) & 0x8000) || (int32_t)(rd32(obj + 0x18) - 0xFFFD8000u) >= 0)
            add32(obj + 0x18, (uint32_t)-0x2000);
    } else {
        clr8(obj + 0x0E, 3);
        if ((rd16(obj + 0x18) & 0x8000) || (int32_t)(rd32(obj + 0x18) - 0x28000u) < 0)
            add32(obj + 0x18, 0x2000);
    }
    if (rd16(v_gameFrames) & 7) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x05663A);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr32(slot + 0x18, (uint32_t)-rd32(obj + 0x18));
    wr32(slot + 0x1C, rom32(0x04CADE + (rd16(v_randomSeed) & 0xC)));
}

/* ---- types $480 and $508: a hatch that opens, then (a $480) launches fliers ($47C) or (a $508)
 * fires a line of fire ($4B0), and closes ---- */

/* $04B49C. Shown. */
void Space480Start(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + O_FLAGS, 0x100);
}

/* $04B4A8. Waiting +$48 frames. */
void Space480Wait(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 4);
    wr16(obj + 0x48, 0);
}

/* loc_04B4D2: its look by +$48 (opening: 0-5, 6-$B, then open and hurting) */
static void hatch_look(uint16_t obj)
{
    uint16_t d7 = 0;
    uint32_t a0 = 0x04C506;
    if (below(obj + 0x48, 6)) {
        ObjectSetup(obj, &a0, &d7);
        return;
    }
    a0 = 0x04C510;
    if (below(obj + 0x48, 0xC)) {
        ObjectSetup(obj, &a0, &d7);
        return;
    }
    d7 = 1;
    a0 = 0x04C51A;
    ObjectSetup(obj, &a0, &d7);
}

/* $04B4BA. Opening over $C frames; then open for $100. */
void Space480Open(uint16_t obj)
{
    add16(obj + 0x48, 1);
    if (!below(obj + 0x48, 0xC)) {
        next(obj, 4);
        wr16(obj + 0x4A, 0x100);
    }
    hatch_look(obj);
}

/* $04B500. Open: from $98 frames left to $80, a $480 launches a flier every 8th frame, a $508
 * fires once (at $97); then closing ($B frames, harmless). */
void Space480Fire(uint16_t obj)
{
    if (!below(obj + 0x4A, 0x80) && below(obj + 0x4A, 0x98)) {
        if (rd16(obj + O_TYPE) == 0x480) {
            uint16_t slot, t;
            if (!(rd16(v_gameFrames) & 7) && SlotFreeHigh(&slot, &t)) {
                wr16(slot + O_TYPE, 0x47C);
                wr16(slot + O_X, rd16(obj + O_X));
                wr16(slot + O_Y, rd16(obj + O_Y));
                wr32(slot + 0x18, 0x20000);
                QueueSound(0xCA);
            }
        } else if (rd16(obj + 0x4A) == 0x97) {
            SpaceFire4B0(obj, 0, 0, 0x6000, 0, 0);
        }
    }
    if (!dec(obj + 0x4A)) return;
    next(obj, 4);
    wr16(obj + 0x48, 0xB);
    wr8(obj + 0x21, 0);
}

/* $04B58C. Closing; closed, from difficulty 4 it opens again after $100 frames, else it is
 * removed. */
void Space480Close(uint16_t obj)
{
    if (!dec(obj + 0x48)) {
        hatch_look(obj);
        return;
    }
    if (!below(v_difficulty, 4)) {
        wr16(obj + O_STATE, 4);
        wr16(obj + O_FLAGS, 0x100);
        wr16(obj + 0x48, 0x100);
        return;
    }
    wr16(obj + O_FLAGS, 0x1000);
}

/* $04B460. Shot down: points, a wreck; else its step. */
void Space480Run(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) & 0x8000) {
        AddScore(obj, 0x2424);
        wr16(obj + 0x5C, 0xC);
        SpaceWreck(obj);
        return;
    }
    call_object(rom32(0x04B488 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $04B458. Types $480 and $508. */
void Space480(uint16_t obj)
{
    Space480Run(obj);
    ObjectRemoveOffscreen(obj);
}

/* ---- type $45C: the fight of stage 5's first hold (at $C9E0): its parts are the $458 at
 * $DE80 (a6, its counter at +$4A, +$4C) and the shots it calls in by that counter; messages
 * between. +$48's byte: bit 0 hurtable, bit 1 the other ending taken ---- */

#define MSG_SPEED 0x80C4
#define MSG_COUNT 0x80C6
#define MSG_TEXT  0x80CE
#define MSG_WHO   0x80D6

/* a message: its script, letters' speed and count */
static void message(uint16_t step, uint32_t script, uint16_t count, uint16_t speed)
{
    wr16(MSG_STEP, step);
    wr32(MSG_SCRIPT, script);
    wr16(MSG_COUNT, count);
    wr16(MSG_SPEED, speed);
}

/* $04B6E0. Once no message shows, the camera holds and no tiles are loading: in place ($120,
 * $170), hidden; its tiles loaded ($4B730). */
void Space45CStep00(uint16_t obj)
{
    if (rd16(MSG_STEP) || !bit8(v_cameraHold, 7) || rd16(v_jobKind)) return;
    next(obj, 4);
    wr16(obj + O_X, 0x120);
    wr16(obj + O_Y, 0x170);
    wr16(obj + O_FLAGS, 0);
    wr8(obj + 0x21, 0);
    wr8(obj + 0x48, 0);
    wr16(obj + 0x4C, 0);
    uint32_t list = 0x04B730;
    NextJob(&list);
}

/* $04B73A. The tiles in: shown, rising at 1. */
void Space45CStep04(uint16_t obj)
{
    if (rd16(v_jobKind)) return;
    wr32(obj + 0x1C, 0xFFFF0000);
    uint32_t a0 = 0x04C540;
    uint16_t d7 = 1;
    ObjectSetupNext(obj, &a0, &d7);
}

/* $04B756. Up to y $100: then still ($20 frames). */
void Space45CStep08(uint16_t obj)
{
    if (!below(obj + O_Y, 0x100)) return;
    next(obj, 4);
    wr16(obj + 0x4A, 0x20);
    wr32(obj + 0x1C, 0);
}

/* $04B770. Then its message ($3A4B1). */
void Space45CStep0C(uint16_t obj)
{
    if (!dec(obj + 0x4A)) return;
    next(obj, 4);
    message(0x28, 0x03A4B1, 0x80, 8);
}

/* $04B798. The message done: hurtable (vitality $C8), and its part at a6 made a $458 (the
 * counter). */
void Space45CStep10(uint16_t obj)
{
    if (rd16(MSG_STEP)) return;
    set8(obj + 0x48, 0);
    wr16(obj + O_VITALITY, 0xC8);
    wr16(obj + 0x4E, 0x3E8);
    wr16(obj + 0x50, 0xEA60);
    next(obj, 4);
    wr16(MSG_STEP, 0x28);
    wr32(MSG_SCRIPT, 0x03A343);
    wr16(MSG_COUNT, 0x30);
    wr16(MSG_SPEED, 0);
    uint16_t a6 = (uint16_t)reg_a6;
    wr16(a6 + O_TYPE, 0x458);
    wr16(a6 + O_STATE, 0);
    wr16(a6 + O_FLAGS, 0);
    wr16(a6 + 0x48, 0xD8A0);
    wr16(a6 + 0x4E, 0);
    wr16(a6 + 0x50, 0xE0);
    wr16(a6 + 0x52, 0x9A);
}

/* $04BF3E. The bomb: a $468 at $DEE0. */
void Space45CBomb(void)
{
    wr16(0xDEE0 + O_TYPE, 0x468);
    wr16(0xDEE0 + O_STATE, 0);
    wr16(0xDEE0 + O_FLAGS, 0x8000);
    wr16(0xDEE0 + 0x0E, 0x85BC);
    wr16(0xDEE0 + 0x08, 0x500);
    wr16(0xDEE0 + 0x0A, 0xF8F8);
    wr8(0xDEE0 + 0x21, 0x10);
    wr32(0xDEE0 + 0x2C, 0xFD03FD03);
    wr16(0xDEE0 + 0x26, 0x3E8);
    wr16(0xDEE0 + O_X, 0x120);
    wr16(0xDEE0 + O_Y, 0xF0);
    wr16(0xDEE0 + 0x48, 0x80);
    wr16(0xDEE0 + 0x4A, 0x1770);
    QueueSound(0xDF);
}

/* the counter (the $458 at $DE80: +$4A a word, +$4C its high byte) */
#define COUNTER  0xDECA
#define COUNTER2 0xDECC

/* the volleys: by the counter, the next (+$4C); a flash (type $464 and the like, see Space464)
 * every so many frames at a random place in a quarter of the screen the player is not in */

/* loc_04BC8E: the flash at a6 placed */
static void flash_place(void)
{
    uint16_t a6 = (uint16_t)reg_a6;
    uint16_t target = PlayerTarget();
    uint16_t d0 = 0;
    if (!below(target + O_X, 0x120)) d0 = 4;
    if (!below(target + O_Y, 0x100)) d0 = (uint16_t)(d0 + 8);
    uint16_t d1 = rd8(0xFF0A) & 0xC;
    if (d1 == d0) d1 = (uint16_t)(d1 + 4) & 0xC;
    wr16(a6 + O_X, (uint16_t)((rd8(v_randomSeed) & 0x3F) + tab16(0x04BCEC, d1)));
    d1 = (uint16_t)(d1 + 2);
    wr16(a6 + O_Y, (uint16_t)((rd8(0xFF09) & 0x3F) + tab16(0x04BCEC, d1)));
}

/* a flash of type `type` (loc_04BC7E: $464) */
static void flash(uint16_t type)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    set_a6(slot);
    wr16(slot + O_TYPE, type);
    flash_place();
}

/* the next volley once the counter reaches n */
static void volley_next(uint16_t obj, uint16_t n)
{
    if (!below(COUNTER, n)) add16(obj + 0x4C, 4);
}

/* $04BC48. Until the counter's high byte reaches $10: a shot at the player every $10th frame. */
void Space45CVolley00(uint16_t obj)
{
    if ((int8_t)(rd8(COUNTER2) - 0x10) >= 0) add16(obj + 0x4C, 4);
    if (rd16(v_gameFrames) & 0xF) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    set_a6(slot);
    shot460_aimed(obj);
}

/* $04BC64. To 1: a flash every $20th frame. */
void Space45CVolley04(uint16_t obj)
{
    volley_next(obj, 1);
    if (rd16(v_gameFrames) & 0x1F) return;
    flash(0x464);
}

/* $04BCFC. To 2: every $10th. */
void Space45CVolley08(uint16_t obj)
{
    volley_next(obj, 2);
    if (rd16(v_gameFrames) & 0xF) return;
    flash(0x464);
}

/* loc_04BD26: a flash every 8th frame */
static void flash8(void)
{
    if (rd16(v_gameFrames) & 7) return;
    flash(0x464);
}

/* $04BD18. To 8: every 8th. */
void Space45CVolley0C(uint16_t obj)
{
    volley_next(obj, 8);
    flash8();
}

/* $04BD34. To $12: every frame of 8 in 16. */
void Space45CVolley10(uint16_t obj)
{
    volley_next(obj, 0x12);
    if (rd16(v_gameFrames) & 8) return;
    flash(0x464);
}

/* $04BD50. To $22: a fan of three ($46C) every $20th. */
void Space45CVolley14(uint16_t obj)
{
    volley_next(obj, 0x22);
    if (rd16(v_gameFrames) & 0x1F) return;
    flash(0x46C);
}

/* loc_04BD8C: a fan of five ($470) every $20th frame */
static void flash_five(void)
{
    if (rd16(v_gameFrames) & 0x1F) return;
    flash(0x470);
}

/* $04BD7E. To $30: fans of five. */
void Space45CVolley18(uint16_t obj)
{
    volley_next(obj, 0x30);
    flash_five();
}

/* $04BDAC. To $37: every 8th frame a flash, one in four a fan of three. */
void Space45CVolley1C(uint16_t obj)
{
    volley_next(obj, 0x37);
    if (rd16(v_gameFrames) & 7) return;
    flash(rd16(v_randomSeed) & 3 ? 0x464 : 0x46C);
}

/* $04BDE6. To $45: a flash every 4th frame. */
void Space45CVolley20(uint16_t obj)
{
    volley_next(obj, 0x45);
    if (rd16(v_gameFrames) & 3) return;
    flash(0x464);
}

/* $04BE02. To $50: fans of five. */
void Space45CVolley24(uint16_t obj)
{
    if (below(COUNTER, 0x50)) {
        flash_five();
        return;
    }
    add16(obj + 0x4C, 4);
}

/* $04BE12. To $52: every 8th frame a flash, one in four a bouncing shot ($474). */
void Space45CVolley28(uint16_t obj)
{
    volley_next(obj, 0x52);
    if (rd16(v_gameFrames) & 7) return;
    flash(rd16(v_randomSeed) & 3 ? 0x464 : 0x474);
}

/* $04BE4C. To $60: every 8th frame a flash or (frame bit 3 clear) a bouncing shot. */
void Space45CVolley2C(uint16_t obj)
{
    volley_next(obj, 0x60);
    if (rd16(v_gameFrames) & 7) return;
    flash(bit8(v_gameFrames + 1, 3) ? 0x464 : 0x474);
}

/* $04BE84. To $64: a bouncing shot every $10th frame. */
void Space45CVolley30(uint16_t obj)
{
    volley_next(obj, 0x64);
    if (rd16(v_gameFrames) & 0xF) return;
    flash(0x474);
}

/* $04BEB2. To $70: every $10th frame a flash or (frame bit 4 clear) a fan of five. */
void Space45CVolley34(uint16_t obj)
{
    volley_next(obj, 0x70);
    if (rd16(v_gameFrames) & 0xF) return;
    flash(bit8(v_gameFrames + 1, 4) ? 0x464 : 0x470);
}

/* $04BEEA. To $75: fans of five. */
void Space45CVolley38(uint16_t obj)
{
    if (below(COUNTER, 0x75)) {
        flash_five();
        return;
    }
    add16(obj + 0x4C, 4);
}

/* $04BEFA. To $80: a flash every 8th frame. */
void Space45CVolley3C(uint16_t obj)
{
    if (below(COUNTER, 0x80)) {
        flash8();
        return;
    }
    add16(obj + 0x4C, 4);
}

/* $04BF0A. To $90: a bouncing shot every 8th frame. */
void Space45CVolley40(uint16_t obj)
{
    volley_next(obj, 0x90);
    if (rd16(v_gameFrames) & 7) return;
    flash(0x474);
}

/* $04BF3A. Fans of five. */
void Space45CVolley44(uint16_t obj)
{
    (void)obj;
    flash_five();
}

/* $04B7F8. Fighting: +$58 a sprite word by the frame; once the players are in, after +$50
 * frames the bomb, and the volley (+$4C). (No player in: its part's step 8.) */
void Space45CStep14(uint16_t obj)
{
    uint16_t d0 = (uint16_t)((uint16_t)(rd16(v_gameFrames) << 1) & 0x18) + 2;
    wr16(obj + 0x58, rom16(0x056802 + d0) | 0x8000);
    if (!rd8(v_playersIn)) {
        wr16((uint16_t)reg_a6 + O_STATE, 8);
        return;
    }
    add16(obj + 0x50, 0xFFFF);
    if (!rd16(obj + 0x50)) {
        wr16(obj + 0x50, 0xEA60);
        Space45CBomb();
    }
    call_object(rom32(0x04BC00 + (uint32_t)(int16_t)rd16(obj + 0x4C)), obj);
}

/* $04B826. Beaten: puffs about it ($4A frames, a blast sound every 8th); then its part's step
 * $C and the message at $3A4BB. */
void Space45CStep18(uint16_t obj)
{
    BrawlerPuff(obj);
    if (dec(obj + 0x4A)) {
        next(obj, 4);
        wr16((uint16_t)reg_a6 + O_STATE, 0xC);
        message(0x28, 0x03A4BB, 0x7FFF, 8);
    }
    BlastSound8();
}

/* $04B85E. Until either player presses C (the message blinking): the bonus message, its points
 * from the counter's part (+$4A, $930410 when +$4E is set). */
void Space45CStep1C(uint16_t obj)
{
    wr16(MSG_COUNT, 0x7FFF);
    if (!((rd8(v_padPressed) | rd8(v_padPressed + 1)) & 0x10)) {
        wr16(MSG_SPEED, rd16(v_gameFrames) & 8 ? 0 : 0xFFFF);
        return;
    }
    next(obj, 4);
    wr16(MSG_STEP, 0x40);
    wr16(MSG_COUNT, 0x40);
    wr16(MSG_WHO, 0);
    uint16_t a6 = (uint16_t)reg_a6;
    if (!rd16(a6 + 0x4E)) {
        wr32(MSG_SCRIPT, 0x03A438);
        wr32(MSG_TEXT, rd32(a6 + 0x4A));
        return;
    }
    wr32(MSG_SCRIPT, 0x03A47F);
    wr32(MSG_TEXT, 0x930410);
}

/* $04B8CC. The message done: its part removed, the tiles at $4B8FA loaded; $40 frames. */
void Space45CStep20(uint16_t obj)
{
    if (rd16(MSG_STEP)) return;
    next(obj, 4);
    wr16(obj + 0x4A, 0x40);
    wr16((uint16_t)reg_a6 + O_FLAGS, 0x1000);
    uint32_t list = 0x04B8FA;
    NextJob(&list);
}

/* $04B904. Then removed. */
void Space45CStep24(uint16_t obj)
{
    if (dec(obj + 0x4A)) wr16(obj + O_FLAGS, 0x1000);
}

/* $04B914. The other ending: the message at $3A4DA, hidden, a puff (type $100) at $DEE0 in its
 * place. */
void Space45CStep28(uint16_t obj)
{
    if (rd16(MSG_STEP)) return;
    next(obj, 4);
    message(0x28, 0x03A4DA, 0x100, 0);
    wr16(obj + O_FLAGS, 0);
    wr8(obj + 0x21, 0);
    wr16(0xDEE0 + O_TYPE, 0x100);
    wr32(0xDEE0 + 0x48, 0x05639C);
    wr16(0xDEE0 + O_STATE, 0);
    wr16(0xDEE0 + O_X, rd16(obj + O_X));
    wr16(0xDEE0 + O_Y, rd16(obj + O_Y));
    QueueSound(0xCA);
}

/* $04B96C. Then the message at $3A4FB. */
void Space45CStep2C(uint16_t obj)
{
    if (rd16(MSG_STEP)) return;
    next(obj, 4);
    message(0x28, 0x03A4FB, 0xC0, 0);
    QueueSound(0xDF);
}

/* $04B99C. Then as beaten (step $18, +$4A -1). */
void Space45CStep30(uint16_t obj)
{
    if (rd16(MSG_STEP)) return;
    wr16(obj + O_STATE, 0x18);
    wr16(obj + 0x4A, 0xFFFF);
}

/* loc_04B690: the objects after it gone (but the $458) */
static void space45c_clear(uint16_t obj)
{
    ObjectsVanishFrom((uint16_t)(obj + 0x60), 0x458, 0);
}

/* $04B5BA. Type $45C: shown, it flickers (frame bit 2); +$5A a sprite word by the frame. Its
 * part at $DE80 in a6. No player in: Space45CStep14. Its part counting (+$4E set): the other
 * ending (once). Its vitality out while hurtable: its part's step 8, beaten (step $18), the
 * objects after it gone. Else its step. */
void Space45C(uint16_t obj)
{
    if (bit8(obj + O_FLAGS, 7)) {
        if (bit8(v_gameFrames + 1, 2)) set8(obj + 0x0E, 3);
        else clr8(obj + 0x0E, 3);
    }
    wr16(obj + 0x5A, tab16(0x0461CA, (uint16_t)((uint16_t)(rd16(v_gameFrames) << 1) & 6)) | 0x6000);
    reg_a6 = 0xFFFFDE80u;
    uint16_t a6 = 0xDE80;
    if (!rd8(v_playersIn)) {
        Space45CStep14(obj);
        return;
    }
    if (rd16(a6 + 0x4E) && rd16(a6 + O_TYPE) == 0x458) {
        if (!bit8(obj + 0x48, 1)) {
            wr8(obj + 0x48, 2);
            wr16(obj + O_STATE, 0x28);
            message(0x28, 0x03A4CE, 0x80, 8);
            QueueSound(0xA3);
            space45c_clear(obj);
            return;
        }
    } else if ((rd16(obj + O_VITALITY) & 0x8000) && bit8(obj + 0x48, 0)) {
        clr8(obj + 0x48, 0);
        wr16(a6 + O_STATE, 8);
        wr16(obj + O_STATE, 0x18);
        wr16(obj + 0x5C, rd16(obj + O_X));
        wr16(obj + 0x4A, 0x80);
        wr16(obj + O_FLAGS, 0);
        wr16(obj + O_VITALITY, 0);
        wr8(obj + 0x21, 0);
        space45c_clear(obj);
        return;
    }
    call_object(rom32(0x04B6AC + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $484: the big turret (a gun on an arm, as $450) that holds the camera: it waits
 * for the hold, then fires along its arm and at the player ---- */

/* $04BA3A. Its set-up (as $450's). */
void Space484Start(uint16_t obj)
{
    turret_setup(obj);
}

/* $04BA3E. The arm placed (turned toward d7: what the previous object's routine left, the
 * dispatcher's d7 passed on); once the camera holds and no message shows: hurtable (vitality
 * $7CE), its vitality shown (message step $10). */
void Space484Wait(uint16_t obj, uint16_t d7)
{
    SpaceArm(obj, d7);
    if (!rd8(v_cameraHold) || rd16(MSG_STEP)) return;
    next(obj, 4);
    wr16(obj + 0x48, 0x30);
    wr16(obj + O_VITALITY, 0x7CE);
    wr16(MSG_STEP, 0x10);
    wr16(MSG_WHO, obj);
}

/* $04BB36. With the player on the left half, every $10th frame a shot at it. */
void Space484AtPlayer(uint16_t obj)
{
    if (!below(PlayerTarget() + O_X, 0x100)) return;
    if (rd16(v_gameFrames) & 0xF) return;
    SpaceShotAimed(obj);
}

/* $04BB1C. Along its arm a shot ($454), or above the easiest level on odd random a bursting one
 * ($500). */
void Space484Shot(uint16_t obj)
{
    if (!rd16(v_difficulty) || bit8(0xFF09, 0)) {
        SpaceShot454(obj);
        return;
    }
    shot_along(obj, 0x500);
}

/* loc_04BB54: from difficulty 4, four shots ($504) back left, spread down */
static void four_504(uint16_t obj)
{
    if (below(v_difficulty, 4)) return;
    uint32_t d5 = 0xFFFD0000;
    for (int i = 0; i < 4; i++) {
        uint16_t slot, t;
        if (!SlotFreeHigh(&slot, &t)) return;
        wr16(slot + O_TYPE, 0x504);
        wr16(slot + 0x56, 0x1C);
        wr16(slot + O_X, rd16(obj + O_X));
        wr16(slot + O_Y, rd16(obj + O_Y));
        wr32(slot + 0x1C, d5);
        wr32(slot + 0x18, 0xFFFF8000);
        d5 += 0x20000;
    }
}

/* $04BA6E. Fighting: shots at the player; the arm turned to it; once out, every $40 frames a
 * shot along it (and from difficulty 4 four more). */
void Space484Fight(uint16_t obj)
{
    Space484AtPlayer(obj);
    uint16_t d7 = SpaceAim(obj);
    SpaceArm(obj, d7);
    if (SpaceArmExtend(obj)) return;
    if (!dec(obj + 0x48)) return;
    wr16(obj + 0x48, 0x40);
    wr16(obj + 0x54, 0x10);
    Space484Shot(obj);
    four_504(obj);
}

/* $04BA9E. Beaten: $48 frames of puffs about it (every other frame, a blast sound every 8th). */
void Space484Burst(uint16_t obj)
{
    if (dec(obj + 0x48)) {
        next(obj, 4);
        wr16(obj + 0x48, 0x40);
        return;
    }
    BlastSound8();
    if (bit8(v_gameFrames + 1, 0)) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x05639C);
    wr16(slot + O_X, (uint16_t)((rd8(v_randomSeed) & 0x1F) + (uint16_t)(rd16(obj + O_X) - 0x10)));
    wr16(slot + O_Y, (uint16_t)((rd8(0xFF09) & 0x1F) + (uint16_t)(rd16(obj + O_Y) - 0x10)));
}

/* $04BB06. Then $40 frames; removed, and the boss slot's +4 set to $3C. */
void Space484End(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    wr16(obj + O_FLAGS, 0x1000);
    wr16(0xC984, 0x3C);
}

/* $04B9B2. Type $484. Its vitality out (once fighting): points, its gun left as a wreck, puffs
 * (step $C); then its step. Step 4 reads d7 as the 68k's dispatcher leaves it, the previous
 * object's: the C passes 0. */
void Space484(uint16_t obj)
{
    if (!below(obj + O_STATE, 8) && (rd16(obj + O_VITALITY) & 0x8000)) {
        AddScore(obj, 0x21999);
        wr16(obj + O_STATE, 0xC);
        wr16(obj + 0x48, 0x7F);
        wr16(obj + O_FLAGS, 0);
        wr16(obj + O_VITALITY, 0);
        wr8(obj + 0x21, 0);
        uint16_t gun = rd16(obj + 0x4E);
        wr16(gun + O_X, rd16(obj + O_X));
        wr16(gun + O_Y, rd16(obj + O_Y));
        uint32_t a0 = 0x04C4F6;
        uint16_t d7 = 0;
        ObjectSetupNext(gun, &a0, &d7);
        wr16(gun + 0x0E, 0xA470);
        wr16(gun + O_TYPE, 0x44C);
    }
    uint32_t fn = rom32(0x04BA26 + (uint32_t)(int16_t)rd16(obj + O_STATE));
    if (fn == 0x04BA3E) Space484Wait(obj, 0);
    else call_object(fn, obj);
}

/* ---- type $4A4: a gunship flying a course by its kind (+6, its first step): its guns on
 * timers (+$48): a shot at the player, a spread to the left, a shot aimed along +$56, a beam
 * ahead, lines of fire up and down ---- */

/* loc_04B180: every so often (by the difficulty) a shot at the player */
static void gunship_aimed(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    wr16(obj + 0x48, tab16(0x04B19C, rd16(v_difficulty)));
    SpaceShotAimed(obj);
}

/* loc_04B1A4: every so often a spread to the left, from 16 ahead */
static void gunship_spread(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    wr16(obj + 0x48, tab16(0x04B1D0, rd16(v_difficulty)));
    spread(0x100, (uint16_t)(rd16(obj + O_X) - 0x10), rd16(obj + O_Y));
}

/* loc_04B1E2: every so often a shot ($454) at the player */
static void gunship_454(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    wr16(obj + 0x48, tab16(0x04B206, rd16(v_difficulty)));
    wr16(obj + 0x56, SpaceAim(obj));
    SpaceShot454(obj);
}

/* loc_04C34E: a beam (type $10, hurting) at d6, d7 moving at d5 */
static void beam(uint32_t d5, uint16_t d6, uint16_t d7)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x10);
    wr16(slot + O_FLAGS, 0xCA00);
    wr16(slot + 0x0E, 0xE000);
    wr32(slot + 0x08, 0x0E09C6);
    wr8(slot + 0x20, 0x80);
    wr8(slot + 0x21, 0x10);
    wr32(slot + 0x2C, 0xFF01F20E);
    wr8(slot + 0x23, 0x20);
    wr16(slot + 0x26, 2);
    wr16(slot + O_X, d6);
    wr16(slot + O_Y, d7);
    wr32(slot + 0x18, d5);
}

/* $04B20E. Every so often (by the difficulty's step) a beam to the right. */
void Space4A4Beam(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    wr16(obj + 0x48, tab16(0x04B246, rd16(RANK)));
    uint16_t d6 = (uint16_t)(rd16(obj + O_X) + 0x18), d7 = rd16(obj + O_Y);
    QueueSound(0xC7);
    beam(0x84000, d6, d7);
}

/* loc_04B256: every $60 frames two lines of fire, up and down */
static void gunship_fire(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    wr16(obj + 0x48, 0x60);
    SpaceFire4B0Of32(obj, 0xFFFFFFFF, 0xFFFEE000, 0xFFFFA000, 0);
    SpaceFire4B0Of32(obj, 0xFFFFFFFF, 0x12000, 0xFFFFA000, 0);
}

/* $04AF28. Its set-up: its course (the step) from its kind; vitality $159. */
void Space4A4Start(uint16_t obj)
{
    wr16(obj + O_STATE, rd16(obj + 6));
    wr16(obj + O_FLAGS, 0xC000);
    wr16(obj + 0x0E, 0x8000);
    wr32(obj + 0x08, 0x0E09AE);
    wr16(obj + 0x0C, 0);
    wr8(obj + 0x20, 0x7C);
    wr8(obj + 0x21, 0x30);
    wr8(obj + 0x23, 0x20);
    wr32(obj + 0x2C, 0xF010F010);
    wr32(obj + 0x28, 0xF20EF20E);
    wr16(obj + O_VITALITY, 0x159);
    wr16(obj + 0x26, 0x1E);
    wr16(obj + 0x48, 0x40);
    wr16(obj + 0x4A, 0);
}

/* $04AF80. Kind 4: left at $D000 a frame; spreads while right of $C0. */
void Space4A4Step04(uint16_t obj)
{
    add32(obj + O_X, (uint32_t)-0xD000);
    if (!below(obj + O_X, 0xC0)) gunship_spread(obj);
}

/* $04AF94. Kind $3C: down at $A000; spreads once below $A0. */
void Space4A4Step3C(uint16_t obj)
{
    add32(obj + O_Y, 0xA000);
    if (!below(obj + O_Y, 0xA0)) gunship_spread(obj);
}

/* $04AFA8. Kind 8: left at $4000 to $140, then up at $C000; lines of fire. */
void Space4A4Step08(uint16_t obj)
{
    if (!below(obj + O_X, 0x140)) add32(obj + O_X, (uint32_t)-0x4000);
    else add32(obj + O_Y, (uint32_t)-0xC000);
    gunship_fire(obj);
}

/* $04AFCA. Kind $C: turned (moving right at $8000). */
void Space4A4Step0C(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + 0x0E, 0xA800);
    wr32(obj + 0x18, 0x8000);
    wr16(obj + O_FLAGS, 0xCC00);
}

/* $04AFE4. Right of $9C: still, rising, for $200 frames. */
void Space4A4Step10(uint16_t obj)
{
    if (below(obj + O_X, 0x9C)) return;
    next(obj, 4);
    wr32(obj + 0x18, 0);
    wr16(obj + 0x1C, 0xFFFF);
    wr16(obj + 0x4A, 0x200);
}

/* $04B01C. Nothing. */
void Space4A4Step18(uint16_t obj)
{
    (void)obj;
}

/* $04B004. Beams, bobbing between y $B0 and $140; after $200 frames away left. */
void Space4A4Step14(uint16_t obj)
{
    if (dec(obj + 0x4A)) {
        next(obj, 4);
        wr32(obj + 0x18, 0xFFFF4000);
        wr32(obj + 0x1C, 0);
        Space4A4Step18(obj);
        return;
    }
    Space4A4Beam(obj);
    if (bit8(obj + 0x1C, 7)) {
        if (below(obj + O_Y, 0xB0)) wr16(obj + 0x1C, 1);
        return;
    }
    if (!below(obj + O_Y, 0x140)) wr16(obj + 0x1C, 0xFFFF);
}

/* $04B060. Kind $50: right at $A000, beams. */
void Space4A4Step50(uint16_t obj)
{
    wr16(obj + 0x0E, 0xA800);
    add32(obj + O_X, 0xA000);
    Space4A4Beam(obj);
}

/* $04B072. Kind $1C: left at $C000; shots at the player while right of $C0. */
void Space4A4Step1C(uint16_t obj)
{
    add32(obj + O_X, (uint32_t)-0xC000);
    if (!below(obj + O_X, 0xC0)) gunship_aimed(obj);
}

/* $04B086. Kind $20: right at $A000 to $100, then up at $8000; shots at the player. */
void Space4A4Step20(uint16_t obj)
{
    wr16(obj + 0x0E, 0x8800);
    if (below(obj + O_X, 0x100)) add32(obj + O_X, 0xA000);
    else add32(obj + O_Y, (uint32_t)-0x8000);
    gunship_aimed(obj);
}

/* $04B0AE. Kind $24: left at $E000 to $120, then $80 frames still. */
void Space4A4Step24(uint16_t obj)
{
    add32(obj + O_X, (uint32_t)-0xE000);
    if (!below(obj + O_X, 0x120)) {
        gunship_aimed(obj);
        return;
    }
    next(obj, 4);
    wr16(obj + 0x4A, 0x80);
}

/* $04B0CC. Still for +$4A frames (its gun's timer run twice as fast), shooting. */
void Space4A4Step28(uint16_t obj)
{
    add16(obj + 0x48, 0xFFFF);
    if (!dec(obj + 0x4A)) {
        gunship_aimed(obj);
        return;
    }
    next(obj, 4);
}

/* $04B0DE. Left at $A000, shooting. */
void Space4A4Step2C(uint16_t obj)
{
    add32(obj + O_X, (uint32_t)-0xA000);
    gunship_aimed(obj);
}

/* $04B0EA. Kind $30: down at $C800 to $EC, then $C0 frames still. */
void Space4A4Step30(uint16_t obj)
{
    add32(obj + O_Y, 0xC800);
    if (below(obj + O_Y, 0xEC)) {
        gunship_aimed(obj);
        return;
    }
    next(obj, 4);
    wr16(obj + 0x4A, 0xC0);
}

/* $04B108. Right at $8000, shooting. */
void Space4A4Step38(uint16_t obj)
{
    add32(obj + O_X, 0x8000);
    gunship_aimed(obj);
}

/* $04B114. Kind $40: left at $F200 to $AC (shots $454 at the player). */
void Space4A4Step40(uint16_t obj)
{
    wr16(obj + 0x0E, 0xA000);
    add32(obj + O_X, (uint32_t)-0xF200);
    if (!below(obj + O_X, 0xAC)) {
        gunship_454(obj);
        return;
    }
    next(obj, 4);
}

/* $04B132. Then down at $C000 (shooting from the difficulty's step 8). */
void Space4A4Step44(uint16_t obj)
{
    add32(obj + O_Y, 0xC000);
    if (below(RANK, 8)) return;
    gunship_454(obj);
}

/* $04B148. Kind $48: right at $8000 to $120 (shooting from the difficulty's step 6), then $B0
 * frames. */
void Space4A4Step48(uint16_t obj)
{
    wr16(obj + 0x0E, 0xA000);
    add32(obj + O_X, 0x8000);
    if (below(obj + O_X, 0x120)) {
        if (below(RANK, 6)) return;
        gunship_454(obj);
        return;
    }
    next(obj, 4);
    wr16(obj + 0x4A, 0xB0);
}

/* $04B16C. Still for +$4A frames, then left at $C000; shooting. */
void Space4A4Step4C(uint16_t obj)
{
    if (dec(obj + 0x4A)) add32(obj + O_X, (uint32_t)-0xC000);
    gunship_454(obj);
}

/* $04AECA. Its step. */
void Space4A4Step(uint16_t obj)
{
    call_object(rom32(0x04AED4 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $04AE88. Type $4A4: its step; shot down: points, a spread at the player, a big wreck (type
 * $54). */
void Space4A4(uint16_t obj)
{
    Space4A4Step(obj);
    if (!(rd16(obj + O_VITALITY) & 0x8000)) {
        ObjectRemoveOffscreen(obj);
        return;
    }
    AddScore(obj, 0x5000);
    SpaceSpread(obj, rd16(obj + O_X), rd16(obj + O_Y));
    wr16(obj + O_TYPE, 0x54);
    wr16(obj + O_STATE, 0);
    wr8(obj + 0x21, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    QueueSound(0xBE);
}

/* ---- shots, beams and lines of fire ($4A8-$4BC, $50C-$51C) ---- */

/* $04CB36. Type $4A8: a shot (its sprite word from $C9DC); hitting something it becomes a puff. */
void Space4A8(uint16_t obj)
{
    wr16(obj + 0x0E, rd16(0xC9DC));
    if (!bit8(obj + 0x22, 4)) {
        ObjectRemoveOffscreen(obj);
        return;
    }
    puff_hit(obj);
}

/* $04CC0C. Type $4AC: a beam, removed past x $240. */
void Space4AC(uint16_t obj)
{
    if (!below(obj + O_X, 0x240)) wr16(obj + O_FLAGS, 0x1000);
}

/* $04CC1E. Type $514: a falling flame drifting right; removed past x $1E0 or y $160. */
void Space514(uint16_t obj)
{
    add32(obj + O_X, 0x8000);
    if (below(obj + O_X, 0x1E0)) {
        add16(obj + O_Y, 1);
        if (below(obj + O_Y, 0x160)) return;
    }
    wr16(obj + O_FLAGS, 0x1000);
}

/* $04CC46. Type $50C: for +$48 frames, every so often (by the difficulty) a beam ($4AC, one of
 * six slots from $DC40) from the left edge at the player's row (to 32): its size from the slot
 * ($DCA0 the big one) or the random bit. */
void Space50C(uint16_t obj)
{
    if (dec(obj + 0x48)) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    if (!dec(obj + 0x4A)) return;
    QueueSound(0xC7);
    wr16(obj + 0x4A, tab16(0x04CD02, rd16(v_difficulty)));
    uint16_t slot = 0xDC40, t;
    if (!SlotFree6(&slot, &t)) return;
    uint16_t d7 = rd16(PlayerTarget() + O_Y) & 0xFFE0;
    uint32_t a1;
    if (slot == 0xDCA0) a1 = 0x04CD26;
    else if (bit8(0xFF09, 0)) a1 = 0x04CD0A;
    else a1 = 0x04CD18;
    wr16(slot + O_TYPE, 0x4AC);
    wr16(slot + O_FLAGS, 0xC800);
    wr16(slot + 0x0E, 0x8000);
    wr8(slot + 0x20, 0x78);
    wr8(slot + 0x21, 0x10);
    wr8(slot + 0x23, 0x20);
    wr16(slot + 0x26, 4);
    wr16(slot + O_X, 0);
    wr32(slot + 0x08, rom32(a1));
    wr32(slot + 0x2C, rom32(a1 + 4));
    wr32(slot + 0x18, rom32(a1 + 8));
    wr16(slot + O_Y, (uint16_t)(d7 + rom16(a1 + 12)));
}

/* $04CD34. Type $51C: moving right a pixel every other frame (removed at $1C0); every $20
 * frames by turns a falling flame ($514) at the top ($94), or above the easiest level a $518
 * of kind 4 $30 behind. */
void Space51C(uint16_t obj)
{
    if (!bit8(v_gameFrames + 1, 0)) {
        add16(obj + O_X, 1);
        if (!below(obj + O_X, 0x1C0)) {
            wr16(obj + O_FLAGS, 0x1000);
            return;
        }
    }
    if (!dec(obj + 0x4A)) return;
    wr16(obj + 0x4A, 0x20);
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(obj + 0x4C, rd16(obj + 0x4C) ^ 1);
    if (!rd16(obj + 0x4C)) {
        wr16(slot + O_TYPE, 0x514);
        wr16(slot + O_FLAGS, 0xC000);
        wr16(slot + 0x0E, 0xE000);
        wr8(slot + 0x20, 0x78);
        wr8(slot + 0x21, 0x10);
        wr8(slot + 0x23, 0x20);
        wr16(slot + 0x26, 3);
        wr16(slot + O_X, rd16(obj + O_X));
        wr16(slot + O_Y, 0x94);
        wr32(slot + 0x08, 0x0E09D8);
        wr32(slot + 0x2C, 0xFF01D22E);
        return;
    }
    if (!rd16(v_difficulty)) return;
    wr16(slot + O_TYPE, 0x518);
    wr16(slot + 6, 4);
    wr16(slot + O_X, (uint16_t)(rd16(obj + O_X) - 0x30));
    wr16(slot + O_Y, 0x94);
}

/* loc_04CEC4: it becomes a puff (type $100, the animation at $563EE) */
static void puff_burst(uint16_t obj)
{
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x0563EE);
    wr16(obj + O_STATE, 0);
    wr8(obj + 0x21, 0);
    QueueSound(0xCA);
}

/* $04CDE2. Type $4B0: a fireball (facing its way): set up; moving for +$48 frames, then still
 * and (for $10 frames) speeding by +$4C, +$50, leaving puffs every third frame. Shot (a
 * point; from difficulty 4 a shot back) or hitting something: a puff. */
void Space4B0(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) & 0x8000) {
        AddScore(obj, 1);
        SpaceShotIfHard(obj);
        puff_burst(obj);
        return;
    }
    if (bit8(obj + 0x22, 4)) {
        puff_burst(obj);
        return;
    }
    if (rd16(obj + 0x18) & 0x8000) set8(obj + 0x0E, 3);
    else clr8(obj + 0x0E, 3);
    if (!rd16(obj + O_STATE)) {
        uint32_t a0 = 0x04C550;
        uint16_t d7;
        ObjectSetupAll(obj, &a0, &d7);
        wr16(obj + O_FLAGS, 0x8C00);
        wr16(obj + 0x26, 8);
        return;
    }
    if (below(obj + O_STATE, 8)) {
        if (!dec(obj + 0x48)) {
            ObjectRemoveOffscreen(obj);
            return;
        }
        next(obj, 4);
        wr16(obj + 0x5E, 0);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr16(obj + 0x4A, 0x10);
        QueueSound(0xCE);
        return;
    }
    if (!(rd16(obj + 0x4A) & 0x8000)) {
        add16(obj + 0x4A, 0xFFFF);
        add32(obj + 0x1C, rd32(obj + 0x50));
        add32(obj + 0x18, rd32(obj + 0x4C));
    }
    if (!dec(obj + 0x5E)) {
        ObjectRemoveOffscreen(obj);
        return;
    }
    wr16(obj + 0x5E, 2);
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) {
        ObjectRemoveOffscreen(obj);
        return;
    }
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x056618);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr32(slot + 0x18, (uint32_t)((int32_t)(0u - rd32(obj + 0x18)) >> 1));
}

/* $04CF70. The object at `slot` made a rock ($4B8; its size, +$48, from the frame), at a random
 * y from $7C, falling at a random speed; from the right moving left (or, +$4C negative, from
 * the left moving right) at one of four speeds. */
void Space4B4Rock(uint16_t obj, uint16_t slot)
{
    wr16(slot + O_TYPE, 0x4B8);
    uint16_t d0 = rd16(v_gameFrames) & 3;
    wr16(slot + 0x48, d0);
    wr16(slot + O_Y, (uint16_t)(rd8(0xFF0A) + 0x7C));
    wr32(slot + 0x1C, (uint32_t)(int32_t)(int16_t)rd16(v_randomSeed));
    uint32_t v = rom32(0x04CFD8 + (uint32_t)(d0 << 2));
    if (rd16(obj + 0x4C) & 0x8000) {
        wr32(slot + 0x18, v);
        wr16(slot + O_X, 0x80);
        return;
    }
    wr32(slot + 0x18, 0u - v);
    wr16(slot + O_X, 0x1C0);
}

/* $04CEE2. Type $4B4: for +$48 frames, rocks every so often (by the difficulty's step); with
 * +$4E set each one carries a $4D4 (kind $2C, in one of the four slots from $DD00). */
void Space4B4(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + 0x50, 0);
        return;
    }
    if (dec(obj + 0x48)) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    if (!dec(obj + 0x50)) return;
    wr16(obj + 0x50, tab16(0x04CFC8, rd16(RANK)));
    uint16_t rock, t;
    if (!SlotFreeHigh(&rock, &t)) return;
    Space4B4Rock(obj, rock);
    if (!rd16(obj + 0x4E)) return;
    uint16_t slot = 0xDD00;
    if (!SlotFree4(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x4D4);
    wr16(slot + 6, 0x2C);
    wr16(slot + O_X, 0x80);
    wr16(slot + O_Y, 0x80);
    wr16(slot + 0x58, 0xFFE8);
    wr16(slot + 0x5A, 0);
    wr16(slot + 0x5C, rock);
    wr16(slot + 0x5E, 0x4B8);
}

/* $04CFE8. Types $4B8, $510: a rock: its size from +$48 (0, 1, 2-3); hit by a player's shot
 * (+$22 bit 5) it flies off at a random angle (once); hitting a player it bursts. */
void Space4B8(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr8(obj + 0x23, 0x20);
        uint32_t a0;
        if (dec(obj + 0x48)) a0 = 0x04C566;
        else if (dec(obj + 0x48)) a0 = 0x04C57C;
        else a0 = 0x04C592;
        uint16_t d7;
        ObjectSetupAll(obj, &a0, &d7);
        return;
    }
    if (below(obj + O_STATE, 8) && bit8(obj + 0x22, 5)) {
        next(obj, 4);
        uint16_t d0 = rd16(v_randomSeed) & 0x1E0;
        wr32(obj + 0x1C, (uint32_t)sine(d0) << 3);
        wr32(obj + 0x18, (uint32_t)cosine(d0) << 3);
        return;
    }
    if (!bit8(obj + 0x22, 4)) {
        ObjectRemoveOffscreen(obj);
        return;
    }
    if (rd16(obj + 0x40) == 0xA400 || rd16(obj + 0x40) == 0xA600) puff_burst(obj);
}

/* $04D0A2. It becomes a small puff (type $100, the animation at $5663A), still. */
void Space4BCPuff(uint16_t obj)
{
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x05663A);
    wr16(obj + O_STATE, 0);
    wr8(obj + 0x21, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    QueueSound(0xDD);
}

/* loc_04C1D8: a ring of sixteen shots ($4A8), the first at a random angle */
static void ring16(uint16_t obj)
{
    uint16_t d5 = rd16(v_randomSeed) & 0x1F0, slot;
    uint16_t x = rd16(obj + O_X), y = rd16(obj + O_Y);
    if (!Spawn4A8Round(d5, x, y, &slot)) return;
    for (int i = 0; i < 15; i++) {
        d5 = (uint16_t)(d5 + 0x20) & 0x1FE;
        if (!Spawn4A8(d5, x, y, SINES, COSINES, &slot)) return;
    }
}

/* $04D08A. Types $4BC, $524, $528: a bomb (facing right unless moving left): trailing puffs
 * every 4th frame, then still and blinking for its last $10 frames (+$48); then it bursts: a
 * $4BC into a ring of 8, a $528 of 16, a $524 into a spread at the player. Shot down: points, a
 * shot at the player; hitting something: a puff. */
void Space4BC(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) & 0x8000) {
        AddScore(obj, 0x3889);
        uint16_t slot;
        SpawnTowardPlayer(obj, &slot);
        Space4BCPuff(obj);
        return;
    }
    if (!rd16(obj + O_STATE)) {
        uint32_t a0 = 0x04C5A8;
        uint16_t d7;
        ObjectSetupAll(obj, &a0, &d7);
        if (!(rd16(obj + 0x18) & 0x8000)) set8(obj + 0x0E, 3);
        else clr8(obj + 0x0E, 3);
        return;
    }
    if (bit8(obj + 0x22, 4)) {
        Space4BCPuff(obj);
        return;
    }
    if (dec(obj + 0x48)) {
        uint16_t type = rd16(obj + O_TYPE);
        Space4BCPuff(obj);
        if (type == 0x4BC) SpaceRing(obj);
        else if (type == 0x528) ring16(obj);
        else SpaceSpread(obj, rd16(obj + O_X), rd16(obj + O_Y));
        return;
    }
    if (below(obj + 0x48, 0x10)) {
        wr32(obj + 0x18, 0);
        if (bit8(v_gameFrames + 1, 0)) clr8(obj + O_FLAGS, 7);
        else set8(obj + O_FLAGS, 7);
        return;
    }
    if (rd16(v_gameFrames) & 3) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x056618);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    if (bit8(obj + 0x0E, 3)) wr16(slot + O_X, (uint16_t)(rd16(slot + O_X) - 0x10));
    else wr16(slot + O_X, (uint16_t)(rd16(slot + O_X) + 0x10));
}

/* ---- shot down or hit: the puffs of the small ships ---- */

/* loc_04BBCC: it becomes a puff (type $100, the animation at $563EE), harmless */
static void puff_563ee(uint16_t obj)
{
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x0563EE);
    wr16(obj + O_STATE, 0);
    wr8(obj + 0x21, 0);
}

/* loc_04BBBE: shot down: a sound, from difficulty 6 a shot at the player, a puff */
static void shot_down(uint16_t obj)
{
    QueueSound(0xDD);
    SpaceShotIfHardest(obj);
    puff_563ee(obj);
}

/* loc_04BB9E: hitting something a puff; shot down, points (d0) and a puff; else removed off
 * the screen */
static void hit_or_shot(uint16_t obj, uint32_t points)
{
    if (bit8(obj + 0x22, 4)) {
        puff_563ee(obj);
        return;
    }
    if (!(rd16(obj + O_VITALITY) & 0x8000)) {
        ObjectRemoveOffscreen(obj);
        return;
    }
    AddScore(obj, points);
    shot_down(obj);
}

/* loc_04A3D2: removed */
static void remove_obj(uint16_t obj) { wr16(obj + O_FLAGS, 0x1000); }

/* ---- type $4C0: a squadron's leader: fliers ($4C4) of two kinds at a time, by its kind (+6,
 * its first step), every so many frames ---- */

/* $04A058. A flier ($4C4) of kind d6 (from $CE00). */
void Space4C0Spawn(uint16_t kind)
{
    uint16_t slot, t;
    if (!SlotFreeMid(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x4C4);
    wr16(slot + 6, kind);
}

/* $04A06C. The same from $D400. */
void Space4C0SpawnHigh(uint16_t kind)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x4C4);
    wr16(slot + 6, kind);
}

/* $049F74. Its kind's step. */
void Space4C0Start(uint16_t obj)
{
    wr16(obj + O_STATE, rd16(obj + 6));
    wr16(obj + 0x48, 0);
    wr16(obj + 0x56, 0);
    wr16(obj + 0x5C, 0);
}

/* a pair of fliers every `wait` frames, `count` pairs */
static void squadron(uint16_t obj, uint16_t wait, uint16_t k1, uint16_t k2, uint16_t count)
{
    if (!dec(obj + 0x48)) return;
    wr16(obj + 0x48, wait);
    Space4C0Spawn(k1);
    Space4C0Spawn(k2);
    add16(obj + 0x56, 1);
    if (!below(obj + 0x56, count)) remove_obj(obj);
}

/* $049F88. Kind 4: four pairs of kinds 4 and $1C, 8 frames apart. */
void Space4C0Step04(uint16_t obj)
{
    squadron(obj, 8, 4, 28, 4);
}

/* $049FB2. Kind 8: five pairs of $34 and $44, $C apart. */
void Space4C0Step08(uint16_t obj)
{
    squadron(obj, 0xC, 52, 68, 5);
}

/* $049FDC. Kind $C: five pairs of $54 and $5C, $A apart. */
void Space4C0Step0C(uint16_t obj)
{
    squadron(obj, 0xA, 84, 92, 5);
}

/* $04A006. Kind $10: $50 pairs of $64 and $6C ($F apart, from $D400), +$5C down by 4 each. */
void Space4C0Step10(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    add16(obj + 0x5C, (uint16_t)-4);
    wr16(obj + 0x48, 0xF);
    Space4C0SpawnHigh(100);
    Space4C0SpawnHigh(108);
    add16(obj + 0x56, 1);
    if (!below(obj + 0x56, 0x50)) remove_obj(obj);
}

/* $04A034. Kind $14: for $600 frames one of kind $74 every $20. */
void Space4C0Step14(uint16_t obj)
{
    if (dec(obj + 0x48)) {
        wr16(obj + 0x48, 0x20);
        Space4C0Spawn(116);
    }
    add16(obj + 0x56, 1);
    if (!below(obj + 0x56, 0x600)) remove_obj(obj);
}

/* $049F52. Type $4C0. */
void Space4C0(uint16_t obj)
{
    call_object(rom32(0x049F5C + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $4C4: a flier on a course by its kind (+6, its first step), moving 16 times the
 * sine tables' entry +$56 a frame ---- */

/* $04A3DA. Its sprite by the frame (every 8). */
void Space4C4Look(uint16_t obj)
{
    wr16(obj + 0x0E, bit8(v_gameFrames + 1, 3) ? 0xE310 : 0xE300);
}

/* $04A3FA. Moved along its angle (+$56) at 16 (a1, a2: the sine tables, as the dispatcher
 * leaves them). */
void Space4C4Move(uint16_t obj)
{
    uint16_t d0 = rd16(obj + 0x56);
    add32(obj + O_Y, (uint32_t)sine(d0) << 4);
    add32(obj + O_X, (uint32_t)cosine(d0) << 4);
}

/* loc_04A3F4 */
static void move_turned(uint16_t obj)
{
    wr16(obj + 0x56, rd16(obj + 0x56) & 0x1FE);
    Space4C4Move(obj);
}

/* $04A148. Its course; its look. */
void Space4C4Start(uint16_t obj)
{
    wr16(obj + O_STATE, rd16(obj + 6));
    wr16(obj + O_X, 0);
    wr16(obj + O_Y, 0);
    uint32_t a0 = 0x04C5BE;
    uint16_t d7 = 2;
    ObjectSetup(obj, &a0, &d7);
}

/* from the right at y, moving left */
static void flier_enter(uint16_t obj, uint16_t y)
{
    next(obj, 4);
    wr16(obj + O_Y, y);
    wr16(obj + O_X, 0x1C0);
    wr16(obj + 0x56, 0x100);
}

/* $04A162. Kind 4: from the right at $B8. */
void Space4C4Step04(uint16_t obj)
{
    flier_enter(obj, 0xB8);
}

/* $04A17A. Kind $1C: at $148. */
void Space4C4Step1C(uint16_t obj)
{
    flier_enter(obj, 0x148);
}

/* $04A192. Left to $15C; then (above the easiest level, d5) a shot at the player. */
void Space4C4Step08(uint16_t obj, uint16_t d5)
{
    Space4C4Move(obj);
    if (!below(obj + O_X, 0x15C)) return;
    next(obj, 4);
    if (d5) SpaceShotAimed(obj);
}

/* turning by `by` (of $1F8) until the angle is `to` */
static void flier_turn(uint16_t obj, uint16_t by, uint16_t to)
{
    Space4C4Move(obj);
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) + by) & 0x1F8);
    if (rd16(obj + 0x56) == to) next(obj, 4);
}

/* $04A1AC. Turning down ($80). */
void Space4C4Step0C(uint16_t obj)
{
    flier_turn(obj, (uint16_t)-8, 0x80);
}

/* $04A1CA. Turning up ($180). */
void Space4C4Step24(uint16_t obj)
{
    flier_turn(obj, 8, 0x180);
}

/* $04A1E8. Down to $100. */
void Space4C4Step10(uint16_t obj)
{
    Space4C4Move(obj);
    if (!below(obj + O_Y, 0x100)) next(obj, 4);
}

/* $04A1FC. Up to $100. */
void Space4C4Step28(uint16_t obj)
{
    Space4C4Move(obj);
    if (below(obj + O_Y, 0x100)) next(obj, 4);
}

/* loc_04A228: on; from difficulty 4 (d5) a shot at the player */
static void flier_on(uint16_t obj, uint16_t d5)
{
    next(obj, 4);
    if ((int16_t)(d5 - 4) >= 0) SpaceShotAimed(obj);
}

/* $04A210. Turning back left ($100); then on (a shot from difficulty 4, d5). */
void Space4C4Step14(uint16_t obj, uint16_t d5)
{
    Space4C4Move(obj);
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) + 8) & 0x1F8);
    if (rd16(obj + 0x56) != 0x100) return;
    flier_on(obj, d5);
}

/* $04A236. The same the other way. */
void Space4C4Step2C(uint16_t obj, uint16_t d5)
{
    Space4C4Move(obj);
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 8) & 0x1F8);
    if (rd16(obj + 0x56) != 0x100) return;
    flier_on(obj, d5);
}

/* $04A250. Away left, removed past $78. */
void Space4C4Step18(uint16_t obj)
{
    Space4C4Move(obj);
    if (below(obj + O_X, 0x78)) remove_obj(obj);
}

/* $04A266. Kind $34: from the right at $E8. */
void Space4C4Step34(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + O_X, 0x1C0);
    wr16(obj + O_Y, 0xE8);
    wr16(obj + 0x56, 0x100);
}

/* $04A27E. Kind $44: at $118. */
void Space4C4Step44(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + O_X, 0x1C0);
    wr16(obj + O_Y, 0x118);
    wr16(obj + 0x56, 0x100);
}

/* $04A296. Left to $140. */
void Space4C4Step38(uint16_t obj)
{
    Space4C4Move(obj);
    if (below(obj + O_X, 0x140)) next(obj, 4);
}

/* loc_04A2C2: shooting at the player when the frame's bits (by the difficulty's step) clear */
static void flier_shoot(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & tab16(0x04A2DC, rd16(RANK)))) SpaceShotAimed(obj);
}

/* $04A2AA. Looping round one way back to the right (angle 0), shooting. */
void Space4C4Step3C(uint16_t obj)
{
    Space4C4Move(obj);
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 8) & 0x1F8);
    if (!rd16(obj + 0x56)) {
        next(obj, 4);
        return;
    }
    flier_shoot(obj);
}

/* $04A2EC. The other way. */
void Space4C4Step4C(uint16_t obj)
{
    Space4C4Move(obj);
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) + 8) & 0x1F8);
    if (rd16(obj + 0x56)) {
        flier_shoot(obj);
        return;
    }
    next(obj, 4);
}

/* $04A304. Away right, removed past $1C8. */
void Space4C4Step40(uint16_t obj)
{
    Space4C4Move(obj);
    if (!below(obj + O_X, 0x1C8)) remove_obj(obj);
}

/* $04A31A. Kinds $54, $5C: from the right at $100. */
void Space4C4Step54(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + O_Y, 0x100);
    wr16(obj + O_X, 0x1C0);
    wr16(obj + 0x56, 0x100);
}

/* loc_04A33E: weaving: removed past $78, else moved and 2 more left */
static void flier_weave(uint16_t obj)
{
    wr16(obj + 0x56, rd16(obj + 0x56) & 0x1FE);
    if (below(obj + O_X, 0x78)) {
        remove_obj(obj);
        return;
    }
    Space4C4Move(obj);
    add16(obj + O_X, (uint16_t)-2);
}

/* $04A332. Weaving, turning one way. */
void Space4C4Step58(uint16_t obj)
{
    add16(obj + 0x56, (uint16_t)-8);
    flier_weave(obj);
}

/* $04A33A. The other way. */
void Space4C4Step60(uint16_t obj)
{
    add16(obj + 0x56, 8);
    flier_weave(obj);
}

/* loc_04A368: from the left ($71), its turning x +$4E from $DFFC */
static void flier_left(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + 0x4E, (uint16_t)(rd16(0xDFFC) + 0x1F0));
    wr16(obj + O_X, 0x71);
}

/* $04A358. Kind $64: from the left at $A8. */
void Space4C4Step64(uint16_t obj)
{
    wr16(obj + O_Y, 0xA8);
    flier_left(obj);
}

/* $04A362. Kind $6C: at $14C. */
void Space4C4Step6C(uint16_t obj)
{
    wr16(obj + O_Y, 0x14C);
    flier_left(obj);
}

/* loc_04A3AC: right fast, removed past $1C8 */
static void flier_dash(uint16_t obj)
{
    add32(obj + O_X, 0x64000);
    if (!below(obj + O_X, 0x1C8)) remove_obj(obj);
}

/* $04A380. Right to +$4E, then down fast (removed past $170). */
void Space4C4Step68(uint16_t obj)
{
    if ((int16_t)(rd16(obj + 0x4E) - rd16(obj + O_X)) >= 0) {
        flier_dash(obj);
        return;
    }
    add32(obj + O_Y, 0x68000);
    if (!below(obj + O_Y, 0x170)) remove_obj(obj);
}

/* $04A3A0. Right to +$4E, then up fast (removed above $88). */
void Space4C4Step70(uint16_t obj)
{
    if ((int16_t)(rd16(obj + 0x4E) - rd16(obj + O_X)) >= 0) {
        flier_dash(obj);
        return;
    }
    add32(obj + O_Y, (uint32_t)-0x68000);
    if (below(obj + O_Y, 0x88)) remove_obj(obj);
}

/* $04A418. Kind $74: from the right at a random y, vitality $14, $28 frames. */
void Space4C4Step74(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + O_VITALITY, 0x14);
    wr16(obj + 0x26, 4);
    wr16(obj + 0x56, 0x100);
    wr16(obj + 0x5C, 0x28);
    wr16(obj + O_X, 0x1C8);
    wr16(obj + O_Y, (uint16_t)((rd16(v_randomSeed) & 0x7F) + 0xC0));
}

/* loc_04A472: turning down-left (to $C0) */
static void flier_down(uint16_t obj)
{
    if (!below(obj + 0x56, 0xC0)) add16(obj + 0x56, (uint16_t)-0x10);
    move_turned(obj);
}

/* loc_04A490: turning up-left (to $140) */
static void flier_up(uint16_t obj)
{
    if (below(obj + 0x56, 0x140)) add16(obj + 0x56, 0x10);
    move_turned(obj);
}

/* $04A44C. Wandering for +$5C frames (then the next, its time by the difficulty): kept between
 * y $A0 and $160, else turning at random. */
void Space4C4Step78(uint16_t obj)
{
    if (dec(obj + 0x5C)) {
        next(obj, 4);
        wr16(obj + 0x5C, tab16(0x04A4B6, rd16(v_difficulty)));
    }
    if (below(obj + O_Y, 0xA0)) {
        flier_down(obj);
        return;
    }
    if (!below(obj + O_Y, 0x160)) {
        flier_up(obj);
        return;
    }
    Random();
    if (!(rd16(v_randomSeed) & 0x8000)) flier_up(obj);
    else flier_down(obj);
}

/* $04A4BE. +$5C frames; then a shot at the player. */
void Space4C4Step7C(uint16_t obj)
{
    if (!dec(obj + 0x5C)) return;
    next(obj, 4);
    SpaceShotAimed(obj);
}

/* $04A4CE. Away, turning to the nearer edge (removed above $88 or below $168). */
void Space4C4Step80(uint16_t obj)
{
    if (below(obj + O_Y, 0x100)) {
        if (below(obj + O_Y, 0x88)) {
            remove_obj(obj);
            return;
        }
        if (below(obj + 0x56, 0x180)) add16(obj + 0x56, 4);
        move_turned(obj);
        return;
    }
    if (!below(obj + O_Y, 0x168)) {
        remove_obj(obj);
        return;
    }
    if (!below(obj + 0x56, 0x80)) add16(obj + 0x56, (uint16_t)-4);
    move_turned(obj);
}

/* $04A080. Type $4C4: hitting something a puff; shot down (85 points) a puff; else its look and
 * its step (d5 the difficulty, d6 its step, a1 a2 the sine tables). */
void Space4C4(uint16_t obj)
{
    if (bit8(obj + 0x22, 4)) {
        puff_563ee(obj);
        return;
    }
    if (rd16(obj + O_VITALITY) & 0x8000) {
        AddScore(obj, 85);
        shot_down(obj);
        return;
    }
    Space4C4Look(obj);
    uint16_t d5 = rd16(v_difficulty);
    uint32_t fn = rom32(0x04A0C4 + (uint32_t)(int16_t)rd16(obj + O_STATE));
    switch (fn) {
    case 0x04A192: Space4C4Step08(obj, d5); break;
    case 0x04A210: Space4C4Step14(obj, d5); break;
    case 0x04A236: Space4C4Step2C(obj, d5); break;
    default: call_object(fn, obj); break;
    }
}

/* ---- type $4CC: a mine: blinking for +$48 frames (by the difficulty's step), then shown and
 * (kind, +6, set) thrown at the player; a shot at the player when +$56 runs out ---- */

/* $04A7CC. Its sprite by the frame (every 8). */
void Space4CCLook(uint16_t obj)
{
    wr16(obj + 0x0E, bit8(v_gameFrames + 1, 3) ? 0xE330 : 0xE320);
}

/* $04A7E6. +$56 by the difficulty's step; with a kind, thrown at the player at 16. */
void Space4CCThrow(uint16_t obj)
{
    wr16(obj + 0x56, tab16(0x04A828, rd16(RANK)));
    if (!rd16(obj + 6)) return;
    uint16_t d2 = SpaceAim(obj);
    wr32(obj + 0x1C, (uint32_t)sine(d2) << 4);
    wr32(obj + 0x18, (uint32_t)cosine(d2) << 4);
}

/* $04A748. Its set-up ($35 less the difficulty's step); blinking, then armed. */
void Space4CCRun(uint16_t obj)
{
    uint16_t d0 = rd16(obj + O_STATE);
    if (!d0) {
        wr16(obj + 0x48, (uint16_t)(0x35 - rd16(RANK)));
        uint32_t a0 = 0x04C5D4;
        uint16_t d7;
        ObjectSetupAll(obj, &a0, &d7);
        return;
    }
    Space4CCLook(obj);
    if (d0 == 8) {
        if (!(rd16(obj + 0x56) & 0x8000) && dec(obj + 0x56)) SpaceShotAimed(obj);
        add32(obj + O_X, rd32(obj + 0x18));
        add32(obj + O_Y, rd32(obj + 0x1C));
        return;
    }
    if (dec(obj + 0x48)) {
        next(obj, 4);
        wr8(obj + 0x21, 0x30);
        Space4CCThrow(obj);
        set8(obj + O_FLAGS, 7);
        return;
    }
    if (bit8(v_gameFrames + 1, 0)) clr8(obj + O_FLAGS, 7);
    else set8(obj + O_FLAGS, 7);
}

/* $04A73E. Type $4CC (40 points). */
void Space4CC(uint16_t obj)
{
    Space4CCRun(obj);
    hit_or_shot(obj, 40);
}

/* ---- type $4D0: mines ($4CC) laid by its kind (+6, its first step) ---- */

/* $04A542. Its kind's step; a sound. */
void Space4D0Start(uint16_t obj)
{
    wr16(obj + O_STATE, rd16(obj + 6));
    wr16(obj + 0x48, 0);
    wr16(obj + 0x4A, 0);
    QueueSound(0xC7);
}

/* loc_04A576: a column of five mines (kind 1) at x from y $D0, $18 apart; then removed */
static void mine_column(uint16_t obj, uint16_t x)
{
    uint16_t y = 0xD0;
    for (int i = 0; i < 5; i++) {
        uint16_t slot, t;
        if (!SlotFreeMid(&slot, &t)) break;
        wr16(slot + O_TYPE, 0x4CC);
        wr16(slot + O_X, x);
        wr16(slot + O_Y, y);
        wr16(slot + 6, 1);
        y = (uint16_t)(y + 0x18);
    }
    remove_obj(obj);
}

/* $04A55A. A column at $1A0. */
void Space4D0Step04(uint16_t obj)
{
    mine_column(obj, 0x1A0);
}

/* $04A562. At $170. */
void Space4D0Step08(uint16_t obj)
{
    mine_column(obj, 0x170);
}

/* $04A56A. At $140. */
void Space4D0Step0C(uint16_t obj)
{
    mine_column(obj, 0x140);
}

/* $04A572. At $A0. */
void Space4D0Step10(uint16_t obj)
{
    mine_column(obj, 0xA0);
}

/* loc_04A5B8: count + 1 mines (no kind) on a ring about the player (the offsets a1, a2 from 8
 * pairs, wrapping), moving out at the angle d4 (on by $40 each); then removed */
static void mine_ring(uint16_t obj, uint32_t a1, uint32_t a2, uint16_t d4, uint16_t count)
{
    uint16_t target = PlayerTarget();
    uint16_t d5 = rd16(target + O_X), d6 = rd16(target + O_Y);
    for (;;) {
        uint16_t slot, t;
        if (!SlotFreeMid(&slot, &t)) break;
        wr16(slot + O_TYPE, 0x4CC);
        uint16_t d1 = rom16(a1), d2 = rom16(a2);
        a1 += 2;
        a2 += 2;
        wr16(slot + O_X, (uint16_t)(d1 + d5));
        wr16(slot + O_Y, (uint16_t)(d2 + d6));
        wr32(slot + 0x18, (uint32_t)cosine(d4) << 4);
        wr32(slot + 0x1C, (uint32_t)sine(d4) << 4);
        if (a1 >= 0x04A72E) a1 = 0x04A71E;
        if (a2 >= 0x04A73E) a2 = 0x04A72E;
        d4 = (uint16_t)(d4 + 0x40) & 0x1FE;
        if (count-- == 0) break;
    }
    remove_obj(obj);
}

/* $04A5A8. Eight about the player. */
void Space4D0Step14(uint16_t obj)
{
    mine_ring(obj, 0x04A71E, 0x04A72E, 0, 7);
}

/* $04A63C. Seven from a random place in the ring. */
void Space4D0Step18(uint16_t obj)
{
    uint16_t d4 = rd16(v_randomSeed) & 0x1C0;
    uint32_t d0 = d4 >> 5;
    d4 = (uint16_t)(d4 + 0x100) & 0x1FE;
    mine_ring(obj, 0x04A71E + d0, 0x04A72E + d0, d4, 6);
}

/* $04A66A. Eight, moving from the angle $120. */
void Space4D0Step1C(uint16_t obj)
{
    mine_ring(obj, 0x04A71E, 0x04A72E, 0x120, 7);
}

/* $04A680. For $200 frames, every so often (by the difficulty's step) a mine at the left edge
 * (the right in step $24) in the player's half. */
void Space4D0Step20(uint16_t obj)
{
    add16(obj + 0x4A, 1);
    if (!below(obj + 0x4A, 0x200)) {
        remove_obj(obj);
        return;
    }
    if (!dec(obj + 0x48)) return;
    wr16(obj + 0x48, tab16(0x04A70E, rd16(RANK)));
    int low = !below(PlayerTarget() + O_Y, 0x100);
    uint16_t slot, t;
    if (!SlotFreeMid(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x4CC);
    wr16(slot + O_X, 0x71);
    if (rd16(obj + O_STATE) == 0x24) wr16(slot + O_X, 0x1CF);
    wr16(slot + 6, 1);
    uint16_t d0 = rd16(v_randomSeed) & 0x3F;
    wr16(slot + O_Y, (uint16_t)(d0 + (low ? 0x118 : 0x98)));
}

/* $04A510. Type $4D0. */
void Space4D0(uint16_t obj)
{
    call_object(rom32(0x04A51A + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $4D4: a small ship by its kind (+6, its first step): $18 darts in and fires, $2C
 * rides its carrier (+$5C, while that one's type is still +$5E; at +$58, +$5A) and fires, $34
 * and $3C carry an item (dropped for them) about; shot down it is thrown up and falls into a
 * puff ---- */

/* $04A96C. Its kind's step; its look. */
void Space4D4Start(uint16_t obj)
{
    wr16(obj + O_STATE, rd16(obj + 6));
    wr16(obj + 0x4A, 0x40);
    uint32_t a0 = 0x04C5EA;
    uint16_t d7 = 1;
    ObjectSetup(obj, &a0, &d7);
}

/* $04A984. Shot down: rising, slowing; then its look falling. */
void Space4D4Step04(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    next(obj, 4);
    wr16(obj + 0x0E, 0xE373);
    wr16(obj + 0x48, 8);
}

/* $04A9A2. Falling 8 frames; then (from difficulty 4 a shot at the player) a puff. */
void Space4D4Step08(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if (!dec(obj + 0x48)) return;
    SpaceShotIfHard(obj);
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x05639C);
    wr16(obj + O_STATE, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    QueueSound(0xDD);
}

/* $04A9D8. Facing left; its look; speeding left (left of $120) or right. */
void Space4D4Step0C(uint16_t obj)
{
    clr8(obj + 0x0E, 3);
    uint32_t a0 = 0x04C5EA;
    uint16_t d7 = 1;
    ObjectSetup(obj, &a0, &d7);
    next(obj, below(obj + O_X, 0x120) ? 4 : 8);
}

/* $04AA00. Speeding left. */
void Space4D4Step10(uint16_t obj)
{
    add32(obj + 0x18, (uint32_t)-0x2000);
}

/* $04AA0A. Speeding right. */
void Space4D4Step14(uint16_t obj)
{
    add32(obj + 0x18, 0x2000);
}

/* $04AA14. Kind $18: in from the right at a random y, toward the player's row, for a random
 * $10-$2F frames. */
void Space4D4Step18(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + 0x48, (uint16_t)((rd8(v_randomSeed) & 0x1F) + 0x10));
    wr16(obj + O_X, 0x1C4);
    wr16(obj + O_Y, (uint16_t)((rd16(v_randomSeed) & 0x3F) + 0xE0));
    wr32(obj + 0x18, 0xFFFDE000);
    if ((int16_t)(rd16(PlayerTarget() + O_Y) - rd16(obj + O_Y)) < 0) wr32(obj + 0x1C, 0xFFFF2000);
    else wr32(obj + 0x1C, 0xE000);
}

/* $04AA6C. Then still ($14 frames). */
void Space4D4Step1C(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 4);
    wr16(obj + 0x48, 0x14);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x0E, 0xE365);
}

/* loc_04AABE: a shot ($52C) to the left */
static void small_ship_fire(uint16_t obj)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    QueueSound(0xB6);
    wr16(slot + O_TYPE, 0x52C);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) - 2));
    wr16(slot + O_X, (uint16_t)(rd16(slot + O_X) - 6));
    wr32(slot + 0x18, 0xFFF98000);
}

/* $04AA8E. Firing (at 8 frames left, its firing look); then 8 frames. */
void Space4D4Step20(uint16_t obj)
{
    if (dec(obj + 0x48)) {
        next(obj, 4);
        wr16(obj + 0x48, 8);
        return;
    }
    if (rd16(obj + 0x48) != 8) return;
    wr16(obj + 0x0E, 0xE369);
    wr16(obj + 0x08, 0x900);
    wr16(obj + 0x0A, 0xF4F8);
    small_ship_fire(obj);
}

/* $04AB3E. Nothing. */
void Space4D4Step28(uint16_t obj)
{
    (void)obj;
}

/* $04AAF4. Then away right, drifting from the player's row. */
void Space4D4Step24(uint16_t obj)
{
    if (!dec(obj + 0x48)) {
        Space4D4Step28(obj);
        return;
    }
    next(obj, 4);
    wr16(obj + 0x0E, 0xE361);
    wr16(obj + 0x08, 0x500);
    wr16(obj + 0x0A, 0xF8F8);
    wr32(obj + 0x18, 0x26000);
    if ((int16_t)(rd16(PlayerTarget() + O_Y) - rd16(obj + O_Y)) < 0) {
        wr32(obj + 0x1C, 0x10000);
        Space4D4Step28(obj);
        return;
    }
    wr32(obj + 0x1C, 0xFFFF0000);
}

/* loc_04ABF0: its carrier gone: on its own (step $C) */
static void small_ship_free(uint16_t obj)
{
    wr16(obj + O_STATE, 0xC);
    wr16(obj + 0x0E, 0xE361);
    wr16(obj + 0x08, 0x500);
    wr16(obj + 0x0A, 0xF8F8);
}

/* its carrier (+$5C), 0 when it is gone (its type no longer +$5E) */
static uint16_t carrier(uint16_t obj)
{
    uint16_t c = rd16(obj + 0x5C);
    return rd16(c) == rd16(obj + 0x5E) ? c : 0;
}

/* $04AB40. Kind $2C: riding its carrier (facing its way); every so often (by the difficulty's
 * step) firing. */
void Space4D4Step2C(uint16_t obj)
{
    uint16_t c = carrier(obj);
    if (!c) {
        small_ship_free(obj);
        return;
    }
    wr16(obj + O_X, (uint16_t)(rd16(c + O_X) + rd16(obj + 0x58)));
    wr16(obj + O_Y, (uint16_t)(rd16(c + O_Y) + rd16(obj + 0x5A)));
    wr16(obj + 0x0E, bit8(c + 0x18, 7) ? 0xE35D : 0xE361);
    if (!dec(obj + 0x4A)) return;
    wr16(obj + 0x4A, tab16(0x04ABAC, rd16(RANK)));
    next(obj, 4);
    wr16(obj + 0x48, 8);
    wr16(obj + 0x0E, 0xE365);
    small_ship_fire(obj);
}

/* $04ABBC. Riding, firing for 8 frames; then riding again. */
void Space4D4Step30(uint16_t obj)
{
    uint16_t c = carrier(obj);
    if (!c) {
        small_ship_free(obj);
        return;
    }
    wr16(obj + O_X, (uint16_t)(rd16(c + O_X) + rd16(obj + 0x58)));
    wr16(obj + O_Y, (uint16_t)(rd16(c + O_Y) + rd16(obj + 0x5A)));
    if (dec(obj + 0x48)) add16(obj + O_STATE, (uint16_t)-4);
}

/* $04AC58. Carrying its item (16 to its right): wandering toward a random point (a new one
 * every $20 frames) eased by $1400 a frame to 2.2 across and 2 up or down; after +$4C frames
 * drifting down. */
void Space4D4Step38(uint16_t obj)
{
    uint16_t c = carrier(obj);
    if (!c) {
        small_ship_free(obj);
        return;
    }
    wr16(c + O_X, rd16(obj + O_X));
    wr16(c + O_Y, rd16(obj + O_Y));
    wr16(c + O_X, (uint16_t)(rd16(c + O_X) - 0x10));
    if (!(rd16(v_gameFrames) & 0x1F)) {
        wr16(obj + 0x48, (uint16_t)((rd8(v_randomSeed) & 0x3F) + 0x100));
        wr16(obj + 0x4A, (uint16_t)((rd8(0xFF09) & 0x3F) + 0xE0));
    }
    if (rd16(obj + 0x4C) & 0x8000) {
        add32(obj + 0x1C, 0x1400);
        return;
    }
    add16(obj + 0x4C, 0xFFFF);
    if ((int16_t)(rd16(obj + 0x48) - rd16(obj + O_X)) >= 0) {
        if ((rd16(obj + 0x18) & 0x8000) || (int32_t)(rd32(obj + 0x18) - 0x23000u) < 0)
            add32(obj + 0x18, 0x1400);
    } else {
        if (!(rd16(obj + 0x18) & 0x8000) || (int32_t)(rd32(obj + 0x18) - 0xFFFDD000u) >= 0)
            add32(obj + 0x18, (uint32_t)-0x1400);
    }
    if ((int16_t)(rd16(obj + 0x4A) - rd16(obj + O_Y)) >= 0) {
        if ((rd16(obj + 0x1C) & 0x8000) || (int32_t)(rd32(obj + 0x1C) - 0x20000u) < 0)
            add32(obj + 0x1C, 0x1400);
        return;
    }
    if (!(rd16(obj + 0x1C) & 0x8000) || (int32_t)(rd32(obj + 0x1C) - 0xFFFE0000u) >= 0)
        add32(obj + 0x1C, (uint32_t)-0x1400);
}

/* loc_04AC14: carrying the item at `item` (its type kept at +$5E; it drifts left): in from the
 * right at $100, vitality $1E */
static void carry_start(uint16_t obj, uint16_t item)
{
    wr16(obj + 0x0E, 0xC35D);
    wr16(obj + O_STATE, 0x38);
    wr16(obj + 0x5C, item);
    wr16(obj + 0x5E, rd16(item));
    wr32(item + 0x18, 0xFFFF8000);
    wr32(item + 0x1C, 0);
    wr16(obj + O_Y, 0x100);
    wr16(obj + O_X, 0x1C0);
    wr16(obj + 0x48, 0x120);
    wr16(obj + 0x4A, 0x100);
    wr16(obj + 0x4C, 0x200);
    wr16(obj + O_VITALITY, 0x1E);
    Space4D4Step38(obj);
}

/* $04AC0A. Kind $34: a random item to carry (DropItemRandom; the slot it takes found first,
 * as its a0; none: removed). */
void Space4D4Step34(uint16_t obj)
{
    uint16_t item;
    int found = SlotFreeLow(&item);
    DropItemRandom(obj);
    if (!found) {
        remove_obj(obj);
        return;
    }
    carry_start(obj, item);
}

/* $04AD40. Kind $3C: an item of kind 15 (DropItem; the slot it takes found first, as its a0). */
void Space4D4Step3C(uint16_t obj)
{
    uint16_t item;
    int found = SlotFreeLow(&item);
    DropItem(obj);
    if (!found) {
        remove_obj(obj);
        return;
    }
    carry_start(obj, item);
}

/* $04A8E2. Shot down (5 points): thrown up and back (step 4); then its step. */
void Space4D4Run(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) & 0x8000) {
        AddScore(obj, 5);
        wr16(obj + O_STATE, 4);
        wr16(obj + O_VITALITY, 0);
        wr8(obj + 0x21, 0);
        wr32(obj + 0x1C, 0xFFFD8000);
        wr32(obj + 0x18, 0xC000);
        wr16(obj + 0x0E, 0xE36F);
        wr16(obj + 0x08, 0x500);
        wr16(obj + 0x0A, 0xF8F8);
    }
    call_object(rom32(0x04A92C + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $04A8AE. Type $4D4: removed out of x $58-$1E7, y $70-$187. */
void Space4D4(uint16_t obj)
{
    Space4D4Run(obj);
    if (below(obj + O_X, 0x58) || !below(obj + O_X, 0x1E8) || below(obj + O_Y, 0x70)
        || !below(obj + O_Y, 0x188))
        remove_obj(obj);
}

/* ---- type $4D8: small ships ($4D4 of kind $18) every so often (by the difficulty's step) for
 * +$48 frames ---- */

/* $04A84A. Its kind's step. */
void Space4D8Start(uint16_t obj)
{
    wr16(obj + O_STATE, rd16(obj + 6));
    wr16(obj + 0x4A, 0);
}

/* $04A856. Launching. */
void Space4D8Run(uint16_t obj)
{
    if (dec(obj + 0x4A)) {
        wr16(obj + 0x4A, tab16(0x04A89E, rd16(RANK)));
        uint16_t slot, t;
        if (SlotFreeMid(&slot, &t)) {
            wr16(slot + O_TYPE, 0x4D4);
            wr16(slot + 6, 0x18);
            wr16(slot + O_X, 0x1CF);
            wr16(slot + O_Y, 0x16F);
        }
    }
    if (dec(obj + 0x48)) remove_obj(obj);
}

/* $04A838. Type $4D8. */
void Space4D8(uint16_t obj)
{
    call_object(rom32(0x04A842 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $4DC: a snake: its head (+6 set) steers at the player, the others (+$5A the head)
 * follow it from the head's trail ($9000: x, $9060: y, $90C0: its look, 48 words each, +$5C
 * the place in it) ---- */

/* $049D46. The trail's first halves cleared. */
void SnakeTrailClear(void)
{
    for (int i = 0; i < 12; i++) {
        wr32((uint16_t)(0x9000 + 4 * i), 0);
        wr32((uint16_t)(0x9060 + 4 * i), 0);
        wr32((uint16_t)(0x90C0 + 4 * i), 0);
    }
}

/* $049D9A. The trail moved up a word, the head's x, y and look (+$5E) put first. */
void SnakeTrail(uint16_t obj)
{
    for (uint16_t i = 0x5C; ; i = (uint16_t)(i - 2)) {
        wr16((uint16_t)(0x9000 + i + 2), rd16((uint16_t)(0x9000 + i)));
        wr16((uint16_t)(0x9060 + i + 2), rd16((uint16_t)(0x9060 + i)));
        wr16((uint16_t)(0x90C0 + i + 2), rd16((uint16_t)(0x90C0 + i)));
        if (!i) break;
    }
    wr16(0x9000, rd16(obj + O_X));
    wr16(0x9060, rd16(obj + O_Y));
    wr16(0x90C0, rd16(obj + 0x5E));
}

/* $049C70. The head: in from the left, its trail cleared; six body parts behind it (+$5C $10
 * apart) and the tail (in one of the last four slots). */
void SnakeStart(uint16_t obj)
{
    next(obj, 8);
    wr16(obj + 0x4E, 0x140);
    wr16(obj + O_X, 0x80);
    wr16(obj + O_Y, 0x100);
    wr32(obj + 0x18, 0x10000);
    uint32_t a0 = 0x04C57C;
    uint16_t d7 = 2;
    ObjectSetup(obj, &a0, &d7);
    wr16(obj + O_FLAGS, 0x8000);
    wr8(obj + 0x20, 0x80);
    wr8(obj + 0x23, 0x20);
    SnakeTrailClear();
    uint16_t d5 = 16, part, t;
    for (int i = 0; i < 6; i++) {
        if (!SlotFreeHigh(&part, &t)) return;
        wr16(part + O_TYPE, 0x4DC);
        wr16(part + O_STATE, 4);
        wr16(part + 6, 0);
        wr8(part + 0x23, 0x20);
        wr16(part + 0x5C, d5);
        wr16(part + 0x5A, obj);
        a0 = 0x04C57C;
        d7 = 2;
        ObjectSetup(part, &a0, &d7);
        wr16(part + O_FLAGS, 0x8000);
        d5 = (uint16_t)(d5 + 0x10);
    }
    if (!SlotFreeLast(&part)) return;
    wr16(part + O_TYPE, 0x4DC);
    wr16(part + O_STATE, 0x10);
    wr16(part + 6, 0);
    wr16(part + 0x5C, d5);
    wr16(part + 0x5A, obj);
    a0 = 0x04C5EA;
    d7 = 0;
    ObjectSetup(part, &a0, &d7);
    wr16(part + O_FLAGS, 0x8000);
    wr8(part + 0x20, 0x80);
}

/* $049D64. A body part: where the trail says (+$5C on). */
void SnakeBody(uint16_t obj)
{
    uint16_t i = rd16(obj + 0x5C);
    wr16(obj + O_X, rd16((uint16_t)(0x9000 + i)));
    wr16(obj + O_Y, rd16((uint16_t)(0x9060 + i)));
    uint32_t a = (uint32_t)(int32_t)(int16_t)rd16((uint16_t)(0x90C0 + i)) + 0x049DE0;
    wr16(obj + 0x0E, rom16(a));
    wr16(obj + 0x08, rom16(a + 2));
    wr16(obj + 0x0A, rom16(a + 4));
}

/* loc_049E36: the head's look stepped (every 8 frames, +$5E 0-$12 by 6), its speed (+$48,
 * long) down by the difficulty's table to $8000 (then back up), moved along its angle (+$56);
 * the angle turned by 8 toward the player (or, from step $C, toward the left: $100) */
static void snake_head(uint16_t obj)
{
    if (dec(obj + 0x4C)) {
        wr16(obj + 0x4C, 8);
        add16(obj + 0x5E, 6);
        if (!below(obj + 0x5E, 0x13)) wr16(obj + 0x5E, 0);
        uint16_t d0 = rd16(obj + 0x5E);
        wr16(obj + 0x0E, rom16(0x049DE0 + d0));
        wr16(obj + 0x08, rom16(0x049DE0 + d0 + 2));
        wr16(obj + 0x0A, rom16(0x049DE0 + d0 + 4));
    }
    uint16_t d0 = (uint16_t)(rd16(v_difficulty) << 1);
    uint32_t d1 = rom32(0x049DF8 + (uint32_t)(int16_t)d0), d2 = rom32(0x049E08 + (uint32_t)(int16_t)d0);
    wr32(obj + 0x48, rd32(obj + 0x48) - d1);
    if ((int32_t)(rd32(obj + 0x48) - 0x8000u) < 0) wr32(obj + 0x48, d2);
    d0 = rd16(obj + 0x56);
    int32_t speed = (int16_t)rd16(obj + 0x48);
    add32(obj + O_Y, (uint32_t)(sine(d0) * speed) << 2);
    add32(obj + O_X, (uint32_t)(cosine(d0) * speed) << 2);
    uint16_t d7 = below(obj + O_STATE, 0xC) ? SpaceAim(obj) : 0x100;
    d7 = (uint16_t)(d7 - rd16(obj + 0x56));
    int up;
    if (d7 & 0x8000) up = (int16_t)(uint16_t)(d7 - 0xFF00) < 0;
    else up = (int16_t)(d7 - 0x100) < 0;
    if (up) add16(obj + 0x56, 8);
    else add16(obj + 0x56, (uint16_t)-8);
    wr16(obj + 0x56, rd16(obj + 0x56) & 0x1FE);
}

/* $049E2A. The head, for +$4E frames; then step $C. */
void SnakeHead(uint16_t obj)
{
    if (dec(obj + 0x4E)) next(obj, 4);
    snake_head(obj);
}

/* $049E18. The head turning away left; removed left of $60. */
void SnakeLeave(uint16_t obj)
{
    if (below(obj + O_X, 0x60)) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    snake_head(obj);
}

/* $049F18. The tail: at the head, $C up, flipped by the head's angle. */
void SnakeTail(uint16_t obj)
{
    uint16_t head = rd16(obj + 0x5A);
    wr16(obj + O_X, rd16(head + O_X));
    wr16(obj + O_Y, rd16(head + O_Y));
    wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 0xC));
    uint16_t d0 = (uint16_t)(rd16(head + 0x56) + 0x80) & 0x1FE;
    if ((int16_t)(d0 - 0x100) < 0) set8(obj + 0x0E, 3);
    else clr8(obj + 0x0E, 3);
}

/* $049C2E. Type $4DC: the head records its trail; the others go when the head does. */
void Snake(uint16_t obj)
{
    if (rd16(obj + 6)) {
        SnakeTrail(obj);
    } else if (rd16(rd16(obj + 0x5A)) != 0x4DC) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    call_object(rom32(0x049C5C + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $518: a drifting mine, from difficulty 4 firing ($52C) along the player's row ---- */

/* $049B6A. Its look; vitality $50. */
void Space518Start(uint16_t obj)
{
    wr16(obj + O_STATE, rd16(obj + 6));
    uint32_t a0 = 0x04C5D4;
    uint16_t d7 = 2;
    ObjectSetup(obj, &a0, &d7);
    wr16(obj + O_VITALITY, 0x50);
    wr16(obj + 0x48, 0);
    wr8(obj + 0x21, 0x30);
    wr16(obj + 0x26, 0xA);
}

/* $049B94. Drifting right and down; from difficulty 4, once its wait is out, a shot toward the
 * player when within $C of its row ($40 frames apart). */
void Space518Run(uint16_t obj)
{
    add32(obj + O_X, 0x8000);
    add16(obj + O_Y, 1);
    Space4CCLook(obj);
    if (below(v_difficulty, 4)) return;
    if (!(rd16(obj + 0x48) & 0x8000)) {
        add16(obj + 0x48, 0xFFFF);
        return;
    }
    uint16_t target = PlayerTarget();
    uint16_t d0 = (uint16_t)(rd16(target + O_Y) - rd16(obj + O_Y));
    if (d0 & 0x8000) d0 = (uint16_t)-d0;
    if ((int16_t)(d0 - 0xC) >= 0) return;
    QueueSound(0xC7);
    wr16(obj + 0x48, 0x40);
    uint16_t d6 = rd16(obj + O_X);
    uint32_t d5;
    if ((int16_t)(rd16(target + O_X) - rd16(obj + O_X)) < 0) {
        d5 = 0xFFF98000;
        d6 = (uint16_t)(d6 - 0x10);
    } else {
        d5 = 0x68000;
        d6 = (uint16_t)(d6 + 0x10);
    }
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x52C);
    wr16(slot + O_X, d6);
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr32(slot + 0x18, d5);
}

/* $049B58. Its step. */
void Space518Step(uint16_t obj)
{
    call_object(rom32(0x049B62 + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $049B4A. Type $518 ($4008 points). */
void Space518(uint16_t obj)
{
    Space518Step(obj);
    hit_or_shot(obj, 0x4008);
}

/* ---- type $520: a ship shuttling left and right at random (removed left of $80) ---- */

/* $04AD4E. Type $520. */
void Space520(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        uint32_t a0 = 0x04C5EA;
        uint16_t d7 = 0;
        ObjectSetupNext(obj, &a0, &d7);
        wr16(obj + O_FLAGS, 0x8100);
        clr8(obj + 0x0E, 7);
        wr8(obj + 0x20, 0xBC);
        wr16(obj + 0x48, 0);
        wr16(obj + 0x4A, 0);
        wr16(obj + 0x4C, 0);
        return;
    }
    if (below(obj + O_X, 0x80)) {
        remove_obj(obj);
        return;
    }
    if (!(rd16(obj + 0x48) & 0x8000)) {
        if (!dec(obj + 0x48)) return;
        wr16(obj + 0x4A, 0x20);
    }
    if (dec(obj + 0x4A)) {
        wr16(obj + 0x48, (uint16_t)((rd16(v_randomSeed) & 0x3F) + 8));
        wr16(obj + 0x4C, rd16(obj + 0x4C) ^ 1);
    }
    if (!rd16(obj + 0x4C)) {
        add32(obj + O_X, (uint32_t)-0x8000);
        clr8(obj + 0x0E, 3);
        return;
    }
    add32(obj + O_X, 0x8000);
    set8(obj + 0x0E, 3);
}

/* ---- type $530: until the camera's step $14, gunships ($4A4) from a list ($4AE48: type, x, y,
 * kind) at waits from $4AE38 ($7650: the end) ---- */

/* $04ADEE. Type $530. */
void Space530(uint16_t obj)
{
    if (!below(v_cameraStep, 0x14)) {
        remove_obj(obj);
        return;
    }
    if (rd16(obj + 0x48) == 0x7650) return;
    if (!dec(obj + 0x48)) return;
    uint16_t d0 = rd16(obj + 0x4A);
    wr16(obj + 0x48, tab16(0x04AE38, d0));
    uint32_t list = 0x04AE48 + (uint32_t)(int16_t)(uint16_t)(d0 << 2);
    add16(obj + 0x4A, 2);
    uint16_t slot;
    spawn_list(list, &slot);
}

/* ---- stage 6's flames and blasts ($54C-$560) ---- */

/* $04FB3A. A fading copy (type $550) of it here (its +$20, its sprite's palette bits at +$48),
 * moving at d2, d3. */
void Stage6Trail(uint16_t obj, uint32_t d2, uint32_t d3)
{
    uint16_t slot, t;
    if (!SlotFreeFromC980(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x550);
    wr8(slot + 0x20, rd8(obj + 0x20));
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr16(slot + 0x48, rd16(obj + 0x0E) & 0x6000);
    wr32(slot + 0x18, d2);
    wr32(slot + 0x1C, d3);
}

/* $04F74A. Type $54C: a flame rising faster and faster, swaying wider (about +$4C), leaving
 * fading copies; removed above y $40. */
void Flame54C(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + O_FLAGS, 0x8480);
        wr16(obj + 0x08, 0xF00);
        wr16(obj + 0x0A, 0xF0F0);
        wr16(obj + 0x4A, 0xFFF0);
        wr16(obj + 0x4C, rd16(obj + O_X));
        wr16(obj + 0x26, 0x10);
        wr8(obj + 0x21, 0x10);
        wr32(obj + 0x2C, 0xF440F808);
        QueueSound(0xB9);
    }
    add32(obj + 0x1C, (uint32_t)-0x2000);
    if (below(obj + O_Y, 0x40)) {
        remove_obj(obj);
        return;
    }
    add16(obj + 0x4A, 1);
    uint16_t d0 = (uint16_t)(rd16(obj + 0x48) + 0x24) & 0x1FE;
    wr16(obj + 0x48, d0);
    uint32_t x = ((uint32_t)(cosine(d0) * (int16_t)rd16(obj + 0x4A)) << 2) + rd32(obj + 0x4C);
    wr32(obj + O_X, x);
    Stage6Trail(obj, 0, 0x70000);
    wr8(obj + 0x20, bit8(obj + 0x48, 0) ? 0x14 : 8);
}

/* loc_04FB04: its mappings by its size (the sprite's tile, $380 on): two sizes of pieces, the
 * offset growing */
static void fade_size(uint16_t obj)
{
    uint16_t d0 = rd16(obj + 0x0E) & 0x7F0;
    wr16(obj + 0x08, (int16_t)(d0 - 0x3F0) < 0 ? 0xF00 : 0xA00);
    d0 = (uint16_t)(((int16_t)(uint16_t)(d0 - 0x370) >> 5) | 0xF0);
    wr16(obj + 0x0A, (uint16_t)(d0 | (uint16_t)(d0 << 8)));
}

/* loc_04F81E: its tile (+$4A) and sprite word (with +$48's palette) */
static void fade_set(uint16_t obj, uint16_t d0)
{
    wr16(obj + 0x4A, d0);
    wr16(obj + 0x0E, d0 | rd16(obj + 0x48));
    fade_size(obj);
}

/* $04F7F6. Type $550: a fading copy: its tile stepped from $380 by $10 to $470, then removed. */
void Fade550(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + 0x4A, 0x380);
        wr16(obj + O_FLAGS, 0x8C80);
    }
    uint16_t d0 = (uint16_t)(rd16(obj + 0x4A) + 0x10);
    if ((int16_t)(d0 - 0x470) >= 0) {
        remove_obj(obj);
        return;
    }
    fade_set(obj, d0);
}

/* $04F9D4. Type $554: the same backwards, from $460 down to $380. */
void Fade554(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + 0x4A, 0x460);
        wr16(obj + O_FLAGS, 0x8C80);
    }
    uint16_t d0 = (uint16_t)(rd16(obj + 0x4A) - 0x10);
    if ((int16_t)(d0 - 0x380) < 0) {
        remove_obj(obj);
        return;
    }
    fade_set(obj, d0);
}

/* $04F86C. Type $558's set-up: turning one way (+$48 bit 8 set) or the other. */
void Flame558Start(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + O_FLAGS, 0x8480);
    wr16(obj + 0x0E, 0x6380);
    wr16(obj + 0x08, 0xF00);
    wr16(obj + 0x0A, 0xF0F0);
    wr16(obj + 0x4A, 0x10);
    if (!bit8(obj + 0x48, 0)) next(obj, 4);
    QueueSound(0xB9);
}

/* loc_04F8AE: on along its angle d7 (4 down, 16 across) until it points down ($80): then
 * falling; hurting after +$4A frames */
static void flame_arc(uint16_t obj, uint16_t d7)
{
    d7 &= 0x1FE;
    wr16(obj + 0x48, d7);
    if (d7 == 0x80) {
        wr16(obj + O_STATE, 0xC);
        return;
    }
    if (dec(obj + 0x4A)) wr8(obj + 0x20, 0x14);
    add32(obj + O_Y, (uint32_t)sine(d7) << 2);
    add32(obj + O_X, (uint32_t)cosine(d7) << 4);
}

/* $04F8A6. Turning down one way (d7: +$48, as the dispatcher loads it). */
void Flame558Left(uint16_t obj)
{
    flame_arc(obj, (uint16_t)(rd16(obj + 0x48) + 8));
}

/* $04F8AC. The other way. */
void Flame558Right(uint16_t obj)
{
    flame_arc(obj, (uint16_t)(rd16(obj + 0x48) - 8));
}

/* $04F8FA. Falling to the floor ($128): it becomes a $55C, rolling one way, and another rolls
 * the other. */
void Flame558Fall(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if (below(obj + O_Y, 0x128)) return;
    wr16(obj + O_TYPE, 0x55C);
    wr16(obj + O_Y, 0x128);
    wr16(obj + O_FLAGS, 0);
    wr16(obj + 0x48, 1);
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x55C);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
}

/* $04F82E. Type $558: a flame thrown in an arc, leaving fading copies (every 4th frame). */
void Flame558(uint16_t obj)
{
    if (rd16(obj + O_STATE) && !(rd16(v_gameFrames) & 3)) Stage6Trail(obj, 0, 0x8000);
    call_object(rom32(0x04F85C + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $04F93C. Type $55C: a fire rolling along the floor (left, or right with +$48 set; removed at
 * the edges), a blast sound, the sprites shaking; every 8th frame a flame ($104) thrown up
 * behind. */
void Fire55C(uint16_t obj)
{
    BlastSound8();
    wr16(v_shakeSprites, 4);
    uint32_t d7;
    if (!rd16(obj + 0x48)) {
        add32(obj + O_X, (uint32_t)-0x10000);
        if (below(obj + O_X, 0x80)) {
            remove_obj(obj);
            return;
        }
        d7 = 0x4000;
    } else {
        add32(obj + O_X, 0x10000);
        if (!below(obj + O_X, 0x1C0)) {
            remove_obj(obj);
            return;
        }
        d7 = 0xFFFFC000;
    }
    if (rd16(v_gameFrames) & 7) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x104);
    wr32(slot + 0x48, 0x05663A);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr32(slot + 0x18, d7);
    wr32(slot + 0x1C, 0xFFFB8000);
    wr8(slot + 0x21, 0x10);
    wr16(slot + 0x26, 0xC);
    wr32(slot + 0x2C, 0xF808F808);
}

/* loc_04FA84: on (4 frames to the next ring); a ring of count + 1 objects of type d1 (+$48
 * d4) at d3 from here, moving out at 4, from the angle d2 (back by $40 each) */
static void blast_ring(uint16_t obj, uint16_t d1, uint16_t d2, uint16_t d3, uint16_t d4, uint16_t count)
{
    next(obj, 4);
    wr16(obj + 0x48, 4);
    for (;;) {
        uint16_t slot, t;
        if (!SlotFreeFromC980(&slot, &t)) return;
        wr16(slot + O_TYPE, d1);
        wr16(slot + 0x48, d4);
        wr8(slot + 0x21, 0x10);
        wr32(slot + 0x2C, 0xFC04FC04);
        wr16(slot + 0x26, 0x18);
        int32_t d5 = sine(d2), d6 = cosine(d2);
        wr32(slot + 0x1C, (uint32_t)d5 << 2);
        wr32(slot + 0x18, (uint32_t)d6 << 2);
        wr32(slot + O_Y, (uint32_t)(d5 * (int16_t)d3) + rd32(obj + O_Y));
        wr32(slot + O_X, (uint32_t)(d6 * (int16_t)d3) + rd32(obj + O_X));
        d2 = (uint16_t)(d2 - 0x40) & 0x1FE;
        if (count-- == 0) return;
    }
}

/* $04FA1E. Type $560's rings: the first at $40, five from angle 0. */
void Blast560Ring0(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    blast_ring(obj, 0x554, 0, 0x40, 0, 4);
}

/* $04FA28. At $80: four from $1E0 (+$48 $4000). */
void Blast560Ring1(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    blast_ring(obj, 0x554, 0x1E0, 0x80, 0x4000, 3);
}

/* $04FA46. At $C0: five ($6000). */
void Blast560Ring2(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    blast_ring(obj, 0x554, 0, 0xC0, 0x6000, 4);
}

/* $04FA62. At $100: four. */
void Blast560Ring3(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    blast_ring(obj, 0x554, 0x1E0, 0x100, 0x4000, 3);
}

/* $04FA6A. At $140: five; then removed. */
void Blast560Ring4(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    remove_obj(obj);
    blast_ring(obj, 0x554, 0, 0x140, 0x6000, 4);
}

/* $04FA00. Type $560: a blast, rings of fading flames ($554) growing out. */
void Blast560(uint16_t obj)
{
    call_object(rom32(0x04FA0A + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* ---- type $548: stage 6's boss on foot (at $A800): it walks at the nearer player and picks
 * a move by where the players are, how hurt it is (+$24) and its clock (+$50, $1600 frames);
 * +$5C's byte: bit 0 not hurtable, bit 1 busy, bit 2 not kept to the room; +$5E the hits it
 * takes before it is knocked down, +$5A its blinking ---- */

#define BOSS6_MOVE 0x9834        /* long: the dash's push on the camera */
#define BOSS6_RISE 0x9838
#define BOSS6_DASH 0x983C

/* loc_030CF4 (the boss on foot's, type $380): blinking while +$5A counts down (hidden on odd
 * counts), +$23 bit 2 set meanwhile */
static void boss_blink(uint16_t obj)
{
    clr8(obj + 0x23, 2);
    uint16_t d0 = rd16(obj + 0x5A);
    if (d0 && !(d0 & 0x8000)) {
        set8(obj + 0x23, 2);
        wr16(obj + 0x5A, (uint16_t)(d0 - 1));
        if (!bit8(obj + 0x5B, 0)) {
            wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) & 0x7FFF);
            return;
        }
    }
    wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) | 0x8000);
}

/* $04F502. Its sprite's offset cleared; its wait (+$48) by the difficulty's step. */
void Boss548Wait(uint16_t obj)
{
    wr16(obj + 0x0C, 0);
    wr16(obj + 0x48, tab16(0x04F518, rd16(RANK)));
}

/* $04F4FC. Back to walking (step $C), after a wait. */
void Boss548Rest(uint16_t obj)
{
    wr16(obj + O_STATE, 0xC);
    Boss548Wait(obj);
}

/* $04F528. Its blinking's length (by the difficulty). */
void Boss548Blink(uint16_t obj)
{
    wr16(obj + 0x5A, tab16(0x03DEB8, rd16(v_difficulty)));
}

/* $04F53A. On the floor (24 below): Z clear. */
int Boss548Floor(uint16_t obj)
{
    uint16_t d3, d4;
    return ObjectFloor(obj, 24, &d3, &d4);
}

/* $04F688. A puff of dust (type $100, $564FC) at its feet, moving at d7. */
void Boss548Dust(uint16_t obj, uint32_t d7)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x0564FC);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) + 0x18));
    wr32(slot + 0x18, d7);
}

/* $04F678. Two, either way. */
void Boss548Dusts(uint16_t obj)
{
    Boss548Dust(obj, 0xFFFF4000);
    Boss548Dust(obj, 0xC000);
}

/* loc_04F6C6: a skid's dust ($56580) at its feet, moving at d7 */
static void skid_dust(uint16_t obj, uint32_t d7)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x056580);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) + 0x18));
    wr32(slot + 0x18, d7);
}

/* $04F714. A puff of dust under the object at a1, moving at d7, rising. */
void Boss548DustAt(uint16_t at, uint32_t d7)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x0564FC);
    wr16(slot + O_X, rd16(at + O_X));
    wr16(slot + O_Y, rd16(at + O_Y));
    wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) + 0x10));
    wr32(slot + 0x18, d7);
    wr32(slot + 0x1C, 0xFFFF4000);
}

/* $04F6F4. Two under the object at a0 (the player it holds), either way. */
void Boss548DustsAt(uint16_t at)
{
    Boss548DustAt(at, 0xFFFF0000);
    Boss548DustAt(at, 0x10000);
}

/* $04F4CA. Every other frame an afterimage (type $2F8) of it. */
void Boss548Shadow(uint16_t obj)
{
    if (bit8(v_gameFrames + 1, 0)) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x2F8);
    wr16(slot + 0x48, rd16(obj + O_X));
    wr16(slot + 0x4A, rd16(obj + O_Y));
    wr16(slot + 0x52, 2);
    wr16(slot + 0x54, 2);
}

/* $04EE44. A flame ($54C) from 12 below it, its sprite word d1, +$48 d2. */
void Boss548Flame(uint16_t obj, uint16_t d1, uint16_t d2)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x54C);
    wr16(slot + 0x0E, d1);
    wr16(slot + 0x48, d2);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) + 0xC));
}

/* the moves it picks */

/* loc_04F542: a jump (step $24), hurting, half its speed across */
static void boss6_jump(uint16_t obj)
{
    wr16(obj + O_STATE, 0x24);
    wr8(obj + 0x21, 0x32);
    wr32(obj + 0x2C, 0xF010F010);
    wr16(obj + 0x26, 8);
    wr32(obj + 0x18, (uint32_t)((int32_t)rd32(obj + 0x18) >> 1));
    wr32(obj + 0x1C, 0xFFFE8000);
    wr8(obj + 0x5C, 2);
    wr32(obj + 0x08, 0x0BFC66);
    wr16(obj + 0x0C, 0);
}

/* loc_04F582: a crouch before a leap (step $28; its speed kept at +$4C) */
static void boss6_crouch(uint16_t obj)
{
    wr16(obj + O_STATE, 0x28);
    wr16(obj + 0x48, 0x10);
    wr32(obj + 0x4C, rd32(obj + 0x18));
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr8(obj + 0x5C, 2);
    wr32(obj + 0x08, 0x0BFF72);
    wr16(obj + 0x0C, 0);
    QueueSound(0xE1);
}

/* loc_04F5B8: a grab (step $38) */
static void boss6_grab(uint16_t obj)
{
    wr16(obj + O_STATE, 0x38);
    wr8(obj + 0x5C, 2);
    wr8(obj + 0x21, 0x21);
    wr32(obj + 0x2C, 0xE414E21E);
    wr16(obj + 0x40, 0);
}

/* loc_04F5E8: a charge's wind-up */
static void boss6_windup(uint16_t obj)
{
    Boss548Wait(obj);
    wr32(obj + 0x18, 0);
    wr8(obj + 0x21, 0x20);
    wr8(obj + 0x5C, 2);
    wr32(obj + 0x08, 0x0C007E);
}

/* loc_04F5D8: a dash (step $44) */
static void boss6_dash(uint16_t obj)
{
    wr16(obj + O_STATE, 0x44);
    boss6_windup(obj);
}

/* loc_04F5E2: flames (step $30) */
static void boss6_flames(uint16_t obj)
{
    wr16(obj + O_STATE, 0x30);
    boss6_windup(obj);
}

/* loc_04F606: a throw of a flame (step $40) */
static void boss6_throw(uint16_t obj)
{
    wr16(obj + O_STATE, 0x40);
    wr16(obj + 0x48, 0x18);
    wr32(obj + 0x18, 0);
    wr8(obj + 0x5C, 2);
    wr32(obj + 0x08, 0x0BFF5E);
    wr16(obj + 0x0C, 0);
}

/* loc_04F62A: a blast (step $4C) */
static void boss6_blast(uint16_t obj)
{
    wr16(obj + O_STATE, 0x4C);
    wr16(obj + 0x48, 0x2C);
    wr32(obj + 0x18, 0);
    wr8(obj + 0x21, 0x20);
    wr8(obj + 0x5C, 2);
    wr32(obj + 0x08, 0x0C0022);
    wr16(obj + 0x0C, 0);
}

/* loc_04F654: a spin up (step $50) */
static void boss6_spin(uint16_t obj)
{
    wr16(obj + O_STATE, 0x50);
    wr16(obj + 0x48, 0x20);
    wr32(obj + 0x18, 0);
    wr8(obj + 0x5C, 2);
    wr32(obj + 0x08, 0x0BFF72);
    wr16(obj + 0x0C, 0);
}

/* $04E9D0. Its set-up: off the right at the floor ($118), hidden in the scene. */
void Boss548Start(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + O_FLAGS, 0xEC00);
    wr16(obj + 0x0E, 0xEC0);
    wr16(obj + 0xDA, 0xD800);
    wr8(obj + 0x20, 0x10);
    wr16(obj + 0x50, rd16(v_gameFrames) & 0x3FF);
    wr32(obj + 0x08, 0x0BFB62);
    wr16(obj + 0x0C, 0);
    wr8(obj + 0x21, 0x20);
    wr8(obj + 0x23, 0x20);
    wr32(obj + 0x28, 0xEE0CF010);
    wr16(obj + O_X, 0x220);
    wr16(obj + O_Y, 0x118);
    wr16(obj + 0x58, 0);
    wr16(obj + 0x5A, 0);
    wr8(obj + 0x5C, 7);
    wr16(obj + 0x5E, 9);
}

/* $04EA3C. Once no message shows: walking in from the right. */
void Boss548Enter(uint16_t obj)
{
    if (rd16(MSG_STEP)) return;
    next(obj, 4);
    wr32(obj + 0x18, 0xFFFF0000);
    QueueSound(0xF6);
}

/* $04EA5A. At $168: standing $80 frames; vitality $157E, shown (message step $10), its music. */
void Boss548Arrive(uint16_t obj)
{
    if (!below(obj + O_X, 0x168)) return;
    next(obj, 4);
    wr32(obj + 0x18, 0);
    wr16(obj + 0x48, 0x80);
    wr32(obj + 0x08, 0x0C0022);
    wr16(obj + 0x0C, 0);
    wr16(obj + O_VITALITY, 0x157E);
    wr16(MSG_STEP, 0x10);
    wr16(MSG_WHO, obj);
    QueueSound(0x84);
}

/* loc_04EAA0: walking (step $10), hurtable */
static void boss6_walk(uint16_t obj)
{
    wr8(obj + 0x5C, 0);
    wr8(obj + 0x21, 0x22);
    wr32(obj + 0x28, 0xEE0CF010);
    next(obj, 4);
    wr32(obj + 0x08, 0x0BFB62);
    wr16(obj + 0x0C, 0);
}

/* $04EA98. Standing (+$48 frames), then walking. */
void Boss548Stand(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    boss6_walk(obj);
}

/* $04EAC6. Walking at the nearer player; its next move by where they both are: on the floor
 * ($118), above $F8, how far across (d0) and up (d5), its vitality (below $514), its clock
 * (from $700, from $E00). */
void Boss548Walk(uint16_t obj)
{
    Boss548Floor(obj);
    uint16_t dx, d5, a0;
    uint16_t d0 = NearestPlayer(obj, &dx, &d5, &a0);
    if (dx & 0x8000) {
        set8(obj + 0x0E, 3);
        wr32(obj + 0x18, 0xFFFF0000);
    } else {
        clr8(obj + 0x0E, 3);
        wr32(obj + 0x18, 0x10000);
    }
    uint16_t d7 = tab16(0x04ECB2, rd16(v_difficulty));
    /* cmpi.w #n,d0 then bmi: the subtraction's sign */
#define D0_BELOW(n) ((int16_t)(uint16_t)(d0 - (n)) < 0)
    int dy_far = !(d5 & 0x8000) && !((uint16_t)(d5 - 0x20) & 0x8000);
    if (below(obj + O_VITALITY, 0x514) || !below(obj + 0x50, 0xE00)) {
        if (!below(obj + O_Y, 0x118)) {
            if (below(a0 + O_Y, 0xF8)) {
                boss6_spin(obj);
                return;
            }
            if (D0_BELOW(0x20)) boss6_flames(obj);
            else if (!D0_BELOW(0x40)) boss6_dash(obj);
            else if (!D0_BELOW(0x20)) boss6_crouch(obj);
            return;
        }
        if (!below(a0 + O_Y, 0x118)) {
            if (!D0_BELOW(0x32)) boss6_dash(obj);
            else if (D0_BELOW(0x30)) boss6_jump(obj);
            return;
        }
        if (dy_far) {
            boss6_jump(obj);
            return;
        }
        if (D0_BELOW(0x30)) {
            boss6_blast(obj);
            return;
        }
        if (!D0_BELOW((uint16_t)(d7 - 0x30))) boss6_dash(obj);
        return;
    }
    if (!below(obj + 0x50, 0x700)) {
        if (!below(obj + O_Y, 0x118)) {
            if (below(a0 + O_Y, 0xF8)) {
                if (D0_BELOW(0x20)) boss6_flames(obj);
                return;
            }
            if (D0_BELOW(0x1F)) {
                boss6_flames(obj);
                return;
            }
            if (below(obj + O_X, 0x104)) {
                clr8(obj + 0x0E, 3);
                wr32(obj + 0x18, 0x15000);
                return;
            }
            if (!below(obj + O_X, 0x170)) {
                set8(obj + 0x0E, 3);
                wr32(obj + 0x18, 0xFFFEB000);
                return;
            }
            boss6_crouch(obj);
            return;
        }
        if (!below(a0 + O_Y, 0x118)) {
            boss6_throw(obj);
            return;
        }
        if (D0_BELOW(0x1F)) {
            boss6_grab(obj);
            return;
        }
        if (!(rd16(v_randomSeed) & 3)) boss6_throw(obj);
        else boss6_jump(obj);
        return;
    }
    if (!below(obj + O_Y, 0x118)) {
        if (!(rd16(v_gameFrames) & 0x3FF)) {
            boss6_blast(obj);
            return;
        }
        if (!below(a0 + O_Y, 0xF8)) {
            if (!(rd16(v_gameFrames) & 0xFF)) {
                boss6_throw(obj);
                return;
            }
            goto near;
        }
        if (!D0_BELOW(0x20)) return;
        if (!bit8(0xFF09, 0)) boss6_crouch(obj);
        else boss6_flames(obj);
        return;
    }
    if (!below(a0 + O_Y, 0x118)) {
        if (!bit8(0xFF09, 1)) {
            if (D0_BELOW(0x58)) boss6_jump(obj);
            return;
        }
        if (D0_BELOW(0x48) || !(rd16(v_gameFrames) & 0x7F)) boss6_jump(obj);
        else boss6_throw(obj);
        return;
    }
    if (!D0_BELOW(0x20)) goto far;
    if (dy_far) {
        boss6_jump(obj);
        return;
    }
near:
    if (D0_BELOW(0x1F)) {
        boss6_grab(obj);
        return;
    }
far:
    if (!D0_BELOW(d7)) boss6_dash(obj);
#undef D0_BELOW
}

/* $04ECBA. Knocked up: falling to the floor; then (the sprites shaking) resting. */
void Boss548Knocked(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    if (!Boss548Floor(obj)) return;
    wr16(v_shakeSprites, 4);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr32(obj + 0x08, 0x0BFF72);
    Boss548Rest(obj);
}

/* $04ECE8. Flinching (+$58 frames), then walking. */
void Boss548Flinch(uint16_t obj)
{
    if (!dec(obj + 0x58)) return;
    wr16(obj + O_STATE, 0xC);
    boss6_walk(obj);
}

/* $04ECFA. Held by a player (+$23 bit 1, cleared each frame); let go: thrown. */
void Boss548Held(uint16_t obj)
{
    int held = bit8(obj + 0x23, 1);
    clr8(obj + 0x23, 1);
    if (held) return;
    next(obj, 4);
}

/* $04ED0A. Thrown (facing back): landing it loses $1A4 vitality and is knocked up again. */
void Boss548Thrown(uint16_t obj)
{
    if (!bit8(obj + 0x18, 7)) set8(obj + 0x0E, 3);
    else clr8(obj + 0x0E, 3);
    add32(obj + 0x1C, 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    if (!Boss548Floor(obj)) return;
    Boss548Blink(obj);
    wr16(obj + 0x5E, 0x14);
    wr16(obj + O_STATE, 0x14);
    wr16(obj + O_VITALITY, (uint16_t)(rd16(obj + O_VITALITY) - 0x1A4));
    wr32(obj + 0x08, 0x0BFC96);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x18, (uint32_t)((int32_t)rd32(obj + 0x18) >> 2));
    wr32(obj + 0x1C, 0xFFFD8000);
    wr16(v_shakeSprites, 8);
    QueueSound(0xEF);
}

/* loc_04ED9E: landed: walking again after $10 frames, dust either way */
static void boss6_land(uint16_t obj)
{
    wr16(obj + O_STATE, 0xC);
    wr8(obj + 0x21, 0x22);
    wr16(obj + 0x48, 0x10);
    wr32(obj + 0x08, 0x0BFF72);
    wr16(obj + 0x0C, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    Boss548Dusts(obj);
}

/* $04ED7C. Jumping: falling to the floor's line ($118). */
void Boss548Jump(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    if (below(obj + O_Y, 0x118)) return;
    wr16(obj + O_Y, 0x118);
    wr16(v_shakeSprites, 0xC);
    boss6_land(obj);
}

/* $04EDC8. Crouched (+$48 frames), then leaping (its kept speed across). */
void Boss548Crouch(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 4);
    wr32(obj + 0x18, rd32(obj + 0x4C));
    wr32(obj + 0x1C, 0xFFF98000);
    wr32(obj + 0x08, 0x0BFCF6);
    wr16(obj + 0x0C, 0);
}

/* $04EDF0. Leaping: falling to a floor. */
void Boss548Leap(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    if (!Boss548Floor(obj)) return;
    wr16(v_shakeSprites, 6);
    boss6_land(obj);
}

/* $04EE0E. Winding up (the sprites shaking); then two flames, either way. */
void Boss548Flames(uint16_t obj)
{
    wr16(v_shakeSprites, 2);
    if (!dec(obj + 0x48)) return;
    next(obj, 4);
    wr32(obj + 0x08, 0x0BFBF6);
    wr16(obj + 0x0C, 0);
    wr16(obj + 0x48, 0x40);
    Boss548Flame(obj, 0x6380, 0);
    Boss548Flame(obj, 0x380, 0x100);
}

/* $04EE6E. $40 frames turning about (frame bit 2); then resting. */
void Boss548Turning(uint16_t obj)
{
    if (dec(obj + 0x48)) {
        Boss548Rest(obj);
        wr32(obj + 0x08, 0x0BFB06);
        wr8(obj + 0x5C, 0);
        return;
    }
    if (bit8(v_gameFrames + 1, 2)) set8(obj + 0x0E, 3);
    else clr8(obj + 0x0E, 3);
}

/* $04EEA4. Grabbing: a player caught (touching it, +$40) is held; else (by the difficulty's
 * step) a crouch or a blast. */
void Boss548Grab(uint16_t obj)
{
    uint16_t d0;
    if (!bit8(obj + 0x22, 0) || !(d0 = rd16(obj + 0x40))) {
        wr8(obj + 0x21, 0x22);
        wr8(obj + 0x5C, 0);
        wr16(obj + O_FLAGS, 0xEC00);
        if (below(RANK, 6)) boss6_crouch(obj);
        else boss6_blast(obj);
        return;
    }
    next(obj, 4);
    wr8(obj + 0x21, 0);
    set8(d0 + 0x23, 1);
    wr16(d0 + O_X, rd16(obj + O_X));
    wr16(d0 + O_Y, rd16(obj + O_Y));
    wr16(obj + 0x48, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    QueueSound(0xDF);
    clr8(obj + 0x0E, 3);
}

/* $04EF12. Swinging the held player (+$40) about for $68 frames (a slam and dust every $20);
 * then throwing it away from the middle. */
void Boss548Swing(uint16_t obj)
{
    uint16_t p = rd16(obj + 0x40);
    set8(p + 0x23, 1);
    wr32(p + 0x18, 0);
    wr32(p + 0x1C, 0);
    wr16(obj + O_FLAGS, 0xCC00);
    add16(obj + 0x48, 1);
    uint16_t d7 = rd16(obj + 0x48);
    if ((int16_t)(d7 - 0x68) < 0) {
        set8(obj + 0x0E, 3);
        clr8(p + 0x0E, 3);
        if (!(d7 & 0x20)) {
            clr8(obj + 0x0E, 3);
            set8(p + 0x0E, 3);
        }
        d7 &= 0x1F;
        if (!d7) {
            wr16(v_shakeSprites, 0xC);
            Boss548DustsAt(p);
        }
        d7 &= 0x18;
        wr32(obj + 0x08, rom32(0x04F024 + d7));
        uint16_t d1 = rom16(0x04F024 + d7 + 4);
        if (!bit8(obj + 0x0E, 3)) d1 = (uint16_t)-d1;
        wr16(p + O_X, (uint16_t)(d1 + rd16(obj + O_X)));
        wr16(p + O_Y, (uint16_t)(rd16(obj + O_Y) + rom16(0x04F024 + d7 + 6)));
        return;
    }
    wr16(obj + O_STATE, 0xC);
    wr16(obj + 0x40, 0);
    wr8(obj + 0x21, 0x22);
    wr8(obj + 0x5C, 0);
    wr16(obj + O_FLAGS, 0xEC00);
    wr16(p + O_X, rd16(obj + O_X));
    wr16(p + O_Y, rd16(obj + O_Y));
    wr16(p + O_Y, (uint16_t)(rd16(p + O_Y) - 0x10));
    set8(p + 0x23, 4);
    clr8(p + 0x23, 1);
    wr32(obj + 0x08, 0x0BFD56);
    wr16(obj + 0x0C, 0);
    wr16(obj + 0x48, 0x30);
    wr32(p + 0x1C, 0xFFFB8000);
    if (!below(obj + O_X, 0x120)) {
        set8(obj + 0x0E, 3);
        wr32(p + 0x18, 0xFFFB8000);
        return;
    }
    clr8(obj + 0x0E, 3);
    wr32(p + 0x18, 0x48000);
}

/* $04F044. +$48 frames; then a thrown flame ($558, right; turned the other way when facing
 * left), walking again after $40. */
void Boss548Throw(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    wr16(obj + O_STATE, 0xC);
    wr16(obj + 0x48, 0x40);
    wr8(obj + 0x5C, 0);
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x558);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr16(slot + O_X, (uint16_t)(rd16(slot + O_X) + 0x10));
    if (!bit8(obj + 0x0E, 3)) return;
    wr16(slot + 0x48, 0x100);
    wr16(slot + O_X, (uint16_t)(rd16(slot + O_X) - 0x20));
}

/* $04F096. +$48 frames; then a dash at the player it goes after (+$48), hurting, at a speed by
 * the difficulty. */
void Boss548DashStart(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    QueueSound(0xB9);
    next(obj, 4);
    wr32(obj + 0x08, 0x0BFBF6);
    wr16(obj + 0x0C, 0);
    wr16(BOSS6_DASH, 1);
    wr32(BOSS6_RISE, 0);
    wr16(obj + 0x26, 7);
    wr8(obj + 0x21, 0x30);
    wr32(obj + 0x2C, 0xF010F010);
    uint16_t target = PlayerTarget();
    wr16(obj + 0x48, target);
    clr8(obj + 0x0E, 3);
    wr32(obj + 0x18, rom32(0x04F10A + (uint32_t)(int16_t)(uint16_t)(rd16(v_difficulty) << 1)));
    if ((int16_t)(rd16(target + O_X) - rd16(obj + O_X)) >= 0) return;
    set8(obj + 0x0E, 3);
    wr32(obj + 0x18, 0u - rd32(obj + 0x18));
}

/* $04F11A. Dashing (afterimages) until past its player or at the room's edge: then skidding
 * (dust) and resting. */
void Boss548Dash(uint16_t obj)
{
    wr16(v_shakeSprites, 1);
    Boss548Floor(obj);
    AfterImage(obj);
    uint16_t a0 = rd16(obj + 0x48);
    uint16_t d0 = rd16(a0 + O_X);
    if (!bit8(obj + 0x18, 7)) {
        wr32(BOSS6_MOVE, 0xFFFC0000);
        if (below(obj + O_X, 0x1B8) && !((uint16_t)(d0 - rd16(obj + O_X)) & 0x8000)) {
            skid_dust(obj, 0xFFFE0000);
            return;
        }
    } else {
        wr32(BOSS6_MOVE, 0x40000);
        if (!below(obj + O_X, 0x88) && ((uint16_t)(d0 - rd16(obj + O_X)) & 0x8000)) {
            skid_dust(obj, 0x20000);
            return;
        }
    }
    wr16(v_shakeSprites, 0xA);
    wr8(obj + 0x5C, 0);
    wr32(obj + 0x08, 0x0BFB06);
    wr8(obj + 0x21, 0x22);
    wr16(BOSS6_DASH, 0);
    wr32(obj + 0x18, 0);
    Boss548Rest(obj);
}

/* $04F366. Winding up (afterimages); then a blast ($560 at $DE80, under it). */
void Boss548Blast(uint16_t obj)
{
    Boss548Shadow(obj);
    if (!dec(obj + 0x48)) return;
    wr16(v_shakeSprites, 0x10);
    QueueSound(0xBD);
    wr8(obj + 0x5C, 0);
    wr8(obj + 0x21, 0x22);
    wr32(obj + 0x08, 0x0BFF72);
    Boss548Rest(obj);
    add16(obj + 0x48, 8);
    wr16(0xDE80 + O_TYPE, 0x560);
    wr16(0xDE80 + O_STATE, 0);
    wr16(0xDE80 + O_X, rd16(obj + O_X));
    wr16(0xDE80 + O_Y, rd16(obj + O_Y));
    wr16(0xDE80 + O_Y, (uint16_t)(rd16(0xDE80 + O_Y) + 0xC));
}

/* $04F19E. Spinning up (dust every 8th frame, turning about); then a leap up, hurting. */
void Boss548Spin(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 7)) Boss548Dusts(obj);
    if (bit8(v_gameFrames + 1, 3)) set8(obj + 0x0E, 3);
    else clr8(obj + 0x0E, 3);
    wr16(v_shakeSprites, 2);
    if (!dec(obj + 0x48)) return;
    QueueSound(0xCA);
    next(obj, 4);
    wr32(obj + 0x08, 0x0BFBF6);
    wr16(obj + 0x0C, 0);
    wr16(BOSS6_DASH, 1);
    wr32(BOSS6_MOVE, 0);
    wr32(BOSS6_RISE, 0x38000);
    wr16(obj + 0x26, 9);
    wr8(obj + 0x21, 0x30);
    wr32(obj + 0x2C, 0xF810F010);
    wr32(obj + 0x1C, 0xFFF90000);
}

/* $04F220. Rising (afterimages) until it falls: then (below difficulty 4) a jump at the
 * player, or a dive at it. */
void Boss548Rise(uint16_t obj)
{
    AfterImage(obj);
    add32(obj + 0x1C, 0x4000);
    if (rd32(obj + 0x1C) & 0x80000000u) return;
    if (!below(v_difficulty, 4)) {
        next(obj, 4);
        add16(obj + 0x26, 2);
        wr32(BOSS6_RISE, 0);
        uint16_t target = PlayerTarget();
        uint32_t d1 = (uint16_t)(rd16(target + O_Y) - rd16(obj + O_Y));
        uint16_t d2 = Atan2((int16_t)(rd16(target + O_X) - rd16(obj + O_X)), &d1);
        d2 = (uint16_t)((int16_t)d2 >> 7) & 0x1FE;
        wr32(obj + 0x1C, (uint32_t)sine(d2) << 5);
        wr32(obj + 0x18, (uint32_t)cosine(d2) << 5);
        add32(obj + 0x1C, (uint32_t)-0x38000);
        if (rd16(obj + 0x18) & 0x8000) set8(obj + 0x0E, 3);
        else clr8(obj + 0x0E, 3);
        return;
    }
    wr16(obj + O_STATE, 0x2C);
    wr16(BOSS6_DASH, 0);
    wr32(obj + 0x08, 0x0BFD26);
    wr16(obj + 0x0C, 0);
    wr8(obj + 0x21, 0x22);
    wr32(obj + 0x1C, 0);
    if ((int16_t)(rd16(PlayerTarget() + O_X) - rd16(obj + O_X)) < 0) {
        wr32(obj + 0x18, 0xFFFF0000);
        set8(obj + 0x0E, 3);
        return;
    }
    wr32(obj + 0x18, 0x10000);
    clr8(obj + 0x0E, 3);
}

/* $04F300. Diving (afterimages; falling to 7 at most) to a floor: then a leap's landing. */
void Boss548Dive(uint16_t obj)
{
    AfterImage(obj);
    if ((rd16(obj + 0x1C) & 0x8000) || (int32_t)(rd32(obj + 0x1C) - 0x70000u) < 0) {
        add32(obj + 0x1C, 0x4000);
        if (rd32(obj + 0x1C) & 0x80000000u) return;
    }
    if (!Boss548Floor(obj)) return;
    wr16(obj + O_STATE, 0x2C);
    wr16(BOSS6_DASH, 0);
    wr32(obj + 0x1C, 0xFFFD8000);
    wr32(obj + 0x18, (uint32_t)((int32_t)rd32(obj + 0x18) >> 2));
    wr32(obj + 0x08, 0x0BFCF6);
    wr16(obj + 0x0C, 0);
    wr16(v_shakeSprites, 0xA);
    QueueSound(0xCE);
}

/* $04F3BE. Beaten: shaking in place (+$4A its y) for +$48 frames; then leaping up and about. */
void Boss548Beaten(uint16_t obj)
{
    uint16_t d0 = rd16(obj + 0x4A);
    if (!bit8(v_gameFrames + 1, 1)) d0 = (uint16_t)(d0 + 4);
    wr16(obj + O_Y, d0);
    if (!dec(obj + 0x48)) return;
    next(obj, 4);
    wr16(obj + 0x48, 0x13);
    wr32(obj + 0x08, 0x0BFCC6);
    wr16(obj + 0x0C, 0);
    wr16(obj + 0x1C, 0xFFF9);
    QueueSound(0xA3);
    if (below(obj + O_X, 0x120)) {
        clr8(obj + 0x0E, 3);
        wr16(obj + 0x18, 5);
        return;
    }
    set8(obj + 0x0E, 3);
    wr16(obj + 0x18, 0xFFFB);
}

/* $04F426. Bouncing about the room's edges (puffs, the sprites shaking) +$48 times; then
 * gone, with every object from $CBC0. */
void Boss548Bounce(uint16_t obj)
{
    wr16(v_shakeSprites, 8);
    PuffTrail(obj);
    if (rd16(obj + 0x48) & 0x8000) {
        QueueSound(0xA3);
        wr16(v_shakeSprites, 0x10);
        set8(obj + O_FLAGS, 4);
        Stage6Clear(0, 0);
        return;
    }
    if (below(obj + O_Y, 0x80)) {
        wr16(obj + 0x1C, 7);
        set8(obj + 0x0E, 4);
        add16(obj + 0x48, 0xFFFF);
        return;
    }
    if (!below(obj + O_Y, 0x140)) {
        wr16(obj + 0x1C, 0xFFF9);
        clr8(obj + 0x0E, 4);
        add16(obj + 0x48, 0xFFFF);
        return;
    }
    if (below(obj + O_X, 0x7C)) {
        wr16(obj + 0x18, 5);
        set8(obj + 0x0E, 3);
        add16(obj + 0x48, 0xFFFF);
        return;
    }
    if (below(obj + O_X, 0x1C4)) return;
    wr16(obj + 0x18, 0xFFFB);
    clr8(obj + 0x0E, 3);
    add16(obj + 0x48, 0xFFFF);
}

/* $04E7CE. Its clock (+$50) on; beaten (not while +$5C bit 0): points, step $5C. Kept in the
 * room (unless +$5C bit 2). Hit by a shot (+$22 bit 5) while not busy and not flinching: after
 * +$5E hits knocked up (blinking), else flinching. Grabbed and let go by a player (+$23 bit 1,
 * +$42 the player): held. Then its step. */
void Boss548Run(uint16_t obj)
{
    add16(obj + 0x50, 1);
    if (!below(obj + 0x50, 0x1600)) wr16(obj + 0x50, 0);
    if (!bit8(obj + 0x5C, 0) && (rd16(obj + O_VITALITY) & 0x8000)) {
        AddScore(obj, 0x54109);
        wr16(obj + O_STATE, 0x5C);
        wr16(obj + O_FLAGS, 0xEC00);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr32(obj + 0x08, 0x0BFC96);
        wr16(obj + 0x0C, 0);
        wr16(obj + 0x48, 0x30);
        wr16(obj + 0x4A, rd16(obj + O_Y));
        wr8(obj + 0x5C, 7);
        wr8(obj + 0x21, 0);
        wr16(obj + O_VITALITY, 0);
    } else {
        if (!bit8(obj + 0x5C, 2)) {
            if (below(obj + O_X, 0x88)) wr16(obj + O_X, 0x8A);
            else if (!below(obj + O_X, 0x1B8)) wr16(obj + O_X, 0x1B6);
        }
        if (bit8(obj + 0x22, 5) && !bit8(obj + 0x5C, 1)
            && (!rd16(obj + 0x58) || (rd16(obj + 0x58) & 0x8000))) {
            wr8(obj + 0x5C, 2);
            wr16(obj + 0x0C, 0);
            int knocked = bit8(obj + 0x23, 4);
            clr8(obj + 0x23, 4);
            if (!knocked && !dec(obj + 0x5E)) {
                wr16(obj + O_STATE, 0x18);
                wr16(obj + 0x58, 3);
                wr32(obj + 0x08, 0x0BFC96);
                wr32(obj + 0x18, 0);
                wr32(obj + 0x1C, 0);
            } else {
                QueueSound(0xEF);
                wr16(obj + O_STATE, 0x14);
                Boss548Blink(obj);
                wr16(obj + 0x58, 0);
                wr16(obj + 0x5E, 0xC);
                wr32(obj + 0x08, 0x0BFCC6);
                wr32(obj + 0x1C, 0xFFFD8000);
                wr32(obj + 0x18, bit8(obj + 0x0E, 3) ? 0xFFFF4000 : 0xC000);
            }
        } else {
            int grabbed = bit8(obj + 0x23, 1);
            clr8(obj + 0x23, 1);
            uint16_t d0 = rd16(obj + 0x42);
            if (grabbed && (d0 == 0xA400 || d0 == 0xA600)) {
                wr16(obj + O_STATE, 0x1C);
                wr16(obj + 0x58, 0);
                wr8(obj + 0x5C, 2);
                wr32(obj + 0x08, 0x0BFCC6);
                wr16(obj + 0x0C, 0);
                wr32(obj + 0x18, 0);
                wr32(obj + 0x1C, 0);
                wr8(obj + 0x21, 0);
                wr16(obj + 0x48, 0x10);
            }
        }
    }
    call_object(rom32(0x04E96C + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

/* $04E7C4. Type $548: its frame, then its blinking. */
void Boss548(uint16_t obj)
{
    Boss548Run(obj);
    boss_blink(obj);
}

/* ---- type $398: stage 6's director (in the boss's slot): by the planes' x it holds the camera
 * for each fight, shows the messages, loads the tiles, sends in the fights' objects ($CB60, the
 * one at $A800) and the allies' steps; its step goes by 2 (a table of offsets from $4FBBA) ---- */

#define DIRECTOR6 0x04FBBAu

/* $04FFFE. The four allies, and the fifth at $A800 (type $3D4). */
void Stage6AlliesStart(void)
{
    Stage6Allies(3);
    wr16(0xA800 + O_TYPE, 0x3D4);
    wr16(0xA800 + O_FLAGS, 0xE080);
    wr16(0xA800 + 0x0E, 0x8EC0);
    wr16(0xA800 + O_STATE, 0);
    wr32(0xA800 + 0x08, 0x0BFB06);
    wr16(0xA800 + 0x0C, 0);
    wr16(0xA800 + 0xDA, 0xD800);
    wr16(0xA800 + O_X, 0x168);
    wr16(0xA800 + O_Y, 0x150);
    wr8(0xA800 + 6, 0);
}

/* loc_0500BC: the object at a0 made a d0, everything else cleared */
static void stage6_object(uint16_t a0, uint16_t type)
{
    wr16(a0 + O_TYPE, type);
    wr16(a0 + O_STATE, 0);
    wr16(a0 + O_FLAGS, 0);
    wr8(a0 + 0x21, 0);
    wr16(a0 + O_VITALITY, 0);
    wr32(a0 + 0x18, 0);
    wr32(a0 + 0x1C, 0);
}

/* the fight's object at $CB60 */
static void stage6_fight(uint16_t type)
{
    stage6_object(0xCB60, type);
}

/* tiles loaded from a job list */
static void tiles(uint32_t list)
{
    NextJob(&list);
}

/* a hold at the planes' x: the camera held, a message */
static void stage6_hold(uint16_t obj, uint16_t step, uint32_t script)
{
    add16(obj + O_STATE, 2);
    set8(v_cameraHold, 0);
    wr16(MSG_STEP, step);
    wr32(MSG_SCRIPT, script);
}

/* $04FBBA. Step 0: the allies set up and started, the first objects ($51828). */
void Stage6Dir00(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    wr16(obj + 0x5E, 0);
    Stage6AlliesStart();
    Stage6AlliesStep0();
    SpawnFromList(0x051828, 0xFFFF);
}

/* $04FBD6. (Not reached: step 0 goes to 4.) It becomes a $5B4, every object from it gone. */
void Stage6Dir02(uint16_t obj)
{
    wr16(obj + O_TYPE, 0x5B4);
    wr16(obj + O_STATE, 0);
    ObjectsVanishFrom(obj, 0, 0);
}

/* $04FBEA. At x $2D0: held, the message at $39AF0, the allies' fight steps. */
void Stage6Dir04(uint16_t obj)
{
    if (below(v_planes, 0x2D0)) return;
    stage6_hold(obj, 8, 0x039AF0);
    Stage6AlliesStep2();
}

/* $04FC12. Held: the fight ($39C at $CB60). */
void Stage6Dir06(uint16_t obj)
{
    if (!bit8(v_cameraHold, 7)) return;
    add16(obj + O_STATE, 2);
    stage6_fight(0x39C);
}

/* $04FC28. The fight over: on, its tiles ($50E82). */
void Stage6Dir08(uint16_t obj)
{
    if (rd16(0xCB60)) return;
    add16(obj + O_STATE, 2);
    clr8(v_cameraHold, 7);
    wr16(0xC9E4, 0x16);
    tiles(0x050E82);
}

/* loc_04FC7C: the soldiers from the list at $4FCC6 (+$4C) as the planes reach their x: type
 * $578 plus the high byte, y $80 plus the low, x by the planes */
static void stage6_soldiers(uint16_t obj)
{
    uint16_t d1 = rd16(obj + 0x4C);
    uint16_t d2 = (uint16_t)(tab16(0x04FCC6, d1) - rd16(v_planes));
    if (!(d2 & 0x8000)) return;
    add16(obj + 0x4C, 4);
    d2 = (uint16_t)(d2 + 0x1D0);
    uint16_t slot, t;
    if (!SpawnSlot(&slot, &t)) return;
    d1 = (uint16_t)(d1 + 2);
    uint16_t d3 = tab16(0x04FCC6, d1);
    wr16(slot + O_TYPE, (uint16_t)((d3 >> 8) + 0x578));
    wr16(slot + O_Y, (uint16_t)((d3 & 0xFF) + 0x80));
    wr16(slot + O_X, d2);
}

/* loc_050F02: seven of type $3B0 from the left, $40 apart up from $108 (their +$4A from
 * $50F3A) */
static void stage6_column(void)
{
    uint16_t d1 = (uint16_t)(0xA54 - rd16(v_planes)), d2 = 0x108;
    uint32_t a1 = 0x050F3A;
    for (int i = 0; i < 7; i++) {
        uint16_t slot, t;
        if (!SlotFreeHigh(&slot, &t)) return;
        wr16(slot + O_TYPE, 0x3B0);
        wr16(slot + O_X, d1);
        wr16(slot + O_Y, d2);
        wr16(slot + 0x4A, rom16(a1));
        a1 += 2;
        d2 = (uint16_t)(d2 - 0x40);
    }
}

/* $04FC5E. The soldiers as the planes go; at x $870 held (message step $60), a column. */
void Stage6Dir0C(uint16_t obj)
{
    if (below(v_planes, 0x870)) {
        stage6_soldiers(obj);
        return;
    }
    add16(obj + O_STATE, 2);
    set8(v_cameraHold, 0);
    wr16(MSG_STEP, 0x60);
    stage6_column();
}

/* $04FC56. The soldiers' list from the start. */
void Stage6Dir0A(uint16_t obj)
{
    add16(obj + O_STATE, 2);
    wr16(obj + 0x4C, 0);
    Stage6Dir0C(obj);
}

/* $04FCEC. Until the camera's step $C: then on. */
void Stage6Dir0E(uint16_t obj)
{
    if (below(v_cameraStep, 0xC)) return;
    add16(obj + O_STATE, 2);
    clr8(v_cameraHold, 0);
}

/* $04FD02. At x $A60: held, the message at $39B04. */
void Stage6Dir10(uint16_t obj)
{
    if (below(v_planes, 0xA60)) return;
    stage6_hold(obj, 0x34, 0x039B04);
    Stage6AlliesStep2();
}

/* $04FD2A. Held: the fight ($58C at $CB60). */
void Stage6Dir12(uint16_t obj)
{
    if (!bit8(v_cameraHold, 7)) return;
    add16(obj + O_STATE, 2);
    stage6_fight(0x58C);
}

/* $04FD40. The fight over: $40 frames. */
void Stage6Dir14(uint16_t obj)
{
    if (rd16(0xCB60)) return;
    add16(obj + O_STATE, 2);
    wr16(obj + 0x48, 0x40);
    wr16(0xC9E4, 0x30);
}

/* $04FD5A. Then on (message step $58), the objects at $5183C (d7 + 1 at most: d7 as the 68k's
 * dispatcher leaves it). */
void Stage6Dir16(uint16_t obj, uint16_t d7)
{
    if (!dec(obj + 0x48)) return;
    add16(obj + O_STATE, 2);
    clr8(v_cameraHold, 7);
    wr16(MSG_STEP, 0x58);
    SpawnFromList(0x05183C, d7);
}

/* $04FD7C. At x $F50: held, the message at $39B1C. */
void Stage6Dir18(uint16_t obj)
{
    if (below(v_planes, 0xF50)) return;
    stage6_hold(obj, 0x34, 0x039B1C);
    Stage6AlliesStep2();
}

/* $04FDA4. Held: tiles ($4FDC8). */
void Stage6Dir1A(uint16_t obj)
{
    if (!bit8(v_cameraHold, 7)) return;
    add16(obj + O_STATE, 2);
    tiles(0x04FDC8);
}

/* $04FDD2. The tiles in: the shrinking from $1F, its tiles to VRAM $7000. */
void Stage6Dir1C(uint16_t obj)
{
    if (rd16(v_jobKind)) return;
    add16(obj + O_STATE, 2);
    wr16(obj + 0x48, 0x7000);
    wr16(obj + 0x4A, 0x1F);
}

/* $04FDEC. A shrunk copy of the tiles at $9000 (its size +$4A) to the VRAM at +$48, each frame
 * smaller ($200 on), down to $10; then the boss ($548 at $A800). */
void Stage6Dir1E(uint16_t obj)
{
    wr16(0x8040, rd16(obj + 0x4A));
    ScaleTiles(0x9200, 0x9000, rd16(obj + 0x48));
    add16(obj + 0x48, 0x200);
    add16(obj + 0x4A, 0xFFFF);
    if (!below(obj + 0x4A, 0x10)) return;
    add16(obj + O_STATE, 2);
    stage6_object(0xA800, 0x548);
}

/* $04FE30. The boss gone: on (message step $58), its tiles ($50E8C), the objects at $51868. */
void Stage6Dir20(uint16_t obj)
{
    if (rd16(0xA800)) return;
    add16(obj + O_STATE, 2);
    clr8(v_cameraHold, 7);
    wr16(MSG_STEP, 0x58);
    wr16(0xC9E4, 0x32);
    tiles(0x050E8C);
    SpawnFromList(0x051868, 0xFFFF);
}

/* $04FE6C. At x $1280: held, the message at $39B30, tiles ($50E56). */
void Stage6Dir22(uint16_t obj)
{
    if (below(v_planes, 0x1280)) return;
    stage6_hold(obj, 0x34, 0x039B30);
    tiles(0x050E56);
}

/* $04FE98. Held: $80 frames; a $544 at $DFA0 (the gate). */
void Stage6Dir24(uint16_t obj)
{
    if (!bit8(v_cameraHold, 7)) return;
    add16(obj + O_STATE, 2);
    wr16(obj + 0x48, 0x80);
    wr16(0xDFA0 + O_TYPE, 0x544);
    wr16(0xDFA0 + O_FLAGS, 0x80);
    wr8(0xDFA0 + 0x21, 4);
    wr8(0xDFA0 + 0x46, 0x8F);
    wr32(0xDFA0 + 0x2C, 0xE002A060);
    wr16(0xDFA0 + O_X, 0x120);
    wr16(0xDFA0 + O_Y, 0x140);
}

/* $04FEDC. Then the fight ($3A8 at $CB60). */
void Stage6Dir26(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    add16(obj + O_STATE, 2);
    stage6_fight(0x3A8);
}

/* $04FEF0. The fight over: $E0 frames, tiles ($50E70). */
void Stage6Dir28(uint16_t obj)
{
    if (rd16(0xCB60)) return;
    add16(obj + O_STATE, 2);
    wr16(obj + 0x48, 0xE0);
    tiles(0x050E70);
    wr16(0xC9E4, 0x26);
}

/* $04FF1E. Then on. */
void Stage6Dir2A(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    add16(obj + O_STATE, 2);
    clr8(v_cameraHold, 7);
}

/* $04FF32. At the camera's step $2C: the gate shown, message step $58, a sound; the fight
 * ($564 at $CB60). */
void Stage6Dir2C(uint16_t obj)
{
    if (below(v_cameraStep, 0x2C)) return;
    add16(obj + O_STATE, 2);
    wr16(0xDFA0 + O_FLAGS, 0x180);
    wr16(MSG_STEP, 0x58);
    wr16(0xA010, 0x10);
    QueueSound(0xA6);
    stage6_fight(0x564);
}

/* $04FF68. At x $13F0: held, the message at $39B44; the gate removed. */
void Stage6Dir2E(uint16_t obj)
{
    if (below(v_planes, 0x13F0)) return;
    stage6_hold(obj, 0x34, 0x039B44);
    Stage6AlliesStep2();
    wr16(0xDFA0 + O_FLAGS, 0x1000);
}

/* $04FF9A. Until the fight at $CB60 is over. */
void Stage6Dir30(uint16_t obj)
{
    if (!rd16(0xCB60)) add16(obj + O_STATE, 2);
}

/* $04FFA8. $20 frames; tiles ($50E96). */
void Stage6Dir32(uint16_t obj)
{
    add16(obj + O_STATE, 2);
    wr16(obj + 0x48, 0x20);
    tiles(0x050E96);
}

/* $04FFC8. Then on, the last message (step $5C at $170, $100), three chunks of plane A. */
void Stage6Dir34(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    add16(obj + O_STATE, 2);
    wr16(0xC9E4, 0x3A);
    clr8(v_cameraHold, 7);
    wr16(MSG_STEP, 0x5C);
    wr16(0x80D2, 0x170);
    wr16(0x80D4, 0x100);
    wr16(MSG_COUNT, 0x7FFF);
    PlaneAChunk(0xFFFF58E0, 0xC658);
    PlaneAChunk(0xFFFF5920, 0xC858);
    PlaneAChunk(0xFFFF59A0, 0xCA58);
}

/* $04FFFC. Nothing. */
void Stage6Dir36(uint16_t obj)
{
    (void)obj;
}

/* $04FB72. Type $398. Step $16 reads d7 as the 68k's dispatcher leaves it (the previous
 * object's): the C passes $FFFF (no limit: the list's end mark stops it). */
void Stage6Director(uint16_t obj)
{
    uint32_t fn = DIRECTOR6 + (uint32_t)(int16_t)tab16(0x04FB82, rd16(obj + O_STATE));
    if (fn == 0x04FD5A) Stage6Dir16(obj, 0xFFFF);
    else call_object(fn, obj);
}

/* ---- shared helpers ---- */

/* $04BF98. A free slot among the last four ($DE80-$DFA0): Z set and the slot. */
int SlotFreeLast(uint16_t *slot)
{
    uint16_t t;
    *slot = 0xDE80;
    return SlotFree4(slot, &t);
}

/* $04C3B8. Shot down: (from difficulty 4) a shot at the player; it becomes a wreck (type $44C:
 * the burst's animation at $563EE first), harmless. */
void SpaceWreck(uint16_t obj)
{
    if (!below(v_difficulty, 4)) {
        uint16_t slot;
        SpawnTowardPlayer(obj, &slot);
    }
    wr16(obj + O_TYPE, 0x44C);
    wr32(obj + 0x48, 0x0563EE);
    wr16(obj + O_STATE, 0);
    wr8(obj + 0x21, 0);
    set8(obj + O_FLAGS, 3);
    QueueSound(0xBE);
}

/* $04C11C. The angle to the player it goes after (an even index of 256). */
uint16_t SpaceAim(uint16_t obj)
{
    uint16_t target = PlayerTarget();
    uint32_t d1 = (uint16_t)(rd16(target + O_Y) - rd16(obj + O_Y));
    uint16_t a = Atan2((int16_t)(rd16(target + O_X) - rd16(obj + O_X)), &d1);
    return (uint16_t)(((int16_t)a >> 7) & 0x1FE);
}

/* $04C142. The object at `part` made its gun (type $10, shown), kept at +$4E. */
void SpaceArmPart(uint16_t obj, uint16_t part)
{
    wr16(part + O_TYPE, 0x10);
    wr16(part + O_FLAGS, 0x8000);
    wr8(part + 0x20, 0xB8);
    wr16(obj + 0x4E, part);
}

/* $04C01C. Its gun (+$4E): the look for the arm's angle (+$56: 8 of them, flipped from $F0 to
 * $1F0), placed at +$54 along it; once the arm is out ($16), the angle turned by 2 toward the
 * player's (d7), unless within 4. */
void SpaceArm(uint16_t obj, uint16_t d7)
{
    uint16_t gun = rd16(obj + 0x4E);
    uint32_t a = 0x04C0DC + (uint32_t)(((rd16(obj + 0x56) + 0x10) & 0xE0) >> 2);
    wr16(gun + 0x0E, rom16(a));
    wr16(gun + 0x08, rom16(a + 2));
    wr16(gun + 0x0A, rom16(a + 4));
    if (!below(obj + 0x56, 0xF0) && below(obj + 0x56, 0x1F0)) wr16(gun + 0x0E, rd16(gun + 0x0E) ^ 0x1800);
    uint16_t d0 = rd16(obj + 0x56);
    int32_t len = (int16_t)rd16(obj + 0x54);
    uint32_t d1 = (uint32_t)(sine(d0) * len) << 2;
    uint32_t d2 = (uint32_t)(cosine(d0) * len) << 2;
    wr16(gun + O_Y, (uint16_t)((d1 >> 16) + rd16(obj + O_Y)));
    wr16(gun + O_X, (uint16_t)((d2 >> 16) + rd16(obj + O_X)));
    if (below(obj + 0x54, 0x16)) return;
    d7 = (uint16_t)(d7 - d0);
    if (d7 & 0x8000) {
        if ((int16_t)(uint16_t)(d7 - 0xFF00) < 0) goto up;
        if ((int16_t)(uint16_t)(d7 - 0xFFFC) >= 0) goto keep;
        goto down;
    }
    if ((int16_t)(d7 - 0x100) >= 0) goto down;
    if ((int16_t)(d7 - 4) < 0) goto keep;
up:
    add16(obj + 0x56, 2);
    wr16(obj + 0x56, rd16(obj + 0x56) & 0x1FE);
    return;
down:
    add16(obj + 0x56, (uint16_t)-2);
keep:
    wr16(obj + 0x56, rd16(obj + 0x56) & 0x1FE);
}

/* $04BFFE. The arm drawn out (+$54, every other frame) to $16: N while it is still going. */
int SpaceArmExtend(uint16_t obj)
{
    if (!below(obj + 0x54, 0x16)) return 0;
    if (!bit8(v_gameFrames + 1, 0)) add16(obj + 0x54, 1);
    return 1;
}

/* loc_04BFA8: a shot of type `type` along the arm's angle (+$56) at 16, from 8 along it; a
 * sound either way */
static void shot_along(uint16_t obj, uint16_t type)
{
    uint16_t slot, t;
    if (SlotFreeHigh(&slot, &t)) {
        wr16(slot + O_TYPE, type);
        uint16_t d0 = rd16(obj + 0x56);
        uint32_t d1 = (uint32_t)sine(d0) << 4, d2 = (uint32_t)cosine(d0) << 4;
        wr32(slot + 0x1C, d1);
        wr32(slot + 0x18, d2);
        d1 <<= 3;
        d2 <<= 3;
        wr16(slot + O_Y, (uint16_t)((d1 >> 16) + rd16(obj + O_Y)));
        wr16(slot + O_X, (uint16_t)((d2 >> 16) + rd16(obj + O_X)));
    }
    QueueSound(0xCE);
}

/* $04BFA4. A shot (type $454) along its arm. */
void SpaceShot454(uint16_t obj)
{
    shot_along(obj, 0x454);
}

/* $04C1A6. A ring of eight shots ($4A8) from here, the first at a random angle. */
void SpaceRing(uint16_t obj)
{
    uint16_t d5 = rd16(v_randomSeed) & 0x1E0, slot;
    uint16_t x = rd16(obj + O_X), y = rd16(obj + O_Y);
    if (!Spawn4A8Round(d5, x, y, &slot)) return;
    for (int i = 0; i < 7; i++) {
        d5 = (uint16_t)(d5 + 0x40) & 0x1FE;
        if (!Spawn4A8(d5, x, y, SINES, COSINES, &slot)) return;
    }
}

/* $04C44A. The object at a6 made a shot (type $460, hurting: $26 $A) from this one's place,
 * moving 4 times the tables' entry `angle`. */
void SpaceShot460(uint16_t obj, uint16_t angle)
{
    uint16_t s = (uint16_t)reg_a6;
    wr32(s + 0x1C, (uint32_t)sine(angle) << 2);
    wr32(s + 0x18, (uint32_t)cosine(angle) << 2);
    wr16(s + O_TYPE, 0x460);
    wr16(s + O_FLAGS, 0x8C00);
    wr16(s + 0x08, 0);
    wr16(s + 0x0A, 0xFCFC);
    wr8(s + 0x21, 0x10);
    wr32(s + 0x2C, 0xFF01FF01);
    wr16(s + 0x26, 0xA);
    wr16(s + O_X, rd16(obj + O_X));
    wr16(s + O_Y, rd16(obj + O_Y));
}

/* $04C158. From difficulty 4, a shot at the player. */
void SpaceShotIfHard(uint16_t obj)
{
    uint16_t slot;
    if (!below(v_difficulty, 4)) SpawnTowardPlayer(obj, &slot);
}

/* $04C20C. A line of fire (type $4B0) from here: +$18 d3, +$1C d4, +$4C d5, +$50 d6 (longs),
 * +$48 d7. */
void SpaceFire4B0(uint16_t obj, uint32_t d3, uint32_t d4, uint32_t d5, uint32_t d6, uint16_t d7)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x4B0);
    wr32(slot + 0x18, d3);
    wr32(slot + 0x1C, d4);
    wr32(slot + 0x4C, d5);
    wr32(slot + 0x50, d6);
    wr16(slot + 0x48, d7);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
}

/* $04C20A. The same, +$48 32. */
void SpaceFire4B0Of32(uint16_t obj, uint32_t d3, uint32_t d4, uint32_t d5, uint32_t d6)
{
    SpaceFire4B0(obj, d3, d4, d5, d6, 32);
}

/* $04C2B8. A shot ($4A8) from here at the player (its angle to a 16th). */
void SpaceShotAimed(uint16_t obj)
{
    uint16_t target = PlayerTarget();
    uint32_t d1 = (uint16_t)(rd16(target + O_Y) - rd16(obj + O_Y));
    uint16_t a = Atan2((int16_t)(rd16(target + O_X) - rd16(obj + O_X)), &d1);
    uint16_t d5 = AngleIndex(&a), slot;
    Spawn4A8Round(d5, rd16(obj + O_X), rd16(obj + O_Y), &slot);
}

/* loc_04C260: a shot ($4A8) at angle d5 from d6, d7; above the easiest level two more $30 to
 * either side, from difficulty 4 two more $60 out */
static void spread(uint16_t d5, uint16_t d6, uint16_t d7)
{
    uint16_t slot;
    if (!Spawn4A8Round(d5, d6, d7, &slot)) return;
    if (!rd16(v_difficulty)) return;
    d5 = (uint16_t)(d5 - 0x30) & 0x1FE;
    if (!Spawn4A8(d5, d6, d7, SINES, COSINES, &slot)) return;
    d5 = (uint16_t)(d5 + 0x60) & 0x1FE;
    if (!Spawn4A8(d5, d6, d7, SINES, COSINES, &slot)) return;
    if (below(v_difficulty, 4)) return;
    d5 = (uint16_t)(d5 - 0x90) & 0x1FE;
    if (!Spawn4A8(d5, d6, d7, SINES, COSINES, &slot)) return;
    d5 = (uint16_t)(d5 + 0xC0) & 0x1FE;
    Spawn4A8(d5, d6, d7, SINES, COSINES, &slot);
}

/* $04C23C. That spread from d6, d7 at the player (its angle from here, to a 16th). */
void SpaceSpread(uint16_t obj, uint16_t d6, uint16_t d7)
{
    uint16_t target = PlayerTarget();
    uint32_t d1 = (uint16_t)(rd16(target + O_Y) - rd16(obj + O_Y));
    uint16_t a = Atan2((int16_t)(rd16(target + O_X) - rd16(obj + O_X)), &d1);
    spread(AngleIndex(&a), d6, d7);
}

/* $04C164. From difficulty 6, a shot at the player. */
void SpaceShotIfHardest(uint16_t obj)
{
    uint16_t slot;
    if (below(v_difficulty, 6)) return;
    SpawnTowardPlayer(obj, &slot);
}

/* $03EA52. Every other frame an afterimage (type $288) of it, its flip and palette 0 at $6C0. */
void AfterImage(uint16_t obj)
{
    if (bit8(v_gameFrames + 1, 0)) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x288);
    wr32(slot + O_X, rd32(obj + O_X));
    wr32(slot + O_Y, rd32(obj + O_Y));
    wr16(slot + 0x0E, (rd16(obj + 0x0E) & 0x800) | 0x6C0);
}

/* $03EB82. Every 4th frame a puff ($563EE) left behind, rising; a sound unless it is rising. */
void PuffTrail(uint16_t obj)
{
    if (rd16(v_gameFrames) & 3) return;
    if (!(rd16(obj + 0x1C) & 0x8000)) QueueSound(0xCE);
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x0563EE);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr32(slot + 0x1C, 0xFFFFC000);
}

/* $0500E6. Every object from $CBC0 gone (but types d0, d1). */
void Stage6Clear(uint16_t keep1, uint16_t keep2)
{
    ObjectsVanishFrom(0xCBC0, keep1, keep2);
}

/* $051808. Objects from a list (type, x, y; a negative type ends it), d7 + 1 at most, while
 * there are slots from $CB00. */
void SpawnFromList(uint32_t list, uint16_t count)
{
    do {
        uint16_t slot, t;
        if (!SpawnSlot(&slot, &t)) return;
        uint16_t type = rom16(list);
        list += 2;
        if (type & 0x8000) return;
        wr16(slot + O_TYPE, type);
        wr16(slot + O_X, rom16(list));
        wr16(slot + O_Y, rom16(list + 2));
        list += 4;
    } while ((int16_t)(count = (uint16_t)(count - 1)) != -1);
}

/* ---- the shrinking of a picture (stage 6's gate): the 32x32 pixels at a1 drawn at a size of
 * $8040 (of $20) into $9800 and copied to the VRAM at a4 by the command list at a0 ---- */

/* $0208D6. $9800-$99FF cleared. */
void ScaleClear(void)
{
    for (uint16_t a = 0x9800; a != 0x9A00; a = (uint16_t)(a + 4)) wr32(a, 0);
}

/* $02070A. One column of output bytes (two source rows, d2 and d2 + d0, 16.16, their nibbles
 * paired) at a5 + 2, every 4th byte, stepping d1 across; d2 on by 2 d0 (stopped at $FFFFF),
 * a5 on a byte (to the next tile row every 4). a6 is left past the last byte. */
void ScaleRow(uint32_t step, uint32_t across, uint16_t src, uint32_t *d2, uint16_t *a5)
{
    uint32_t y = *d2;
    uint16_t a2 = src, a3 = src;
    uint32_t a6 = (uint32_t)(int32_t)(int16_t)*a5 + 2;
    uint32_t d5 = 0;
    uint16_t d7 = 0;
    if ((uint16_t)y & 0x8000) d7 = (uint16_t)(d7 + 1);
    a2 = (uint16_t)(a2 + (uint16_t)(((uint16_t)(y >> 16) & 0xFFFC) << 5));
    wr16(0x8042, a2);
    uint16_t d3 = (uint16_t)(y >> 16) & 3;
    y += step;
    if ((int32_t)(y - 0x100000) >= 0) y = 0xFFFFF;
    if (!((uint16_t)y & 0x8000)) d7 = (uint16_t)(d7 + 2);
    a3 = (uint16_t)(a3 + (uint16_t)(((uint16_t)(y >> 16) & 0xFFFC) << 5));
    wr16(0x8044, a3);
    uint16_t d4 = (uint16_t)(y >> 16) & 3;
    y += step;
    *d2 = y;
    do {
        uint8_t b0 = rd8((uint16_t)(a2 + d3));
        if (d7 & 1) b0 = (uint8_t)(b0 << 4);
        b0 &= 0xF0;
        uint8_t b2 = rd8((uint16_t)(a3 + d4));
        if (d7 & 2) b2 = (uint8_t)((int8_t)b2 >> 4);
        b2 &= 0xF;
        wr8((uint16_t)a6, (uint8_t)(b2 | b0));
        d5 += across;
        uint16_t d0 = (uint16_t)(d5 >> 16) & 0xFFFC;
        a2 = (uint16_t)(d0 + rd16(0x8042));
        a3 = (uint16_t)(d0 + rd16(0x8044));
        a6 += 4;
    } while ((int32_t)(d5 - 0x800000) < 0);
    *a5 = (uint16_t)(*a5 + 1);
    if (!(*a5 & 3)) *a5 = (uint16_t)(*a5 - 4 + 0x80);
    reg_a6 = a6;
}

/* $0208C6. The command list at a0 run (the 68k keeps every register). */
void ScaleRun(uint16_t list)
{
    RunCommands((uint32_t)(int32_t)(int16_t)list);
}

/* one copy of `bytes` from `from` (its first word made the length) to the VRAM at vram */
static void scale_part(uint16_t list, uint16_t from, uint16_t bytes, uint16_t vram)
{
    wr16(from, bytes);
    wr16(list + 4, from);
    wr16(list + 6, vram);
    ScaleRun(list);
}

/* $0207D4. The picture's tiles copied by its size: from $19 one block of $200, from $11 three
 * rows of $60, from 9 two of $40, else one of $20. */
void ScaleCopy(uint16_t list, uint16_t vram)
{
    uint16_t n;
    if (!below(0x8040, 0x19)) n = 0x200;
    else if (!below(0x8040, 0x11)) n = 0x60;
    else if (!below(0x8040, 9)) n = 0x40;
    else n = 0x20;
    wr16(0x9800, n);
    wr16(list + 2, 0xFFFF);
    wr16(list + 4, 0x9800);
    wr16(list + 6, vram);
    wr16(list + 8, 0xFFFF);
    vram = (uint16_t)(vram + n);
    ScaleRun(list);
    if (n == 0x200 || n == 0x20) return;
    scale_part(list, 0x9880, n, vram);
    vram = (uint16_t)(vram + n);
    if (n == 0x40) return;
    scale_part(list, 0x9900, n, vram);
}

/* $0206D4. The picture at a1 shrunk to the size at $8040 and copied to the VRAM at a4 (the
 * command list at a0: a copy, kind 1). The step is $8000 / size (a divs: a quotient too big
 * for a word leaves $8000); a size of 0 would be the 68k's divide-by-zero trap. */
void ScaleTiles(uint16_t list, uint16_t src, uint16_t vram)
{
    wr16(list, 1);
    uint16_t a5 = 0x9800;
    uint32_t d0 = 0x8000;
    int16_t size = (int16_t)rd16(0x8040);
    if (size) {
        int32_t q = 0x8000 / size, r = 0x8000 % size;
        if (q >= -0x8000 && q <= 0x7FFF) d0 = (uint32_t)r << 16 | (uint16_t)q;
    }
    d0 = (d0 & 0xFFFF) << 5;
    uint32_t d1 = d0 << 3, d2 = 0;
    ScaleClear();
    do ScaleRow(d0, d1, src, &d2, &a5);
    while ((int32_t)(d2 - 0x100000) < 0);
    ScaleCopy(list, vram);
}

/* stage 6's allies (types $3C8-$3D8 at $C9E0 on, $3D4 at $A800) */

/* $050012. The four allies ($C9E0 on, d7 + 1 of them) from the list at $50090: sprite,
 * mappings, x, type; at y $158. */
void Stage6Allies(uint16_t count)
{
    uint16_t a0 = 0xC9E0;
    uint32_t a1 = 0x050090;
    do {
        wr16(a0 + O_FLAGS, 0xE080);
        wr8(a0 + 0x20, 0);
        wr16(a0 + O_STATE, 0);
        wr16(a0 + 0x0C, 0);
        wr16(a0 + 0x0E, rom16(a1));
        wr32(a0 + 0x08, rom32(a1 + 2));
        wr16(a0 + O_X, rom16(a1 + 6));
        wr16(a0 + O_TYPE, rom16(a1 + 8));
        a1 += 10;
        wr16(a0 + O_Y, 0x158);
        wr8(a0 + 6, 0);
        a0 = (uint16_t)(a0 + 0x60);
    } while ((int16_t)(count = (uint16_t)(count - 1)) != -1);
}

/* $050182. The ally at $A800 (still a $3D4, +6's byte clear): its step from $501BE (d0). */
void Stage6AllyA800(uint16_t d0)
{
    if (rd8(0xA800 + 6) || rd16(0xA800) != 0x3D4) return;
    wr16(0xA800 + O_STATE, tab16(0x0501BE, d0));
}

/* loc_050146: the four allies (still their types, +6's byte clear): their steps from $5019E
 * (d0, on by 8 for each one set) */
static void allies_step(uint16_t d0)
{
    uint16_t a0 = 0xC9E0;
    uint32_t a2 = 0x0501C6;
    for (int i = 0; i < 4; i++) {
        if (!rd8(a0 + 6) && rd16(a0) == rom16(a2)) {
            wr16(a0 + O_STATE, tab16(0x05019E, d0));
            d0 = (uint16_t)(d0 + 8);
        }
        a0 = (uint16_t)(a0 + 0x60);
        a2 += 2;
    }
}

/* $050122. The allies' first steps. */
void Stage6AlliesStep0(void)
{
    Stage6AllyA800(0);
    allies_step(0);
}

/* $05012C. Their second (a fight's start). */
void Stage6AlliesStep2(void)
{
    Stage6AllyA800(2);
    allies_step(2);
}
