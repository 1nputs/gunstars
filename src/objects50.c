/* Object types from $501CE to $55B4C (and $5717C), most of them stage 6's (the final battle's):
 * the allies ($3C8-$3D8: four from $C9E0, the fifth at $A800) and the figure at $DE80 they
 * carry; the shot $3AC, the pod $3B0, the crates $5A4-$5B0 and their dust $65C, the gems' holder
 * $608 and its flashes $60C, the gunners $650/$654; the mech $39C, its bolts $3A0 and its pilot
 * $3A4; the claw machine $404 (with $400, $408); the Golden Silver fight: its director $41C,
 * the parts $420-$42C, the snake $534/$538, the shots $434 and $53C; and type $94 at $5717C.
 * The routines they call outside this range that were not ported yet are under "shared
 * helpers" at the end. */
#include "game.h"

static void next(uint16_t obj, uint16_t by) { wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + by)); }
static void back(uint16_t obj, uint16_t by) { wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) - by)); }
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
/* an animation from its start: +8 the list, +$C its frame */
static void anim(uint16_t obj, uint32_t list)
{
    wr32(obj + 0x08, list);
    wr16(obj + 0x0C, 0);
}
/* a step by the word-offset table at `table` from `base` (movea.w T(pc,d0.w),a0; adda.l) */
static void step_by_offsets(uint16_t obj, uint32_t table, uint32_t base)
{
    call_object(base + (uint32_t)(int16_t)tab16(table, rd16(obj + O_STATE)), obj);
}
/* a step by a table of longs */
static void step_by_longs(uint16_t obj, uint32_t table)
{
    call_object(rom32(table + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}

#define FIGURE 0xDE80            /* stage 6: the figure the allies move (type $10, then $608) */

/* ---- the allies' figure at $DE80: its y by the frame's bit 3 (or a bit the caller tested) ---- */

/* loc_05082E: y $15F when nz, else $15C */
static void figure_bob(uint16_t a0, int nz)
{
    wr16(a0 + O_Y, nz ? 0x15F : 0x15C);
}

/* $050828. The figure at a0 bobs: y $15C, $15F every other 8 frames. */
void FigureBob(uint16_t a0)
{
    figure_bob(a0, bit8(v_gameFrames + 1, 3));
}

/* ---- type $3C8: an ally (at x $120): idles, carries the figure across, cheers ---- */

/* $05022C. Step 0: standing ($E23DE) for $100-$17F frames. */
void Ally3C8Step00(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0E23DE);
    wr16(obj + 0x48, (uint16_t)((rd16(v_randomSeed) & 0x7F) + 0x100));
}

/* $050250. Then $20 frames of $E2422. */
void Ally3C8Step02(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    anim(obj, 0x0E2422);
    wr16(obj + 0x48, 0x20);
}

/* $050272. Then back to step 0. */
void Ally3C8Step04(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    back(obj, 4);
}

/* $050280. $E2422 for $20 frames, four times. */
void Ally3C8Step06(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0E2422);
    wr16(obj + 0x48, 0x20);
    wr16(obj + 0x4A, 3);
}

/* $0502A0. Then $1C frames of $E23DE. */
void Ally3C8Step08(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0x1C);
    anim(obj, 0x0E23DE);
}

/* $0502C2. Then $E2422 again (step 8), or after the fourth back to step 0. */
void Ally3C8Step0A(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    if (dec(obj + 0x4A)) {
        wr16(obj + O_STATE, 0);
        return;
    }
    back(obj, 4);
    wr16(obj + 0x48, 0x20);
    anim(obj, 0x0E2422);
}

/* $0502F0. $E24F6 for $80 frames. */
void Ally3C8Step0C(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0E24F6);
    wr16(obj + 0x48, 0x80);
}

/* $05030A. Then step 0. */
void Ally3C8Step0E(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    wr16(obj + O_STATE, 0);
}

/* $050318. $E24F6 for $18 frames. */
void Ally3C8Step10(uint16_t obj)
{
    next(obj, 2);
    wr16(obj + 0x48, 0x18);
    anim(obj, 0x0E24F6);
}

/* $050332. Then $E23DE for $18 frames. */
void Ally3C8Step12(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0x18);
    anim(obj, 0x0E23DE);
}

/* $050354. Then $E24F6 again for $18 (step $12, over and over). */
void Ally3C8Step14(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    back(obj, 2);
    wr16(obj + 0x48, 0x18);
    anim(obj, 0x0E24F6);
}

/* $050376. Standing; the figure at $DE80 made (type $10, $E2846, at x $70). */
void Ally3C8Step16(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0E23DE);
    wr16(FIGURE + O_TYPE, 0x10);
    wr16(FIGURE + O_FLAGS, 0xC080);
    wr16(FIGURE + 0x0E, 0x8300);
    wr32(FIGURE + 0x08, 0x0E2846);
    wr16(FIGURE + O_X, 0x70);
    wr8(FIGURE + 0x20, 0);
    wr16(FIGURE + O_STATE, 0);
    wr8(FIGURE + 0x21, 0);
}

/* $0503B6. The figure carried right (bobbing) to x $C4: then held $20 frames, flipped. */
void Ally3C8Step18(uint16_t obj)
{
    FigureBob(FIGURE);
    add16(FIGURE + O_X, 1);
    if (below(FIGURE + O_X, 0xC4)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0x20);
    set8(FIGURE + 0x0E, 3);
    wr16(FIGURE + O_Y, 0x15C);
}

/* $0503E4. Then $40 frames more. */
void Ally3C8Step1A(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0x40);
}

/* $0503F8. The figure bobbing every 4 frames; then its +$20 at $CB20 is 4 (2 frames). */
void Ally3C8Step1C(uint16_t obj)
{
    figure_bob(FIGURE, bit8(v_gameFrames + 1, 2));
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    wr16(obj + 0x48, 2);
    wr8(0xCB20, 4);
}

/* $050420. The figure back left, 4 a frame, to x $94: unflipped, its +$20 clear. */
void Ally3C8Step1E(uint16_t obj)
{
    FigureBob(FIGURE);
    wr16(FIGURE + O_X, (uint16_t)(rd16(FIGURE + O_X) - 4));
    if (!below(FIGURE + O_X, 0x94)) return;
    next(obj, 2);
    clr8(FIGURE + 0x0E, 3);
    wr8(FIGURE + 0x20, 0);
}

/* $050446. Right again to $C4 (flipped, +$20 4): back to step $1E until +$48 runs out, then
 * $40 frames more. */
void Ally3C8Step20(uint16_t obj)
{
    FigureBob(FIGURE);
    add16(FIGURE + O_X, 4);
    if (below(FIGURE + O_X, 0xC4)) return;
    set8(FIGURE + 0x0E, 3);
    wr8(FIGURE + 0x20, 4);
    if (!dec(obj + 0x48)) {
        back(obj, 2);
        return;
    }
    next(obj, 2);
    wr16(FIGURE + O_Y, 0x15C);
    wr16(obj + 0x48, 0x40);
}

/* $050488. Then cheering ($E25CA) $A0 frames; the ally at $CB00 to its step $12. */
void Ally3C8Step22(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    anim(obj, 0x0E25CA);
    wr16(obj + 0x48, 0xA0);
    wr16(0xCB04, 0x12);
}

/* $0504AE. Then the allies' first steps. */
void Ally3C8Step24(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    Stage6AlliesStep0();
}

/* $0504B8. $E24F6 for $30 frames; the ally at $CA40 to its step 6. */
void Ally3C8Step26(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0E24F6);
    wr16(obj + 0x48, 0x30);
    wr16(0xCA44, 6);
}

/* $050500. Its own y: $158, $15C every other 8 frames. */
void Ally3C8Bob(uint16_t obj)
{
    wr16(obj + O_Y, bit8(v_gameFrames + 1, 3) ? 0x15C : 0x158);
}

/* $0504D6. Bobbing; then standing at y $158, $20 frames. */
void Ally3C8Step28(uint16_t obj)
{
    Ally3C8Bob(obj);
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    anim(obj, 0x0E23DE);
    wr16(obj + 0x48, 0x20);
    wr16(obj + O_Y, 0x158);
}

/* $05051A. Then cheering $40 frames. */
void Ally3C8Step2A(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    anim(obj, 0x0E25CA);
    wr16(obj + 0x48, 0x40);
}

/* $05053A. Then cheering $50 frames; the ally at $CA40 to its step $14. */
void Ally3C8Step2C(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    anim(obj, 0x0E25CA);
    wr16(obj + 0x48, 0x50);
    wr16(0xCA44, 0x14);
}

/* $050560. Then step 6. */
void Ally3C8Step2E(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    wr16(obj + O_STATE, 6);
}

/* $050570. Cheering $80 frames (to step $2E); the ally at $A800 to its step $12. */
void Ally3C8Step30(uint16_t obj)
{
    back(obj, 2);
    anim(obj, 0x0E25CA);
    wr16(obj + 0x48, 0x80);
    wr16(0xA804, 0x12);
}

/* $05058E. $E24F6; the ally at $CAA0 to its step $12. */
void Ally3C8Step32(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0E24F6);
    wr16(0xCAA4, 0x12);
}

/* $0505A6. That ally moved right (its x) to $100: then $B0 frames. */
void Ally3C8Step34(uint16_t obj)
{
    add16(0xCAB0, 1);
    if (below(0xCAB0, 0x100)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0xB0);
}

/* $0505C0. Then cheering $40 frames. */
void Ally3C8Step36(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    anim(obj, 0x0E25CA);
    wr16(obj + 0x48, 0x40);
}

/* $0505E0. Then step 0. */
void Ally3C8Step38(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    wr16(obj + O_STATE, 0);
}

/* $050632. A type $10 ($E0B10) at x d1, y $160, in a free slot from the middle. */
void Ally3C8Spawn10(uint16_t x)
{
    uint16_t slot, t;
    if (!SlotFreeMid(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x10);
    wr16(slot + O_FLAGS, 0xC100);
    wr16(slot + 0x0E, 0);
    wr32(slot + 0x08, 0x0E0B10);
    wr16(slot + O_X, x);
    wr16(slot + O_Y, 0x160);
}

/* $0505EE. From the camera's step $34: shown ($8B50, $E6072) at y $150, the messages' step
 * clear, two type $10 at x $5C and $1E8. */
void Ally3C8Step3A(uint16_t obj)
{
    if (below(v_cameraStep, 0x34)) return;
    next(obj, 2);
    wr16(obj + O_FLAGS, 0xED00);
    wr16(obj + 0x0E, 0x8B50);
    anim(obj, 0x0E6072);
    wr16(obj + O_Y, 0x150);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(0x80C2, 0);
    Ally3C8Spawn10(0x5C);
    Ally3C8Spawn10(0x1E8);
}

/* $05065E. Its y $150 less half the plane's ($EC02 - $100); from the camera's step $38, $40
 * frames. */
void Ally3C8Step3C(uint16_t obj)
{
    uint16_t d0 = (uint16_t)((int16_t)(uint16_t)(rd16(0xEC02) - 0x100) >> 1);
    wr16(obj + O_Y, (uint16_t)(0x150 - d0));
    if (below(v_cameraStep, 0x38)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0x40);
}

/* $050688. Then a jump ($E6104): x speed 4, y speed -3. */
void Ally3C8Step3E(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    anim(obj, 0x0E6104);
    wr16(obj + 0x18, 4);
    wr16(obj + 0x1C, 0xFFFD);
}

/* $0506AE. Falling ($4000 a frame) to y $140: landed, $10 frames. */
void Ally3C8Step40(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if ((int32_t)rd32(obj + 0x1C) < 0) return;
    if (below(obj + O_Y, 0x140)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0x10);
    wr16(obj + O_Y, 0x140);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    anim(obj, 0x0E6072);
}

/* $0506EA. Then $E5FD8 for $60 frames; the camera's hold bit 7 off. */
void Ally3C8Step42(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0x60);
    anim(obj, 0x0E5FD8);
    clr8(v_cameraHold, 7);
}

/* $050710. Then a sound ($C3) and the figure at $DE80 made a $608, 8 above it. */
void Ally3C8Step44(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    QueueSound(0xC3);
    wr16(FIGURE + O_TYPE, 0x608);
    wr16(FIGURE + O_FLAGS, 0);
    wr16(FIGURE + O_STATE, 0);
    wr16(FIGURE + O_X, rd16(obj + O_X));
    wr16(FIGURE + O_Y, rd16(obj + O_Y));
    wr16(FIGURE + O_Y, (uint16_t)(rd16(FIGURE + O_Y) - 8));
}

/* $050748. Nothing. */
void Ally3C8Step46(uint16_t obj)
{
    (void)obj;
}

/* $05074A. $E6104 for $100 frames, a sound ($A5). */
void Ally3C8Step48(uint16_t obj)
{
    next(obj, 2);
    wr16(obj + 0x48, 0x100);
    anim(obj, 0x0E6104);
    QueueSound(0xA5);
}

/* $05076A. Shaking (shown every other frame, y $140 or $143 every 4); then $E61F0, shown, at
 * y $140. */
void Ally3C8Step4A(uint16_t obj)
{
    if (dec(obj + 0x48)) {
        next(obj, 2);
        anim(obj, 0x0E61F0);
        set8(obj + O_FLAGS, 7);
        wr16(obj + O_Y, 0x140);
        return;
    }
    if (!bit8(v_gameFrames + 1, 0)) set8(obj + O_FLAGS, 7);
    else clr8(obj + O_FLAGS, 7);
    wr16(obj + O_Y, bit8(v_gameFrames + 1, 2) ? 0x143 : 0x140);
}

/* $0507C4. Every 4th frame a puff ($564FC, type $100) near it (x -8..+7, y -8..-5), rising;
 * its +$20 $C0 on odd frames. */
void Ally3C8Step4C(uint16_t obj)
{
    uint16_t slot, t;
    if (!(rd16(v_gameFrames) & 3) && SlotFreeHigh(&slot, &t)) {
        wr16(slot + O_TYPE, 0x100);
        wr32(slot + 0x48, 0x0564FC);
        wr16(slot + O_X, (uint16_t)((rd16(v_randomSeed) & 0xF) - 8 + rd16(obj + O_X)));
        wr16(slot + O_Y, (uint16_t)((rd8(v_randomSeed) & 3) - 8 + rd16(obj + O_Y)));
        wr16(slot + 0x1C, 0xFFFF);
    }
    wr8(obj + 0x20, bit8(v_gameFrames + 1, 0) ? 0xC0 : 0);
}

/* $0501CE. Type $3C8: an ally of stage 6's, by its step. */
void Ally3C8(uint16_t obj)
{
    step_by_offsets(obj, 0x0501DE, 0x05022C);
}

/* ---- type $3CC: an ally (at x $1B6) ---- */

/* $05087A. Step 0: $E26D2 for $80-$FF frames. */
void Ally3CCStep00(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0E26D2);
    wr16(obj + 0x48, (uint16_t)((rd16(v_randomSeed) & 0x7F) + 0x80));
}

/* $05089C. Then $E26FA for 4 frames. */
void Ally3CCStep02(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    anim(obj, 0x0E26FA);
    wr16(obj + 0x48, 4);
}

/* $0508BC. Then step 0. */
void Ally3CCStep04(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    back(obj, 4);
}

/* $0508CA. $E26FA for $140 frames. */
void Ally3CCStep06(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0E26FA);
    wr16(obj + 0x48, 0x140);
}

/* $0508E2. Then $E26E6. */
void Ally3CCStep08(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    anim(obj, 0x0E26E6);
}

/* $0508FC. Nothing. */
void Ally3CCStep0A(uint16_t obj)
{
    (void)obj;
}

/* $0508FE. $E26FA for $180 frames. */
void Ally3CCStep0C(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0E26FA);
    wr16(obj + 0x48, 0x180);
}

/* $050916. Then step 0. */
void Ally3CCStep0E(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    wr16(obj + O_STATE, 0);
}

/* $050924. $E26FA. */
void Ally3CCStep10(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0E26FA);
}

/* $050936. Nothing. */
void Ally3CCStep12(uint16_t obj)
{
    (void)obj;
}

/* $05093E. Its +6 1; knocked ($8B36, $E1C6C) for $14 frames. */
void Ally3CCStep20(uint16_t obj)
{
    wr8(obj + 6, 1);
    next(obj, 2);
    wr16(obj + 0x0E, 0x8B36);
    anim(obj, 0x0E1C6C);
    wr16(obj + 0x48, 0x14);
}

/* $050938. $10 to the left, then as step $20. */
void Ally3CCStep14(uint16_t obj)
{
    wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) - 0x10));
    Ally3CCStep20(obj);
}

/* $050962. Then thrown up and left ($E1D60, speeds -5.25, -4.5). */
void Ally3CCStep16(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    anim(obj, 0x0E1D60);
    wr16(obj + O_FLAGS, 0xEE80);
    wr32(obj + 0x18, 0xFFFAC000);
    wr32(obj + 0x1C, 0xFFFB8000);
}

/* $050992. Rising ($3C00 a frame) until it falls: $E1C46. */
void Ally3CCStep18(uint16_t obj)
{
    add32(obj + 0x1C, 0x3C00);
    if ((int32_t)rd32(obj + 0x1C) < 0) return;
    next(obj, 2);
    anim(obj, 0x0E1C46);
}

/* $0509B0. Falling to y $158: down, flipped, $30 frames. */
void Ally3CCStep1A(uint16_t obj)
{
    add32(obj + 0x1C, 0x3C00);
    if (below(obj + O_Y, 0x158)) return;
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    next(obj, 2);
    wr16(obj + O_Y, 0x158);
    anim(obj, 0x0E1C6C);
    set8(obj + 0x0E, 3);
    wr16(obj + 0x48, 0x30);
}

/* $0509EE. Then $E1CA8 for $20 frames. */
void Ally3CCStep1C(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0x20);
    anim(obj, 0x0E1CA8);
}

/* $050A0E. Then unflipped, $18 frames (step $20 knocks it again). */
void Ally3CCStep1E(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    wr16(obj + 0x48, 0x18);
    clr8(obj + 0x0E, 3);
}

/* $050A28. Then thrown up and left again ($E1D60, speeds -6.625, -3.125). */
void Ally3CCStep22(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    anim(obj, 0x0E1D60);
    wr32(obj + 0x18, 0xFFF96000);
    wr32(obj + 0x1C, 0xFFFCE000);
}

/* $050842. Type $3CC: an ally of stage 6's, by its step. */
void Ally3CC(uint16_t obj)
{
    step_by_offsets(obj, 0x050852, 0x05087A);
}

/* ---- type $3D0: an ally (at x $E0) ---- */

/* $050A7E. Step 0: $E270E for $A0-$DF frames. */
void Ally3D0Step00(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0E270E);
    wr16(obj + 0x48, (uint16_t)((rd8(v_randomSeed) & 0x3F) + 0xA0));
}

/* $050AA0. Then $E2722 or $E2746 (by the frame's bit 2) for $30 frames. */
void Ally3D0Step02(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    wr16(obj + 0x0C, 0);
    wr16(obj + 0x48, 0x30);
    wr32(obj + 0x08, bit8(v_gameFrames + 1, 2) ? 0x0E2746 : 0x0E2722);
}

/* $050AD4. Then step 0. */
void Ally3D0Step04(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    back(obj, 4);
}

/* $050AE2. $E2722 for $90 frames. */
void Ally3D0Step06(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0E2722);
    wr16(obj + 0x48, 0x90);
}

/* $050AFA. Then step 0. */
void Ally3D0Step08(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    wr16(obj + O_STATE, 0);
}

/* $050B08. $E270E. */
void Ally3D0Step0A(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0E270E);
}

/* $050B1C. Nothing. */
void Ally3D0Step0C(uint16_t obj)
{
    (void)obj;
}

/* $050B1E. $E275A. */
void Ally3D0Step0E(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0E275A);
}

/* $050B32. Nothing. */
void Ally3D0Step10(uint16_t obj)
{
    (void)obj;
}

/* $050B34. Its +6 1; $E270E. */
void Ally3D0Step12(uint16_t obj)
{
    wr8(obj + 6, 1);
    next(obj, 2);
    anim(obj, 0x0E270E);
}

/* $050B4C. Bobbing (y $158, $15A) until its x is $100 (moved by another): $E2722 for $60
 * frames at y $158. */
void Ally3D0Step14(uint16_t obj)
{
    wr16(obj + O_Y, bit8(v_gameFrames + 1, 3) ? 0x15A : 0x158);
    if (below(obj + O_X, 0x100)) return;
    next(obj, 2);
    anim(obj, 0x0E2722);
    wr16(obj + 0x48, 0x60);
    wr16(obj + O_Y, 0x158);
}

/* $050B8E. Then $E2722 for $60 frames more. */
void Ally3D0Step16(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    anim(obj, 0x0E2722);
    wr16(obj + 0x48, 0x60);
}

/* $050BAE. Then a leap ($8C00, $E23B0; speeds -2.5, -3.5). */
void Ally3D0Step18(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    wr16(obj + O_FLAGS, 0xEE00);
    wr16(obj + 0x0E, 0x8C00);
    anim(obj, 0x0E23B0);
    wr32(obj + 0x18, 0xFFFD8000);
    wr32(obj + 0x1C, 0xFFFC8000);
}

/* $050BE4. Falling ($4000 a frame); at y $158 it leaps again. */
void Ally3D0Step1A(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if ((int32_t)rd32(obj + 0x1C) < 0) return;
    if (below(obj + O_Y, 0x158)) return;
    wr32(obj + 0x1C, 0xFFFC8000);
}

/* $050A52. Type $3D0: an ally of stage 6's, by its step. */
void Ally3D0(uint16_t obj)
{
    step_by_offsets(obj, 0x050A62, 0x050A7E);
}

/* ---- type $3D4: the ally at $A800 (x $168) ---- */

/* $050C2C. Step 0: $C0022 for $20-$11F frames. */
void Ally3D4Step00(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0C0022);
    wr16(obj + 0x48, (uint16_t)(rd8(v_randomSeed) + 0x20));
}

/* $050C4E. Then $C007E for $100 frames. */
void Ally3D4Step02(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    anim(obj, 0x0C007E);
    wr16(obj + 0x48, 0x100);
}

/* $050C6E. Then step 0. */
void Ally3D4Step04(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    back(obj, 4);
}

/* $050C7C. $C00DA for $80 frames. */
void Ally3D4Step06(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0C00DA);
    wr16(obj + 0x48, 0x80);
}

/* $050C94. Then step 0. */
void Ally3D4Step08(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    wr16(obj + O_STATE, 0);
}

/* $050CA2. $C01F6. */
void Ally3D4Step0A(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0C01F6);
}

/* $050CB4. Nothing. */
void Ally3D4Step0C(uint16_t obj)
{
    (void)obj;
}

/* $050CB6. $C01F6. */
void Ally3D4Step0E(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0C01F6);
}

/* $050CC8. Nothing. */
void Ally3D4Step10(uint16_t obj)
{
    (void)obj;
}

/* $050CCA. Its +6 on by 1; flipped, $C007E for $40 frames. */
void Ally3D4Step12(uint16_t obj)
{
    wr8(obj + 6, (uint8_t)(rd8(obj + 6) + 1));
    next(obj, 2);
    set8(obj + 0x0E, 3);
    anim(obj, 0x0C007E);
    wr16(obj + 0x48, 0x40);
}

/* $050CEC. Then walking off ($BFB62). */
void Ally3D4Step14(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    anim(obj, 0x0BFB62);
}

/* $050D06. Left 1.5 a frame; past x $70 removed. */
void Ally3D4Step16(uint16_t obj)
{
    wr32(obj + O_X, rd32(obj + O_X) - 0x18000);
    if (!below(obj + O_X, 0x70)) return;
    set8(obj + O_FLAGS, 4);
}

/* $050C04. Type $3D4: the ally at $A800, by its step. */
void Ally3D4(uint16_t obj)
{
    step_by_offsets(obj, 0x050C14, 0x050C2C);
}

/* ---- type $3D8: an ally (at x $AC) ---- */

/* $050D48. Step 0: $E27AC for $20-$11F frames. */
void Ally3D8Step00(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0E27AC);
    wr16(obj + 0x48, (uint16_t)(rd8(v_randomSeed) + 0x20));
}

/* $050D6A. Then $E277E for $100 frames. */
void Ally3D8Step02(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    anim(obj, 0x0E277E);
    wr16(obj + 0x48, 0x100);
}

/* $050D8A. Then step 0. */
void Ally3D8Step04(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    back(obj, 4);
}

/* $050D98. $E27AC for $C0 frames. */
void Ally3D8Step06(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0E27AC);
    wr16(obj + 0x48, 0xC0);
}

/* $050DB0. Then step 0. */
void Ally3D8Step08(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    wr16(obj + O_STATE, 0);
}

/* $050DBE. $E27DC. */
void Ally3D8Step0A(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0E27DC);
}

/* $050DD0. Nothing. */
void Ally3D8Step0C(uint16_t obj)
{
    (void)obj;
}

/* $050DD2. $E27DC. */
void Ally3D8Step0E(uint16_t obj)
{
    next(obj, 2);
    anim(obj, 0x0E27DC);
}

/* $050DE4. Nothing. */
void Ally3D8Step10(uint16_t obj)
{
    (void)obj;
}

/* $050DE6. Its +6 on by 1; $E277E for $40 frames; the figure at $DE80 unflipped. */
void Ally3D8Step12(uint16_t obj)
{
    wr8(obj + 6, (uint8_t)(rd8(obj + 6) + 1));
    next(obj, 2);
    anim(obj, 0x0E277E);
    clr8(FIGURE + 0x0E, 3);
    wr16(obj + 0x48, 0x40);
}

/* $050E08. Then walking off with the figure ($E27F0; it unflipped, the figure flipped). */
void Ally3D8Step14(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    next(obj, 2);
    anim(obj, 0x0E27F0);
    clr8(obj + 0x0E, 3);
    set8(FIGURE + 0x0E, 3);
}

/* $050E2E. Left 1 a frame, the figure 2 (bobbing); past x $60 both removed. */
void Ally3D8Step16(uint16_t obj)
{
    wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) - 1));
    wr16(FIGURE + O_X, (uint16_t)(rd16(FIGURE + O_X) - 2));
    FigureBob(FIGURE);
    if (!below(obj + O_X, 0x60)) return;
    set8(obj + O_FLAGS, 4);
    set8(FIGURE + O_FLAGS, 4);
}

/* $050D20. Type $3D8: an ally of stage 6's, by its step. */
void Ally3D8(uint16_t obj)
{
    step_by_offsets(obj, 0x050D30, 0x050D48);
}

/* $050EA0. Type $3AC: a shot flying left (3 a frame; $E0AF8, a sound $C7 at its start),
 * flipped every other frame. */
void Shot3AC(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + O_FLAGS, 0xCB80);
        wr32(obj + 0x08, 0x0E0AF8);
        wr8(obj + 0x21, 0x30);
        wr32(obj + 0x2C, 0xF010F010);
        wr32(obj + 0x28, 0xE020E020);
        wr16(obj + 0x26, 0x0C);
        wr32(obj + 0x18, 0xFFFD0000);
        QueueSound(0xC7);
    }
    if (!bit8(v_gameFrames + 1, 0)) set8(obj + 0x0E, 3);
    else clr8(obj + 0x0E, 3);
}

/* ---- type $3B0: a pod dropped from above: it lands at y $11C and bursts into ten sparks
 * ($1B0) fanning left; shot, it scores and becomes a $54 with its tiles sent ---- */

/* $050FB4. Step 0: vitality $78, the wait before it bursts by the difficulty's step ($50FD8). */
void Pod3B0Start(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + O_FLAGS, 0x100);
    wr32(obj + 0x28, 0xF010F010);
    wr16(obj + O_VITALITY, 0x78);
    wr16(obj + 0x48, tab16(0x050FD8, rd16(0xA20C)));
}

/* $050FE8. Waits, then once down at y $11C: a shake, a sound ($CA), ten sparks ($1B0) from 8
 * left of it, their x speeds -1 to -28 (by -3), their +$20 $B0 down by 4; again $50 frames
 * on. */
void Pod3B0Burst(uint16_t obj)
{
    if (!(rd16(obj + 0x48) & 0x8000)) {
        wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
        return;
    }
    if (below(obj + O_Y, 0x11C)) return;
    wr16(obj + 0x48, 0x50);
    wr16(v_shakeSprites, 0x0A);
    QueueSound(0xCA);
    uint32_t d1 = 0xFFFF0000;
    uint8_t d2 = 0xB0;
    for (int i = 0; i < 10; i++) {
        uint16_t slot, t;
        if (!SlotFreeHigh(&slot, &t)) return;
        wr16(slot + O_TYPE, 0x1B0);
        wr16(slot + O_STATE, 4);
        wr16(slot + O_FLAGS, 0x8B00);
        wr8(slot + 0x20, d2);
        wr16(slot + 0x0E, 0x4B9);
        wr16(slot + 0x08, 0xA00);
        wr16(slot + 0x0A, 0xF4F4);
        wr16(slot + 0x26, 2);
        wr8(slot + 0x21, 0x10);
        wr32(slot + 0x2C, 0xFE02F010);
        wr16(slot + O_X, rd16(obj + O_X));
        wr16(slot + O_Y, rd16(obj + O_Y));
        wr16(slot + O_X, (uint16_t)(rd16(slot + O_X) - 8));
        wr32(slot + 0x18, d1);
        wr16(slot + 0x48, 0x20);
        d1 -= 0x30000;
        d2 = (uint8_t)(d2 - 4);
    }
}

/* $050F48. Type $3B0: shot, it scores ($7000) and becomes a $54 (its tiles, +$4A, sent by two
 * DMAs from $FF4BA0 and $FF4C00); hit only from y $90; below $170 removed; else its step. */
void Pod3B0(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) & 0x8000) {
        AddScore(obj, 0x7000);
        wr16(obj + O_TYPE, 0x54);
        wr8(obj + 0x21, 0);
        wr16(obj + O_FLAGS, 0);
        wr16(obj + O_STATE, 0);
        uint16_t d1 = rd16(obj + 0x4A);
        ChunkDma(0xFFFF4BA0, d1);
        ChunkDma(0xFFFF4C00, (uint16_t)(d1 + 0x200));
        return;
    }
    if ((rd16(obj + O_Y) & 0x8000) || below(obj + O_Y, 0x90)) {
        wr8(obj + 0x21, 0);
    } else {
        wr8(obj + 0x21, 0x20);
        if (!below(obj + O_Y, 0x170)) {
            set8(obj + O_FLAGS, 4);
            return;
        }
    }
    step_by_longs(obj, 0x050FAC);
}

/* ---- types $5A4, $5A8: a crate; $5AC: a block dropping in; $5B0: its pieces; $65C: its
 * dust ---- */

/* loc_05861C: an item of kind d7 (type $68) dropped here, thrown up (DropItem's own body) */
static void drop_item_kind(uint16_t obj, uint16_t kind)
{
    uint16_t slot;
    if (!SlotFreeLow(&slot)) return;
    wr16(slot + O_TYPE, 0x68);
    wr16(slot + 0x50, kind);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr32(slot + 0x1C, 0xFFFD0000);
    wr16(slot + O_FLAGS, 0x8F00);
    wr16(slot + 6, 0);
}

/* $0510B0. Types $5A4, $5A8: a crate (vitality $C7); left of x $80 removed; shot, it scores
 * ($20), becomes a $54 and leaves an item: a random one, or ($5A8) the kind by the
 * difficulty ($5114A). */
void Crate5A4(uint16_t obj)
{
    if (below(obj + O_X, 0x80)) {
        set8(obj + O_FLAGS, 4);
        return;
    }
    if (rd16(obj + O_VITALITY) & 0x8000) {
        AddScore(obj, 0x20);
        uint16_t type = rd16(obj + O_TYPE);
        wr16(obj + O_TYPE, 0x54);
        wr16(obj + O_STATE, 0);
        wr8(obj + 0x21, 0);
        if (type != 0x5A8) {
            DropItemRandom(obj);
            return;
        }
        drop_item_kind(obj, tab16(0x05114A, rd16(v_difficulty)));
        return;
    }
    if (rd16(obj + O_STATE)) return;
    next(obj, 4);
    wr16(obj + O_FLAGS, 0x8100);
    wr16(obj + 0x0E, 0x4001);
    wr16(obj + 0x08, 0xF00);
    wr16(obj + 0x0A, 0xF0F0);
    wr8(obj + 0x20, 0x80);
    wr8(obj + 0x21, 0x24);
    wr32(obj + 0x2C, 0xF008F010);
    wr32(obj + 0x28, 0xEC14EC14);
    wr8(obj + 0x46, 0x8F);
    wr16(obj + O_VITALITY, 0xC7);
}

/* $051152. Type $5AC: a block (vitality $1F3) dropping from y $80 to its own y, 6 a frame
 * (then a shake, a sound $B8); hit, the sprites shake; left of x $88 removed; destroyed, it
 * scores ($573), becomes the dust ($65C, $34 frames) and five pieces ($5B0) fall from it, $20
 * apart up from $20 below it. */
void Block5AC(uint16_t obj)
{
    if (below(obj + O_X, 0x88)) {
        set8(obj + O_FLAGS, 4);
        return;
    }
    if (rd16(obj + 0x4A)) {
        if (rd16(obj + O_VITALITY) & 0x8000) {
            AddScore(obj, 0x573);
            wr16(obj + O_TYPE, 0x65C);
            wr16(obj + 0x48, 0x34);
            wr16(obj + O_FLAGS, 0x100);
            wr8(obj + 0x21, 0);
            uint16_t d5 = (uint16_t)(rd16(obj + O_Y) + 0x20);
            wr16(obj + O_Y, d5);
            for (int i = 0; i < 5; i++) {
                uint16_t slot, t;
                if (!SlotFreeHigh(&slot, &t)) break;
                wr16(slot + O_TYPE, 0x5B0);
                wr16(slot + O_FLAGS, 0x8D00);
                wr16(slot + 0x0E, 0x6001);
                wr16(slot + 0x08, 0xF00);
                wr16(slot + 0x0A, 0xF0F0);
                wr8(slot + 0x20, 0xBC);
                wr16(slot + O_X, rd16(obj + O_X));
                wr16(slot + O_Y, d5);
                wr16(slot + 0x1C, 0xFFFF);
                d5 = (uint16_t)(d5 - 0x20);
            }
            wr16(v_shakeSprites, 0x0F);
            QueueSound(0xBE);
            return;
        }
        if (bit8(obj + 0x22, 5)) wr16(v_shakeSprites, 2);
    }
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + O_FLAGS, 0xC100);
        wr16(obj + 0x0E, 0);
        wr32(obj + 0x08, 0x0E0B10);
        wr8(obj + 0x20, 0x20);
        wr8(obj + 0x21, 0x24);
        wr32(obj + 0x2C, 0x9030EC14);
        wr32(obj + 0x28, 0x9030F010);
        wr8(obj + 0x46, 0x8F);
        wr16(obj + O_VITALITY, 0x1F3);
        wr16(obj + 0x48, rd16(obj + O_Y));
        wr16(obj + 0x4A, 0);
        wr16(obj + O_Y, 0x80);
    }
    if (rd16(obj + 0x4A)) return;
    add16(obj + O_Y, 6);
    if ((int16_t)(rd16(obj + O_Y) - rd16(obj + 0x48)) < 0) return;
    wr16(obj + O_Y, rd16(obj + 0x48));
    add16(obj + 0x4A, 1);
    wr16(v_shakeSprites, 0x10);
    QueueSound(0xB8);
}

/* $05128C. Type $5B0: a piece falling ($4400 a frame); at y $120 a puff ($563EE, type $100)
 * rising. */
void Piece5B0(uint16_t obj)
{
    add32(obj + 0x1C, 0x4400);
    if ((int32_t)rd32(obj + 0x1C) < 0) return;
    if (below(obj + O_Y, 0x120)) return;
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x0563EE);
    wr16(obj + O_STATE, 0);
    wr32(obj + 0x1C, 0xFFFDD000);
}

/* $0512BC. Type $65C: the dust where the block was, +$48 frames (the sprites shaking): every
 * other frame a puff ($564FC, type $104) about it, thrown up and to a side; left of x $88 or
 * when done removed. */
void Dust65C(uint16_t obj)
{
    wr16(v_shakeSprites, 8);
    if (below(obj + O_X, 0x88) || dec(obj + 0x48)) {
        set8(obj + O_FLAGS, 4);
        return;
    }
    if (bit8(v_gameFrames + 1, 0)) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x104);
    wr32(slot + 0x48, 0x0564FC);
    wr16(slot + 0x1C, (uint16_t)((rd16(v_gameFrames) & 3) - 4));
    uint16_t d1 = rd8(v_randomSeed) & 1;
    if (rd16(v_randomSeed) & 0x8000) d1 = (uint16_t)-d1;
    wr16(slot + 0x18, d1);
    wr16(slot + 0x1A, 0x8000);
    wr16(slot + O_X, (uint16_t)((rd16(v_randomSeed) & 0x1F) - 0x10 + rd16(obj + O_X)));
    wr16(slot + O_Y, rd16(obj + O_Y));
}

/* ---- type $608: the gems' holder (what the ally at $C9E0 made of the figure): four gems
 * ($CA40 on, type $10) orbiting it at the radius +$5C (16.16) eased toward +$4A; it rises,
 * shines, shakes the screen, sinks, and leaves; or (with $A204 $10) it follows the boss at
 * $C980 ---- */

/* $051690. A spark ($5668C, type $100) about the gem at a1 (-8..+7 each way); a1 on to the next
 * gem, unless there was no slot. */
void GemSpark(uint16_t *gem)
{
    Random();
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x05668C);
    wr16(slot + O_X, (uint16_t)((rd16(v_randomSeed) & 0xF) - 8 + rd16(*gem + O_X)));
    wr16(slot + O_Y, (uint16_t)((rd8(v_randomSeed) & 0xF) - 8 + rd16(*gem + O_Y)));
    *gem = (uint16_t)(*gem + 0x60);
}

/* $051674. Every 8th frame a spark at each gem. */
void GemSparks(void)
{
    if (rd16(v_gameFrames) & 7) return;
    uint16_t gem = 0xCA40;
    GemSpark(&gem);
    GemSpark(&gem);
    GemSpark(&gem);
    GemSpark(&gem);
}

/* $051560. The radius eased toward +$4A by 1/2 a frame; the four gems placed around it, a
 * quarter turn apart, turning by the frame (radius times the sine, times 4). */
void GemsOrbit(uint16_t obj)
{
    uint16_t d0 = rd16(obj + 0x5C);
    uint16_t diff = (uint16_t)(d0 - rd16(obj + 0x4A));
    if (diff) {
        if (!(diff & 0x8000)) wr32(obj + 0x5C, rd32(obj + 0x5C) - 0x8000);
        else add32(obj + 0x5C, 0x8000);
    }
    uint16_t a0 = 0xCA40;
    d0 = (uint16_t)(rd16(v_gameFrames) << 1) & 0x1FE;
    for (int i = 0; i < 4; i++) {
        int32_t r = (int16_t)rd16(obj + 0x5C);
        uint32_t d1 = (uint32_t)((int32_t)(int16_t)rom16(0x02977C + d0) * r);
        uint32_t d2 = (uint32_t)((int32_t)(int16_t)rom16(0x0297FC + d0) * r);
        d1 = (d1 << 2) + rd32(obj + O_Y);
        d2 = (d2 << 2) + rd32(obj + O_X);
        wr32(a0 + O_Y, d1);
        wr32(a0 + O_X, d2);
        d0 = (uint16_t)(d0 + 0x80) & 0x1FE;
        a0 = (uint16_t)(a0 + 0x60);
    }
}

/* $051536. Shown, shining (its sprite and mappings from $5663C by the frame, in front), then
 * the gems placed. */
void GemsShine(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x8000);
    uint32_t a0 = 0x05663C + (uint16_t)((uint16_t)(rd16(v_gameFrames) << 3) & 0x18);
    wr16(obj + 0x0E, rom16(a0));
    wr16(obj + 0x08, rom16(a0 + 2));
    wr16(obj + 0x0A, rom16(a0 + 4));
    wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x8000);
    GemsOrbit(obj);
}

/* $0515D6. The screen shakes (sprites 2, planes 6), the palettes' priority bit; every other
 * frame a flash ($5639C, type $60C) dropping from $18 below it. */
void GemsFlash(uint16_t obj)
{
    wr16(v_shakeSprites, 2);
    wr16(v_shakePlane, 6);
    wr16(0x808A, 0x8000);
    if (bit8(v_gameFrames + 1, 0)) return;
    uint16_t slot, t;
    if (!SlotFreeFromC980(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x60C);
    wr32(slot + 0x48, 0x05639C);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    add16(slot + O_Y, 0x18);
    wr16(slot + 0x1C, 0x10);
}

/* $051370. Step 0: hidden, radius 0 toward $10, the four gems made (their sprites from
 * $61B14); then step 4 (rising), or with $A204 $10 step $28 (following the boss). */
void Gems00(uint16_t obj)
{
    wr16(obj + O_STATE, 0x28);
    if (rd16(v_stage4) != 0x10) wr16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0);
    wr32(obj + 0x5C, 0);
    wr16(obj + 0x4A, 0x10);
    uint16_t a0 = 0xCA40;
    uint32_t a1 = 0x061B14;
    for (int i = 0; i < 4; i++) {
        wr16(a0 + O_TYPE, 0x10);
        wr16(a0 + O_FLAGS, 0x8000);
        wr16(a0 + 0x0E, rom16(a1));
        a1 += 2;
        wr16(a0 + 0x08, 0);
        wr16(a0 + 0x0A, 0xFCFC);
        wr8(a0 + 0x20, 4);
        a0 = (uint16_t)(a0 + 0x60);
    }
}

/* $0513CA. Rising 1 a frame to y $120: then $A0 frames. */
void Gems04(uint16_t obj)
{
    GemSparks();
    GemsOrbit(obj);
    wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 1));
    if (!below(obj + O_Y, 0x120)) return;
    next(obj, 4);
    wr16(obj + 0x4E, 0xA0);
}

/* $0513EC. Steps 8, $18, $20: hidden, +$4E frames; then the radius toward $20. */
void Gems08(uint16_t obj)
{
    GemSparks();
    clr8(obj + O_FLAGS, 7);
    GemsOrbit(obj);
    if (!dec(obj + 0x4E)) return;
    next(obj, 4);
    wr16(obj + 0x4A, 0x20);
}

/* $05140E. Shining, rising to y $E0 (a sound $C3 every 8 frames): then $10 frames. */
void Gems0C(uint16_t obj)
{
    GemSparks();
    if (!(rd16(v_gameFrames) & 7)) QueueSound(0xC3);
    GemsShine(obj);
    wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 1));
    if (!below(obj + O_Y, 0xE0)) return;
    next(obj, 4);
    wr16(obj + 0x4E, 0x10);
}

/* $051446. Shining and flashing $10 frames; then the object at $C9E0 to its next step, $100
 * frames. */
void Gems10(uint16_t obj)
{
    GemsShine(obj);
    GemsFlash(obj);
    if (!dec(obj + 0x4E)) return;
    next(obj, 4);
    add16(0xC9E4, 2);
    wr16(obj + 0x4E, 0x100);
}

/* $051466. Shining and flashing; then $80 frames (step $18). */
void Gems14(uint16_t obj)
{
    GemsShine(obj);
    GemsFlash(obj);
    if (!dec(obj + 0x4E)) return;
    next(obj, 4);
    wr16(obj + 0x4E, 0x80);
}

/* $051482. Sinking 1 a frame to y $130 (a sound $B5 every 16 frames): then $30 frames. */
void Gems1C(uint16_t obj)
{
    GemSparks();
    if (!(rd16(v_gameFrames) & 0xF)) QueueSound(0xB5);
    GemsOrbit(obj);
    add16(obj + O_Y, 1);
    if (below(obj + O_Y, 0x130)) return;
    next(obj, 4);
    wr16(obj + 0x4E, 0x30);
}

/* $0514BA. The radius toward 0, moving right to x $1B0: then the scene's signal ($A028), it
 * and the gems removed. */
void Gems24(uint16_t obj)
{
    GemSparks();
    if (!(rd16(v_gameFrames) & 0xF)) QueueSound(0xB5);
    wr16(obj + 0x4A, 0);
    GemsOrbit(obj);
    add16(obj + O_X, 1);
    if (below(obj + O_X, 0x1B0)) return;
    wr8(v_sceneSignal, 1);
    set8(obj + O_FLAGS, 4);
    uint16_t a0 = 0xCA40;
    for (int i = 0; i < 4; i++) {
        set8(a0 + O_FLAGS, 4);
        a0 = (uint16_t)(a0 + 0x60);
    }
}

/* $05150C. At the boss ($C980), $10 above it, hidden, the radius toward $14. */
void Gems28(uint16_t obj)
{
    GemSparks();
    wr16(obj + O_X, rd16(v_boss + O_X));
    wr16(obj + O_Y, rd16(v_boss + O_Y));
    wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 0x10));
    clr8(obj + O_FLAGS, 7);
    wr16(obj + 0x4A, 0x14);
    GemsOrbit(obj);
}

/* $05133A. Type $608: the gems' holder, by its step. */
void Gems(uint16_t obj)
{
    step_by_longs(obj, 0x051344);
}

/* $051622. Type $60C: a flash (an animation list) dropping; once falling below y $140 it
 * bounces (+$5C on by 1: falling faster, $6000 a frame), x speed -4..+3. Its x speed's low word
 * is the ROM's word at $8000 (move.w ($8000).l, kept: an immediate #$8000 was meant). */
void Flash60C(uint16_t obj)
{
    if (rd16(obj + 0x5C)) add32(obj + 0x1C, 0x6000);
    AnimList(obj);
    if (rd16(obj + 0x1C) & 0x8000) return;
    if (below(obj + O_Y, 0x140)) return;
    add16(obj + 0x5C, 1);
    wr16(obj + 0x18, (uint16_t)((rd8(v_randomSeed + 1) & 7) - 4));
    wr16(obj + 0x1A, rom16(0x008000));
    wr16(obj + 0x1C, (uint16_t)((rd8(v_randomSeed) & 3) - 6));
}

/* ---- types $650, $654: a gunner (vitality $78) facing the player, firing ($90) every
 * +$48 frames (by the difficulty's step, $517E0); $654 fires from 2 up, $650 (flipped) from 4
 * down; left of x $80 removed; shot, a $54 ---- */

/* $0517C4. The wait before its next shot: $517E0 by the difficulty's step, plus 0-$3F. */
void Gunner650Wait(uint16_t obj)
{
    Random();
    uint16_t d1 = rd16(v_randomSeed) & 0x3F;
    d1 = (uint16_t)(d1 + tab16(0x0517E0, rd16(0xA20C)));
    wr16(obj + 0x48, d1);
}

/* $0516D6. Types $650, $654. */
void Gunner650(uint16_t obj)
{
    if (below(obj + O_X, 0x80)) {
        set8(obj + O_FLAGS, 4);
        return;
    }
    if (rd16(obj + O_VITALITY) & 0x8000) {
        AddScore(obj, 0x53);
        wr16(obj + O_TYPE, 0x54);
        wr16(obj + O_STATE, 0);
        wr16(v_shakeSprites, 8);
        return;
    }
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + 0x4A, 1);
        wr8(obj + 0x20, 0x80);
        wr16(obj + O_FLAGS, 0xE100);
        wr32(obj + 0x08, 0x0E2A58);
        wr16(obj + 0x0E, 0x300);
        wr16(obj + 0x4C, 0xFFFE);
        if (rd16(obj + O_TYPE) != 0x654) {
            set8(obj + 0x0E, 4);
            wr16(obj + 0x4C, 4);
        }
        wr8(obj + 0x21, 0x20);
        wr32(obj + 0x28, 0xF010F010);
        wr16(obj + O_VITALITY, 0x78);
        Gunner650Wait(obj);
    }
    if (!below(obj + O_X, 0x1D0)) return;
    if (!dec(obj + 0x4A)) {
        if (rd16(obj + 0x4A)) return;
        anim(obj, 0x0E2A58);
    }
    uint16_t target = PlayerTarget();
    set8(obj + 0x0E, 3);
    uint16_t d6 = 0xFFF0, d7 = 0x40;
    if ((int16_t)(rd16(target + O_X) - rd16(obj + O_X)) >= 0) {
        clr8(obj + 0x0E, 3);
        d6 = 0x10;
        d7 = 0;
    }
    if (!dec(obj + 0x48)) return;
    uint16_t slot;
    if (!SpawnAtMid(obj, 0x90, rd16(obj + 0x4C), d6, d7, &slot)) return;
    anim(obj, 0x0E2A72);
    wr16(obj + 0x4A, 0x10);
    Gunner650Wait(obj);
}

/* ---- type $39C: the walker, a robot on two legs (19 parts of type $10: itself, $CBC0 on; the
 * legs' three segments and feet at $CCE0-$CE00 and $CEC0-$CFE0, the feet's shadows $D100-
 * $D220, the head $D040, $D0A0). +$54/+$56 and +$58/+$5A are each leg's angle and segment
 * length, +$50 the part the others are placed from (the foot standing), +$5C its way. Beaten
 * it breaks apart: the head ($D0A0) becomes a $3A4 ---- */

#define MECH_FOOT_A 0xCE00
#define MECH_FOOT_B 0xCFE0

/* $052640. A part at a0 (type $10, $C000, sprite $B00): its mappings d0, y d1, +$20 d2,
 * +$21 d1; a0 on to the next slot. */
uint16_t MechPart(uint16_t at, uint32_t map, uint16_t d1, uint8_t d2)
{
    wr16(at + O_TYPE, 0x10);
    wr16(at + O_FLAGS, 0xC000);
    wr16(at + 0x0E, 0xB00);
    wr32(at + 0x08, map);
    wr16(at + O_Y, d1);
    wr8(at + 0x20, d2);
    wr8(at + 0x21, (uint8_t)d1);
    return (uint16_t)(at + 0x60);
}

/* $052686. A small part at a0 (type $10, $8000): y d1, +$21 d1, +$20 d2; a0 on. */
uint16_t MechPartSmall(uint16_t at, uint16_t d1, uint8_t d2)
{
    wr16(at + O_TYPE, 0x10);
    wr16(at + O_FLAGS, 0x8000);
    wr16(at + O_Y, d1);
    wr8(at + 0x21, (uint8_t)d1);
    wr8(at + 0x20, d2);
    return (uint16_t)(at + 0x60);
}

/* $052666. Four leg segments ($444D, 4x4) from a0. */
uint16_t MechSegments(uint16_t at, uint16_t d1, uint8_t d2)
{
    for (int i = 0; i < 4; i++) {
        wr16(at + 0x0E, 0x444D);
        wr16(at + 0x08, 0);
        wr16(at + 0x0A, 0xFCFC);
        at = MechPartSmall(at, d1, d2);
    }
    return at;
}

/* $052614. A shadow's left half ($445A). */
uint16_t MechShadowLeft(uint16_t at, uint16_t d1, uint8_t d2)
{
    wr16(at + 0x0E, 0x445A);
    wr16(at + 0x08, 0x400);
    wr16(at + 0x0A, 0xF800);
    return MechPartSmall(at, d1, d2);
}

/* $05262A. Its right half ($445D). */
uint16_t MechShadowRight(uint16_t at, uint16_t d1, uint8_t d2)
{
    wr16(at + 0x0E, 0x445D);
    wr16(at + 0x08, 0x400);
    wr16(at + 0x0A, 0xF800);
    return MechPartSmall(at, d1, d2);
}

/* $05258A. The walker's parts made: itself ($E29F6), two arms ($E2A3E), a leg's segments, its
 * foot ($E2A16), the other leg's, its foot, the head ($E281C, $E285A), the shadows. (d0 is
 * set to $A8 and $20 before the segments, which do not read it.) */
void MechBuild(uint16_t obj)
{
    uint16_t a0 = obj;
    a0 = MechPart(a0, 0x0E29F6, 0, 20);
    a0 = 0xCBC0;
    a0 = MechPart(a0, 0x0E2A3E, 0, 16);
    a0 = MechPart(a0, 0x0E2A3E, 0, 0x98);
    a0 = MechSegments(a0, 0, 0x98);
    a0 = MechPart(a0, 0x0E2A16, 0, 28);
    a0 = MechSegments(a0, 0, 28);
    a0 = MechPart(a0, 0x0E2A16, 0, 0xA4);
    a0 = MechPart(a0, 0x0E281C, 0, 0x98);
    a0 = MechPart(a0, 0x0E285A, 0, 16);
    a0 = MechShadowLeft(a0, 0, 0xA0);
    a0 = MechShadowRight(a0, 0, 0xA8);
    a0 = MechShadowLeft(a0, 0, 0x18);
    MechShadowRight(a0, 0, 0x20);
}

/* $052722. A leg's three segments from a0 (their offsets +$48, +$4C from the walker's place):
 * at d0, d1 plus the angle d2's cosine and sine times 1, 2, 3 lengths d3; then its foot 8 to
 * the left of the last. (a1, a2: the sine and cosine tables, as every caller sets them.) */
void MechLeg(uint16_t d0, uint16_t d1, uint16_t d2, uint16_t d3, uint16_t at)
{
    uint16_t d4 = d3, d5 = 0, d6 = 0;
    for (int i = 0; i < 3; i++) {
        int32_t s = (int16_t)rom16(0x02977C + (uint32_t)(int16_t)d2);
        int32_t c = (int16_t)rom16(0x0297FC + (uint32_t)(int16_t)d2);
        d5 = (uint16_t)((uint32_t)(s * (int16_t)d4) >> 16);
        d6 = (uint16_t)((uint32_t)(c * (int16_t)d4) >> 16);
        d5 = (uint16_t)(d5 + d1);
        d6 = (uint16_t)(d6 + d0);
        wr16(at + 0x4C, d5);
        wr16(at + 0x48, d6);
        d4 = (uint16_t)(d4 + d3);
        at = (uint16_t)(at + 0x60);
    }
    wr16(at + 0x4C, d5);
    wr16(at + 0x48, (uint16_t)(d6 - 8));
}

/* $0526A2. The parts' offsets: the arms by +$4A, +$4E, the head's fixed, the legs from their
 * hips (32, 16) and (-8, 16) by their angles and lengths. */
void MechLegs(uint16_t obj)
{
    uint16_t d0 = (uint16_t)(12 + rd16(obj + 0x4E));
    wr16(0xCC0C, d0);
    wr16(0xCC6C, d0);
    wr16(0xCC08, (uint16_t)(4 + rd16(obj + 0x4A)));
    wr16(0xCC68, (uint16_t)(0xFFE8 + rd16(obj + 0x4A)));
    wr16(0xD088, 4);
    wr16(0xD08C, 0xFFE9);
    wr16(0xD0E8, 0x14);
    wr16(0xD0EC, 0xFFEA);
    wr16(0xCCC8, 32);
    wr16(0xCCCC, 16);
    MechLeg(32, 16, rd16(obj + 0x54), rd16(obj + 0x56), 0xCCE0);
    wr16(0xCEA8, 0xFFF8);
    wr16(0xCEAC, 0x10);
    MechLeg(0xFFF8, 0x10, rd16(obj + 0x58), rd16(obj + 0x5A), 0xCEC0);
}

/* $052776. A foot's shadow (two halves at a1, a2): $10 left and $A right of it, $12 below
 * (at most y $128). */
void MechShadow(uint16_t foot, uint16_t left, uint16_t right)
{
    wr16(left + O_X, (uint16_t)(rd16(foot + O_X) - 0x10));
    wr16(right + O_X, (uint16_t)(rd16(foot + O_X) + 0x0A));
    uint16_t y = (uint16_t)(rd16(foot + O_Y) + 0x12);
    if (!((int16_t)(y - 0x128) < 0)) y = 0x128;
    wr16(left + O_Y, y);
    wr16(right + O_Y, y);
}

/* $05275A. Both feet's shadows. */
void MechShadows(void)
{
    MechShadow(MECH_FOOT_A, 0xD1C0, 0xD220);
    MechShadow(MECH_FOOT_B, 0xD100, 0xD160);
}

/* loc_051A9A: the parts placed; the head's frames ($E285A; $E281C or, by the leg's angle,
 * $E282A) */
static void mech_tail(uint16_t obj)
{
    MechLegs(obj);
    GroupPlace(obj, 0x0E);
    MechShadows();
    wr32(0xD0A8, 0x0E285A);
    wr32(0xD048, bit8(obj + 0x55, 6) ? 0x0E282A : 0x0E281C);
}

/* loc_051A8A: a step's thud (a sound $A6, a shake), then the parts placed */
static void mech_land(uint16_t obj)
{
    QueueSound(0xA6);
    wr16(v_shakeSprites, 8);
    mech_tail(obj);
}

/* a foot put down (+$50 it, its y $11A) */
static void mech_stand(uint16_t obj, uint16_t foot)
{
    wr16(obj + 0x50, foot);
    wr16(foot + O_Y, 0x11A);
}

/* $0527D2. Which way: the player to the left, Z clear; else Z set one time in 16 (the random
 * seed plus x). dx (d1) returned. */
int MechChooseLeft(uint16_t obj, uint16_t *d1)
{
    uint16_t dy, p;
    NearestPlayer(obj, d1, &dy, &p);
    if (*d1 & 0x8000) return 0;
    return ((uint16_t)(rd16(v_randomSeed) + rd16(obj + O_X)) & 0xF) == 0;
}

/* $0527E2. The same with the player to the right. */
int MechChooseRight(uint16_t obj, uint16_t *d1)
{
    uint16_t dy, p;
    NearestPlayer(obj, d1, &dy, &p);
    if (!(*d1 & 0x8000)) return 0;
    return ((uint16_t)(rd16(v_randomSeed) + rd16(obj + O_X)) & 0xF) == 0;
}

/* $052886. +$52: the pause by the difficulty ($528A4) plus 0-$F. */
void MechPause(uint16_t obj)
{
    uint16_t d0 = rom16(0x0528A4 + (uint32_t)(int16_t)rd16(v_difficulty));
    wr16(obj + 0x52, (uint16_t)(d0 + (rd16(v_randomSeed) & 0xF)));
}

/* loc_052922: every 4th frame (as checked by the callers) a bolt ($3A0, +$48 d5) at a1's
 * place plus d6, d7; a sound $B6 */
static void mech_bolt(uint16_t a1, uint16_t d5, uint16_t d6, uint16_t d7)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x3A0);
    wr16(slot + 0x48, d5);
    wr16(slot + 0x4A, 1);
    wr16(slot + O_X, rd16(a1 + O_X));
    add16(slot + O_X, d6);
    wr16(slot + O_Y, rd16(a1 + O_Y));
    add16(slot + O_Y, d7);
    wr16(slot + O_FLAGS, 0x8C00);
    wr16(slot + 0x0E, 0x546);
    wr16(slot + 0x08, 0x500);
    wr16(slot + 0x0A, 0xF8F8);
    QueueSound(0xB6);
}

/* loc_052900: every 4th frame a bolt from the arm at $CBC0, or $CC20 on frames with bit 2 */
static void mech_arm_bolt(uint16_t d5, uint16_t d6, uint16_t d7)
{
    if (rd16(v_gameFrames) & 3) return;
    mech_bolt(bit8(v_gameFrames + 1, 2) ? 0xCC20 : 0xCBC0, d5, d6, d7);
}

/* $0528AC. Firing ahead: the arms' recoil (+$4A 5, +$4E -5 on frames with bit 2, else 0), a
 * bolt (+$48 0) 16 left and 16 below an arm. */
void MechFire(uint16_t obj)
{
    if (bit8(v_gameFrames + 1, 2)) {
        wr16(obj + 0x4A, 5);
        wr16(obj + 0x4E, 0xFFFB);
    } else {
        wr16(obj + 0x4A, 0);
        wr16(obj + 0x4E, 0);
    }
    mech_arm_bolt(0, 0xFFF0, 16);
}

/* $0528DE. Firing down: the recoil (+$4A 5, +$4E -1), a bolt (+$48 10) $18 left of an arm. */
void MechFireDown(uint16_t obj)
{
    if (!bit8(v_gameFrames + 1, 2)) {
        wr16(obj + 0x4A, 0);
        wr16(obj + 0x4E, 0);
    } else {
        wr16(obj + 0x4A, 5);
        wr16(obj + 0x4E, 0xFFFF);
    }
    mech_arm_bolt(10, 0xFFE8, 0);
}

/* $052970. (The head, type $3A4.) Every 4th frame a bolt from it at d6, d7 (d5 on by $14 and
 * d6 the other way unflipped) plus 0-7 each way at random. */
void MechHeadBolt(uint16_t obj, uint16_t d5, uint16_t d6, uint16_t d7)
{
    if (rd16(v_gameFrames) & 3) return;
    if (!bit8(obj + 0x0E, 3)) {
        d5 = (uint16_t)(d5 + 0x14);
        d6 = (uint16_t)(~d6 + 1);
    }
    d6 = (uint16_t)(d6 + (rd8(v_randomSeed) & 7));
    d7 = (uint16_t)(d7 + (rd8(v_randomSeed + 1) & 7));
    mech_bolt(obj, d5, d6, d7);
}

/* $0529AA. Wrecked: shown every other frame, explosions' sound; bouncing on the floor at $124
 * (+$52 times, a shake, then hidden); every other frame smoke ($5639C) about it. */
void MechWreck(uint16_t obj)
{
    if (rd16(obj + 0x52) & 0x8000) return;
    if (!bit8(v_gameFrames + 1, 0)) clr8(obj + O_FLAGS, 7);
    else set8(obj + O_FLAGS, 7);
    BlastSound8();
    add32(obj + 0x1C, 0x4000);
    if (!((int32_t)rd32(obj + 0x1C) < 0) && !below(obj + O_Y, 0x124)) {
        wr16(v_shakeSprites, 0x0A);
        wr32(obj + 0x1C, 0xFFFCC000);
        if (dec(obj + 0x52)) wr16(obj + O_FLAGS, 0);
    }
    if (bit8(v_gameFrames + 1, 0)) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x05639C);
    wr16(slot + O_X, (uint16_t)((rd8(v_randomSeed) & 0x3F) - 0x1F + rd16(obj + O_X)));
    wr16(slot + O_Y, (uint16_t)((rd8(v_randomSeed) & 0x3F) - 0x1F + rd16(obj + O_Y)));
    wr16(slot + 0x1A, 0x8000);
    wr16(slot + 0x1C, 0xFFFE);
}

/* $05224C. The head's frames: $E2838/$E286E, or $E281C/$E285A on frames with bit 2. */
void MechHeadFrames(void)
{
    if (!bit8(v_gameFrames + 1, 2)) {
        wr32(0xD048, 0x0E2838);
        wr32(0xD0A8, 0x0E286E);
    } else {
        wr32(0xD048, 0x0E281C);
        wr32(0xD0A8, 0x0E285A);
    }
}

/* $0519C6. Step 0: once the messages are done, its parts made, at x $170, y $60, falling. */
void Mech00(uint16_t obj)
{
    if (rd16(0x80C2)) return;
    next(obj, 4);
    wr8(obj + 7, 3);
    wr32(obj + 0x48, 0);
    wr32(obj + 0x4C, 0);
    wr16(obj + 0x54, 0x80);
    wr16(obj + 0x56, 0x18);
    wr16(obj + 0x58, 0x80);
    wr16(obj + 0x5A, 0x18);
    wr8(obj + 0x46, 0x8F);
    wr32(obj + 0x2C, 0xEB08E020);
    wr32(obj + 0x28, 0xEB08E020);
    MechBuild(obj);
    wr16(obj + O_TYPE, 0x39C);
    wr16(obj + O_FLAGS, 0xCC00);
    wr8(obj + 0x21, 0x24);
    wr16(obj + 0x50, obj);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + O_X, 0x170);
    wr16(obj + O_Y, 0x60);
}

/* $051A46. Falling ($6000 a frame), its legs turning, until the foot at $CE00 is down at $11A:
 * standing on it. */
void Mech04(uint16_t obj)
{
    uint16_t d0 = (uint16_t)(rd16(obj + 0x54) - 2) & 0x1FE;
    wr16(obj + 0x54, d0);
    wr16(obj + 0x58, d0);
    add32(obj + 0x1C, 0x6000);
    if (below(0xCE14, 0x11A)) {
        mech_tail(obj);
        return;
    }
    next(obj, 4);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    mech_stand(obj, MECH_FOOT_A);
    mech_land(obj);
}

/* $051AB0. Crouching (the legs shortening to 0, turning to $1C0), the screen shaking: then $10
 * frames (step $10). */
void Mech08(uint16_t obj)
{
    wr16(v_shakeSprites, 0x0C);
    if (rd16(obj + 0x56)) {
        wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 1));
        wr16(obj + 0x5A, rd16(obj + 0x56));
    }
    uint16_t d0 = (uint16_t)(rd16(obj + 0x54) - 4) & 0x1FC;
    wr16(obj + 0x54, d0);
    wr16(obj + 0x58, d0);
    if (d0 == 0x1C0) {
        next(obj, 8);
        wr16(obj + 0x52, 0x10);
    }
    mech_tail(obj);
}

/* $051AF0. (Not reached: step 8 goes to $10.) The legs lengthening for +$52 frames. */
void Mech0C(uint16_t obj)
{
    add16(obj + 0x56, 1);
    wr16(obj + 0x5A, rd16(obj + 0x56));
    wr16(v_shakeSprites, 8);
    if (!dec(obj + 0x52)) {
        mech_tail(obj);
        return;
    }
    next(obj, 4);
}

/* $051B0E. Rising (the legs back to $18, turning to $50). */
void Mech10(uint16_t obj)
{
    if (below(obj + 0x56, 0x18)) {
        add16(obj + 0x56, 1);
        wr16(obj + 0x5A, rd16(obj + 0x56));
    }
    wr16(v_shakeSprites, 2);
    uint16_t d0 = (uint16_t)(rd16(obj + 0x54) + 2) & 0x1FE;
    wr16(obj + 0x54, d0);
    wr16(obj + 0x58, d0);
    if (d0 == 0x50) {
        next(obj, 4);
        wr16(obj + 0x52, 0x80);
    }
    mech_tail(obj);
}

/* $051B50. $80 frames; then the fight: vitality $B5D, the boss's bar ($80C2 $10, $80D6 it),
 * step $38 after $40 frames. */
void Mech14(uint16_t obj)
{
    if (dec(obj + 0x52)) {
        wr16(obj + O_STATE, 0x38);
        wr16(obj + 0x52, 0x40);
        wr8(obj + 7, 0);
        wr16(obj + O_VITALITY, 0xB5D);
        wr16(0x80C2, 0x10);
        wr16(0x80D6, obj);
    }
    mech_tail(obj);
}

/* $051B7E. Steps $18, $54: on the foot at $CE00, leg A to angle 0 (by 4), leg B to $100 (by
 * $10): then onto the other foot. */
void Mech18(uint16_t obj)
{
    wr16(obj + 0x50, MECH_FOOT_A);
    int done = 0;
    if (rd16(obj + 0x54) == 0) done |= 1;
    else wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) - 4) & 0x1FC);
    if (rd16(obj + 0x58) == 0x100) done |= 2;
    else wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 0x10) & 0x1F0);
    if (done != 3) {
        mech_tail(obj);
        return;
    }
    next(obj, 4);
    mech_stand(obj, MECH_FOOT_B);
    mech_land(obj);
}

/* $051BDC. On the foot at $CFE0, both legs turning back by 8 to 0: then on (the player to the
 * left, or 15 times in 16) onto the foot at $CE00, else step $2C. */
void Mech1C(uint16_t obj)
{
    wr16(obj + 0x50, MECH_FOOT_B);
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) - 8) & 0x1F8);
    wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 8) & 0x1F8);
    if (rd16(obj + 0x58)) {
        mech_tail(obj);
        return;
    }
    uint16_t d1 = 0;
    if (MechChooseLeft(obj, &d1)) {
        wr16(obj + O_STATE, 0x2C);
        mech_land(obj);
        return;
    }
    next(obj, 4);
    mech_stand(obj, MECH_FOOT_A);
    mech_land(obj);
}

/* $051C20. On the foot at $CE00, both legs back by 8 to 0: then (the same choice) step $28, or
 * onto the other foot: on to the left (step $1C) or round to step $30 by its way (+$5C), its x
 * and the player's distance. */
void Mech20(uint16_t obj)
{
    wr16(obj + 0x50, MECH_FOOT_A);
    wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 8) & 0x1F8);
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) - 8) & 0x1F8);
    if (rd16(obj + 0x54)) {
        mech_tail(obj);
        return;
    }
    uint16_t d1 = 0;
    if (MechChooseLeft(obj, &d1)) {
        wr16(obj + O_STATE, 0x28);
        mech_land(obj);
        return;
    }
    mech_stand(obj, MECH_FOOT_B);
    int on;
    if (rd16(obj + 0x5C)) on = !below(obj + O_X, 0x1A0);
    else on = (d1 & 0x8000) && !((int16_t)(d1 - 0xFF88) < 0) && !below(obj + O_X, 0x90);
    if (on) back(obj, 4);
    else wr16(obj + O_STATE, 0x30);
    mech_land(obj);
}

/* $051C50. On the foot at $CFE0, leg A to 0 (by $10), leg B to $100 (by 8): then onto the foot
 * at $CE00. */
void Mech24(uint16_t obj)
{
    wr16(obj + 0x50, MECH_FOOT_B);
    int done = 0;
    if (rd16(obj + 0x54) == 0) done |= 1;
    else wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) + 0x10) & 0x1F0);
    if (rd16(obj + 0x58) == 0x100) done |= 2;
    else wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) + 8) & 0x1F8);
    if (done != 3) {
        mech_tail(obj);
        return;
    }
    next(obj, 4);
    mech_stand(obj, MECH_FOOT_A);
    mech_land(obj);
}

/* $051CAE. On the foot at $CE00, both legs on by 8 to 0: then (the player to the right, or 15
 * times in 16) onto the other foot: on (step $2C) or to step $34; else step $20. */
void Mech28(uint16_t obj)
{
    wr16(obj + 0x50, MECH_FOOT_A);
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) + 8) & 0x1F8);
    wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) + 8) & 0x1F8);
    if (rd16(obj + 0x58)) {
        mech_tail(obj);
        return;
    }
    uint16_t d1 = 0;
    if (MechChooseRight(obj, &d1)) {
        wr16(obj + O_STATE, 0x20);
        mech_land(obj);
        return;
    }
    mech_stand(obj, MECH_FOOT_B);
    int on;
    if (rd16(obj + 0x5C)) on = below(obj + O_X, 0x1C0);
    else on = !(d1 & 0x8000);
    if (on) next(obj, 4);
    else wr16(obj + O_STATE, 0x34);
    mech_land(obj);
}

/* $051CDE. On the foot at $CFE0, both legs on by 8 to 0: then (the same choice) back onto the
 * foot at $CE00 (step $28), else step $1C. */
void Mech2C(uint16_t obj)
{
    wr16(obj + 0x50, MECH_FOOT_B);
    wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) + 8) & 0x1F8);
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) + 8) & 0x1F8);
    if (rd16(obj + 0x54)) {
        mech_tail(obj);
        return;
    }
    uint16_t d1 = 0;
    if (MechChooseRight(obj, &d1)) {
        wr16(obj + O_STATE, 0x1C);
        mech_land(obj);
        return;
    }
    back(obj, 4);
    mech_stand(obj, MECH_FOOT_A);
    mech_land(obj);
}

/* $051D22. On the foot at $CFE0, leg A to $50 (by 2), leg B to $50 (back by 4): then step $38
 * after a pause. */
void Mech30(uint16_t obj)
{
    wr16(obj + 0x50, MECH_FOOT_B);
    int done = 0;
    if (rd16(obj + 0x54) == 0x50) done |= 1;
    else wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) + 2) & 0x1FE);
    if (rd16(obj + 0x58) == 0x50) done |= 2;
    else wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 4) & 0x1FC);
    if (done == 3) {
        next(obj, 8);
        MechPause(obj);
    }
    mech_tail(obj);
}

/* $051D76. The same with leg A by 8 and leg B by 2. */
void Mech34(uint16_t obj)
{
    wr16(obj + 0x50, MECH_FOOT_B);
    int done = 0;
    if (rd16(obj + 0x54) == 0x50) done |= 1;
    else wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) + 8) & 0x1F8);
    if (rd16(obj + 0x58) == 0x50) done |= 2;
    else wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) + 2) & 0x1FE);
    if (done == 3) {
        next(obj, 4);
        MechPause(obj);
    }
    mech_tail(obj);
}

/* $051DCA. Standing +$52 frames; then what next, by its x, the player's side and distance, the
 * difficulty: walk left (step $18, +$5C 0 or 2) or right (step $24), crouch to leap (step $3C),
 * fire (step $60), or (no run takes them) steps $54, $64. */
void Mech38(uint16_t obj)
{
    if (!(rd16(obj + 0x52) & 0x8000)) {
        wr16(obj + 0x52, (uint16_t)(rd16(obj + 0x52) - 1));
        mech_tail(obj);
        return;
    }
    if (!below(obj + O_X, 0x1C0)) {
        wr16(obj + 0x5C, 2);
        wr16(obj + O_STATE, 0x18);
        return;
    }
    if (below(obj + O_X, 0x80)) {
        if (below(v_difficulty, 4)) {
            wr16(obj + 0x5C, 2);
            wr16(obj + O_STATE, 0x24);
            return;
        }
        wr16(obj + O_STATE, 0x3C);
        mech_tail(obj);
        return;
    }
    wr16(obj + 0x54, 0x50);
    wr16(obj + 0x58, 0x50);
    uint16_t d1, dy, p;
    uint16_t d0 = NearestPlayer(obj, &d1, &dy, &p);
    if ((d1 & 0x8000) && !bit8(obj + 0x47, 4)) {
        if (!((int16_t)(d0 - 0x68) < 0)) {
            wr16(obj + O_STATE, bit8(v_randomSeed + 1, 0) ? 0x64 : 0x54);
            mech_tail(obj);
            return;
        }
        if (!(rd16(v_gameFrames) & 3)) {
            wr16(obj + 0x5C, 0);
            wr16(obj + O_STATE, 0x18);
            return;
        }
        wr16(obj + O_STATE, 0x60);
        wr16(obj + 0x52, 0x40);
        mech_tail(obj);
        return;
    }
    d0 = rd16(v_difficulty);
    if (d0 == 0 || (d0 == 2 && !bit8(v_gameFrames + 1, 6))) {
        wr16(obj + 0x5C, 0);
        wr16(obj + O_STATE, 0x24);
        return;
    }
    wr16(obj + O_STATE, 0x3C);
    mech_tail(obj);
}

/* $051EBC. Crouching (both legs back by 4 to $1C0): then 5 frames. */
void Mech3C(uint16_t obj)
{
    if (rd16(obj + 0x58) == 0x1C0) {
        next(obj, 4);
        wr16(obj + 0x52, 5);
    } else {
        wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) - 4) & 0x1FC);
        wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 4) & 0x1FC);
    }
    mech_tail(obj);
}

/* $051F78. The legs straightening from the angle d0 (by $20, to $C0). */
void MechStretch(uint16_t obj, uint16_t d0)
{
    if (d0 != 0xC0) {
        d0 = (uint16_t)(d0 + 0x20) & 0x1E0;
        wr16(obj + 0x54, d0);
        wr16(obj + 0x58, d0);
    }
    mech_tail(obj);
}

/* $051EEC. The legs straightening for +$52 frames (from d0, which the objects' loop leaves as
 * the type, $39C: the first angle is ($39C + $20) & $1E0 = $1A0); then a leap (speeds 1.125,
 * -10; standing on itself), the arms' frames $E2A52. */
void Mech40(uint16_t obj, uint16_t d0)
{
    MechStretch(obj, d0);
    if (!dec(obj + 0x52)) {
        mech_tail(obj);
        return;
    }
    next(obj, 4);
    wr16(obj + 0x50, obj);
    wr32(obj + 0x1C, 0xFFF60000);
    wr32(obj + 0x18, 0x12000);
    wr32(0xCBC8, 0x0E2A52);
    wr32(0xCC28, 0x0E2A52);
    mech_tail(obj);
}

/* $051F24. In the air, firing: rising, the legs straightening; falling, they bend (by 4 to
 * $60) until the foot at $CE00 is down: standing on it, the arms' frames $E2A3E. */
void Mech44(uint16_t obj)
{
    MechFire(obj);
    uint16_t d0 = rd16(obj + 0x54);
    add32(obj + 0x1C, 0x6000);
    if ((int32_t)rd32(obj + 0x1C) < 0) {
        MechStretch(obj, d0);
        return;
    }
    if (below(0xCE14, 0x11A)) {
        if (d0 != 0x60) {
            d0 = (uint16_t)(d0 - 4) & 0x1FC;
            wr16(obj + 0x54, d0);
            wr16(obj + 0x58, d0);
        }
        mech_tail(obj);
        return;
    }
    next(obj, 4);
    wr16(obj + 0x4A, 0);
    wr16(obj + 0x4E, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    mech_stand(obj, MECH_FOOT_A);
    wr32(0xCBC8, 0x0E2A3E);
    wr32(0xCC28, 0x0E2A3E);
    mech_land(obj);
}

/* $051FA6. Landed, crouching (by 8 to $1C0): then 8 frames. */
void Mech48(uint16_t obj)
{
    uint16_t d0 = (uint16_t)(rd16(obj + 0x54) - 8) & 0x1F8;
    wr16(obj + 0x54, d0);
    wr16(obj + 0x58, d0);
    if (d0 == 0x1C0) {
        next(obj, 4);
        wr16(obj + 0x52, 8);
    }
    mech_tail(obj);
}

/* $051FCE. Then (left of x $1C0) another leap (step $40, 3 frames), else rising. */
void Mech4C(uint16_t obj)
{
    if (!dec(obj + 0x52)) {
        mech_tail(obj);
        return;
    }
    if (below(obj + O_X, 0x1C0)) {
        wr16(obj + O_STATE, 0x40);
        wr16(obj + 0x52, 3);
        wr32(0xCBC8, 0x0E2A52);
        wr32(0xCC28, 0x0E2A52);
        return;
    }
    next(obj, 4);
    mech_tail(obj);
}

/* $052006. Rising (by 4 to $50): then standing (step $38, no pause). */
void Mech50(uint16_t obj)
{
    uint16_t d0 = (uint16_t)(rd16(obj + 0x54) + 4) & 0x1FC;
    wr16(obj + 0x54, d0);
    wr16(obj + 0x58, d0);
    if (d0 == 0x50) {
        wr16(obj + O_STATE, 0x38);
        wr16(obj + 0x52, 0xFFFF);
    }
    mech_tail(obj);
}

/* $052030. On the foot at $CFE0, both legs back by $20 to 0: then onto the foot at $CE00. */
void Mech58(uint16_t obj)
{
    wr16(obj + 0x50, MECH_FOOT_B);
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) - 0x20) & 0x1E0);
    wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 0x20) & 0x1E0);
    if (rd16(obj + 0x58)) {
        mech_tail(obj);
        return;
    }
    next(obj, 4);
    mech_stand(obj, MECH_FOOT_A);
    mech_land(obj);
}

/* $052066. On the foot at $CE00, both back by $20 to 0: then onto the other foot (step $58
 * again, or left of x $80 step $30). */
void Mech5C(uint16_t obj)
{
    wr16(obj + 0x50, MECH_FOOT_A);
    wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 0x20) & 0x1E0);
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) - 0x20) & 0x1E0);
    if (rd16(obj + 0x54)) {
        mech_tail(obj);
        return;
    }
    if (below(obj + O_X, 0x80)) wr16(obj + O_STATE, 0x30);
    else back(obj, 4);
    mech_stand(obj, MECH_FOOT_B);
    mech_land(obj);
}

/* $0520B0. Firing for +$52 frames (the legs $50, or $44 when +$52 has bit 1); then crouch to
 * leap (step $3C) or stand (step $38, 8 frames, the arms' frames $E2A3E), by the random seed. */
void Mech60(uint16_t obj)
{
    wr32(0xCBC8, 0x0E2A52);
    wr32(0xCC28, 0x0E2A52);
    MechFire(obj);
    if (dec(obj + 0x52)) {
        wr16(obj + 0x4A, 0);
        wr16(obj + 0x4E, 0);
        if (!bit8(v_randomSeed + 1, 0)) {
            wr16(obj + O_STATE, 0x3C);
            wr16(obj + 0x54, 0x50);
            wr16(obj + 0x58, 0x50);
            mech_tail(obj);
            return;
        }
        wr16(obj + O_STATE, 0x38);
        wr16(obj + 0x52, 8);
        wr32(0xCBC8, 0x0E2A3E);
        wr32(0xCC28, 0x0E2A3E);
    }
    uint16_t a = bit8(obj + 0x53, 1) ? 0x44 : 0x50;
    wr16(obj + 0x54, a);
    wr16(obj + 0x58, a);
    mech_tail(obj);
}

/* $052132. Both legs back by 4 to 0: then $20 frames. */
void Mech64(uint16_t obj)
{
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) - 4) & 0x1FE);
    wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 4) & 0x1FE);
    if (!rd16(obj + 0x58)) {
        next(obj, 4);
        wr16(obj + 0x52, 0x20);
    }
    mech_tail(obj);
}

/* $052158. Then $40 frames. */
void Mech68(uint16_t obj)
{
    if (dec(obj + 0x52)) {
        next(obj, 4);
        wr16(obj + 0x52, 0x40);
    }
    mech_tail(obj);
}

/* $05216E. Firing down for +$52 frames (the legs 0, or $1F4 when +$52 has bit 1). */
void Mech6C(uint16_t obj)
{
    wr32(0xCBC8, 0x0E2A3E);
    wr32(0xCC28, 0x0E2A3E);
    MechFireDown(obj);
    if (dec(obj + 0x52)) {
        wr16(obj + 0x4A, 0);
        wr16(obj + 0x4E, 0);
        next(obj, 4);
        mech_tail(obj);
        return;
    }
    uint16_t a = bit8(obj + 0x53, 1) ? 0x1F4 : 0;
    wr16(obj + 0x54, a);
    wr16(obj + 0x58, a);
    mech_tail(obj);
}

/* $0521C8. Rising (both by 4 to $50): then walk left (frames with $A000 bit 2: step $18,
 * +$5C 2), else stand $10 frames. */
void Mech70(uint16_t obj)
{
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) + 4) & 0x1FE);
    wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) + 4) & 0x1FE);
    if (rd16(obj + 0x58) != 0x50) {
        mech_tail(obj);
        return;
    }
    if (bit8(v_gameFrames, 2)) {
        wr16(obj + 0x5C, 2);
        wr16(obj + O_STATE, 0x18);
        return;
    }
    wr16(obj + O_STATE, 0x38);
    wr16(obj + 0x52, 0x10);
    mech_tail(obj);
}

/* $052200. Beaten: the parts from $D040 back to $CC80 (+$52, skipping $D0A0) become blasts
 * ($54), one every 6 frames; the head flashing. */
void Mech74(uint16_t obj)
{
    wr16(v_shakeSprites, 2);
    if (!dec(obj + 0x5C)) {
        MechHeadFrames();
        return;
    }
    wr16(obj + 0x5C, 5);
    if (rd16(obj + 0x52) == 0xD0A0) wr16(obj + 0x52, (uint16_t)(rd16(obj + 0x52) - 0xC0));
    uint16_t a0 = rd16(obj + 0x52);
    wr16(a0 + O_TYPE, 0x54);
    wr16(a0 + O_STATE, 0);
    wr16(obj + 0x52, (uint16_t)(rd16(obj + 0x52) - 0x60));
    if (rd16(obj + 0x52) == 0xCC20) {
        next(obj, 4);
        wr16(obj + 0x5C, 5);
    }
    MechHeadFrames();
}

/* $05227A. Then the last blast: the arm at $CC20 a $8C, the head and the other arm thrown up,
 * the body bouncing away from the middle. */
void Mech78(uint16_t obj)
{
    MechHeadFrames();
    if (!dec(obj + 0x5C)) return;
    next(obj, 4);
    wr16(obj + 0x52, 2);
    wr16(obj + 0x5C, 0xC0);
    wr16(v_shakeSprites, 0x10);
    QueueSound(0xBD);
    wr16(0xCC20 + O_TYPE, 0x8C);
    wr16(0xCC20 + O_STATE, 0);
    wr16(0xCC20 + 0x48, 0);
    wr16(0xCC20 + 0x4A, 0x10);
    wr16(0xCC20 + 0x4C, 8);
    wr16(0xCC20 + O_X, rd16(obj + O_X));
    wr16(0xCC20 + O_Y, rd16(obj + O_Y));
    wr16(0xD042, 0xC400);
    wr16(0xD0A2, 0xCC00);
    wr16(0xCBC2, 0xCC00);
    wr32(0xD05C, 0xFFFB0000);
    wr32(0xD0BC, 0xFFFA0000);
    wr32(0xCBDC, 0xFFFB8000);
    wr32(0xD0B8, 0x9000);
    wr32(0xCBD8, 0x4000);
    wr32(0xD048, 0x0E2846);
    wr32(0xD0A8, 0x0E286E);
    wr32(0xCBC8, 0x0E2A3E);
    wr32(obj + 0x1C, 0xFFFE8000);
    wr32(obj + 0x18, below(obj + O_X, 0xB0) ? 0xC000 : 0xFFFF4000);
}

/* $05234A. Wrecked $C0 frames; then the head ($D040, $D0A0) thrown across. */
void Mech7C(uint16_t obj)
{
    MechWreck(obj);
    if (!dec(obj + 0x5C)) return;
    next(obj, 4);
    wr16(obj + 0x48, 0);
    wr32(0xD058, 0x29000);
    wr32(0xD0B8, 0x1A000);
    wr32(0xD05C, 0x40000);
    wr32(0xD0BC, 0x20000);
    wr16(0xD054, 0x80);
    wr16(0xD0B4, 0x10);
    wr16(0xD050, 0x190);
    wr16(0xD0B0, 0x12C);
    wr8(0xD060, 0x70);
    wr8(0xD0C0, 0x74);
    wr32(0xD0A8, 0x0E2888);
}

/* $0523AC. The head's pieces falling: $D040 bouncing at y $126 (+$48 more times, a sound $B0),
 * $D0A0 landing at $120 ($E28A8); both down: 8 frames. */
void Mech80(uint16_t obj)
{
    MechWreck(obj);
    int d1 = 0;
    add32(0xD05C, 0x4000);
    if (!((int32_t)rd32(0xD05C) < 0) && !below(0xD054, 0x126)) {
        wr16(0xD054, 0x126);
        if (!(rd16(obj + 0x48) & 0x8000)) {
            wr32(0xD05C, 0xFFFD2000);
            wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
        } else {
            wr32(0xD05C, 0);
            wr32(0xD058, 0);
            d1 |= 1;
        }
        QueueSound(0xB0);
    }
    add32(0xD0BC, 0x3800);
    if (below(0xD0B4, 0x120)) return;
    wr32(0xD0BC, 0);
    wr32(0xD0B8, 0);
    wr16(0xD0B4, 0x120);
    wr32(0xD0A8, 0x0E28A8);
    d1 |= 2;
    if (d1 != 3) return;
    next(obj, 4);
    wr16(obj + 0x5C, 8);
}

/* $052444. Then the head ($D0A0) jumps and the arm at $CBC0 is thrown from x $138, y $70. */
void Mech84(uint16_t obj)
{
    if (!dec(obj + 0x5C)) return;
    next(obj, 4);
    wr32(0xD0A8, 0x0E2888);
    wr32(0xD0BC, 0xFFFB8000);
    wr32(0xCBD8, 0x1A000);
    wr32(0xCBDC, 0x20000);
    wr16(0xCBD4, 0x70);
    wr16(0xCBD0, 0x138);
    wr8(0xCBE0, 0x70);
}

/* $052484. Both falling; once the head falls, the arm is removed, the head $E28E8. */
void Mech88(uint16_t obj)
{
    add32(0xCBDC, 0x4000);
    add32(0xD0BC, 0x3800);
    if ((int32_t)rd32(0xD0BC) < 0) return;
    next(obj, 4);
    wr16(0xCBC2, 0x1000);
    wr32(0xD0A8, 0x0E28E8);
}

/* $0524AC. The head lands at y $120 ($E28C8): $20 frames. */
void Mech8C(uint16_t obj)
{
    add32(0xD0BC, 0x3800);
    if (below(0xD0B4, 0x120)) return;
    next(obj, 4);
    wr16(obj + 0x5C, 0x1F);
    wr16(0xD0B4, 0x120);
    wr32(0xD0BC, 0);
    wr32(0xD0A8, 0x0E28C8);
}

/* $0524E0. Then it turns and jumps ($E29B6, speeds 4.125, -4.125), placed from $CE00. */
void Mech90(uint16_t obj)
{
    if (!dec(obj + 0x5C)) return;
    next(obj, 4);
    wr16(obj + 0x5C, 0x20);
    wr32(0xD0A8, 0x0E29B6);
    wr16(0xD0AE, rd16(0xD0AE) ^ 0x800);
    wr16(0xD042, 0xCE00);
    wr32(0xD058, 0x42000);
    wr32(0xD05C, 0xFFFBE000);
}

/* $052518. In the air $21 frames ($E2910 from the fifth); then turned back, $40 frames. */
void Mech94(uint16_t obj)
{
    if (rd16(obj + 0x5C) == 0x1B) wr32(0xD0A8, 0x0E2910);
    add32(0xD05C, 0x4000);
    if (!dec(obj + 0x5C)) return;
    next(obj, 4);
    wr32(0xD0A8, 0x0E29B6);
    wr16(0xD0AE, rd16(0xD0AE) ^ 0x800);
    wr16(obj + 0x5C, 0x40);
}

/* $052554. Then the head becomes a $3A4. */
void Mech98(uint16_t obj)
{
    if (!dec(obj + 0x5C)) return;
    next(obj, 4);
    wr16(0xD0A0 + O_TYPE, 0x3A4);
    wr16(0xD0A0 + O_STATE, 0);
    wr16(0xD0A0 + 0x24, 0);
}

/* $052574. Once the head is gone: removed, the next message ($80C2 $58). */
void Mech9C(uint16_t obj)
{
    if (rd16(0xD0A0)) return;
    wr16(obj + O_FLAGS, 0x1000);
    wr16(0x80C2, 0x58);
}

/* $05191C. Its step (d0 passed on: step $40 reads it). */
void MechStep(uint16_t obj, uint16_t d0)
{
    uint32_t target = rom32(0x051926 + (uint32_t)(int16_t)rd16(obj + O_STATE));
    if (target == 0x051EEC) Mech40(obj, d0);
    else call_object(target, obj);
}

/* $05188E. Type $39C: the walker. Beaten (once): step $74, its score ($39025). Its step (d0
 * from the objects' loop: the type). Hit, the head flashes ($E2838, $E286E) for 4 frames. */
void Mech(uint16_t obj)
{
    uint16_t d0 = rd16(obj + O_TYPE);
    if ((rd16(obj + O_VITALITY) & 0x8000) && !bit8(obj + 7, 1)) {
        wr16(obj + O_STATE, 0x74);
        AddScore(obj, 0x39025);
        set8(obj + 7, 1);
        set8(obj + 7, 0);
        wr16(obj + O_VITALITY, 0);
        wr8(obj + 0x21, 0);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr16(obj + 0x52, 0xD220);
        wr16(obj + 0x5C, 0xFFFF);
    }
    MechStep(obj, d0);
    if (bit8(obj + 7, 0)) return;
    if (!(rd16(obj + 0x5E) & 0x8000)) {
        wr16(obj + 0x5E, (uint16_t)(rd16(obj + 0x5E) - 1));
    } else {
        if (!bit8(obj + 0x22, 5)) return;
        wr16(obj + 0x5E, 3);
    }
    wr32(0xD048, 0x0E2838);
    wr32(0xD0A8, 0x0E286E);
}

/* $052A5A. Type $3A0: the walker's bolt: after +$4A frames its look and speed from $52B06 (by
 * +$48: 10 bytes each), moved once at its start; off the sides removed; hitting something or
 * below y $129 it becomes a puff ($565F6, type $100) rising. */
void MechBolt(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        if (!dec(obj + 0x4A)) return;
        next(obj, 4);
        uint32_t a0 = 0x052B06 + (uint32_t)(int32_t)(int16_t)rd16(obj + 0x48);
        wr16(obj + 0x0E, rom16(a0));
        wr16(obj + 0x08, rom16(a0 + 2));
        wr16(obj + 0x0A, rom16(a0 + 4));
        uint16_t d0 = rom16(a0 + 6), d1 = rom16(a0 + 8);
        wr16(obj + 0x18, d0);
        wr16(obj + 0x1C, d1);
        add16(obj + O_X, d0);
        add16(obj + O_Y, d1);
        wr8(obj + 0x21, 0x10);
        wr32(obj + 0x2C, 0xFC04FC04);
        wr16(obj + 0x26, 3);
    }
    if (below(obj + O_X, 0x80) || !below(obj + O_X, 0x1C0)) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    if (!bit8(obj + 0x22, 4) && below(obj + O_Y, 0x129)) return;
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x0565F6);
    wr16(obj + O_STATE, 0);
    wr8(obj + 0x21, 0);
    wr16(obj + 0x18, 0);
    wr16(obj + 0x1C, 0xFFFD);
}

/* ---- type $3A4: the walker's pilot (its head, once the walker is wrecked): hopping, leaping
 * at the player, firing; hit it is knocked back, grabbed and thrown; beaten it falls and smokes
 * and the allies go on (their step 4) ---- */

/* loc_030CF4 (the boss on foot's, type $380): blinking while +$5A counts down (hidden on odd
 * counts), +$23 bit 2 set meanwhile */
static void pilot_blink(uint16_t obj)
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

/* loc_050136: the allies' step 4 (the one at $A800, then the four from $C9E0 still their
 * types, +6's byte clear: their steps from $5019E, on by 8 for each one set) */
static void allies_step4(void)
{
    Stage6AllyA800(4);
    uint16_t d0 = 4;
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

/* $052CE0. Step 0: hurtable (vitality $33B, the boss's bar), standing ($E28C8). */
void Pilot00(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + 0x5A, 0xFFFF);
    wr16(obj + 0x5C, 6);
    wr8(obj + 7, 2);
    wr8(obj + 0x21, 0x22);
    wr32(obj + 0x28, 0xF010F808);
    wr16(obj + O_VITALITY, 0x33B);
    wr16(0x80C2, 0x10);
    wr16(0x80D6, obj);
    wr32(obj + 0x08, 0x0E28C8);
}

/* $052D1E. Standing +$48 frames; then facing the player, either fire (step $28, $18 frames) or
 * leap at it ($E29D6, speeds +-2.25, -5.19). */
void Pilot04(uint16_t obj)
{
    wr8(obj + 7, 2);
    if (!dec(obj + 0x48)) return;
    Boss380FacePlayer(obj);
    if (!bit8(v_randomSeed + 1, 0)) {
        wr16(obj + O_STATE, 0x28);
        wr16(obj + 0x48, 0x18);
        return;
    }
    wr16(obj + O_STATE, 0x24);
    wr32(obj + 0x08, 0x0E29D6);
    wr8(obj + 7, 0);
    wr32(obj + 0x1C, 0xFFFAD000);
    /* d1 (the player's dx) left by Boss380FacePlayer: negative exactly when it set the flip */
    wr32(obj + 0x18, bit8(obj + 0x0E, 3) ? 0xFFFDC000 : 0x24000);
}

/* $052D80. Fallen: shaking (y +$4A, 3 up, or $240 out of sight on even frames) +$48 frames;
 * then up ($E2900, shown), $20 frames. */
void Pilot08(uint16_t obj)
{
    if (!dec(obj + 0x48)) {
        if (!bit8(v_gameFrames + 1, 0)) {
            wr16(obj + O_Y, 0x240);
            return;
        }
        uint16_t d0 = rd16(obj + 0x4A);
        if (bit8(v_gameFrames + 1, 1)) d0 = (uint16_t)(d0 - 3);
        wr16(obj + O_Y, d0);
        return;
    }
    wr16(obj + O_Y, rd16(obj + 0x4A));
    wr16(obj + O_FLAGS, 0xE000);
    anim(obj, 0x0E2900);
    next(obj, 4);
    wr16(obj + 0x48, 0x20);
}

/* $052DD4. Then beaten for good: smoking (step $2C), sinking; a $2F4 at $DFA0 $10 below it, a
 * shake, a sound ($A5), the allies' step 4. */
void Pilot0C(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    wr16(obj + O_STATE, 0x2C);
    anim(obj, 0x0E2990);
    wr16(obj + O_FLAGS, 0xC700);
    wr16(obj + 0x1C, 0xFFF8);
    wr16(0xDFA0 + O_TYPE, 0x2F4);
    wr16(0xDFA0 + O_FLAGS, 0);
    wr16(0xDFA0 + O_STATE, 0);
    wr16(0xDFA0 + O_X, rd16(obj + O_X));
    wr16(0xDFA0 + O_Y, rd16(obj + O_Y));
    add16(0xDFA0 + O_Y, 0x10);
    wr16(v_shakeSprites, 0x10);
    QueueSound(0xA5);
    allies_step4();
}

/* loc_052EA8: standing again (step 4, at once), not hurtable */
static void pilot_stand(uint16_t obj)
{
    wr16(obj + O_STATE, 4);
    wr16(obj + 0x48, 0xFFFF);
    wr8(obj + 7, 2);
    wr32(obj + 0x08, 0x0E28C8);
}

/* $052EA0. Knocked: +$48 frames, then standing. */
void Pilot10(uint16_t obj)
{
    if (!dec(obj + 0x48)) return;
    pilot_stand(obj);
}

/* $052F0C. Falling ($3800 a frame) to y $120: standing. */
void Pilot14(uint16_t obj)
{
    add32(obj + 0x1C, 0x3800);
    if ((int32_t)rd32(obj + 0x1C) < 0) return;
    if (below(obj + O_Y, 0x120)) return;
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + O_Y, 0x120);
    pilot_stand(obj);
}

/* $052F3C. Grabbed until let go (+$23 bit 1, cleared): then thrown, facing against its speed. */
void Pilot18(uint16_t obj)
{
    if (bit8(obj + 0x23, 1)) {
        clr8(obj + 0x23, 1);
        return;
    }
    next(obj, 4);
    wr16(obj + 0x48, 1);
    if (bit8(obj + 0x18, 7)) clr8(obj + 0x0E, 3);
    else set8(obj + 0x0E, 3);
}

/* $052F6A. Thrown, bouncing at y $120 +$48 times (blinking); then standing, $126 off its
 * vitality, hurtable. */
void Pilot1C(uint16_t obj)
{
    add32(obj + 0x1C, 0x3800);
    if ((int32_t)rd32(obj + 0x1C) < 0) return;
    if (below(obj + O_Y, 0x120)) return;
    wr32(obj + 0x18, 0);
    if (!dec(obj + 0x48)) {
        wr32(obj + 0x1C, 0xFFFD8000);
        BlinkStart(obj);
        return;
    }
    wr16(obj + O_STATE, 4);
    wr32(obj + 0x1C, 0);
    wr16(obj + O_VITALITY, (uint16_t)(rd16(obj + O_VITALITY) - 0x126));
    wr8(obj + 0x21, 0x22);
    clr8(obj + 7, 0);
    set8(obj + 7, 1);
    wr16(obj + O_Y, 0x120);
    wr32(obj + 0x08, 0x0E28C8);
}

/* $052E68. Beaten, falling to y $120: then fallen (step 8, $40 frames). */
void Pilot20(uint16_t obj)
{
    add32(obj + 0x1C, 0x3800);
    if ((int32_t)rd32(obj + 0x1C) < 0) return;
    if (below(obj + O_Y, 0x120)) return;
    wr16(obj + O_Y, 0x120);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + O_STATE, 8);
    wr16(obj + 0x4A, rd16(obj + O_Y));
    wr16(obj + 0x48, 0x40);
}

/* $052EC4. Leaping: facing the player every $40 frames; above y $102 it fires (from difficulty
 * 4 always, from 1 only falling); then as step $14 ($14 frames). */
void Pilot24(uint16_t obj)
{
    if (!(rd16(v_gameFrames) & 0x3F)) Boss380FacePlayer(obj);
    if (rd16(v_difficulty) && below(obj + O_Y, 0x102)
        && (!below(v_difficulty, 4) || !bit8(obj + 0x1C, 7)))
        MechHeadBolt(obj, 0, 0xFFD8, 6);
    wr16(obj + 0x48, 0x14);
    Pilot14(obj);
}

/* $052FD4. Facing the player, firing +$48 frames; then standing $20. */
void Pilot28(uint16_t obj)
{
    Boss380FacePlayer(obj);
    if (!dec(obj + 0x48)) {
        MechHeadBolt(obj, 10, 0xFFDC, 0xFFF2);
        return;
    }
    wr16(obj + O_STATE, 4);
    wr16(obj + 0x48, 0x20);
}

/* $052E32. Smoking: a puff ($564FC, type $100) about it each frame, rising. */
void Pilot2C(uint16_t obj)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x0564FC);
    wr16(slot + O_X, (uint16_t)((rd16(v_randomSeed) & 3) - 2 + rd16(obj + O_X)));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr16(slot + 0x1C, 0xFFFF);
}

/* $052B38. Kept in x $89-$1B7. Beaten: its score ($3210), knocked flying (step $20) or, if
 * it was hurtable (+7 bit 1), fallen (step 8). Hit while hurtable: knocked (step $10, 3 frames)
 * or after 6 hits, or when +$23 bit 4 says, knocked flying (step $14, blinking). Let go
 * (+$23 bit 1): thrown (step $18). Then its step. */
void PilotThink(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) & 0x8000) {
        AddScore(obj, 0x3210);
        wr32(obj + 0x08, 0x0E2990);
        wr16(obj + 0x48, 0x40);
        wr16(obj + 0x4A, rd16(obj + O_Y));
        wr16(obj + O_VITALITY, 0);
        wr8(obj + 0x21, 0);
        wr16(obj + 0x5A, 0);
        if (!bit8(obj + 7, 1)) {
            wr16(obj + O_STATE, 0x20);
            wr32(obj + 0x1C, 0xFFFE0000);
            wr32(obj + 0x18, 0x18000);
            set8(obj + 0x0E, 3);
        } else {
            wr16(obj + O_STATE, 8);
            wr32(obj + 0x1C, 0);
            wr32(obj + 0x18, 0);
        }
        step_by_longs(obj, 0x052CB0);
        return;
    }
    if (below(obj + O_X, 0x88)) wr16(obj + O_X, 0x89);
    if (!below(obj + O_X, 0x1B8)) wr16(obj + O_X, 0x1B7);
    int knocked = 0;
    if (bit8(obj + 0x22, 5) && !bit8(obj + 7, 0)) {
        int was = bit8(obj + 0x23, 4);
        clr8(obj + 0x23, 4);
        if (was) {
            knocked = 1;
        } else if (bit8(obj + 7, 1)) {
            if (dec(obj + 0x5C)) {
                knocked = 1;
            } else {
                wr32(obj + 0x08, 0x0E2990);
                wr16(obj + O_STATE, 0x10);
                set8(obj + 7, 0);
                wr16(obj + 0x48, 3);
                wr32(obj + 0x18, 0);
                step_by_longs(obj, 0x052CB0);
                return;
            }
        }
        if (knocked) {
            wr32(obj + 0x08, 0x0E2990);
            QueueSound(0xAC);
            BlinkStart(obj);
            wr16(obj + 0x5C, 6);
            wr16(obj + O_STATE, 0x14);
            set8(obj + 7, 0);
            wr32(obj + 0x1C, 0xFFFC8000);
            wr32(obj + 0x18, bit8(obj + 0x0E, 3) ? 0x23000 : 0xFFFDD000);
            step_by_longs(obj, 0x052CB0);
            return;
        }
    }
    int let_go = bit8(obj + 0x23, 1);
    clr8(obj + 0x23, 1);
    if (let_go) {
        wr32(obj + 0x08, 0x0E2990);
        wr16(obj + O_STATE, 0x18);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        clr8(obj + 7, 0);
        wr8(obj + 0x21, 0);
    }
    step_by_longs(obj, 0x052CB0);
}

/* $052B2E. Type $3A4: the pilot, then its blinking. */
void Pilot(uint16_t obj)
{
    PilotThink(obj);
    pilot_blink(obj);
}

/* ---- type $404: the claw machine: a body with two arms (eleven objects from it: the body,
 * the shoulders $DBE0, $DF40, the arms' segments $DC40-$DEE0 with the claws at $DD60 and
 * $DEE0), the arms' joints at the angles +$54, +$56 (one arm) and +$58, +$5A (the other),
 * their reach from the sine table built at $9400 (radius by its kind, +7 bits 1-2). It
 * grabs down with one claw, then the other; beaten it falls apart ($2F4) and the next one
 * comes (kind 6 is the last) ---- */

#define CLAW_SINES   0x9400      /* the sine table SineTableBuild leaves (and $9480 its cosines) */
#define CLAW_COSINES 0x9480

/* $053590. A segment's sprite at a0 by the angle d2 (eight looks, $535B2); a6 is left past the
 * entry read. */
void ClawSegmentLook(uint16_t d2, uint16_t at)
{
    uint32_t d7 = (uint16_t)((int16_t)((uint16_t)(d2 + 8) & 0xE0) >> 2);
    uint32_t a6 = d7 + 0x0535B2;
    wr16(at + 0x0E, rom16(a6));
    wr16(at + 0x08, rom16(a6 + 2));
    wr16(at + 0x0A, rom16(a6 + 4));
    reg_a6 = a6 + 6;
}

/* $053518. An arm's upper part from a0: two objects at the angle d2's reach (x from the table
 * at $9400, y from $9480, 16.16) times 1 and 2: d3, d4 the first's offsets; a0 on by two. */
uint16_t ClawUpper(uint16_t d2, uint16_t at, uint32_t *d3, uint32_t *d4)
{
    d2 &= 0x1FE;
    *d3 = (uint32_t)(int32_t)(int16_t)rd16((uint16_t)(CLAW_SINES + d2)) << 8;
    *d4 = (uint32_t)(int32_t)(int16_t)rd16((uint16_t)(CLAW_COSINES + d2)) << 8;
    wr32(at + 0x48, *d3);
    wr32(at + 0x4C, *d4);
    ClawSegmentLook(d2, at);
    at = (uint16_t)(at + 0x60);
    *d3 <<= 1;
    *d4 <<= 1;
    wr32(at + 0x48, *d3);
    wr32(at + 0x4C, *d4);
    return (uint16_t)(at + 0x60);
}

/* $05354E. Its lower part: two objects from the elbow (d3, d4) out by the angle d2's reach,
 * the first from the end of d3, d4 by one, the second from d3, d4 by two. */
uint16_t ClawLower(uint16_t d2, uint16_t at, uint32_t *d3, uint32_t *d4)
{
    d2 &= 0x1FE;
    uint32_t d5 = *d3, d6 = *d4;
    uint32_t d0 = (uint32_t)(int32_t)(int16_t)rd16((uint16_t)(CLAW_SINES + d2)) << 8;
    uint32_t d1 = (uint32_t)(int32_t)(int16_t)rd16((uint16_t)(CLAW_COSINES + d2)) << 8;
    *d3 += d0;
    *d4 += d1;
    wr32(at + 0x48, *d3);
    wr32(at + 0x4C, *d4);
    ClawSegmentLook(d2, at);
    at = (uint16_t)(at + 0x60);
    d0 <<= 1;
    d1 <<= 1;
    d5 += d0;
    d6 += d1;
    wr32(at + 0x48, d5);
    wr32(at + 0x4C, d6);
    return (uint16_t)(at + 0x60);
}

/* $0534AE. The arms placed by their angles and the eleven objects put by their offsets from
 * the leader (+$50). (a3, a4: the tables at $9400, $9480, as it sets them.) */
void ClawPlace(uint16_t obj)
{
    wr32(obj + 0x48, 0);
    wr32(obj + 0x4C, 0);
    wr32(0xDC28, 0xFFE00000);
    wr32(0xDC2C, 0xFFE80000);
    wr32(0xDF88, 0);
    wr32(0xDF8C, 0xFFE80000);
    uint32_t d3 = 0, d4 = 0;
    uint16_t a0 = 0xDC40;
    a0 = ClawUpper(rd16(obj + 0x54), a0, &d3, &d4);
    a0 = ClawLower(rd16(obj + 0x56), a0, &d3, &d4);
    a0 = ClawUpper(rd16(obj + 0x58), a0, &d3, &d4);
    ClawLower(rd16(obj + 0x5A), a0, &d3, &d4);
    GroupPlace(obj, 0x0A);
}

/* $053374. The four angles kept to $1FF. */
void ClawAngles(uint16_t obj)
{
    for (uint16_t o = 0x54; o <= 0x5A; o = (uint16_t)(o + 2))
        wr16(obj + o, rd16(obj + o) & 0x1FF);
}

/* $0533AA. A shoulder at a0 shown ($E000, sprite $B00). */
void ClawShoulder(uint16_t at)
{
    wr16(at + O_FLAGS, 0xE000);
    wr16(at + 0x0E, 0xB00);
    wr16(at + 0x0C, 0);
}

/* $05338E. Both shoulders ($E0086 at $DBE0, $DF344 at $DF40). */
void ClawShoulders(void)
{
    wr32(0xDBE0 + 0x08, 0x0E0086);
    ClawShoulder(0xDBE0);
    wr32(0xDF40 + 0x08, 0x0DF344);
    ClawShoulder(0xDF40);
}

/* $053492. A claw at a0 hurting players (its box, +$23 $20). */
void ClawGrip(uint16_t at)
{
    wr8(at + 0x21, 0);
    wr32(at + 0x2C, 0x0810F010);
    wr16(at + 0x26, 0x14);
    wr8(at + 0x23, 0x20);
}

/* $053486. Both claws ($DD60, $DEE0). */
void ClawGrips(void)
{
    ClawGrip(0xDD60);
    ClawGrip(0xDEE0);
}

/* $0533BE. The eight segments' looks (by $53400 into $53410); the claws; the eleven objects'
 * +$20 from $5343C. */
void ClawSegments(uint16_t obj)
{
    uint16_t a0 = 0xDC40;
    for (uint16_t d1 = 0; d1 < 16; d1 = (uint16_t)(d1 + 2)) {
        uint32_t a1 = 0x053410 + rom16(0x053400 + d1);
        wr16(a0 + O_FLAGS, 0x8100);
        wr16(a0 + 0x0E, rom16(a1));
        wr16(a0 + 0x08, rom16(a1 + 2));
        wr16(a0 + 0x0A, rom16(a1 + 4));
        a0 = (uint16_t)(a0 + 0x60);
    }
    ClawGrips();
    a0 = obj;
    for (int i = 0; i < 11; i++) {
        wr8(a0 + 0x20, rom8(0x05343C + (uint32_t)i));
        a0 = (uint16_t)(a0 + 0x60);
    }
}

/* $053448. The eleven objects hidden, harmless. */
void ClawHide(uint16_t obj)
{
    uint16_t a0 = obj;
    for (int i = 0; i < 11; i++) {
        wr16(a0 + O_FLAGS, 0);
        wr8(a0 + 0x21, 0);
        a0 = (uint16_t)(a0 + 0x60);
    }
}

/* loc_053340: gone: the next one (step 0, hidden at x $300, inactive), or after the last
 * (kind 6) all eleven removed, and the objects at $CAE0, $CB40 */
static void claw_gone(uint16_t obj)
{
    if (rd16(obj + 0x5C) == 6) {
        uint16_t a0 = obj;
        for (int i = 0; i < 11; i++) {
            wr16(a0 + O_FLAGS, 0x1000);
            a0 = (uint16_t)(a0 + 0x60);
        }
        wr16(0xCB02, 0x1000);
        wr16(0xCB62, 0x1000);
        return;
    }
    wr16(obj + O_STATE, 0);
    wr8(obj + 7, 0);
    wr16(obj + O_X, 0x300);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + O_VITALITY, 0);
    wr8(obj + 0x21, 0);
    ClawHide(obj);
    ClawPlace(obj);
}

/* $0530CC. Step 0: at x $200, y $F0, its own leader; vitality and the arms' reach by its kind
 * ($53142, $5314A), the sine table started at $9400. */
void Claw00(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + 0x50, obj);
    wr16(obj + O_X, 0x200);
    wr16(obj + O_Y, 0xF0);
    wr32(obj + 0x54, 0);
    wr32(obj + 0x58, 0);
    wr16(obj + O_FLAGS, 0xC000);
    wr16(obj + 0x0E, 0x801);
    wr32(obj + 0x08, 0x0E2CA2);
    wr8(obj + 0x46, 0x8F);
    wr32(obj + 0x2C, 0xF008E028);
    wr32(obj + 0x28, 0xE802E028);
    uint16_t d0 = rd8(obj + 7) & 6;
    wr16(obj + 0x5C, d0);
    wr16(obj + O_VITALITY, rom16(0x053142 + d0));
    SineTableStart(0x9400, rom16(0x05314A + d0));
}

/* $053152. Once the table is built: hurtable, active, its parts made and placed. */
void Claw04(uint16_t obj)
{
    if (SineTableBuild()) return;
    next(obj, 4);
    wr8(obj + 0x21, 0x24);
    set8(obj + 7, 6);
    ClawSegments(obj);
    ClawShoulders();
    ClawPlace(obj);
}

/* loc_0531A2: the next step, the four angles from a0 */
static void claw_reach(uint16_t obj, uint32_t angles)
{
    next(obj, 4);
    for (int i = 0; i < 4; i++) wr16(obj + 0x54 + 2 * i, rom16(angles + 2 * (uint32_t)i));
    ClawPlace(obj);
}

/* $05317A. Placed from the claw at $DD60 (down at y $13C); the angles from $531B8. */
void Claw08(uint16_t obj)
{
    wr16(obj + 0x50, 0xDD60);
    wr16(0xDD74, 0x13C);
    claw_reach(obj, 0x0531B8);
}

/* $053190. Placed from the claw at $DEE0; the angles from $531C0. */
void Claw18(uint16_t obj)
{
    wr16(obj + 0x50, 0xDEE0);
    wr16(0xDEF4, 0x13C);
    claw_reach(obj, 0x0531C0);
}

/* $0531C8. The first arm reaching (+$54 by 2 to 0). */
void Claw0C(uint16_t obj)
{
    add16(obj + 0x54, 2);
    add16(obj + 0x56, 1);
    wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 4));
    add16(obj + 0x5A, 1);
    ClawAngles(obj);
    if (!rd16(obj + 0x54)) next(obj, 4);
    ClawPlace(obj);
}

/* loc_05320A: an arm down: a shake, 7 frames, a sound $D0 */
static void claw_down(uint16_t obj)
{
    next(obj, 4);
    wr16(v_shakeSprites, 0x10);
    wr16(obj + 0x52, 7);
    ClawPlace(obj);
    QueueSound(0xD0);
}

/* $0531EC. The body swinging over (+$54 by 1 to $20). */
void Claw10(uint16_t obj)
{
    add16(obj + 0x54, 1);
    add16(obj + 0x56, 2);
    add16(obj + 0x58, 1);
    wr16(obj + 0x5A, (uint16_t)(rd16(obj + 0x5A) - 4));
    ClawAngles(obj);
    if (rd16(obj + 0x54) != 0x20) {
        ClawPlace(obj);
        return;
    }
    claw_down(obj);
}

/* $053228. 7 frames. */
void Claw14(uint16_t obj)
{
    if (dec(obj + 0x52)) next(obj, 4);
    ClawPlace(obj);
}

/* $053238. The other arm reaching (+$58 by 2 to 0). */
void Claw1C(uint16_t obj)
{
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) - 4));
    add16(obj + 0x56, 1);
    add16(obj + 0x58, 2);
    add16(obj + 0x5A, 1);
    ClawAngles(obj);
    if (!rd16(obj + 0x58)) next(obj, 4);
    ClawPlace(obj);
}

/* $05325C. Swinging over (+$58 by 1 to $20). */
void Claw20(uint16_t obj)
{
    add16(obj + 0x54, 1);
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 4));
    add16(obj + 0x58, 1);
    add16(obj + 0x5A, 2);
    ClawAngles(obj);
    if (rd16(obj + 0x58) != 0x20) {
        ClawPlace(obj);
        return;
    }
    claw_down(obj);
}

/* $05327E. 7 frames, then the first arm again (step 8). */
void Claw24(uint16_t obj)
{
    if (dec(obj + 0x52)) wr16(obj + O_STATE, 8);
    ClawPlace(obj);
}

/* $053290. Beaten: thrown up and right, its own leader, the shoulders' look $DFF4E; a sound
 * $EF. */
void Claw28(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + 0x50, obj);
    wr16(obj + O_FLAGS, 0xCD00);
    wr32(obj + 0x1C, 0xFFFBC000);
    wr32(obj + 0x18, 0x22000);
    wr16(obj + 0x54, 0x1A0);
    wr16(obj + 0x56, 0x1D0);
    wr16(obj + 0x58, 0x160);
    wr16(obj + 0x5A, 0x1A0);
    wr32(0xDBE8, 0x0DFF4E);
    wr32(0xDF48, 0x0DFF4E);
    wr16(0xDBEC, 0);
    wr16(0xDF4C, 0);
    ClawPlace(obj);
    QueueSound(0xEF);
}

/* $0532F0. Falling, its arms flailing; at y $14C a shake, a blast ($2F4) $10 left of it at
 * $154, and it is gone. */
void Claw2C(uint16_t obj)
{
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) - 2));
    add16(obj + 0x56, 3);
    wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 4));
    add16(obj + 0x5A, 3);
    ClawAngles(obj);
    add32(obj + 0x1C, 0x5000);
    if (below(obj + O_Y, 0x14C)) {
        ClawPlace(obj);
        return;
    }
    wr16(v_shakeSprites, 0x10);
    uint16_t slot, t;
    if (SlotFreeHigh(&slot, &t)) {
        wr16(slot + O_TYPE, 0x2F4);
        wr16(slot + O_X, rd16(obj + O_X));
        wr16(slot + O_X, (uint16_t)(rd16(slot + O_X) - 0x10));
        wr16(slot + O_Y, 0x154);
    }
    claw_gone(obj);
}

/* $053140. Type $400 (and the claw while inactive): nothing. */
void Nothing400(uint16_t obj)
{
    (void)obj;
}

/* $052FFE. Type $404: the claw machine. Inactive (+7 bit 7 clear) nothing; left of x $40 gone;
 * its claws hurt (+$21 $50) while active and down from y $10C; beaten (once), step $28, its
 * score ($6502). Then its step. */
void Claw(uint16_t obj)
{
    if (!bit8(obj + 7, 7)) {
        Nothing400(obj);
        return;
    }
    if (rd16(obj + O_STATE)) {
        if (below(obj + O_X, 0x40)) {
            claw_gone(obj);
            return;
        }
        wr8(0xDD81, (bit8(obj + 7, 6) && !below(0xDD74, 0x10C)) ? 0x50 : 0);
        wr8(0xDF01, (bit8(obj + 7, 6) && !below(0xDEF4, 0x10C)) ? 0x50 : 0);
        if ((rd16(obj + O_VITALITY) & 0x8000) && !bit8(obj + 7, 0)) {
            wr16(obj + O_STATE, 0x28);
            AddScore(obj, 0x6502);
            wr16(obj + O_VITALITY, 0);
            wr8(obj + 0x21, 0);
        }
    }
    step_by_longs(obj, 0x05309C);
}

/* $0535F2. Type $408: the claw machine's switch: gone at once, it sets the claw's kind (+7 of
 * the one at $DB80, from its own +6) unless the claw is already active. */
void ClawSwitch(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x1000);
    if (bit8(0xDB87, 7)) return;
    wr8(0xDB87, rd8(obj + 6));
}

/* ---- type $41C: the Golden Silver fight's director (at $C980; no recorded run reaches it).
 * Golden Silver is the eight parts from $C9E0 (types $420, then $428, $534/$538); the
 * director spins the floor (+$48 the scroll, +$4C its speed 16.16, +$50 the tiles' phase, +$54
 * an angle of 12 bits, +$58 the height, +$5C a timer or a long speed), fades the palettes
 * ($8014-$801A), and keeps the arms' angles at $9000-$900E, the walk's step at $9010, the hit
 * flash $9014/$9016, the lean $9012, the sway $9018, the swipe $901A-$9024 ---- */

#define GS        0xC9E0         /* Golden Silver */
#define GS_SPEED  0xC9F8         /* its x speed (long) */
#define GS_TABLE  0x055B6A       /* a quarter sine, $200 words to $3FFF */

/* asl.w / asr.w by a register (the count mod 64) */
static uint16_t asl_w(uint16_t v, uint16_t n) { n &= 63; return n > 15 ? 0 : (uint16_t)(v << n); }
static uint16_t asr_w(uint16_t v, uint16_t n) { n &= 63; if (n > 15) n = 15; return (uint16_t)((int16_t)v >> n); }
/* the sine/cosine table's entry (masked to $1FE) shifted left n, its high word */
static uint16_t sin_hi(uint16_t a, int n) { return (uint16_t)(((uint32_t)(int32_t)(int16_t)rom16(0x02977C + (a & 0x1FE)) << n) >> 16); }
static uint16_t cos_hi(uint16_t a, int n) { return (uint16_t)(((uint32_t)(int32_t)(int16_t)rom16(0x0297FC + (a & 0x1FE)) << n) >> 16); }
/* the four fades of the palettes ($8014-$801A) */
static void fades_set(uint16_t v)
{
    wr16(0x8014, v);
    wr16(0x8016, v);
    wr16(0x8018, v);
    wr16(0x801A, v);
}
/* the four fades less one (subq.w); 1 when the last is now 0 */
static int fades_down(void)
{
    for (uint16_t a = 0x8014; a <= 0x801A; a = (uint16_t)(a + 2)) wr16(a, (uint16_t)(rd16(a) - 1));
    return rd16(0x801A) == 0;
}
/* cmpi.l #v then bmi: 1 when the long is below v (the N flag of the difference) */
static int below32(uint16_t a, uint32_t v) { return (int32_t)(rd32(a) - v) < 0; }

/* $054436. The scroll table $E400-$E5FF cleared. */
void GsClearScroll(void)
{
    for (uint16_t a = 0xE400; a != 0xE600; a = (uint16_t)(a + 4)) wr32(a, 0);
}

/* $0541FE. A light's colour ($E324, $E3A4): $EEE on frames with bit 3, else $6CE. */
void GsLightA(void)
{
    uint16_t c = bit8(v_gameFrames + 1, 3) ? 0xEEE : 0x6CE;
    wr16(0xE324, c);
    wr16(0xE3A4, c);
}

/* loc_0541F6: both lights ($E322, $E3A2: $E0A on frames with bit 1, else $CEE) */
static void gs_lights(void)
{
    GsLightA();
    uint16_t c = bit8(v_gameFrames + 1, 1) ? 0xE0A : 0xCEE;
    wr16(0xE322, c);
    wr16(0xE3A2, c);
}

/* $0543AC. The floor's scroll: +$48 brought into -1..0; the lines' offsets ($E602 on and
 * $E502 back, then $E6A6 on) stepping by it; the rows' ($E506 on) from +$50. */
void GsScroll(uint16_t obj)
{
    if (bit8(obj + 0x48, 7)) {
        while (below32(obj + 0x48, 0xFFFF0000)) add32(obj + 0x48, 0x10000);
    } else {
        do wr32(obj + 0x48, rd32(obj + 0x48) - 0x10000);
        while (!((int32_t)rd32(obj + 0x48) < 0));
    }
    uint16_t a0 = 0xE602, a1 = 0xE502;
    uint32_t d0 = 0, d1 = rd32(obj + 0x48);
    for (int i = 0; i < 0x29; i++) {
        d0 += d1;
        wr16(a0, (uint16_t)(d0 >> 16));
        wr16(a1, (uint16_t)(d0 >> 16));
        a0 = (uint16_t)(a0 + 4);
        a1 = (uint16_t)(a1 - 4);
    }
    for (int i = 0; i < 0x17; i++) {
        d0 += d1;
        wr16(a0, (uint16_t)(d0 >> 16));
        a0 = (uint16_t)(a0 + 4);
    }
    uint16_t w = (uint16_t)((rd32(obj + 0x50) << 5) >> 16);
    a0 = 0xE506;
    for (int i = 0; i < 0x2F; i++) {
        wr16(a0, w);
        a0 = (uint16_t)(a0 + 4);
    }
}

/* $054280. One of the floor's tile chunks sent to the VRAM at d1: the next from a0 (steps
 * below $C4 and $CC), else blank ($FF4000); a0 returned. */
uint32_t GsTileChunk(uint16_t obj, uint16_t d1, uint32_t a0)
{
    uint32_t d0 = 0xFFFF4000;
    if (rd16(obj + O_STATE) == 0xCC || below(obj + O_STATE, 0xC4)) {
        d0 = bus_rd32(a0);
        a0 += 4;
    }
    ChunkDma(d0, d1);
    return a0;
}

/* $05424A. The floor's eight chunks by its phase (+$50: the list at $542AC). */
void GsTiles(uint16_t obj)
{
    uint16_t d0 = (uint16_t)((rd32(obj + 0x50) << 3) >> 16) & 0x1C;
    uint32_t a0 = rom32(0x0542AC + d0);
    uint32_t d1 = 0xC220;
    for (int i = 0; i < 4; i++) {
        a0 = GsTileChunk(obj, (uint16_t)d1, a0);
        d1 += 8;
        a0 = GsTileChunk(obj, (uint16_t)d1, a0);
        d1 += 0x1F8;
    }
}

/* $0541A0. The floor turning: its speed kept to -$6000..$6000, the scroll on, the phase back by
 * it; the scroll, the tiles, the lights. */
void GsSpin(uint16_t obj)
{
    if (bit8(obj + 0x4C, 7)) {
        if (below32(obj + 0x4C, 0xFFFFA000)) wr32(obj + 0x4C, 0xFFFFA000);
    } else {
        if (!below32(obj + 0x4C, 0x6000)) wr32(obj + 0x4C, 0x6000);
    }
    wr32(obj + 0x48, rd32(obj + 0x48) + rd32(obj + 0x4C));
    wr32(obj + 0x50, rd32(obj + 0x50) - rd32(obj + 0x4C));
    GsScroll(obj);
    GsTiles(obj);
    gs_lights();
}

/* $05484C. The sine of a 12-bit angle d0 (by quarters of the table at $55B6A). */
uint16_t GsSine(uint16_t d0)
{
    uint16_t d1 = d0 & 0x3FE;
    d0 &= 0xFFE;
    if ((int16_t)(d0 - 0x400) < 0) return rom16(GS_TABLE + d1);
    if ((int16_t)(d0 - 0x800) < 0) return rom16(GS_TABLE + (d1 ^ 0x3FE));
    if ((int16_t)(d0 - 0xC00) < 0) return (uint16_t)(~rom16(GS_TABLE + d1) + 1);
    return (uint16_t)(0 - rom16(GS_TABLE + (d1 ^ 0x3FE)));
}

/* $05489E. Its cosine. */
uint16_t GsCosine(uint16_t d0)
{
    uint16_t d1 = d0 & 0x3FE;
    d0 &= 0xFFE;
    if ((int16_t)(d0 - 0x400) < 0) return rom16(GS_TABLE + (d1 ^ 0x3FE));
    if ((int16_t)(d0 - 0x800) < 0) return (uint16_t)(~rom16(GS_TABLE + d1) + 1);
    if ((int16_t)(d0 - 0xC00) < 0) return (uint16_t)(0 - rom16(GS_TABLE + (d1 ^ 0x3FE)));
    return rom16(GS_TABLE + d1);
}

/* $0549CC. Golden Silver's hit flash: hit, 3 frames of the palette $6000 ($9016). */
void GsHitFlash(void)
{
    if (!(rd16(0x9014) & 0x8000)) {
        wr16(0x9014, (uint16_t)(rd16(0x9014) - 1));
        wr16(0x9016, 0x6000);
        return;
    }
    if (bit8(GS + 0x22, 5)) wr16(0x9014, 2);
    wr16(0x9016, 0);
}

/* loc_0547DC: the eight parts spread about the floor's angle (+$54): their depth (+$50) by
 * its sine, x (+$48) by its cosine, y (+$4C) the height (+$58) less an eighth each */
static void gs_arrange(uint16_t obj)
{
    GsHitFlash();
    uint16_t d0 = (uint16_t)((rd32(obj + 0x4C) << 7) >> 16);
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) + d0) & 0xFFE);
    uint16_t a2 = GS;
    uint32_t d3 = rd32(obj + 0x58);
    uint32_t d5 = (uint32_t)((int32_t)d3 >> 3);
    uint16_t d4 = 0x100, d6 = 0xF0;
    for (int i = 0; i < 8; i++) {
        uint16_t d1 = GsSine(rd16(obj + 0x54));
        uint32_t p = (uint32_t)((int32_t)(int16_t)d1 * (int16_t)d6);
        d1 = (uint16_t)((uint16_t)((p << 2) >> 16) + 0x100) & 0x1FE;
        wr16(a2 + 0x50, d1);
        d1 = GsCosine(rd16(obj + 0x54));
        p = (uint32_t)((int32_t)(int16_t)d1 * (int16_t)d4);
        wr32(a2 + 0x48, p << 3);
        wr32(a2 + 0x4C, d3);
        d3 -= d5;
        d4 = (uint16_t)(d4 - 0x20);
        d6 = (uint16_t)(d6 - 0x20);
        a2 = (uint16_t)(a2 + 0x60);
    }
}

/* $054644. Golden Silver's eight parts made (type $420 from $C9E0, the first hurtable,
 * vitality $577). The 68k clears +6 of the director (a5), not of the parts, seven times. */
void GsParts(uint16_t obj)
{
    uint16_t a0 = GS;
    wr16(a0 + 6, 1);
    wr16(a0 + O_VITALITY, 0x577);
    wr16(a0 + 0x26, 9);
    wr8(a0 + 0x23, 0x20);
    wr32(a0 + 0x2C, 0xF010F20E);
    wr32(a0 + 0x28, 0xF010F010);
    for (int i = 0; i < 8; i++) {
        if (i) wr16(obj + 6, 0);
        wr16(a0 + O_TYPE, 0x420);
        wr16(a0 + O_FLAGS, 0x8000);
        a0 = (uint16_t)(a0 + 0x60);
    }
}

/* $0546DE. A hand (a0) that hurts: +6 2, its box. */
void GsHand(uint16_t at)
{
    wr16(at + 6, 2);
    wr8(at + 0x21, 0);
    wr32(at + 0x2C, 0xF20EF20E);
    wr16(at + 0x26, 0x0B);
}

/* $054692. Its second form: fifteen parts of type $428 from $C9E0 (their +$20 from $546FA, x 0),
 * the hands at $CC80 and $CCE0. */
void GsParts428(void)
{
    uint16_t a0 = GS;
    uint32_t a1 = 0x0546FA;
    wr16(a0 + 6, 1);
    wr32(a0 + 0x28, 0xF828F010);
    for (int i = 0; i < 15; i++) {
        if (i) wr16(a0 + 6, 0);
        wr16(a0 + O_TYPE, 0x428);
        wr16(a0 + O_FLAGS, 0x8000);
        wr8(a0 + 0x20, rom8(a1++));
        wr16(a0 + O_X, 0);
        a0 = (uint16_t)(a0 + 0x60);
    }
    GsHand(0xCC80);
    GsHand(0xCCE0);
}

/* $054736. Every 8th frame a $424 (at depth $1E0, x, y at random -$80..$7F). */
void GsSpawn424(void)
{
    if (rd16(v_gameFrames) & 7) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x424);
    wr16(slot + O_FLAGS, 0x8000);
    wr16(slot + 0x50, 0x1E0);
    wr16(slot + 0x48, (uint16_t)((rd16(v_randomSeed) & 0xFF) - 0x80));
    Random();
    wr16(slot + 0x4C, (uint16_t)((rd16(v_randomSeed) & 0xFF) - 0x80));
}

/* $054796. From difficulty 1, every $40 frames a $42C (depth $1E0, x -$70..$70 by $20, y
 * -$30). */
void GsShot42C(void)
{
    if (!rd16(v_difficulty)) return;
    if (rd16(v_gameFrames) & 0x3F) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x42C);
    wr16(slot + O_FLAGS, 0x8000);
    wr16(slot + 0x50, 0x1E0);
    wr16(slot + 0x48, (uint16_t)((rd16(v_randomSeed) & 0xE0) - 0x70));
    wr16(slot + 0x4C, 0xFFD0);
}

/* $05478C. The same while Golden Silver's vitality is below $1F4. */
void GsShotIfWeak(void)
{
    if (!below(0xCA04, 0x1F4)) return;
    GsShot42C();
}

/* $05490A. Four puffs ($565D4) from Golden Silver, their speeds from $54952, $54962. */
void GsBurst(void)
{
    for (uint16_t d6 = 0; d6 < 16; d6 = (uint16_t)(d6 + 4)) {
        uint16_t slot, t;
        if (!SlotFreeHigh(&slot, &t)) return;
        wr16(slot + O_TYPE, 0x100);
        wr32(slot + 0x48, 0x0565D4);
        wr32(slot + 0x18, rom32(0x054952 + d6));
        wr32(slot + 0x1C, rom32(0x054962 + d6));
        wr16(slot + O_X, rd16(GS + O_X));
        wr16(slot + O_Y, rd16(GS + O_Y));
    }
}

/* $054972. (Type $420.) In front of it, every other frame, on the screen: a puff ($565D4)
 * rising from $10 below it. */
void GsSpark(uint16_t obj)
{
    if (below(0xC9D8, 0x60)) return;
    if (bit8(v_gameFrames + 1, 0)) return;
    if (below(obj + O_X, 0x80) || !below(obj + O_X, 0x1C0)) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x0565D4);
    wr32(slot + 0x1C, 0xFFFB8000);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    add16(slot + O_Y, 0x10);
}

/* $0549FA. Every 4th frame its lean ($9012) toward its way: moving right down to -2, left up to
 * $E, standing to $A. */
void GsLean(void)
{
    if (rd16(v_gameFrames) & 3) return;
    if (bit8(GS_SPEED, 7)) {
        if ((rd16(0x9012) & 0x8000) || below(0x9012, 0xE)) add16(0x9012, 1);
        return;
    }
    if (rd32(GS_SPEED)) {
        if (!(rd16(0x9012) & 0x8000) || !below(0x9012, 0xFFFE))
            wr16(0x9012, (uint16_t)(rd16(0x9012) - 1));
        return;
    }
    if (rd16(0x9012) == 0xA) return;
    if (!below(0x9012, 0xA)) wr16(0x9012, (uint16_t)(rd16(0x9012) - 1));
    else add16(0x9012, 1);
}

/* $054D00. The parts' depths from Golden Silver's (+$50). */
void GsDepths(void)
{
    uint16_t d0 = rd16(GS + 0x50);
    wr16(0xCA90, d0);
    wr16(0xCAF0, d0);
    d0 = (uint16_t)(d0 - 8);
    wr16(0xCC70, d0);
    wr16(0xCCD0, d0);
    wr16(0xCD30, d0);
    d0 = (uint16_t)(d0 + 4);
    wr16(0xCD90, d0);
    wr16(0xCDF0, d0);
    wr16(0xCE50, d0);
    d0 = (uint16_t)(d0 + 8);
    wr16(0xCEB0, d0);
    wr16(0xCF10, d0);
    wr16(0xCF70, d0);
    d0 = (uint16_t)(d0 + 4);
    wr16(0xCB50, d0);
    wr16(0xCBB0, d0);
    wr16(0xCC10, d0);
}

/* one limb segment: at the angle (the sine to y, the cosine to x, both shifted n) from the
 * offsets at (bx, by), to (tx, ty); dy more on y */
static void gs_segment(uint16_t angle, int n, uint16_t bx, uint16_t by, uint16_t tx, uint16_t ty, uint16_t dy)
{
    uint16_t d1 = (uint16_t)(sin_hi(angle, n) + rd16(by) - dy);
    uint16_t d2 = (uint16_t)(cos_hi(angle, n) + rd16(bx));
    wr16(ty, d1);
    wr16(tx, d2);
}

/* $054A5A. Golden Silver moved by its speeds (its offsets +$48, +$4C), the flash, the depths,
 * the lean; then its parts' offsets: the head and body by the lean, the arms by $9006/$900E
 * and $9004/$900C (or the swipe's $901A/$901C), the legs by $9000/$9008 and $9002/$900A; its
 * hands' speed $CA2C by the frame. */
void GsLimbs(void)
{
    wr32(GS + 0x48, rd32(GS + 0x48) + rd32(GS + 0x18));
    wr32(GS + 0x4C, rd32(GS + 0x4C) + rd32(GS + 0x1C));
    GsHitFlash();
    GsDepths();
    GsLean();
    uint16_t d0 = rd16(0x9012);
    wr16(0xCA88, (uint16_t)(rd16(GS + 0x48) + d0));
    wr16(0xCA8C, (uint16_t)(rd16(GS + 0x4C) + 0x1C));
    d0 = (uint16_t)(d0 << 1);
    wr16(0xCAE8, (uint16_t)(rd16(GS + 0x48) + d0));
    wr16(0xCAEC, (uint16_t)(rd16(GS + 0x4C) + 0x38));
    gs_segment(rd16(0x9006), 5, 0xCA88, 0xCA8C, 0xCB48, 0xCB4C, 0xA);
    uint16_t a = rd16(0x900E) & 0x1FE;
    gs_segment(a, 7, 0xCB48, 0xCB4C, 0xCBA8, 0xCBAC, 0);
    a = (uint16_t)(a + 0x70) & 0x1FE;
    gs_segment(a, 7, 0xCBA8, 0xCBAC, 0xCC08, 0xCC0C, 0);
    a = bit8(0x901E, 7) ? rd16(0x901A) : rd16(0x9004);
    gs_segment(a, 5, 0xCA88, 0xCA8C, 0xCC68, 0xCC6C, 0xA);
    a = (bit8(0x901E, 7) ? rd16(0x901C) : rd16(0x900C)) & 0x1FE;
    gs_segment(a, 7, 0xCC68, 0xCC6C, 0xCCC8, 0xCCCC, 0);
    if (!bit8(0x901E, 1)) a = (uint16_t)(a + 0x70) & 0x1FE;
    gs_segment(a, 7, 0xCCC8, 0xCCCC, 0xCD28, 0xCD2C, 0);
    gs_segment(rd16(0x9000), 6, 0xCAE8, 0xCAEC, 0xCD88, 0xCD8C, 0);
    gs_segment(rd16(0x9000), 7, 0xCD88, 0xCD8C, 0xCDE8, 0xCDEC, 0);
    gs_segment(rd16(0x9008), 7, 0xCDE8, 0xCDEC, 0xCE48, 0xCE4C, 0);
    gs_segment(rd16(0x9002), 6, 0xCAE8, 0xCAEC, 0xCEA8, 0xCEAC, 0);
    gs_segment(rd16(0x9002), 7, 0xCEA8, 0xCEAC, 0xCF08, 0xCF0C, 0);
    gs_segment(rd16(0x900A), 7, 0xCF08, 0xCF0C, 0xCF68, 0xCF6C, 0);
    wr32(0xCA2C, bit8(v_gameFrames + 1, 2) ? 0x20000 : 0xFFFF0000);
}

/* $054D4A. The fifteen parts' y speeds (+$48 of $C9E0 on: the long at $CA28) turned over. */
void GsFlip(void)
{
    uint16_t a0 = 0xCA28;
    for (int i = 0; i < 15; i++) {
        wr32(a0, (uint32_t)(0 - rd32(a0)));
        a0 = (uint16_t)(a0 + 0x60);
    }
}

/* $054EFA. The eight limb angles kept to $1FE. */
void GsAngles(void)
{
    static const uint16_t at[8] = { 0x9004, 0x900C, 0x9006, 0x900E, 0x9000, 0x9008, 0x9002, 0x900A };
    for (int i = 0; i < 8; i++) wr16(at[i], rd16(at[i]) & 0x1FE);
}

/* $054D7C. The walk's first pose; the swipe's arms (if $901E bit 0, cleared) with it. */
void GsWalk00(void)
{
    wr16(0x9010, 4);
    wr16(0x9004, 0x1C0);
    wr16(0x900C, 0x1E0);
    wr16(0x9006, 0xC0);
    wr16(0x900E, 0xC0);
    wr16(0x9000, 0xC0);
    wr16(0x9008, 0x40);
    wr16(0x9002, 0x20);
    wr16(0x900A, 0x1C0);
    int was = bit8(0x901E, 0);
    clr8(0x901E, 0);
    if (!was) return;
    wr16(0x901A, 0x1C0);
    wr16(0x901C, 0x1E0);
}

/* $054DCA. A step: the limbs turning until $9004 is $60. */
void GsWalk04(void)
{
    add16(0x9004, 0x14);
    add16(0x900C, 0x10);
    wr16(0x9006, (uint16_t)(rd16(0x9006) - 0xC));
    wr16(0x900E, (uint16_t)(rd16(0x900E) - 0xC));
    wr16(0x9000, (uint16_t)(rd16(0x9000) - 0xC));
    add16(0x9008, 4);
    add16(0x9002, 0x1C);
    add16(0x900A, 0xC);
    GsAngles();
    if (rd16(0x9004) == 0x60) add16(0x9010, 4);
}

/* $054E0C. Then until $9004 is $C0: the other side's pose. */
void GsWalk08(void)
{
    wr16(0x9006, (uint16_t)(rd16(0x9006) - 0x10));
    wr16(0x900E, (uint16_t)(rd16(0x900E) - 0x14));
    add16(0x9004, 0xC);
    add16(0x900C, 0x10);
    wr16(0x9000, (uint16_t)(rd16(0x9000) - 8));
    wr16(0x9008, (uint16_t)(rd16(0x9008) - 0x14));
    wr16(0x9002, (uint16_t)(rd16(0x9002) - 8));
    add16(0x900A, 0x10);
    GsAngles();
    if (rd16(0x9004) != 0xC0) return;
    add16(0x9010, 4);
    wr16(0x9006, 0x1C0);
    wr16(0x900E, 0x1E0);
    wr16(0x9004, 0xC0);
    wr16(0x900C, 0xC0);
    wr16(0x9002, 0xC0);
    wr16(0x900A, 0x40);
    wr16(0x9000, 0x20);
    wr16(0x9008, 0x1C0);
}

/* $054E7C. The other step: until $9006 is $60. */
void GsWalk0C(void)
{
    add16(0x9006, 0x14);
    add16(0x900E, 0x10);
    wr16(0x9004, (uint16_t)(rd16(0x9004) - 0xC));
    wr16(0x900C, (uint16_t)(rd16(0x900C) - 0xC));
    wr16(0x9002, (uint16_t)(rd16(0x9002) - 0xC));
    add16(0x900A, 4);
    add16(0x9000, 0x1C);
    add16(0x9008, 0xC);
    GsAngles();
    if (rd16(0x9006) == 0x60) add16(0x9010, 4);
}

/* $054EBE. Then until $9006 is $C0: the first pose again. */
void GsWalk10(void)
{
    wr16(0x9004, (uint16_t)(rd16(0x9004) - 0x10));
    wr16(0x900C, (uint16_t)(rd16(0x900C) - 0x14));
    add16(0x9006, 0xC);
    add16(0x900E, 0x10);
    wr16(0x9002, (uint16_t)(rd16(0x9002) - 8));
    wr16(0x900A, (uint16_t)(rd16(0x900A) - 0x14));
    wr16(0x9000, (uint16_t)(rd16(0x9000) - 8));
    add16(0x9008, 0x10);
    GsAngles();
    if (rd16(0x9006) == 0xC0) GsWalk00();
}

/* $054D5E. The walk, by its step ($9010). */
void GsWalk(void)
{
    call68k(rom32(0x054D68 + (uint32_t)(int16_t)rd16(0x9010)));
}

/* loc_054FB4: the sway's next wait, $18 frames one way or the other */
static void gs_sway_pick(void)
{
    wr16(0x9018, bit8(v_randomSeed + 1, 0) ? 0xFFE8 : 0x18);
}

/* $054F20. Golden Silver swaying in x (its offset +$48 kept to -$200..$80): its speed eased
 * toward 2 or -2 ($800 a frame), turning when the count ($9018) runs out. */
void GsSway(void)
{
    uint16_t d0 = (uint16_t)(rd16(GS + 0x48) + 0x200);
    if (!((int16_t)(d0 - 0x280) < 0)) {
        wr16(0x9018, 0xFFFF);
        wr32(GS_SPEED, rd32(GS_SPEED) - 0x800);
        return;
    }
    if ((int16_t)(d0 - 0x200) < 0) {
        wr16(0x9018, 1);
        add32(GS_SPEED, 0x800);
        return;
    }
    if (!(rd16(0x9018) & 0x8000)) {
        if (!bit8(GS_SPEED, 7)) {
            wr16(0x9018, (uint16_t)(rd16(0x9018) - 1));
            if (!rd16(0x9018)) {
                gs_sway_pick();
                return;
            }
        }
        if (!below32(GS_SPEED, 0x20000)) return;
        add32(GS_SPEED, 0x800);
        return;
    }
    if (bit8(GS_SPEED, 7)) {
        add16(0x9018, 1);
        if (!rd16(0x9018)) {
            gs_sway_pick();
            return;
        }
    }
    if (below32(GS_SPEED, 0xFFFE0000)) return;
    wr32(GS_SPEED, rd32(GS_SPEED) - 0x800);
}

/* $054FCE. Golden Silver beaten (its vitality below 0): its +$23 to the director's, the score
 * ($12504), step $44 for $C0 frames. */
void GsBeaten(uint16_t obj)
{
    if (!(rd16(0xCA04) & 0x8000)) return;
    wr8(obj + 0x23, rd8(0xCA03));
    AddScore(obj, 0x12504);
    wr16(obj + O_STATE, 0x44);
    wr16(obj + 0x5C, 0xC0);
}

/* loc_054FF6: the same in its second form ($35008, step $B0) */
static void gs_beaten2(uint16_t obj)
{
    if (!(rd16(0xCA04) & 0x8000)) return;
    wr8(obj + 0x23, rd8(0xCA03));
    AddScore(obj, 0x35008);
    wr16(obj + O_STATE, 0xB0);
    wr16(obj + 0x5C, 0xC0);
}

/* $05501E. Explosions' sound; every other frame a burning piece ($FC: $5639C, or from $CA30
 * $80 $56450) thrown right from $C above Golden Silver, its first frame set, +$20 8. */
void GsSmoke(void)
{
    BlastSound8();
    if (bit8(v_gameFrames + 1, 0)) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0xFC);
    wr32(slot + 0x48, 0x05639C);
    if (!below(0xCA30, 0x80)) wr32(slot + 0x48, 0x056450);
    wr16(slot + O_X, rd16(GS + O_X));
    wr16(slot + O_Y, rd16(GS + O_Y));
    wr16(slot + O_Y, (uint16_t)(rd16(slot + O_Y) - 0xC));
    wr32(slot + 0x1C, (uint32_t)(int32_t)(int16_t)rd16(v_randomSeed) + 0xFFFE0000);
    wr32(slot + 0x18, 0x68000);
    AnimList(slot);
    wr8(slot + 0x20, 8);
}

/* $05509C. The director shown at $120, $E0 as the glow ($5639E by the frame). */
void GsGlow(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x8000);
    uint32_t a0 = 0x05639E + (uint16_t)((uint16_t)(rd16(v_gameFrames) << 2) & 0x18);
    wr16(obj + 0x0E, rom16(a0));
    wr16(obj + 0x08, rom16(a0 + 2));
    wr16(obj + 0x0A, rom16(a0 + 4));
    wr16(obj + O_X, 0x120);
    wr16(obj + O_Y, 0xE0);
    wr8(obj + 0x20, 8);
}

/* $0550D6. The floor's spin dying (+$5C down by $400: the speed +-+$5C by its bit 10, 0 once it
 * is spent): Z set when the speed is 0. */
int GsSpinDown(uint16_t obj)
{
    wr16(obj + 0x5C, (uint16_t)(rd16(obj + 0x5C) - 0x400));
    if (rd16(obj + 0x5C) & 0x8000) {
        wr32(obj + 0x4C, 0);
        return 1;
    }
    uint32_t d0 = (uint32_t)(int32_t)(int16_t)rd16(obj + 0x5C);
    uint32_t v = bit8(obj + 0x5C, 2) ? d0 : (uint32_t)(0 - d0);
    wr32(obj + 0x4C, v);
    return v == 0;
}

/* $055106. A jolt of the floor (speed -$2000, +$5C $4000), a sound $D1. */
void GsSpinStart(uint16_t obj)
{
    wr32(obj + 0x4C, 0xFFFFE000);
    wr16(obj + 0x5C, 0x4000);
    QueueSound(0xD1);
}

/* $05511E. Four colours from a0 to $E338. */
void GsColours(uint32_t a0)
{
    for (uint16_t a1 = 0xE338; a1 != 0xE340; a1 = (uint16_t)(a1 + 2)) {
        wr16(a1, bus_rd16(a0));
        a0 += 2;
    }
}

/* $05513C. The palettes faded by +$5C (its bits 2-4), adjusted. */
void GsFade(uint16_t obj)
{
    fades_set((uint16_t)((rd16(obj + 0x5C) & 0x1C) >> 2));
    wr16(0x800A, 0x8000);
    AdjustPalettes();
}

/* $055162. $9600-$97FF cleared. */
void GsClear9600(void)
{
    for (uint16_t a = 0x9600; a != 0x9800; a = (uint16_t)(a + 4)) wr32(a, 0);
}

/* loc_055174: the rays' lines around $96C0 (+$5C + 1 each way: $48 up, $47 down) */
static void gs_rays(uint16_t obj)
{
    uint16_t d7 = rd16(obj + 0x5C);
    if (d7 & 0x8000) return;
    uint16_t d0 = 0x48, d1 = 0x47, a0 = 0x96C0, a1 = 0x96C0;
    do {
        a0 = (uint16_t)(a0 - 2);
        wr16(a0, d0);
        wr16(a1, d1);
        a1 = (uint16_t)(a1 + 2);
        d0 = (uint16_t)(d0 + 1);
        d1 = (uint16_t)(d1 - 1);
    } while ((int16_t)(d7 = (uint16_t)(d7 - 1)) != -1);
}

/* $055ACE. The pictures for the fight: commands $55B4C, a picture shrunk from $9000 to the
 * VRAM at $6000 (by 1), commands $55B56, another to $8000 (by 9), commands $55B60. */
void GsPictures(void)
{
    RunCommands(0x055B4C);
    wr16(0x8046, 1);
    ScaleShrink(0x9200, 0x9000, 0x6000);
    RunCommands(0x055B56);
    wr16(0x8046, 9);
    ScaleShrink(0x9200, 0x9000, 0x8000);
    RunCommands(0x055B60);
}

/* loc_053780, loc_053784, loc_0537D4: the director's frame's end: (the glow,) (Golden Silver's
 * defeat,) the floor's spin, the parts arranged */
static void gs_spin_tail(uint16_t obj)
{
    GsSpin(obj);
    gs_arrange(obj);
}
static void gs_glow_tail(uint16_t obj)
{
    GsGlow(obj);
    gs_spin_tail(obj);
}
static void gs_fight_tail(uint16_t obj)
{
    GsBeaten(obj);
    gs_spin_tail(obj);
}

/* $053A76. The floor's spin eased toward the player ($400 a frame), unless already that way. */
void GsTurnToPlayer(uint16_t obj)
{
    uint16_t p = PlayerTarget();
    uint16_t d0 = (uint16_t)(rd16(p + O_X) - rd16(0xC9F0));
    if (d0 & 0x8000) {
        if (rd16(obj + 0x4C) & 0x8000) return;
        wr32(obj + 0x4C, rd32(obj + 0x4C) - 0x400);
        return;
    }
    uint32_t s = rd32(obj + 0x4C);
    if (s && !(s & 0x80000000u)) return;
    add32(obj + 0x4C, 0x400);
}

/* $0539F8. Hovering: the height (+$58) down to -$30 (or with the frame's bit 5 up to -$20);
 * there (from difficulty 4 also at -$20), one time in 32 or with the player within $18, the
 * drop (speed -2.25). */
void GsHover(uint16_t obj)
{
    GsTurnToPlayer(obj);
    if (!(rd16(obj + 0x58) & 0x8000)) {
        wr32(obj + 0x58, rd32(obj + 0x58) - 0x10000);
        return;
    }
    if (bit8(v_gameFrames + 1, 5)) {
        if (below(obj + 0x58, 0xFFE0)) {
            add32(obj + 0x58, 0x10000);
            return;
        }
        if (below(v_difficulty, 4)) return;
    } else if (!below(obj + 0x58, 0xFFD0)) {
        wr32(obj + 0x58, rd32(obj + 0x58) - 0x10000);
        return;
    }
    if (rd16(v_randomSeed) & 0x1F) {
        uint16_t p = PlayerTarget();
        uint16_t d0 = (uint16_t)(rd16(p + O_X) - rd16(0xC9F0));
        if (d0 & 0x8000) d0 = (uint16_t)-d0;
        if (!((int16_t)(d0 - 0x18) < 0)) return;
    }
    next(obj, 4);
    wr32(obj + 0x5C, 0xFFFDC000);
}

/* $0536E4. Step 0: the plane's flag off, no shake, the scroll table cleared, the scroll mode
 * 0; once the jobs are done, $100 frames: the pictures made, every object from $C9E0 gone. */
void Gs00(uint16_t obj)
{
    clr8(v_planeFlagsB, 7);
    wr16(v_shakeSprites, 0);
    GsClearScroll();
    wr8(VDP_REG(11) + 1, 0);
    if (rd16(v_jobKind)) return;
    next(obj, 4);
    wr16(obj + 0x48, 0x100);
    wr16(0x8002, 0xF000);
    GsPictures();
    ObjectsVanishFrom(GS, 0, 0);
}

/* $05376E. Until the message is done ($80C2): then $40 frames. */
void Gs08(uint16_t obj)
{
    if (!rd16(0x80C2)) {
        next(obj, 4);
        wr16(obj + 0x5C, 0x40);
    }
    gs_glow_tail(obj);
}

/* $053722. After +$48 frames (the lights): the floor (scroll mode 3, every field cleared, angle
 * $400), the message $39AD8, Golden Silver's parts. */
void Gs04(uint16_t obj)
{
    if (!dec(obj + 0x48)) {
        gs_lights();
        return;
    }
    next(obj, 4);
    set8(v_planeFlagsB, 7);
    wr8(VDP_REG(11) + 1, 3);
    wr16(0x808A, 0x8000);
    wr16(0x9014, 0);
    wr32(obj + 0x48, 0);
    wr32(obj + 0x4C, 0);
    wr32(obj + 0x50, 0);
    wr16(obj + 0x54, 0x400);
    wr32(obj + 0x58, 0);
    wr16(0x80C2, 0x34);
    wr32(0x80C8, 0x039AD8);
    GsParts(obj);
    Gs08(obj);
}

/* $05378C. Steps $C, $5C, $6C, $78, $88: fading in (by +$5C) the glow; then hidden. */
void Gs0C(uint16_t obj)
{
    GsFade(obj);
    if (!dec(obj + 0x5C)) {
        gs_glow_tail(obj);
        return;
    }
    next(obj, 4);
    wr16(obj + O_FLAGS, 0);
    gs_spin_tail(obj);
}

/* $0537A4. The floor spinning up ($400 a frame to $3000) until the angle is $C00: a jolt. */
void Gs10(uint16_t obj)
{
    if (below32(obj + 0x4C, 0x3000)) add32(obj + 0x4C, 0x400);
    if (!below(obj + 0x54, 0xC00)) {
        next(obj, 4);
        wr16(obj + 0x54, 0xC00);
        GsSpinStart(obj);
    }
    gs_spin_tail(obj);
}

/* $0537E0. Once the spin has died: the fight (step $30, $80 frames; the boss's bar on Golden
 * Silver, vitality $A30). */
void Gs14(uint16_t obj)
{
    if (GsSpinDown(obj)) {
        wr16(obj + O_STATE, 0x30);
        wr16(obj + 0x5C, 0x80);
        wr16(0x80C2, 0x10);
        wr16(0x80D6, GS);
        wr16(0xCA04, 0xA30);
    }
    gs_spin_tail(obj);
}

/* $05380A. What next: from vitality $76C, or from $320 one time in 8, spin toward the middle
 * (step $1C or $24); else hover (step $34). */
void Gs18(uint16_t obj)
{
    if (below(GS + O_VITALITY, 0x76C)
        && (below(GS + O_VITALITY, 0x320) || (rd16(v_randomSeed) & 7))) {
        wr16(obj + O_STATE, 0x34);
        gs_fight_tail(obj);
        return;
    }
    wr32(obj + 0x5C, 0);
    if (below(GS + O_X, 0x120)) {
        wr16(obj + O_STATE, 0x24);
        wr32(obj + 0x4C, 0x400);
    } else {
        next(obj, 4);
        wr32(obj + 0x4C, 0xFFFFFC00);
    }
    gs_fight_tail(obj);
}

/* loc_0538FA: the spin stopped: a jolt, step $2C */
static void gs_spin_stop(uint16_t obj)
{
    wr16(obj + O_STATE, 0x2C);
    GsSpinStart(obj);
    gs_fight_tail(obj);
}

/* loc_053910: a shot maybe; the distance turned on (+$5C) by the speed */
static void gs_swing_on(uint16_t obj)
{
    GsShot42C();
    wr32(obj + 0x5C, rd32(obj + 0x5C) + rd32(obj + 0x4C));
    gs_fight_tail(obj);
}

/* loc_053890: spinning left (to -$2000); from difficulty 4 sometimes the other step of the
 * pair; after -$20 of turn, faster (or stopped, one time in 2 past -$5800) */
static void gs_swing_left(uint16_t obj)
{
    if (!below32(obj + 0x4C, 0xFFFFE000)) wr32(obj + 0x4C, rd32(obj + 0x4C) - 0x400);
    if (!below(v_difficulty, 4) && !below32(obj + 0x5C, 0xFFF80000) && (rd8(v_randomSeed + 1) & 0x80)) {
        if (rd16(obj + O_STATE) != 0x1C) back(obj, 8);
        next(obj, 4);
    }
    if (below32(obj + 0x5C, 0xFFE00000)) {
        wr32(obj + 0x5C, 0);
        if (below32(obj + 0x4C, 0xFFFFA800) && !bit8(v_randomSeed + 1, 0)) {
            gs_spin_stop(obj);
            return;
        }
        wr32(obj + 0x4C, rd32(obj + 0x4C) - 0x800);
    }
    gs_swing_on(obj);
}

/* loc_053948: the same spinning right */
static void gs_swing_right(uint16_t obj)
{
    if (below32(obj + 0x4C, 0x2000)) add32(obj + 0x4C, 0x400);
    if (!below(v_difficulty, 4) && below32(obj + 0x5C, 0x80000) && (rd8(v_randomSeed + 1) & 0x80)) {
        if (rd16(obj + O_STATE) != 0x24) back(obj, 8);
        next(obj, 4);
    }
    if (below32(obj + 0x5C, 0x200000)) {
        gs_swing_on(obj);
        return;
    }
    wr32(obj + 0x5C, 0);
    if (!below32(obj + 0x4C, 0x5800) && !bit8(v_randomSeed + 1, 0)) {
        gs_spin_stop(obj);
        return;
    }
    add32(obj + 0x4C, 0x800);
    gs_swing_on(obj);
}

/* $053868. Spinning left, rising to height $60. */
void Gs1C(uint16_t obj)
{
    if (below(obj + 0x58, 0x60)) add16(obj + 0x58, 1);
    gs_swing_left(obj);
}

/* $05387A. Spinning left, the height toward $30. */
void Gs20(uint16_t obj)
{
    if (below(obj + 0x58, 0x30)) add16(obj + 0x58, 1);
    else wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 1));
    gs_swing_left(obj);
}

/* $053920. Spinning right, rising to $60. */
void Gs24(uint16_t obj)
{
    if (below(obj + 0x58, 0x60)) add16(obj + 0x58, 1);
    gs_swing_right(obj);
}

/* $053932. Spinning right, the height toward $30. */
void Gs28(uint16_t obj)
{
    if (below(obj + 0x58, 0x30)) add16(obj + 0x58, 1);
    else wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 1));
    gs_swing_right(obj);
}

/* $0539BE. The spin dying: then $40 frames. */
void Gs2C(uint16_t obj)
{
    if (GsSpinDown(obj)) {
        next(obj, 4);
        wr16(obj + 0x5C, 0x40);
    }
    gs_fight_tail(obj);
}

/* $0539D4. Then (step $58) $C0 frames of shots. */
void Gs30(uint16_t obj)
{
    if (dec(obj + 0x5C)) {
        wr16(obj + O_STATE, 0x58);
        wr16(obj + 0x5C, 0xC0);
    }
    gs_fight_tail(obj);
}

/* $0539EC. Hovering. */
void Gs34(uint16_t obj)
{
    GsShotIfWeak();
    GsHover(obj);
    gs_fight_tail(obj);
}

/* $053AB0. Dropping ($6000 a frame) until Golden Silver is at y $138: a thud, a burst. */
void Gs38(uint16_t obj)
{
    GsShotIfWeak();
    add32(obj + 0x5C, 0x6000);
    wr32(obj + 0x58, rd32(obj + 0x58) + rd32(obj + 0x5C));
    if (!below(0xC9F4, 0x138)) {
        next(obj, 4);
        wr16(obj + O_Y, 0x138);
        wr32(obj + 0x4C, 0);
        wr16(obj + 0x5C, 0x20);
        wr16(v_shakeSprites, 8);
        QueueSound(0xD1);
        GsBurst();
    }
    gs_fight_tail(obj);
}

/* $053AFA. Down +$5C frames. */
void Gs3C(uint16_t obj)
{
    GsShotIfWeak();
    if (dec(obj + 0x5C)) next(obj, 4);
    gs_fight_tail(obj);
}

/* $053B0E. Rising 6 a frame; back above 0, what next (step $18). */
void Gs40(uint16_t obj)
{
    wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 6));
    if (rd16(obj + 0x58) & 0x8000) wr16(obj + O_STATE, 0x18);
    GsTurnToPlayer(obj);
    gs_fight_tail(obj);
}

/* $053B24. Shooting (above -$20, after $20 frames): every $10 frames a $438 from Golden Silver
 * aimed at the weaker player (with a5 set to $C9E0, and after it to $C980: the director's slot,
 * the 68k's own assumption); else sinking 2 a frame; after +$5C frames what next. */
void Gs58(uint16_t obj)
{
    GsTurnToPlayer(obj);
    if ((rd16(obj + 0x58) & 0x8000) && below(obj + 0x58, 0xFFE0) && !below(obj + 0x5C, 0x20)) {
        if (!(rd16(v_gameFrames) & 0xF)) {
            uint16_t target = PlayerWeaker();
            uint16_t slot;
            SpawnAimed(GS, target, 0x438, 0x10, 0, &slot);
            obj = v_boss;
        }
    } else {
        wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 2));
    }
    if (dec(obj + 0x5C)) wr16(obj + O_STATE, 0x18);
    gs_fight_tail(obj);
}

/* loc_053BC2: beaten, burning: explosions, a shake, every other frame a blast ($5639C) about
 * Golden Silver, thrown up */
static void gs_burning(uint16_t obj)
{
    BlastSound8();
    wr16(v_shakeSprites, 8);
    if (!bit8(v_gameFrames + 1, 0)) {
        uint16_t slot, t;
        if (SlotFreeHigh(&slot, &t)) {
            wr16(slot + O_TYPE, 0x100);
            wr32(slot + 0x48, 0x05639C);
            wr16(slot + O_X, (uint16_t)((rd16(v_randomSeed) & 0x3F) - 0x20 + rd16(GS + O_X)));
            wr16(slot + O_Y, rd16(GS + O_Y));
            add16(slot + O_Y, 8);
            wr32(slot + 0x1C, 0xFFFD8000);
        }
    }
    gs_spin_tail(obj);
}

/* $053B88. Beaten: no spin, sinking to -$30, burning +$5C frames; then $80 frames. */
void Gs44(uint16_t obj)
{
    wr32(obj + 0x4C, 0);
    if (!(rd16(obj + 0x58) & 0x8000) || !below(obj + 0x58, 0xFFD0))
        wr16(obj + 0x58, (uint16_t)(rd16(obj + 0x58) - 1));
    if (dec(obj + 0x5C) && below(obj + 0x58, 0xFFD0)) {
        next(obj, 4);
        wr16(obj + 0x5C, 0x80);
        gs_spin_tail(obj);
        return;
    }
    gs_burning(obj);
}

/* $053C1C. Burning $80 frames; then the fall (speed -2). */
void Gs48(uint16_t obj)
{
    if (dec(obj + 0x5C)) {
        next(obj, 4);
        wr32(obj + 0x5C, 0xFFFE0000);
    }
    gs_burning(obj);
}

/* $053C34. Falling until Golden Silver is at y $138: a crash, the palettes to fade from 7,
 * every object from $C9E0 gone (but the burning pieces, $100). */
void Gs4C(uint16_t obj)
{
    GsTurnToPlayer(obj);
    add32(obj + 0x5C, 0x4000);
    wr32(obj + 0x58, rd32(obj + 0x58) + rd32(obj + 0x5C));
    if (!below(0xC9F4, 0x138)) {
        next(obj, 4);
        wr16(v_shakeSprites, 0x10);
        QueueSound(0xBD);
        fades_set(7);
        ObjectsVanishFrom(GS, 0x100, 0);
    }
    gs_burning(obj);
}

/* $053C8C. Fading every other frame to 0: then $80 frames. */
void Gs50(uint16_t obj)
{
    if (!bit8(v_gameFrames + 1, 0) && fades_down()) {
        next(obj, 4);
        wr16(obj + 0x5C, 0x80);
    }
    PalettesFull();
}

/* $053CBA. Then the second form's arrival (step $5C, below difficulty 4 step $6C): a slow spin,
 * a sound $D9. */
void Gs54(uint16_t obj)
{
    if (!dec(obj + 0x5C)) return;
    wr16(obj + O_STATE, 0x5C);
    if (below(v_difficulty, 4)) wr16(obj + O_STATE, 0x6C);
    wr32(obj + 0x4C, 0x100);
    wr16(obj + 0x5C, 0x80);
    QueueSound(0xD9);
}

/* $053D06. Steps $64, $80: +$5C frames of $424s. */
void Gs64(uint16_t obj)
{
    if (dec(obj + 0x5C)) {
        next(obj, 4);
        wr16(obj + 0x5C, 0x80);
    }
    GsSpawn424();
    gs_spin_tail(obj);
}

/* $053CF2. Steps $60, $7C: $200 frames of them. (Its difficulty test leads nowhere.) */
void Gs60(uint16_t obj)
{
    next(obj, 4);
    wr16(obj + 0x5C, 0x200);
    Gs64(obj);
}

/* $053D20. Steps $68, $84: $80 frames; then a sound $D9. */
void Gs68(uint16_t obj)
{
    if (!dec(obj + 0x5C)) {
        gs_spin_tail(obj);
        return;
    }
    next(obj, 4);
    wr16(obj + 0x5C, 0x80);
    QueueSound(0xD9);
}

/* loc_05470A: Golden Silver's second form: $C9E0 a $534, the ten after it $538 (+$5C $10 on
 * each) */
static void gs_become_534(void)
{
    uint16_t a0 = GS;
    wr16(a0 + O_TYPE, 0x534);
    wr16(a0 + O_STATE, 0);
    wr16(a0 + O_VITALITY, 0);
    uint16_t d0 = 0;
    for (int i = 0; i < 10; i++) {
        a0 = (uint16_t)(a0 + 0x60);
        d0 = (uint16_t)(d0 + 0x10);
        wr16(a0 + O_TYPE, 0x538);
        wr16(a0 + 0x5C, d0);
    }
}

/* $053D3C. The colours $55134; the second form. */
void Gs70(uint16_t obj)
{
    next(obj, 4);
    GsColours(0x055134);
    gs_become_534();
}

/* $053D4E. While it is a $534: the spin eased by its x speed's low bit; once beaten, on ($88
 * at difficulty 0). */
void Gs74(uint16_t obj)
{
    if (rd16(GS) != 0x534) {
        next(obj, 4);
        if (!rd16(v_difficulty)) wr16(obj + O_STATE, 0x88);
        wr16(obj + 0x5C, 0x80);
        QueueSound(0xD9);
    }
    GsSpin(obj);
    if (bit8(GS + 0x1B, 0)) add32(obj + 0x4C, 0x200);
    else wr32(obj + 0x4C, rd32(obj + 0x4C) - 0x200);
}

/* $053DA0. Step $CC, $80 frames, a sound $F6. */
void Gs8C(uint16_t obj)
{
    GsSpin(obj);
    wr16(obj + O_STATE, 0xCC);
    wr16(obj + 0x5C, 0x80);
    QueueSound(0xF6);
}

/* $053EA8. The spin faster ($400 a frame) (loc_053E74 too). */
void GsSpinOn(uint16_t obj)
{
    add32(obj + 0x4C, 0x400);
    GsSpin(obj);
}

/* $053EA0. The walk, the limbs, the spin. */
void GsMove(uint16_t obj)
{
    GsWalk();
    GsLimbs();
    GsSpinOn(obj);
}

/* $053E1C. Golden Silver walking in from the right ($8040, faster right of $190) until x
 * $1E0: then its depth 8, x offset $100, $100 frames. */
void Gs90(uint16_t obj)
{
    if (below(GS + O_X, 0x190)) wr32(0x8040, rd32(0x8040) - 0x20000);
    else wr32(0x8040, rd32(0x8040) - 0x52000);
    if (!below(GS + O_X, 0x1E0)) {
        wr16(GS + 0x50, 8);
        wr32(GS + 0x48, 0x1000000);
        next(obj, 4);
        wr16(obj + 0x5C, 0x100);
        return;
    }
    wr32(GS + 0x48, rd32(0x8040));
    GsWalk();
    GsLimbs();
    GsFlip();
    GsSpinOn(obj);
}

/* $053DBA. Step $CC: after $80 frames the third form: colours $5512C, fifteen $428 parts, the
 * walk and the arms reset, vitality $7FFF, a sound $88; then as step $90. */
void GsCC(uint16_t obj)
{
    if (!dec(obj + 0x5C)) {
        GsSpinOn(obj);
        return;
    }
    wr16(obj + O_STATE, 0x90);
    GsColours(0x05512C);
    GsParts428();
    wr16(0x9010, 0);
    wr16(0x9016, 0);
    wr16(0x9014, 0xFFFF);
    wr16(0x9018, 0);
    wr32(0x8040, 0);
    wr16(0x901E, 0);
    wr16(0x9012, 0);
    wr32(GS_SPEED, 0);
    wr32(0xC9FC, 0);
    wr32(0xCA28, 0);
    wr32(0xCA2C, 0);
    wr16(0xCA30, 0x110);
    wr16(0xCA04, 0x7FFF);
    QueueSound(0x88);
    Gs90(obj);
}

/* $053E80. $100 frames walking in place; then moving left (-1.25), depth $40. */
void Gs94(uint16_t obj)
{
    wr16(0xCA04, 0x7FFF);
    if (dec(obj + 0x5C)) {
        next(obj, 4);
        wr32(GS_SPEED, 0xFFFEC000);
        wr16(0xCA30, 0x40);
    }
    GsMove(obj);
}

/* $053EB4. Coming forward (its depth down to 8 while the director is left of $140) until left
 * of $120: the fight (vitality $903, the boss's bar, $60 frames). */
void Gs98(uint16_t obj)
{
    wr16(GS + O_VITALITY, 0x7FFF);
    if (below(obj + O_X, 0x140) && rd16(GS + 0x50) != 8) {
        wr16(GS + 0x50, (uint16_t)(rd16(GS + 0x50) - 1));
        GsMove(obj);
        return;
    }
    if (!below(GS + O_X, 0x120)) {
        GsMove(obj);
        return;
    }
    next(obj, 4);
    wr16(GS + O_VITALITY, 0x903);
    wr16(0x80C2, 0x10);
    wr16(0x80D6, GS);
    wr16(0x9020, 0x60);
    wr16(0x9024, 0);
    GsMove(obj);
}

/* loc_053F12: swaying, moving, beaten maybe */
static void gs_attack_tail(uint16_t obj)
{
    GsSway();
    GsMove(obj);
    gs_beaten2(obj);
}

/* $053F06. Walking $9020 frames. */
void Gs9C(uint16_t obj)
{
    if (dec(0x9020)) next(obj, 4);
    gs_attack_tail(obj);
}

/* $053F1E. Coming nearer (depth $CA30 to $80): then the swipe ($901E $82, its arms at $80,
 * $2C frames). */
void GsA0(uint16_t obj)
{
    add16(0xCA30, 2);
    if (below(0xCA30, 0x80)) {
        gs_attack_tail(obj);
        return;
    }
    next(obj, 4);
    wr8(0x901E, 0x82);
    wr16(0x901A, 0x80);
    wr16(0x901C, 0x80);
    wr16(0x9020, 0x2C);
    gs_attack_tail(obj);
}

/* loc_053F58: the swipe's arms turning ($10 a frame; a sound $DB each turn) */
static void gs_swipe(uint16_t obj)
{
    add16(0x901A, 0x10);
    add16(0x901C, 0x10);
    if (!below(0x901C, 0x1FE)) {
        wr16(0x901C, rd16(0x901C) & 0x1FE);
        QueueSound(0xDB);
    }
    gs_attack_tail(obj);
}

/* $053F4C. Swiping $9020 frames. */
void GsA4(uint16_t obj)
{
    if (dec(0x9020)) next(obj, 4);
    gs_swipe(obj);
}

/* $053F82. Swiping, coming forward (depth down 8 to 8): then a shake, $24 frames. */
void GsA8(uint16_t obj)
{
    wr16(0xCA30, (uint16_t)(rd16(0xCA30) - 8));
    if (!below(0xCA30, 8)) {
        gs_swipe(obj);
        return;
    }
    wr16(0xCA30, 8);
    next(obj, 4);
    wr16(0x9020, 0x24);
    wr16(v_shakeSprites, 0x10);
    gs_swipe(obj);
}

/* $053FAA. Then a fireball ($434, at $DE80 or $DEE0 in turn) from its hand ($CCE0); back to
 * walking (step $9C) for the difficulty's time ($53FF6). */
void GsAC(uint16_t obj)
{
    if (!dec(0x9020)) {
        gs_swipe(obj);
        return;
    }
    uint16_t a0 = 0xDE80;
    wr16(0x9024, rd16(0x9024) ^ 1);
    if (!rd16(0x9024)) a0 = 0xDEE0;
    wr16(a0 + O_TYPE, 0x434);
    wr16(a0 + O_X, rd16(0xCCF0));
    wr16(a0 + O_Y, rd16(0xCCF4));
    wr16(a0 + O_STATE, 0);
    wr16(obj + O_STATE, 0x9C);
    wr16(0x9020, rom16(0x053FF6 + (uint32_t)(int16_t)rd16(v_difficulty)));
    wr8(0x901E, 0);
    gs_swipe(obj);
}

/* $053FFE. Beaten: burning +$5C frames, swaying. */
void GsB0(uint16_t obj)
{
    wr16(0x901E, 0);
    if (dec(obj + 0x5C)) next(obj, 4);
    GsSmoke();
    GsSway();
    GsMove(obj);
}

/* $05401A. Burning, stopped, sinking back (depth to $F0): then a crash, the palettes to fade
 * from 7, every object from $C9E0 gone. */
void GsB4(uint16_t obj)
{
    GsSmoke();
    wr32(GS_SPEED, 0);
    add16(0xCA30, 2);
    if (below(0xCA30, 0xF0)) {
        GsMove(obj);
        return;
    }
    next(obj, 4);
    QueueSound(0xBD);
    fades_set(7);
    ObjectsVanishFrom(GS, 0, 0);
    GsSpinOn(obj);
}

/* $054066. Fading every other frame to 0: then $80 frames, a sound $F6. */
void GsB8(uint16_t obj)
{
    if (!bit8(v_gameFrames + 1, 0) && fades_down()) {
        next(obj, 4);
        wr16(obj + 0x5C, 0x80);
        QueueSound(0xF6);
    }
    PalettesFull();
    GsSpinOn(obj);
}

/* $0540A2. Exploding at $120, $E0 for +$5C frames; then the light ($E0AE0 at y $E3), the end
 * message ($80C2 $2C), white at $E31A, the H-interrupt's rays (effect 4, $9600 cleared). */
void GsBC(uint16_t obj)
{
    wr16(obj + O_X, 0x120);
    wr16(obj + O_Y, 0xE0);
    PuffNear(obj);
    BlastSound8();
    wr16(v_shakeSprites, 6);
    if (!dec(obj + 0x5C)) {
        GsSpinOn(obj);
        return;
    }
    next(obj, 4);
    wr16(obj + 0x5C, 0x80);
    wr16(obj + O_Y, 0xE3);
    wr16(obj + O_FLAGS, 0xC000);
    wr16(obj + 0x0E, 0x8000);
    wr32(obj + 0x08, 0x0E0AE0);
    ObjectsVanishFrom(GS, 0, 0);
    wr16(0x80C2, 0x2C);
    GsSpinOn(obj);
    wr16(0xE31A, 0xEEE);
    wr16(0xE39A, 0xEEE);
    wr16(v_hintEffect, 4);
    wr16(v_hintState, 0);
    wr16(obj + 0x5C, 0);
    GsClear9600();
    gs_rays(obj);
}

/* $054126. The rays widening (8 a frame to $80); then off, the palettes to fade from 7. */
void GsC0(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0);
    GsSpin(obj);
    add16(obj + 0x5C, 8);
    if (below(obj + 0x5C, 0x80)) {
        gs_rays(obj);
        return;
    }
    next(obj, 4);
    wr16(v_hintEffect, 0);
    wr16(v_hintState, 0);
    wr16(obj + 0x5C, 0);
    fades_set(7);
    PalettesFull();
}

/* $05419C. Step $C8: the spin only. */
void GsC8(uint16_t obj)
{
    GsSpin(obj);
}

/* $054164. Fading by one every $D frames; past 0 the stage is cleared ($A002 bit 7). */
void GsC4(uint16_t obj)
{
    if (dec(obj + 0x5C)) {
        wr16(obj + 0x5C, 0xC);
        uint16_t d0 = (uint16_t)(rd16(0x8014) - 1);
        if (d0 & 0x8000) {
            next(obj, 4);
            set8(v_stageClear, 7);
        }
        fades_set(d0);
    }
    PalettesFull();
    GsC8(obj);
}

/* $05360A. Type $41C: the Golden Silver fight's director, by its step. */
void Gs(uint16_t obj)
{
    step_by_longs(obj, 0x053614);
}

/* ---- Golden Silver's parts ($420-$42C, $534, $538) and shots ($434, $53C), placed by depth
 * (+$50: the scale from the table at $55B6A), x (+$48) and y (+$4C) about $120, $E0 ---- */

/* $054458. Placed: x $120 + +$48, y $E0 + +$4C, both scaled by the depth (shifted by d5 into
 * the table); in front while nearer than $100. */
void GsPlace(uint16_t obj, uint8_t d5)
{
    if (below(obj + 0x50, 0x100)) set8(obj + 0x0E, 7);
    uint16_t d0 = asl_w(rd16(obj + 0x50) & 0x1FE, d5);
    uint16_t d1 = (uint16_t)(~rom16(GS_TABLE + (uint32_t)(int16_t)d0) + 0x4001);
    uint32_t d2 = (uint32_t)((int32_t)(int16_t)rd16(obj + 0x48) * (int16_t)d1);
    uint32_t d3 = (uint32_t)((int32_t)(int16_t)rd16(obj + 0x4C) * (int16_t)d1);
    wr16(obj + O_X, (uint16_t)((uint16_t)((d2 << 2) >> 16) + 0x120));
    wr16(obj + O_Y, (uint16_t)((uint16_t)((d3 << 2) >> 16) + 0xE0));
}

/* $054448. The same, its priority (+$20) by the depth. */
void GsPlaceDeep(uint16_t obj, uint8_t d5)
{
    wr8(obj + 0x20, (uint8_t)(((int16_t)rd16(obj + 0x50) >> 1 & 0x7C) + 4));
    GsPlace(obj, d5);
}

/* $0544BE. Its sprite and mappings from a0 (returned past them). */
uint32_t GsLook(uint16_t obj, uint32_t a0)
{
    wr16(obj + 0x0E, rom16(a0));
    wr16(obj + 0x08, rom16(a0 + 2));
    wr16(obj + 0x0A, rom16(a0 + 4));
    return a0 + 6;
}

/* $0544AA. Its look by the depth ($544F0, the depth shifted by d5, in 8s). */
uint32_t GsLookBy(uint16_t obj, uint8_t d5)
{
    uint16_t d0 = asr_w(rd16(obj + 0x50), d5) & 0xF8;
    return GsLook(obj, 0x0544F0 + (uint32_t)(int32_t)(int16_t)d0);
}

/* $0544CC. The same $28 back, or from $100 the far look ($545D0). */
void GsLookFar(uint16_t obj, uint8_t d5)
{
    uint16_t d0 = rd16(obj + 0x50);
    if (!((int16_t)(d0 - 0x100) < 0)) {
        GsLook(obj, 0x0545D0);
        return;
    }
    d0 = (uint16_t)((asr_w(d0, d5) & 0xF8) - 0x28);
    GsLook(obj, 0x0544F0 + (uint32_t)(int32_t)(int16_t)d0);
}

/* $0545F0. A small one's look ($54624 by the depth in 8s, or from $100 $5463C). */
void GsLookSmall(uint16_t obj)
{
    uint32_t a0 = 0x054624;
    uint16_t d0 = rd16(obj + 0x50);
    if (!((int16_t)(d0 - 0x100) < 0)) a0 = 0x05463C;
    else a0 += (uint32_t)(int32_t)(int16_t)((uint16_t)((int16_t)d0 >> 3) & 0x18);
    wr16(obj + 0x0E, rom16(a0));
    wr16(obj + 0x08, rom16(a0 + 2));
    wr16(obj + 0x0A, rom16(a0 + 4));
}

/* $055198. Type $420: a part of Golden Silver's first form; the first one (+6) hurts and
 * smokes while near (depth below $20), its look $A420. */
void Gs420(uint16_t obj)
{
    GsLookFar(obj, 0);
    GsPlaceDeep(obj, 0);
    if (rd16(obj + 6) && below(obj + 0x50, 0x20)) {
        wr8(obj + 0x21, 0x30);
        wr16(obj + 0x0E, 0xA420);
        wr16(obj + 0x08, 0xF00);
        wr16(obj + 0x0A, 0xF0F0);
        wr16(obj + 0x0E, rd16(obj + 0x0E) | rd16(0x9016));
        if (!below(0xC984, 0x28)) return;
        GsSpark(obj);
        return;
    }
    if (rd16(obj + 6)) wr8(obj + 0x21, 0);
    wr16(obj + 0x0E, rd16(obj + 0x0E) | rd16(0x9016));
}

/* $0551F6. Type $424: a piece coming at the screen (depth from $1E0 down 8 a frame, drifting
 * toward the player); there, $20 frames hurting, flashing; then a blast ($5639C). */
void Gs424(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 8));
        if (rd16(obj + 0x50) & 0x8000) {
            next(obj, 4);
            wr16(obj + 0x5C, 0x20);
            return;
        }
        uint16_t p = PlayerTarget();
        if ((uint16_t)(rd16(p + O_X) - rd16(obj + O_X)) & 0x8000) wr32(obj + 0x48, rd32(obj + 0x48) - 0x8000);
        else add32(obj + 0x48, 0x8000);
        GsLookSmall(obj);
        GsPlaceDeep(obj, 1);
        return;
    }
    wr16(obj + 0x5C, (uint16_t)(rd16(obj + 0x5C) - 1));
    uint16_t t = rd16(obj + 0x5C);
    if (t & 0x8000) {
        wr16(obj + O_TYPE, 0x100);
        wr32(obj + 0x48, 0x05639C);
        wr16(obj + O_STATE, 0);
        wr8(obj + 0x21, 0);
        QueueSound(0xCE);
        return;
    }
    if (t) {
        if (!bit8(v_gameFrames + 1, 0)) set8(obj + O_FLAGS, 7);
        else clr8(obj + O_FLAGS, 7);
        return;
    }
    wr8(obj + 0x21, 0x10);
    wr32(obj + 0x2C, 0xF40CF40C);
    wr16(obj + 0x26, 0x0C);
    wr8(obj + 0x23, 0x20);
}

/* $055362. (The hand at $CCE0, every other frame.) An afterimage ($250) of it. */
void GsTrail(uint16_t obj)
{
    if (obj != 0xCCE0) return;
    if (bit8(v_gameFrames + 1, 0)) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x250);
    wr16(slot + O_FLAGS, 0x8000);
    wr8(slot + 0x20, rd8(obj + 0x20));
    wr16(slot + 0x0E, rd16(obj + 0x0E));
    wr16(slot + 0x08, rd16(obj + 0x08));
    wr16(slot + 0x0A, rd16(obj + 0x0A));
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr16(slot + 0x48, 1);
    wr16(slot + 0x4A, 0x0C);
    wr16(slot + 0x0E, rd16(obj + 0x0E) | 0xC000);
}

/* $0552A6. Type $428: a part of the third form: its look by depth, placed; behind below y
 * $118; the hands (+6 2) hurt while swiping near, the body (+6 1) is hit while near. */
void Gs428(uint16_t obj)
{
    if (!below(obj + 0x50, 0x100)) GsLook(obj, 0x0545C0);
    else GsLookBy(obj, 0);
    GsPlace(obj, 1);
    wr16(obj + 0x0E, rd16(obj + 0x0E) | rd16(0x9016));
    if (!below(obj + O_Y, 0x118)) clr8(obj + 0x0E, 7);
    if (!rd16(obj + 6)) return;
    if (bit8(obj + 7, 0)) {
        if (!(rd16(obj + O_VITALITY) & 0x8000) && below(obj + 0x50, 0xA)) set8(obj + 0x21, 5);
        else clr8(obj + 0x21, 5);
        return;
    }
    if (below(obj + 0x50, 8)) set8(obj + 0x0E, 7);
    if (bit8(obj + 7, 1) && bit8(0x901E, 7)) {
        GsTrail(obj);
        if (below(obj + 0x50, 8) && !(rd16(obj + O_VITALITY) & 0x8000)) {
            set8(obj + 0x21, 4);
            return;
        }
    }
    clr8(obj + 0x21, 4);
}

/* $0553C0. Type $42C: a cannon coming forward (depth from $1E0 down 4 a frame); there,
 * vitality 8, firing a pellet ($1A0) every $20 frames once above y $78; shot, a blast. */
void Gs42C(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) & 0x8000) {
        AddScore(obj, 0x50);
        wr16(obj + O_TYPE, 0x100);
        wr32(obj + 0x48, 0x05639C);
        wr16(obj + O_STATE, 0);
        wr8(obj + 0x21, 0);
        QueueSound(0xCA);
        return;
    }
    if (!rd16(obj + O_STATE)) {
        wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 4));
        if (!(rd16(obj + 0x50) & 0x8000)) {
            GsLookSmall(obj);
            GsPlaceDeep(obj, 1);
            return;
        }
        next(obj, 4);
        wr32(obj + 0x28, 0xF010F010);
        wr8(obj + 0x21, 0x20);
        wr16(obj + O_VITALITY, 8);
        wr16(obj + 0x5C, 0x20);
        wr8(obj + 0x20, 8);
        return;
    }
    if (below(obj + O_Y, 0x78)) return;
    if (!dec(obj + 0x5C)) return;
    wr16(obj + O_FLAGS, 0x8680);
    wr16(obj + 0x1C, 0xFFF0);
    uint16_t slot;
    if (!SpawnAt(obj, 0x1A0, 0x10, 0, 0x20, &slot)) return;
    PelletBehind(slot);
    wr8(slot + 0x20, 4);
}

/* $0558FA. Type $434: Golden Silver's fireball: pushed left (by the difficulty's step,
 * $559F6; the one at $DE80 also up) a little more each frame, flickering ($55A06), leaving
 * puffs ($565D4) every 4th frame. */
void Gs434(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        QueueSound(0xB9);
        next(obj, 4);
        wr16(obj + O_FLAGS, 0x8E80);
        wr8(obj + 0x21, 0x10);
        wr32(obj + 0x2C, 0xF810F810);
        wr16(obj + 0x26, 7);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr8(obj + 0x20, 4);
        wr32(obj + 0x48, rom16(0x0559F6 + (uint32_t)(int16_t)rd16(0xA20C)));
        wr32(obj + 0x4C, 0);
        wr16(obj + 0x52, 0);
        if (obj == 0xDE80) wr32(obj + 0x4C, 0x1800);
    }
    wr32(obj + 0x18, rd32(obj + 0x18) - rd32(obj + 0x48));
    wr32(obj + 0x1C, rd32(obj + 0x1C) - rd32(obj + 0x4C));
    add16(obj + 0x52, 6);
    if (!below(obj + 0x52, 0x18)) wr16(obj + 0x52, 0);
    uint32_t a0 = 0x055A06 + rd16(obj + 0x52);
    wr16(obj + 0x0E, rom16(a0));
    wr16(obj + 0x08, rom16(a0 + 2));
    wr16(obj + 0x0A, rom16(a0 + 4));
    if (rd16(obj + 0x4E)) wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x6000);
    if (rd16(v_gameFrames) & 3) return;
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x0565D4);
    wr32(slot + 0x18, 0x4E20);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, (uint16_t)(rd16(obj + O_Y) - 0x10 + (rd16(v_randomSeed) & 0x1F)));
}

/* $055A1E. Type $53C: a shot aimed at the player (speed 1 at its angle), down at y $148 a
 * puff; its look ($56654, $5666C by the frame) and palette cycling. */
void Gs53C(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + O_FLAGS, 0x8E80);
        wr8(obj + 0x21, 0x10);
        wr32(obj + 0x2C, 0xFC04F907);
        wr16(obj + 0x26, 8);
        uint16_t d7 = SpaceAim(obj);
        wr32(obj + 0x1C, (uint32_t)(int32_t)(int16_t)rom16(0x02977C + (uint32_t)(int16_t)d7) << 3);
        wr32(obj + 0x18, (uint32_t)(int32_t)(int16_t)rom16(0x0297FC + (uint32_t)(int16_t)d7) << 3);
    }
    if (!below(obj + O_Y, 0x148)) {
        wr16(obj + O_TYPE, 0x100);
        wr32(obj + 0x48, 0x05639C);
        wr16(obj + O_STATE, 0);
        wr8(obj + 0x21, 0);
        wr32(obj + 0x1C, 0xFFFE0000);
        return;
    }
    uint32_t a0 = bit8(v_gameFrames + 1, 1) ? 0x05666C : 0x056654;
    wr16(obj + 0x0E, rom16(a0));
    wr16(obj + 0x08, rom16(a0 + 2));
    wr16(obj + 0x0A, rom16(a0 + 4));
    uint16_t d0 = (uint16_t)((uint16_t)((rd16(v_gameFrames) & 0xC) << 9) | 0x8000);
    wr16(obj + 0x0E, rd16(obj + 0x0E) | d0);
}

/* ---- type $534: Golden Silver's second form, a snake: its head (+$48, +$4C the offsets
 * swaying by speeds +$18, +$1C turning at their ends; +$50 the depth) leaves its places in the
 * history at $9800 (x), $9940 (y), $9A80 (look), $9BC0 (+$20 and its hurt bits) for the ten
 * segments ($538, +$5C how far back) ---- */

/* $05571E. Its x sway: the speed (+$18, its low bit the way) eased by $2000 to +-4, turning
 * past -$40 / $40. */
void GsSnakeSwayX(uint16_t obj)
{
    uint32_t d0 = rd32(obj + 0x18);
    if (!(d0 & 1)) {
        if ((rd16(obj + 0x48) & 0x8000) && below(obj + 0x48, 0xFFC0)) d0 |= 1;
        if (!((int32_t)(d0 - 0xFFFC0000) < 0)) d0 -= 0x2000;
    } else {
        if (!(rd16(obj + 0x48) & 0x8000) && !below(obj + 0x48, 0x40)) d0 &= ~1u;
        if ((int32_t)(d0 - 0x40000) < 0) d0 += 0x2000;
    }
    wr32(obj + 0x18, d0);
    wr32(obj + 0x48, rd32(obj + 0x48) + d0);
}

/* $055784. Its y sway: the same by $1000 to -2..2, turning past -$C000 / -$6000. */
void GsSnakeSwayY(uint16_t obj)
{
    uint32_t d0 = rd32(obj + 0x1C);
    if (!(d0 & 1)) {
        if (!(rd16(obj + 0x4C) & 0x8000)) {
            d0 -= 0x1000;
        } else {
            if (below32(obj + 0x4C, 0xFFFF4000)) d0 |= 1;
            if (!((int32_t)(d0 - 0xFFFE0000) < 0)) d0 -= 0x1000;
        }
    } else {
        if (!(rd16(obj + 0x4C) & 0x8000)) {
            d0 -= 0x1000;
        } else {
            if (!below32(obj + 0x4C, 0xFFFFA000)) d0 &= ~1u;
            if ((int32_t)(d0 - 0x20000) < 0) d0 += 0x1000;
        }
    }
    wr32(obj + 0x1C, d0);
    wr32(obj + 0x4C, rd32(obj + 0x4C) + d0);
}

/* loc_055640: the history moved on a place (159 entries of each), the head's look, place and
 * +$20 at its start; near (depth below 8) it hurts and every +$5E frames (by the difficulty,
 * $55716) a shot ($53C) leaves it; beaten, +$20's bit 1 tells the segments to break */
static void gs_snake_history(uint16_t obj)
{
    for (int k = 158; k >= 0; k--) {
        wr16((uint16_t)(0x9802 + 2 * k), rd16((uint16_t)(0x9800 + 2 * k)));
        wr16((uint16_t)(0x9942 + 2 * k), rd16((uint16_t)(0x9940 + 2 * k)));
    }
    for (int k = 158; k >= 0; k--) {
        wr16((uint16_t)(0x9A82 + 2 * k), rd16((uint16_t)(0x9A80 + 2 * k)));
        wr8((uint16_t)(0x9BC1 + k), rd8((uint16_t)(0x9BC0 + k)));
    }
    uint32_t a0 = GsLookBy(obj, 0);
    wr16(0x9A80, (uint16_t)(a0 - 0x0544F6));
    wr16(obj + 0x0E, rd16(obj + 0x0E) | rd16(0x9016));
    GsPlaceDeep(obj, 1);
    wr16(0x9800, rd16(obj + O_X));
    wr16(0x9940, rd16(obj + O_Y));
    wr8(0x9BC0, rd8(obj + 0x20));
    if (rd16(obj + O_VITALITY) & 0x8000) {
        set8(0x9BC0, 1);
        wr8(obj + 0x21, 0);
        return;
    }
    if (!below(obj + 0x50, 8)) {
        wr8(obj + 0x21, 0);
        return;
    }
    set8(0x9BC0, 0);
    wr8(obj + 0x21, 0x20);
    if (!dec(obj + 0x5E)) return;
    wr16(obj + 0x5E, rom16(0x055716 + (uint32_t)(int16_t)rd16(v_difficulty)));
    uint16_t slot;
    if (!SlotFreeLast(&slot)) return;
    wr16(slot + O_TYPE, 0x53C);
    wr16(slot + O_STATE, 0);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    QueueSound(0xB6);
}

/* $055638. It sways and the history moves on. */
void GsSnakeMove(uint16_t obj)
{
    GsSnakeSwayX(obj);
    GsSnakeSwayY(obj);
    gs_snake_history(obj);
}

/* $0554D0. Step 0: the boss's bar, vitality $95D, depth $EC, $200 frames; the history cleared. */
void GsSnake00(uint16_t obj)
{
    wr16(0x80C2, 0x10);
    wr16(0x80D6, obj);
    wr16(obj + O_VITALITY, 0x95D);
    next(obj, 4);
    wr16(obj + O_FLAGS, 0x8000);
    wr32(obj + 0x28, 0xF010F010);
    wr32(obj + 0x48, 0);
    wr16(obj + 0x4C, 0x20);
    wr16(obj + 0x50, 0xEC);
    wr16(obj + 0x5C, 0x200);
    for (uint16_t a = 0x9800; a != 0x9C00; a = (uint16_t)(a + 4)) wr32(a, 0);
    GsSnakeMove(obj);
}

/* $05551C. Weaving (the depth between $80 and $E0 by its x way) +$5C frames. */
void GsSnake04(uint16_t obj)
{
    GsSnakeMove(obj);
    if (dec(obj + 0x5C)) {
        next(obj, 4);
        wr16(obj + 0x5E, 0x40);
    }
    if (bit8(obj + 0x1B, 0)) {
        if (!below(obj + 0x50, 0xE0)) return;
        add16(obj + 0x50, 2);
        return;
    }
    if (below(obj + 0x50, 0x80)) return;
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 2));
}

/* $05555C. Coming forward (depth down 4 a frame to 2): then 0-2 more turns of its sway. */
void GsSnake08(uint16_t obj)
{
    wr16(obj + 0x50, (uint16_t)(rd16(obj + 0x50) - 4));
    if (!below(obj + 0x50, 2)) {
        GsSnakeMove(obj);
        return;
    }
    next(obj, 4);
    wr16(obj + 0x50, 2);
    wr16(obj + 0x5A, rd16(obj + 0x1A) & 1);
    wr16(obj + 0x5C, (uint16_t)((rd16(v_randomSeed) & 3) - 1));
    GsSnakeMove(obj);
}

/* $055592. Near, flashing when hit; counting its sway's turns. */
void GsSnake0C(uint16_t obj)
{
    if (bit8(obj + 0x22, 5)) wr16(0x9016, 0x6000);
    GsSnakeMove(obj);
    uint16_t d0 = rd16(obj + 0x1A) & 1;
    if (d0 != rd16(obj + 0x5A) && dec(obj + 0x5C)) {
        next(obj, 4);
        return;
    }
    wr16(obj + 0x5A, d0);
}

/* $0555CA. Going back (depth up 4 to $D0). */
void GsSnake10(uint16_t obj)
{
    add16(obj + 0x50, 4);
    if (below(obj + 0x50, 0xD0)) {
        GsSnakeMove(obj);
        return;
    }
    next(obj, 4);
    wr16(obj + 0x5A, rd16(obj + 0x1A) & 1);
    GsSnakeMove(obj);
}

/* $0555EC. Far, until its sway turns: then forward again (step 8). */
void GsSnake14(uint16_t obj)
{
    GsSnakeMove(obj);
    uint16_t d0 = rd16(obj + 0x1A) & 1;
    if (d0 == rd16(obj + 0x5A)) {
        wr16(obj + 0x5A, d0);
        return;
    }
    wr16(obj + O_STATE, 8);
}

/* $05560E. Beaten: shaking, the history moving on (the head still) +$5C frames. */
void GsSnake18(uint16_t obj)
{
    wr16(v_shakeSprites, 4);
    if (!dec(obj + 0x5C)) {
        gs_snake_history(obj);
        return;
    }
    next(obj, 4);
    wr16(obj + 0x5C, 0x20);
}

/* $055628. Then $20 frames, and gone. */
void GsSnake1C(uint16_t obj)
{
    if (dec(obj + 0x5C)) wr16(obj + O_FLAGS, 0x1000);
}

/* $055480. Type $534: the snake. Beaten (before step $18): step $18, $90 frames, hidden. */
void GsSnake(uint16_t obj)
{
    wr16(0x9016, 0);
    if ((rd16(obj + O_VITALITY) & 0x8000) && below(obj + O_STATE, 0x18)) {
        wr16(obj + O_STATE, 0x18);
        wr16(obj + 0x5C, 0x90);
        wr16(obj + O_FLAGS, 0);
    }
    step_by_longs(obj, 0x0554B0);
}

/* $0557EE. Type $538: a segment of the snake, +$5C bytes back in the history: its place,
 * look, +$20; the first ones (below $CC20) hurt and pass their damage to the head's
 * vitality ($CA04); told to break, a blast thrown out at a random angle. */
void GsSnakeSegment(uint16_t obj)
{
    if (bit8(obj + 0x22, 5)) wr16(0x9016, 0x6000);
    if (!rd16(obj + O_STATE)) {
        next(obj, 4);
        wr16(obj + O_FLAGS, 0x8000);
        wr32(obj + 0x28, 0xF010F010);
    }
    uint16_t d0 = rd16(obj + 0x5C);
    wr16(obj + O_X, rd16((uint16_t)(0x9800 + d0)));
    wr16(obj + O_Y, rd16((uint16_t)(0x9940 + d0)));
    uint32_t a0 = 0x0544F0 + (uint32_t)(int32_t)(int16_t)rd16((uint16_t)(0x9A80 + d0));
    a0 = GsLook(obj, a0);
    wr16(obj + 0x0E, rd16(obj + 0x0E) | rd16(0x9016));
    if ((int32_t)(a0 - 0x0545A6) < 0) wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x8000);
    d0 = (uint16_t)((int16_t)d0 >> 1);
    wr8(obj + 0x20, rd8((uint16_t)(0x9BC0 + d0)));
    if (bit8(obj + 0x20, 1)) {
        wr16(obj + O_TYPE, 0x100);
        wr32(obj + 0x48, 0x05639C);
        wr16(obj + O_STATE, 0);
        wr8(obj + 0x21, 0);
        uint16_t a = (uint16_t)((rd16(v_randomSeed) & 0x1C0) + 0x20);
        wr32(obj + 0x1C, (uint32_t)(int32_t)(int16_t)rom16(0x02977C + a) << 3);
        wr32(obj + 0x18, (uint32_t)(int32_t)(int16_t)rom16(0x0297FC + a) << 4);
        QueueSound(0xCA);
        return;
    }
    if (!bit8(obj + 0x20, 0)) {
        wr8(obj + 0x21, 0);
        return;
    }
    if (!((int32_t)((uint32_t)(int32_t)(int16_t)obj - 0xFFFFCC20u) < 0)) return;
    wr8(obj + 0x21, 0x20);
    if (rd16(0xCA04) & 0x8000) return;
    wr16(0xCA04, (uint16_t)(rd16(0xCA04) + rd16(obj + O_VITALITY)));
    wr16(obj + O_VITALITY, 0);
}

/* ---- type $94 ($5717C): a spark that never runs past its first frame: its step 0 adds 1 (not
 * 4) to the step, so the next frame's table read is a long at an odd address ($57187), which
 * on the 68000 is an address error. No run makes one. ---- */

/* $05718E. Step 0: shown ($E13F4, $6480 and the palette), its +$C d0 (the objects' loop
 * leaves the type there, $94); the step set to 1 (sic). */
void Spark94Start(uint16_t obj, uint16_t d0)
{
    wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) + 1));
    wr32(obj + 0x08, 0x0E13F4);
    wr16(obj + O_FLAGS, 0xEF00);
    wr8(obj + 0x20, 0x80);
    wr16(obj + 0x0E, 0x6480);
    wr16(obj + 0x0C, d0);
    wr16(obj + 0x26, 0x0A);
    wr16(obj + 0x0E, rd16(obj + 0x0E) | rd16(0x808A));
}

/* $0571C0. Step 4: nothing. */
void Spark94Idle(uint16_t obj)
{
    (void)obj;
}

/* $05717C. Type $94, by its step (d0 from the objects' loop: the type). Step 1 reads the table
 * at an odd address: the 68k takes an address error there; the C reads the long $05718E00 and
 * dispatches to it (an address no routine has). */
void Spark94(uint16_t obj)
{
    uint32_t target = rom32(0x057186 + (uint32_t)(int16_t)rd16(obj + O_STATE));
    if (target == 0x05718E) Spark94Start(obj, rd16(obj + O_TYPE));
    else call_object(target, obj);
}

/* ---- shared helpers ---- */

/* $056AFE. An object of `type` at (dx, dy) from this one, `arg` at its +$48, in a free slot
 * from the middle (SpawnAt's twin): Z set and the slot, if there was one. */
int SpawnAtMid(uint16_t obj, uint16_t type, uint16_t dy, uint16_t dx, uint16_t arg, uint16_t *slot)
{
    uint16_t t;
    if (!SlotFreeMid(slot, &t)) return 0;
    wr16(*slot + O_TYPE, type);
    wr16(*slot + O_X, (uint16_t)(dx + rd16(obj + O_X)));
    wr16(*slot + O_Y, (uint16_t)(dy + rd16(obj + O_Y)));
    wr16(*slot + 0x48, arg);
    return 1;
}

/* $02A240. An angle (d2, 16 bits) to an index of words (d2) and to a 16th of the turn (d0). */
uint16_t AngleSixteenth(uint16_t *angle)
{
    *angle = (uint16_t)(((int16_t)*angle >> 7) & 0x1FE);
    return (uint16_t)((*angle + 0x10) & 0x1E0);
}

/* $056AC8. SpawnAt, its +$48 the angle from here to the object at a0 (in 32nds, times 4). */
int SpawnAimed(uint16_t obj, uint16_t target, uint16_t type, uint16_t dy, uint16_t dx, uint16_t *slot)
{
    uint32_t d1 = (uint16_t)(rd16(target + O_Y) - rd16(obj + O_Y));
    uint16_t a = Atan2((int16_t)(rd16(target + O_X) - rd16(obj + O_X)), &d1);
    uint16_t d0 = AngleSixteenth(&a);
    d0 = (uint16_t)((int16_t)d0 >> 2) & 0x7C;
    return SpawnAt(obj, type, dy, dx, d0, slot);
}

/* $02A038. The player with the less vitality (of those playing and shown or blinking; equal,
 * by the frame's bit 9). */
uint16_t PlayerWeaker(void)
{
    uint8_t p = rd8(v_players);
    if (!(p & 1)) return (p & 2) ? v_player2 : v_player1;
    if (!(p & 2)) return v_player1;
    if (!bit8(v_player1 + 0x23, 2) && !bit8(v_player1 + O_FLAGS, 7)) return v_player2;
    if (!bit8(v_player2 + 0x23, 2) && !bit8(v_player2 + O_FLAGS, 7)) return v_player1;
    uint16_t v1 = rd16(v_player1 + O_VITALITY), v2 = rd16(v_player2 + O_VITALITY);
    if (v1 == v2) return bit8(v_gameFrames, 1) ? v_player2 : v_player1;
    return (int16_t)(v1 - v2) < 0 ? v_player1 : v_player2;
}

/* $03146E. +$5A, the blinking's time, by the difficulty ($31480). */
void BlinkStart(uint16_t obj)
{
    wr16(obj + 0x5A, rom16(0x031480 + (uint32_t)(int16_t)rd16(v_difficulty)));
}

/* $03D2F6. A puff ($565F6, type $100) within $20 of it at random. */
void PuffNear(uint16_t obj)
{
    uint16_t slot, t;
    if (!SlotFreeHigh(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x100);
    wr32(slot + 0x48, 0x0565F6);
    wr16(slot + O_X, (uint16_t)((rd8(v_randomSeed) & 0x3F) + (uint16_t)(rd16(obj + O_X) - 0x20)));
    wr16(slot + O_Y, (uint16_t)((rd8(v_randomSeed + 1) & 0x3F) + (uint16_t)(rd16(obj + O_Y) - 0x20)));
}

/* $020686. The picture at a1 shrunk at sizes from $20 down by $8046 to 0, each copied to the
 * VRAM at a4 by the command list at a0 (as ScaleTiles: the step $8000 / size, a divs; a
 * quotient too big for a word leaves $8000). The 68k leaves a5 at $9800 on (its callers save
 * it). */
void ScaleShrink(uint16_t list, uint16_t src, uint16_t vram)
{
    wr16(list, 1);
    wr16(0x8040, 0x20);
    do {
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
        wr16(0x8040, (uint16_t)(rd16(0x8040) - rd16(0x8046)));
    } while (rd16(0x8040) && !(rd16(0x8040) & 0x8000));
}
