/* Object types no recorded run reaches (their code all `;!` in the disassembly), ported from the
 * disassembly alone:
 * - type $2C8 ($4050E): a gun turret like GunPod's;
 * - the dice palace's rooms: $14 ($42358) the serpent of 32 segments; $264 ($429F8) the cage
 *   and $268 ($42BFA) its balls; $26C ($42F66) the hopping giant of 15 parts; $2FC ($43EEE) the
 *   machine drawn in plane A, $300 ($445E2) its panels, $304 ($446CC) its guard; $270
 *   ($446F6) the peg room, $274 ($4495C) its runners, $278 ($44A08) its fuse; $2D4 ($44E80)
 *   the race; $2D8 ($45010) the wheel and its cars $2DC, $2E8, $2E4, $2E0, $2EC ($45488-
 *   $45798), $2F0 ($45994) the mortar's bomb, $310 ($45B68) the dropper's drop; $32C ($469D4)
 *   the maw, $33C ($4734E) its ball; $35C ($4743E) the brawler boss; $364 ($4841A) the spring,
 *   $368 ($48A70) its pod; $43C ($48BDA) the captain's room, $658 ($48C6E) its wisp;
 * - $4C8 ($468BA) a piece wobbling round another; $440 ($497C2) a quake director (the ground's
 *   rows waving, riders let in);
 * - stage 5's space pieces: $478 ($4C6EE) a bouncing shot, $468 ($4C906) a homing missile,
 *   $504 ($4CB4A) a charge; stage 6's boss of seven forms again, $488 ($4D196), its shards $48C
 *   ($4E3D0) and mines $494 ($4E4F0).
 * Code of other files' routines these jump into (loc_ labels, inner labels) is copied here as
 * static helpers, as each comment says. Registers a routine reads as its caller left them are
 * reproduced (d0's high word in DiceBrawlerLeap, as Brawler4CCrouch's). */
#include "game.h"

#define SINES   0x02977Cu
#define COSINES 0x0297FCu

static int16_t sine(uint16_t i) { return (int16_t)rom16(SINES + (uint32_t)(int16_t)i); }
static int16_t cosine(uint16_t i) { return (int16_t)rom16(COSINES + (uint32_t)(int16_t)i); }
static void add16(uint16_t a, uint16_t v) { wr16(a, (uint16_t)(rd16(a) + v)); }
static void add32(uint16_t a, uint32_t v) { wr32(a, rd32(a) + v); }
static int neg16(uint16_t v) { return (v & 0x8000) != 0; }
static int neg32(uint32_t v) { return (v & 0x80000000u) != 0; }
/* cmpi.w #imm,(m) then bmi / bpl: the N flag of the subtraction */
static int below16(uint16_t a, uint16_t imm) { return neg16((uint16_t)(rd16(a) - imm)); }
static int below32(uint16_t a, uint32_t imm) { return neg32(rd32(a) - imm); }
/* the same on a value: cmp then bmi */
static int lt16(uint16_t v, uint16_t imm) { return neg16((uint16_t)(v - imm)); }
/* subq.w #1,(m) then bmi: the counter run out */
static int count_out(uint16_t a)
{
    wr16(a, (uint16_t)(rd16(a) - 1));
    return neg16(rd16(a));
}
/* a step of an object's table of routines (a word index into longs) */
static void dispatch(uint32_t table, uint16_t obj)
{
    call_object(rom32(table + (uint32_t)(int16_t)rd16(obj + O_STATE)), obj);
}
/* a word of a table indexed by the difficulty ($FFA20C) */
static uint16_t by_difficulty(uint32_t table)
{
    return rom16(table + (uint32_t)(int16_t)rd16(0xA20C));
}

/* The dice palace's rooms: the room ends (DiceRoomEnd, $8052 = 1) when the player the room is
 * for ($80D8: $A400 or $A600) has left the game while any is in */
static int room_player_gone(uint16_t obj)
{
    if (rd8(v_players)) {
        uint16_t p = rd16(0x80D8);
        if (p == 0xA400) {
            if (bit8(v_players, 0)) return 0;
        } else if (p == 0xA600) {
            if (bit8(v_players, 1)) return 0;
        }
        wr16(0x8052, 1);
        DiceRoomEnd(obj);
        return 1;
    }
    return 0;
}

/* ---- shared helpers ---- */

/* $02A27A. Sound $BE every 8 frames */
void SoundBEEvery8(void)
{
    if (rd16(v_gameFrames) & 7) return;
    QueueSound(0xBE);
}

/* ======================================================================================== */
/* ---- type $2C8: a gun turret (as GunPod's, $120 of vitality, not on the floor line until it
 * fires); shot: 4,600 points, a blast ($2F4), sound $BE ---- */

/* GunPod's helpers (objects40.c has them inside its routines, .GunPodDelay ...) */
static void turret_delay(uint16_t obj)
{
    wr16(obj + 0x48, by_difficulty(0x04063E));
}

static void turret_shoot(uint16_t obj)
{
    wr16(obj + O_Y, BelowCamera220(0x100));
    uint16_t p = PlayerTarget();
    int16_t dx = (int16_t)(rd16(p + O_X) - rd16(obj + O_X));
    uint32_t d1 = (uint16_t)(rd16(p + O_Y) - rd16(obj + O_Y));
    uint16_t a = (uint16_t)(((int16_t)Atan2(dx, &d1) >> 7) & 0x1FE);
    uint16_t slot, t;
    if (!SlotFreeMid(&slot, &t)) return;
    wr16(slot + O_TYPE, 0x2AC);
    wr16(slot + O_X, rd16(obj + O_X));
    wr16(slot + O_Y, rd16(obj + O_Y));
    wr32(slot + 0x1C, (uint32_t)(int32_t)sine(a) << 3);
    wr32(slot + 0x18, (uint32_t)(int32_t)cosine(a) << 3);
}

/* $04054A. Its set-up ($120 of vitality), its delay by the difficulty */
void Turret2C8Start(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0x8100);
    wr16(obj + 0x0E, 0x26C0);
    wr16(obj + 0x08, 0x0A00);
    wr16(obj + 0x0A, 0xF4F4);
    wr16(obj + O_VITALITY, 0x120);
    wr8(obj + 0x21, 0x20);
    wr32(obj + 0x28, 0xF010F010);
    wr8(obj + 0x20, 0xC0);
    turret_delay(obj);
}

/* $040586. Its delay counted down: a spark ($2AC) at its player, then the delay again; its
 * colours flashing ($26C0 / $26C9, loc_40A8E) */
void Turret2C8Fire(uint16_t obj)
{
    if (count_out(obj + 0x48)) {
        turret_shoot(obj);
        turret_delay(obj);
    }
    wr16(obj + 0x0E, rom16(0x040AA6 + (rd16(v_gameFrames) & 2)));
}

/* $04050E. Type $2C8: its vitality spent, 4,600 points, a blast ($2F4), sound $BE; else its step */
void Turret2C8(uint16_t obj)
{
    if (neg16(rd16(obj + O_VITALITY))) {
        AddScore(obj, 0x460);
        wr16(obj + O_TYPE, 0x2F4);
        wr16(obj + O_STATE, 0);
        wr8(obj + 0x21, 0);
        QueueSound(0xBE);
        return;
    }
    dispatch(0x040542, obj);
}

/* ======================================================================================== */
/* ---- type $14: the dice palace's serpent: a head (this object, at $C980) and 31 segments
 * behind it (type $10, $60 apart) following its trail of positions ($9000 x, $9100 y, $9200
 * frames, a word each, newest first; segment k takes entry 4k). It moves at its angle (+$56)
 * on a circle of sines ($9400 / $9480, SineTableStart's of radius 4): roaming, turning toward
 * a new angle at the edges (+$54), now and then rushing right, falling, rising, charging its
 * player. Its vitality takes the damage of every second segment's; spent: 230,800 points and
 * the segments flung apart; the room ends ---- */

/* $04299C. The head's frame (+$4C) and box (+$08) by its angle */
static void serpent_pose(uint16_t obj)
{
    uint16_t a = rd16(obj + 0x56);
    wr16(obj + 0x4C, rom16(0x0429C8 + (uint16_t)(((int16_t)a >> 4) & 0xE)));
    wr32(obj + 0x08, rom32(0x0429D8 + (uint16_t)(((int16_t)a >> 4) & 0x1C)));
}

/* $0426C6. While it plays (states 4 to $1C), the damage taken by segments 2, 4 ... 22 moved to
 * the head (theirs put back to $B04); the N flag of the head's vitality (0 in the other
 * states) */
static int serpent_damage(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) return 0;
    if (!below16(obj + O_STATE, 0x20)) return 0;
    uint16_t v = rd16(obj + O_VITALITY), a = obj;
    for (int k = 0; k <= 0xA; k++) {
        a = (uint16_t)(a + 0xC0);
        v = (uint16_t)(v - (uint16_t)(0xB04 - rd16(a + O_VITALITY)));
        wr16(a + O_VITALITY, 0xB04);
    }
    wr16(obj + O_VITALITY, v);
    return neg16(v);
}

/* $042778. The speed from the circle of sines at angle `a`: y from $9480, x from $9400, << 8 */
static void serpent_speed_at(uint16_t obj, uint16_t a)
{
    wr32(obj + 0x1C, (uint32_t)(int32_t)(int16_t)rd16((uint16_t)(0x9480 + a)) << 8);
    wr32(obj + 0x18, (uint32_t)(int32_t)(int16_t)rd16((uint16_t)(0x9400 + a)) << 8);
}

/* $042768. The same at its angle */
static void serpent_speed(uint16_t obj)
{
    serpent_speed_at(obj, rd16(obj + 0x56));
}

/* $042792. A new angle to turn to (+$54) every 8 frames, by chance */
static void serpent_wander(uint16_t obj)
{
    if (rd16(v_frameCount) & 7) return;
    wr16(obj + 0x54, rd16(v_randomSeed) & 0x1F0);
}

/* $0427AC. The trail: its x, y and frame put in front, the 124 entries before moved on one */
static void serpent_trail(uint16_t obj)
{
    wr16(0x9000, rd16(obj + O_X));
    wr16(0x9100, rd16(obj + O_Y));
    wr16(0x9200, rd16(obj + 0x4C));
    for (int k = 0x7B; k >= 0; k--) {
        wr16((uint16_t)(0x9002 + 2 * k), rd16((uint16_t)(0x9000 + 2 * k)));
        wr16((uint16_t)(0x9102 + 2 * k), rd16((uint16_t)(0x9100 + 2 * k)));
        wr16((uint16_t)(0x9202 + 2 * k), rd16((uint16_t)(0x9200 + 2 * k)));
    }
}

/* $0427F4. The 31 segments put on the trail: segment k at entry 4k */
static void serpent_place(void)
{
    uint16_t seg = 0xC980;
    for (uint16_t k = 1; k <= 0x1F; k++) {
        seg = (uint16_t)(seg + 0x60);
        wr16(seg + O_X, rd16((uint16_t)(0x9000 + 8 * k)));
        wr16(seg + O_Y, rd16((uint16_t)(0x9100 + 8 * k)));
        wr16(seg + 0x0E, rd16((uint16_t)(0x9200 + 8 * k)));
    }
}

/* $042832. At an edge it is heading for (left $A0, right $1A0, top $B0, bottom $E0 + the
 * difficulty's * 4): an angle away from it (+$54, a random part of a quarter), state $C */
static void serpent_edges(uint16_t obj)
{
    uint16_t bottom = (uint16_t)((uint16_t)(rd16(0xA20C) << 2) + 0xE0);
    uint16_t x = rd16(obj + O_X), y = rd16(obj + O_Y);
    uint16_t a = rd16(v_randomSeed) & 0x70;
    if (neg16(rd16(obj + 0x18))) {
        if (lt16(x, 0xA0)) { a = (uint16_t)(a + 0x40); goto turn; }
    } else {
        if (!lt16(x, 0x1A0)) { a = (uint16_t)(a + 0x140); goto turn; }
    }
    if (neg16(rd16(obj + 0x1C))) {
        if (!lt16(y, 0xB0)) return;
        a = (uint16_t)(a + 0x1C0);
    } else {
        if (lt16(y, bottom)) return;
        a = (uint16_t)(a + 0xC0);
    }
turn:
    wr16(obj + 0x54, a);
    wr16(obj + O_STATE, 0x0C);
}

/* $0428AE. In the middle of the room ($E0-$15F, $B8-$CF), once its count (+$4E, reloaded by
 * the difficulty) runs out: a rush right (state $10) or, by chance, a charge (state $18) after
 * 1-4 turns (+$50) */
static void serpent_centre(uint16_t obj)
{
    if (below16(obj + O_X, 0xE0) || !below16(obj + O_X, 0x160)) return;
    if (below16(obj + O_Y, 0xB8) || !below16(obj + O_Y, 0xD0)) return;
    if (!count_out(obj + 0x4E)) return;
    wr16(obj + 0x4E, by_difficulty(0x042916));
    if (!bit8(v_randomSeed + 1, 1)) {
        wr16(obj + O_STATE, 0x10);
        return;
    }
    wr16(obj + 0x50, (uint16_t)((rd16(v_randomSeed) & 3) + 1));
    wr16(obj + O_STATE, 0x18);
}

/* $042926. The angle (+$56) turned by $10 toward +$54 (the shorter way); Z when there */
static int serpent_steer(uint16_t obj)
{
    uint16_t a = rd16(obj + 0x56);
    uint16_t d = (uint16_t)((rd16(obj + 0x54) - a) & 0x1FE);
    if (!d) return 1;
    if (lt16(d, 0x100)) a = (uint16_t)(a + 0x10);
    else a = (uint16_t)(a - 0x10);
    wr16(obj + 0x56, a & 0x1F0);
    return 0;
}

/* $04295A. +$50 set when its angle is within $10 of the way to its player ($80D8) */
static void serpent_aim(uint16_t obj)
{
    uint16_t p = rd16(0x80D8);
    int16_t dx = (int16_t)(rd16(obj + O_X) - rd16(p + O_X));
    uint32_t d1 = (uint16_t)(rd16(p + O_Y) - rd16(obj + O_Y));
    uint16_t d = (uint16_t)((((int16_t)Atan2(dx, &d1) >> 7) + 0x180) & 0x1FE);
    d = (uint16_t)(d - rd16(obj + 0x56));
    if (neg16(d)) d ^= 0xFFFF;
    if (lt16(d, 0x10)) wr16(obj + 0x50, 1);
}

/* the end of most states: the speed at its angle, the trail, the segments placed */
static void serpent_move(uint16_t obj)
{
    serpent_speed(obj);
    serpent_trail(obj);
    serpent_place();
}

/* $0423EA. The trail cleared; once the sines are free ($80C2 clear): the head and its 31
 * segments set up (every second one of the first 27 with vitality), the circle of sines begun */
void DiceSerpentStart(uint16_t obj)
{
    for (uint16_t k = 0; k <= 0x7C; k++) {
        wr16((uint16_t)(0x9000 + 2 * k), 0);
        wr16((uint16_t)(0x9100 + 2 * k), 0);
        wr16((uint16_t)(0x9200 + 2 * k), 0);
    }
    if (rd16(0x80C2)) return;
    add16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0xCC00);
    wr32(obj + 0x08, 0x000E07F4);
    wr16(obj + O_VITALITY, 0xB04);
    wr16(obj + 0x26, 4);
    wr8(obj + 0x21, 0x30);
    wr32(obj + 0x2C, 0xF808F808);
    wr32(obj + 0x28, 0xF40CF40C);
    wr16(obj + O_X, 0x120);
    wr16(obj + O_Y, 0x80);
    wr16(obj + 0x4E, 0);
    wr16(obj + 0x0E, 0x300);
    wr8(obj + 0x20, 0);
    uint16_t seg = obj, odd = 0;
    for (int16_t n = 0x1E; n >= 0; n--) {
        seg = (uint16_t)(seg + 0x60);
        wr16(seg + O_Y, 0);
        wr16(seg + O_X, 0);
        wr16(seg + O_TYPE, 0x10);
        wr16(seg + O_FLAGS, 0x8C00);
        wr16(seg + 0x08, 0x0A00);
        wr16(seg + 0x0A, 0xF4F4);
        wr16(seg + 0x0E, 0x300);
        if (n < 4) continue;
        odd++;
        if (odd & 1) continue;
        wr16(seg + O_VITALITY, 0xB04);
        wr16(seg + 0x26, 4);
        wr8(seg + 0x21, 0x30);
        wr32(seg + 0x2C, 0xF808F808);
        wr32(seg + 0x28, 0xF40CF40C);
    }
    SineTableStart(0x9400, 4);
}

/* $0424D6. The circle of sines built; then the sines taken ($80C2 = $10, $80D6 = $C980) */
void DiceSerpentSines(uint16_t obj)
{
    if (SineTableBuild()) return;
    add16(obj + O_STATE, 4);
    wr16(0x80C2, 0x10);
    wr16(0x80D6, 0xC980);
}

/* $0424F2. Roaming */
void DiceSerpentRoam(uint16_t obj)
{
    serpent_centre(obj);
    serpent_edges(obj);
    serpent_wander(obj);
    serpent_steer(obj);
    serpent_move(obj);
}

/* $04250E. Turning away from an edge; there: roaming again */
void DiceSerpentTurn(uint16_t obj)
{
    if (serpent_steer(obj)) wr16(obj + O_STATE, 8);
    serpent_move(obj);
}

/* $042528. Rushing right, curling ($10 a frame), x + 1 a frame until $1B0 */
void DiceSerpentRush(uint16_t obj)
{
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 0x10));
    wr16(obj + 0x56, rd16(obj + 0x56) & 0x1F0);
    serpent_move(obj);
    add16(obj + O_X, 1);
    if (below16(obj + O_X, 0x1B0)) return;
    add16(obj + O_STATE, 4);
    wr16(obj + 0x56, 0);
}

/* $04255C. Curling ($8 a frame after the delay +$52) until straight; landing on y $140: the
 * screen shaken, sound $A6, turned up ($100), a delay by the difficulty; past x $80 leftward:
 * roaming again */
void DiceSerpentFall(uint16_t obj)
{
    if (rd16(obj + 0x56)) {
        uint16_t d = rd16(obj + 0x52);
        if (d) wr16(obj + 0x52, (uint16_t)(d - 1));
        else {
            add16(obj + 0x56, 8);
            wr16(obj + 0x56, rd16(obj + 0x56) & 0x1F8);
        }
    }
    if (!neg16(rd16(obj + 0x1C)) && !below16(obj + O_Y, 0x140)) {
        wr16(v_shakeSprites, 0x10);
        QueueSound(0xA6);
        wr16(obj + O_Y, 0x140);
        wr16(obj + 0x56, 0x100);
        wr16(obj + 0x52, rd16(0xA20C) ^ 0xE);
    }
    serpent_move(obj);
    if (below16(obj + O_X, 0x80)) wr16(obj + O_STATE, 8);
}

/* $0425DC. Rising (y - 1 every 4 frames), curling $10 a frame; each full turn counts +$50 down:
 * spent, state $1C */
void DiceSerpentRise(uint16_t obj)
{
    if (!(rd16(v_frameCount) & 3)) wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 1));
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 0x10));
    wr16(obj + 0x56, rd16(obj + 0x56) & 0x1F0);
    if (!rd16(obj + 0x56) && count_out(obj + 0x50)) {
        add16(obj + O_STATE, 4);
        wr16(obj + 0x50, 0);
    }
    serpent_move(obj);
}

/* $04261E. Curling until it faces its player (+$50 set), then charging; out of the room ($80-$1BF,
 * $60-$13F): the screen shaken, sound $A6, roaming again turned up */
void DiceSerpentCharge(uint16_t obj)
{
    if (below16(obj + O_X, 0x80) || !below16(obj + O_X, 0x1C0) || below16(obj + O_Y, 0x60)
        || !below16(obj + O_Y, 0x140)) {
        wr16(v_shakeSprites, 0x10);
        QueueSound(0xA6);
        wr16(obj + O_STATE, 8);
        wr16(obj + 0x56, 0x100);
    }
    if (!rd16(obj + 0x50)) {
        wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 0x10));
        wr16(obj + 0x56, rd16(obj + 0x56) & 0x1F0);
        serpent_aim(obj);
    }
    serpent_move(obj);
}

/* $042686. Broken: the frames still turned ($10 a frame) for +$48 frames, then the room ends */
void DiceSerpentBreak(uint16_t obj)
{
    if (count_out(obj + 0x48)) {
        DiceRoomEnd(obj);
        return;
    }
    add16(obj + 0x56, 0x10);
    wr16(obj + 0x56, rd16(obj + 0x56) & 0x1FE);
    serpent_trail(obj);
    uint16_t seg = 0xC980;
    for (uint16_t k = 1; k <= 0x1F; k++) {
        seg = (uint16_t)(seg + 0x60);
        wr16(seg + 0x0E, rd16((uint16_t)(0x9200 + 8 * k)));
    }
}

/* $042358. The room ended if its player left; the screen kept shaking a little; its frame by
 * its angle; its vitality spent (loc_42706): 230,800 points, sound $D2, state $20, the head and
 * segments flung apart (their speeds round the circle of sines, $20 apart), left without
 * collisions; the 68k's a5 is left at $C980 (the object loop goes on from there) */
void DiceSerpent(uint16_t obj)
{
    if (room_player_gone(obj)) return;
    if (below16(v_shakeSprites, 4)) wr16(v_shakeSprites, 2);
    serpent_pose(obj);
    if (!serpent_damage(obj)) {
        dispatch(0x0423C6, obj);
        return;
    }
    wr16(obj + O_VITALITY, 0xFFFF);
    AddScore(obj, 0x23080);
    QueueSound(0xD2);
    wr16(obj + 0x48, 0x60);
    wr16(obj + O_STATE, 0x20);
    uint16_t a = 0, seg = obj;
    for (int k = 0; k <= 0x1F; k++) {
        serpent_speed_at(seg, a);
        a = (uint16_t)((a + 0x20) & 0x1FE);
        wr16(seg + O_FLAGS, 0x8E00);
        wr8(seg + 0x21, 0);
        seg = (uint16_t)(seg + 0x60);
    }
    wr16(0xC980 + O_FLAGS, 0xCC00);
    reg_a5_left = 0xC980;
}

/* ======================================================================================== */
/* ---- type $264: the dice palace's cage: five pieces (this one, the top and bottom jaws at
 * $C9E0 / $CA40 opening and closing by +$48, the inner pair at $CAA0 / $CB00 by half of it)
 * sliding left and right; it lets out 2-6 balls (type $268, from $CB60, by the difficulty);
 * all gone: the pieces burst in turn (40,000 points each pair) and the room ends ---- */

/* $042F40. Object a0 made a puff ($100, its routine $563EE) at (x, y) with y speed `vy` */
static void cage_puff(uint16_t a0, uint16_t x, uint16_t y, uint32_t vy)
{
    wr16(a0 + O_TYPE, 0x100);
    wr32(a0 + 0x48, 0x0563EE);
    wr32(a0 + 0x1C, vy);
    wr16(a0 + O_STATE, 0);
    wr8(a0 + 0x21, 0);
    wr16(a0 + O_X, x);
    wr16(a0 + O_Y, y);
}

/* $042EFA. Object a0 burst: two puffs where it is (x speeds -1 and 1, a new slot's the second),
 * the screen shaken, 40,000 points, sound $C5 */
static void cage_burst(uint16_t obj, uint16_t a0, uint32_t vy)
{
    wr16(v_shakeSprites, 0x10);
    uint16_t x = rd16(a0 + O_X), y = rd16(a0 + O_Y), t;
    cage_puff(a0, x, y, vy);
    wr32(a0 + 0x18, 0xFFFF0000);
    if (SlotFreeHigh(&a0, &t)) {
        cage_puff(a0, x, y, vy);
        wr32(a0 + 0x18, 0x10000);
    }
    AddScore(obj, 0x4000);
    QueueSound(0xC5);
}

/* $042AD6. The balls let out (+$56 their count less one, by the difficulty) from $CB60 */
static void cage_balls(uint16_t obj)
{
    uint16_t a0 = 0xCB60, n = rom16(0x042B16 + (uint32_t)(int16_t)rd16(v_difficulty));
    wr16(obj + 0x56, n);
    for (uint16_t k = 0; ; k++) {
        wr16(a0 + O_TYPE, 0x268);
        wr16(a0 + O_STATE, 0);
        wr16(a0 + O_X, rom16(0x042B1E + 2 * k));
        wr16(a0 + O_Y, 0x80);
        a0 = (uint16_t)(a0 + 0x60);
        if (n-- == 0) break;
    }
}

/* $042D0E. The four jaws placed on it: the outer +$48 above and below (with its speed), the
 * inner half of that */
static void cage_jaws(uint16_t obj)
{
    uint16_t x = rd16(obj + O_X), y = rd16(obj + O_Y), open = rd16(obj + 0x48);
    wr16(0xC9F4, (uint16_t)(y - open));
    wr16(0xC9F0, x);
    wr32(0xC9F8, rd32(obj + 0x18));
    wr16(0xCA54, (uint16_t)(y + open));
    wr16(0xCA50, x);
    wr32(0xCA58, rd32(obj + 0x18));
    uint16_t half = (uint16_t)((int16_t)open >> 1);
    wr16(0xCAB4, (uint16_t)(y - half));
    wr16(0xCAB0, x);
    wr16(0xCB14, (uint16_t)(y + half));
    wr16(0xCB10, x);
}

/* $042D66. The jaws snapping open (8 a frame to $40) and closing slowly (1 a frame to $A) */
static void cage_snap(uint16_t obj)
{
    if (rd16(obj + 0x4A)) {
        add16(obj + 0x48, 8);
        if (below16(obj + 0x48, 0x3F)) return;
        wr16(obj + 0x4A, 0);
        wr16(obj + 0x48, 0x40);
        return;
    }
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    if (!below16(obj + 0x48, 0xA)) return;
    wr16(obj + 0x4A, 2);
    wr16(obj + 0x48, 0xA);
}

/* $042DA6. Sliding right to x $170, left to $D0 */
static void cage_slide(uint16_t obj)
{
    if (!rd16(obj + 0x4C)) {
        wr32(obj + 0x18, 0x8000);
        if (below16(obj + O_X, 0x170)) return;
        wr16(obj + 0x4C, 2);
        return;
    }
    wr32(obj + 0x18, 0xFFFF8000);
    if (!below16(obj + O_X, 0xD0)) return;
    wr16(obj + 0x4C, 0);
}

/* $042A5C. The five pieces set up (type $10 the four jaws), its jaws' colours */
void DiceCageStart(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    uint16_t a0 = obj;
    for (int k = 0; k <= 4; k++) {
        wr16(a0 + O_TYPE, 0x10);
        wr16(a0 + O_FLAGS, 0x8000);
        wr16(a0 + 0x0E, 0x2330);
        wr8(a0 + 0x21, 0x24);
        wr16(a0 + 0x08, 0x0F00);
        wr16(a0 + 0x0A, 0xF0F0);
        wr8(a0 + 0x46, 0x8F);
        wr32(a0 + 0x2C, 0xF010F010);
        wr32(a0 + 0x28, 0xF20EF20E);
        a0 = (uint16_t)(a0 + 0x60);
    }
    wr16(obj + O_TYPE, 0x264);
    wr16(obj + O_FLAGS, 0x8C00);
    wr8(obj + 0x20, 4);
    wr16(obj + O_X, 0x120);
    wr16(obj + O_Y, 0x100);
    wr16(obj + 0x48, 0x3F);
    wr16(0xC9EE, 0x2320);
    wr16(0xCA4E, 0x3320);
}

/* $042B2A. Waiting for the room's sines to be free ($80C2 clear): the balls let out */
void DiceCageWait(uint16_t obj)
{
    if (!rd16(0x80C2)) {
        add16(obj + O_STATE, 4);
        cage_balls(obj);
    }
    cage_jaws(obj);
}

/* $042B40. Sliding and snapping; once no ball is left (loc_42DE2): next state, its pieces
 * without collisions */
void DiceCagePlay(uint16_t obj)
{
    cage_slide(obj);
    cage_snap(obj);
    cage_jaws(obj);
    uint16_t a0 = 0xCB60, n = rd16(obj + 0x56);
    for (;;) {
        if (rd16(a0 + O_TYPE) == 0x268) return;
        a0 = (uint16_t)(a0 + 0x60);
        if (n-- == 0) break;
    }
    add16(obj + O_STATE, 4);
    wr16(obj + 0x48, 0x20);
    wr16(obj + 0x4A, 0);
    wr32(obj + 0x18, 0);
    a0 = obj;
    for (int k = 0; k <= 4; k++) {
        wr8(a0 + 0x21, 0);
        a0 = (uint16_t)(a0 + 0x60);
    }
}

/* $042B50. The outer jaws burst */
void DiceCageBurstOuter(uint16_t obj)
{
    if (!count_out(obj + 0x48)) return;
    add16(obj + O_STATE, 4);
    wr16(obj + 0x48, 0x10);
    cage_burst(obj, 0xC9E0, 0xFFFE8000);
    cage_burst(obj, 0xCA40, 0x18000);
}

/* $042B80. The inner jaws burst */
void DiceCageBurstInner(uint16_t obj)
{
    if (!count_out(obj + 0x48)) return;
    add16(obj + O_STATE, 4);
    wr16(obj + 0x48, 0x10);
    cage_burst(obj, 0xCAA0, 0xFFFF4000);
    cage_burst(obj, 0xCB00, 0xC000);
}

/* $042BB0. Its own burst (from a new slot where it is); it hidden */
void DiceCageBurst(uint16_t obj)
{
    if (!count_out(obj + 0x48)) return;
    add16(obj + O_STATE, 4);
    wr16(obj + 0x48, 0x40);
    uint16_t a0, t;
    if (SlotFreeHigh(&a0, &t)) {
        wr16(a0 + O_X, rd16(obj + O_X));
        wr16(a0 + O_Y, rd16(obj + O_Y));
        cage_burst(obj, a0, 0);
    }
    wr16(obj + O_FLAGS, 0);
    wr8(obj + 0x21, 0);
}

/* $042BEC. After +$48 frames the room ends */
void DiceCageEnd(uint16_t obj)
{
    if (count_out(obj + 0x48)) DiceRoomEnd(obj);
}

/* $0429F8. Type $264: the room ended if its player left; else its step */
void DiceCage(uint16_t obj)
{
    if (room_player_gone(obj)) return;
    dispatch(0x042A40, obj);
}

/* ---- type $268: the cage's ball, bouncing in the room ($90-$1B0, above $140, off the walls
 * the touch bits of +$47 report); struck: lit for 6 frames ($310), the sines taken ($80C2 =
 * $20, $80D6 it); shot: 9,590 points, a burst ($8C) ---- */

/* $042E28. Bounced: up off the ceiling (or y $A8), down off the floor ($140), sideways off the
 * walls ($90, $1B0) */
static void ball_bounce(uint16_t obj)
{
    uint8_t touch = rd8(obj + 0x47);
    if (bit8(obj + 0x1C, 7)) {
        if ((touch & 0x01) || (touch & 0x10) || below16(obj + O_Y, 0xA8)) wr16(obj + 0x1C, 1);
    } else if ((touch & 0x02) || (touch & 0x20)) {
        wr16(obj + 0x1C, 0xFFFF);
    } else if (!below16(obj + O_Y, 0x140)) {
        wr16(obj + O_Y, 0x140);
        wr16(obj + 0x1C, 0xFFFF);
    }
    if (bit8(obj + 0x18, 7)) {
        if ((touch & 0x04) || (touch & 0x40)) { wr16(obj + 0x18, 1); return; }
        if (!below16(obj + O_X, 0x90)) return;
        wr16(obj + O_X, 0x90);
        wr16(obj + 0x18, 1);
        return;
    }
    if ((touch & 0x08) || (touch & 0x80)) { wr16(obj + 0x18, 0xFFFF); return; }
    if (below16(obj + O_X, 0x1B0)) return;
    wr16(obj + O_X, 0x1B0);
    wr16(obj + 0x18, 0xFFFF);
}

/* $042C98. Its set-up: a random way across, up */
void CageBallStart(uint16_t obj)
{
    Random();
    add16(obj + O_STATE, 4);
    wr8(obj + 0x20, 8);
    wr16(obj + O_FLAGS, 0x8C00);
    wr16(obj + 0x0E, 0x300);
    wr8(obj + 0x21, 0x2C);
    wr8(obj + 0x46, 0x8F);
    wr16(obj + 0x08, 0x0F00);
    wr16(obj + 0x0A, 0xF0F0);
    wr32(obj + 0x2C, 0xF40CF40C);
    wr32(obj + 0x28, 0xF20EF20E);
    wr16(obj + 0x18, rom16(0x042CFE + (rd16(v_randomSeed) & 2)));
    wr16(obj + 0x1C, 0xFFFF);
    wr16(obj + O_VITALITY, 0x17F);
}

/* $042D02. Bouncing, its touch bits cleared */
void CageBallBounce(uint16_t obj)
{
    ball_bounce(obj);
    wr8(obj + 0x47, 0);
}

/* $042BFA. Type $268: its vitality spent, the screen shaken, a burst ($8C), 9,590 points; else
 * lit while struck, its step */
void CageBall(uint16_t obj)
{
    if (neg16(rd16(obj + O_VITALITY))) {
        wr16(v_shakeSprites, 0x10);
        wr16(obj + O_TYPE, 0x8C);
        wr16(obj + O_STATE, 0);
        wr8(obj + 0x21, 0);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr16(obj + 0x48, 0x60);
        wr16(obj + 0x4A, 4);
        wr16(obj + 0x4C, 8);
        AddScore(obj, 0x2590);
        return;
    }
    uint16_t lit = rd16(obj + 0x48);
    if (lit && !neg16(lit)) {
        wr16(obj + 0x48, (uint16_t)(lit - 1));
    } else if (bit8(obj + 0x22, 5)) {
        wr16(0x80C2, 0x20);
        wr16(0x80D6, obj);
        wr16(obj + 0x0E, 0x310);
        wr16(obj + 0x48, 6);
    } else {
        wr16(obj + 0x0E, 0x300);
    }
    dispatch(0x042C90, obj);
}

/* ======================================================================================== */
/* ---- type $26C: the dice palace's hopping giant: a group of 15 (this one at $C980; the body
 * $C9E0 with the group's vitality, $169 at the start; the head $CA40; two arms of three
 * segments from $CAA0 / $CB60 ... and two legs from $CCE0 / $CE00), each member at an angle
 * (+$56, set by a pose list of 14 bytes) and a length from the one before, the four joints'
 * bends in $CA30, $CA90, $CAF0, $CB50, mirrored while +$54 bit 0 is set (facing right). It
 * hops, stamps and swings at its player ($80D8, whose +$72 it keeps in +$5E meanwhile);
 * struck on an arm or leg it is knocked away; its vitality spent: it shatters into puffs, falls
 * off and the room ends ---- */

/* $0439E0. The joints' bends cleared, the 14 members' angles (+$56) from a pose list */
static void giant_pose(uint32_t list)
{
    wr16(0xCA30, 0);
    wr16(0xCA90, 0);
    wr16(0xCAF0, 0);
    wr16(0xCB50, 0);
    uint16_t a0 = 0xC9E0;
    for (int k = 0; k <= 0xD; k++) {
        wr16(a0 + 0x56, (uint16_t)((rom8(list++) << 1) & 0x1FE));
        a0 = (uint16_t)(a0 + 0x60);
    }
}

/* $0439AC. Member a0's offset (+$48 x, +$4C y, longs) from a1's: the sine and cosine of
 * `angle` (a half-turn more for the cosine when mirrored) times `len` */
static void giant_joint(uint16_t obj, uint16_t a0, uint16_t a1, uint16_t angle, uint16_t len)
{
    angle &= 0x1FE;
    int16_t s = sine(angle);
    if (bit8(obj + 0x54, 0)) angle = (uint16_t)((angle + 0x100) & 0x1FE);
    int16_t c = cosine(angle);
    wr32(a0 + 0x48, rd32(a1 + 0x48) + (uint32_t)((int32_t)c * (int16_t)len));
    wr32(a0 + 0x4C, rd32(a1 + 0x4C) + (uint32_t)((int32_t)s * (int16_t)len));
}

/* $043942. The same from the group's centre (no offset before it) */
static void giant_limb_from(uint16_t obj, uint16_t a0, uint16_t len)
{
    uint16_t angle = (uint16_t)((rd16(obj + 0x56) + rd16(a0 + 0x56)) & 0x1FE);
    int16_t s = sine(angle);
    if (bit8(obj + 0x54, 0)) angle = (uint16_t)((angle + 0x100) & 0x1FE);
    int16_t c = cosine(angle);
    wr32(a0 + 0x48, (uint32_t)((int32_t)c * (int16_t)len));
    wr32(a0 + 0x4C, (uint32_t)((int32_t)s * (int16_t)len));
}

/* $043978, $043986, $043994, $0439A2: member a0 bent by a joint's angle ($CA90, $CA30, $CB50,
 * $CAF0) after the group's and its own */
static void giant_bent(uint16_t obj, uint16_t a0, uint16_t a1, uint16_t bend, uint16_t len)
{
    giant_joint(obj, a0, a1, (uint16_t)(rd16(obj + 0x56) + rd16(a0 + 0x56) + rd16(bend)), len);
}

/* $04380E. The members' offsets from the group's angle, then each placed from the one in +$50
 * (GroupPlace, 15 of them) */
static void giant_limbs(uint16_t obj)
{
    wr32(obj + 0x48, 0);
    wr32(obj + 0x4C, 0);
    uint16_t turn = rd16(obj + 0x56);
    giant_limb_from(obj, 0xC9E0, 0x48);
    giant_limb_from(obj, 0xCA40, 0x60);
    giant_joint(obj, 0xCAA0, obj, (uint16_t)(turn + rd16(0xCAA0 + 0x56)), 0x50);
    giant_bent(obj, 0xCB00, 0xCAA0, 0xCA30, 0x30);
    giant_bent(obj, 0xCB60, 0xCB00, 0xCA30, 0x30);
    giant_joint(obj, 0xCBC0, obj, (uint16_t)(turn + rd16(0xCBC0 + 0x56)), 0x50);
    giant_bent(obj, 0xCC20, 0xCBC0, 0xCA90, 0x30);
    giant_bent(obj, 0xCC80, 0xCC20, 0xCA90, 0x30);
    giant_bent(obj, 0xCCE0, 0xCA40, 0xCAF0, 0x50);
    giant_bent(obj, 0xCD40, 0xCCE0, 0xCAF0, 0x30);
    giant_bent(obj, 0xCDA0, 0xCD40, 0xCAF0, 0x30);
    giant_bent(obj, 0xCE00, 0xCA40, 0xCB50, 0x50);
    giant_bent(obj, 0xCE60, 0xCE00, 0xCB50, 0x30);
    giant_bent(obj, 0xCEC0, 0xCE60, 0xCB50, 0x30);
    GroupPlace(obj, 0xE);
}

/* $0436F6. The group set up: three plain members (this one, the body, the head), twelve
 * segments with collisions, the hands' and feet's frames */
static void giant_setup(uint16_t obj)
{
    uint16_t a0 = obj;
    for (int k = 0; k <= 2; k++) {
        wr16(a0 + O_TYPE, 0x10);
        wr16(a0 + O_FLAGS, 0x8000);
        wr16(a0 + 0x0E, 0x315);
        wr16(a0 + 0x08, 0x0F00);
        wr16(a0 + 0x0A, 0xF0F0);
        wr8(a0 + 0x20, 4);
        a0 = (uint16_t)(a0 + 0x60);
    }
    wr16(obj + O_TYPE, 0x26C);
    wr16(0xC9EE, 0x300);
    wr16(0xC9E8, 0x0E00);
    wr16(0xC9EA, 0xF0F4);
    wr16(obj + O_FLAGS, 0x8C00);
    wr8(obj + 0x20, 8);
    wr8(0xCA60, 0x0C);
    for (int k = 0; k <= 0xB; k++) {
        wr16(a0 + O_TYPE, 0x10);
        wr16(a0 + O_FLAGS, 0x8000);
        wr16(a0 + 0x0E, 0x325);
        wr16(a0 + 0x08, 0x0500);
        wr16(a0 + 0x0A, 0xF8F8);
        wr32(a0 + 0x2C, 0xF010F010);
        wr8(a0 + 0x23, 0x20);
        wr16(a0 + 0x26, 8);
        a0 = (uint16_t)(a0 + 0x60);
    }
    wr8(0xCAC0, 0x14);
    wr8(0xCB20, 0x14);
    wr8(0xCB80, 0x14);
    wr8(0xCD00, 0x14);
    wr8(0xCD60, 0x14);
    wr8(0xCDC0, 0x14);
    wr16(0xCB6E, 0x329);
    wr16(0xCB68, 0x0800);
    wr16(0xCB6A, 0xF4FC);
    wr16(0xCC8E, 0xB29);
    wr16(0xCC88, 0x0800);
    wr16(0xCC8A, 0xF4FC);
    wr16(0xCDAE, 0x329);
    wr16(0xCDA8, 0x0800);
    wr16(0xCDAA, 0xF4FC);
    wr16(0xCECE, 0xB29);
    wr16(0xCEC8, 0x0800);
    wr16(0xCECA, 0xF4FC);
    wr32(obj + 0x28, 0xE030FE02);
}

/* $0437F8. Its own collision box (for the stomp) */
static void giant_stomp_box(uint16_t obj)
{
    wr16(obj + 0x26, 0x14);
    wr8(obj + 0x21, 0x10);
    wr32(obj + 0x2C, 0xF024F010);
}

/* $043BE0 (0) and $043BE6 ($10): six members from a0 given that collision kind (+$21) */
static void giant_collide(uint16_t a0, uint8_t kind)
{
    for (int k = 0; k <= 5; k++) {
        wr8(a0 + 0x21, kind);
        a0 = (uint16_t)(a0 + 0x60);
    }
}

/* $043B42. Stopped at the room's sides ($90, $1B0) */
static void giant_walls(uint16_t obj)
{
    if (bit8(obj + 0x18, 7)) {
        if (!below16(obj + O_X, 0x90)) return;
        wr32(obj + 0x18, 0);
        wr16(obj + O_X, 0x90);
        return;
    }
    if (below16(obj + O_X, 0x1B0)) return;
    wr32(obj + 0x18, 0);
    wr16(obj + O_X, 0x1B0);
}

/* $043BFC. The screen shaken, the body lowered, the 14 members made puffs ($100, $563EE) */
static void giant_shatter(void)
{
    wr16(v_shakeSprites, 0x10);
    add16(0xC9F4, 0x18);
    uint16_t a0 = 0xC9E0;
    for (int k = 0; k <= 0xD; k++) {
        wr16(a0 + O_TYPE, 0x100);
        wr16(a0 + O_STATE, 0);
        wr32(a0 + 0x48, 0x0563EE);
        a0 = (uint16_t)(a0 + 0x60);
    }
}

/* $043C2C. Its frame and box flashing (every 4 frames, flipped every 8) */
static void giant_flash(uint16_t obj)
{
    uint16_t f = (uint16_t)((int16_t)rd16(v_gameFrames) >> 2);
    uint16_t i = f & 2;
    wr16(obj + 0x0E, rom16(0x043C6E + i));
    if (f & 4) wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
    wr16(obj + 0x08, rom16(0x043C72 + i));
    wr16(obj + 0x0A, rom16(0x043C76 + i));
}

/* $043A6A. Its player ($80D8) behind it: turning (state $14 / $18); within $60 in front, by
 * chance (the difficulty's table): a hop (state $1C) or, the player above the head, a crouch
 * ($40) */
static void giant_choose(uint16_t obj)
{
    uint16_t p = rd16(0x80D8);
    uint16_t d = (uint16_t)(rd16(p + O_X) - rd16(obj + O_X));
    if (!bit8(obj + 0x54, 0)) {
        if (!neg16(d)) { wr16(obj + O_STATE, 0x14); return; }
        if (lt16(d, 0xFFA0)) return;
    } else {
        if (neg16(d)) { wr16(obj + O_STATE, 0x18); return; }
        if (!lt16(d, 0x60)) return;
    }
    Random();
    uint16_t r = (uint16_t)((rd16(v_randomSeed) + rd16(v_gameFrames)) & 0xFF);
    uint16_t chance = by_difficulty(0x043B0A);
    if (neg16((uint16_t)(chance - r))) return;
    p = rd16(0x80D8);
    if (!lt16(rd16(p + O_Y), rd16(0xCA54))) wr16(obj + O_STATE, 0x1C);
    else wr16(obj + O_STATE, 0x40);
}

/* $043B1A. Struck on a limb ($22 bit 1): arms and legs without collisions, state $2C */
static void giant_struck(uint16_t obj)
{
    giant_collide(0xCAA0, 0);
    giant_collide(0xCCE0, 0);
    wr16(obj + O_STATE, 0x2C);
    wr16(obj + 0x50, obj);
    giant_pose(0x043A16);
    giant_limbs(obj);
}

/* $043B80. Shot ($22 bit 5): $28 off the body's vitality, knocked away up and back (state $3C) */
static void giant_knocked(uint16_t obj)
{
    wr16(0xCA04, (uint16_t)(rd16(0xCA04) - 0x28));
    giant_collide(0xCAA0, 0);
    giant_collide(0xCCE0, 0);
    wr8(obj + 0x21, 0);
    wr32(obj + 0x1C, 0xFFFE0000);
    if (!bit8(obj + 0x54, 0)) wr32(obj + 0x18, 0x40000);
    else wr32(obj + 0x18, 0xFFFC0000);
    wr16(obj + O_STATE, 0x3C);
    wr16(obj + 0x50, obj);
    wr16(obj + 0x56, 0);
    giant_pose(0x043A40);
    giant_limbs(obj);
}

/* $043104. Its vitality ($169 on the body), the sines taken by the body ($80D6) */
void DiceGiantReady(uint16_t obj)
{
    wr16(obj + 0x52, 0);
    wr16(0xCA04, 0x169);
    wr16(0x80D6, 0xC9E0);
    wr8(obj + 0x21, 0x22);
}

/* $0430CC. Its player's +$72 kept (+$5E) and cleared; at x $170, y $118, the group set up and
 * placed (state $54) */
void DiceGiantStart(uint16_t obj)
{
    uint16_t p = rd16(0x80D8);
    wr16(obj + 0x5E, rd16(p + 0x72));
    wr16(p + 0x72, 0);
    wr16(obj + O_STATE, 0x54);
    wr16(obj + O_X, 0x170);
    wr16(obj + O_Y, 0x118);
    giant_setup(obj);
    wr16(obj + 0x50, obj);
    giant_pose(0x043A16);
    giant_limbs(obj);
    DiceGiantReady(obj);
}

/* $04311E. Standing for +$52 frames, watching its player; then winding its limbs up from the
 * left foot ($CDA0) */
void DiceGiantStand(uint16_t obj)
{
    if (count_out(obj + 0x52)) {
        add16(obj + O_STATE, 4);
        wr16(obj + 0x50, 0xCDA0);
        giant_pose(0x043A16);
        giant_limbs(obj);
        return;
    }
    giant_choose(obj);
    giant_limbs(obj);
}

/* $043146. Its joints bent one way until $CB50 is $30, then from the right foot ($CEC0) */
void DiceGiantStepA(uint16_t obj)
{
    wr16(0xCA90, (uint16_t)((rd16(0xCA90) - 8) & 0x1FE));
    wr16(0xCA30, (uint16_t)((rd16(0xCA30) + 8) & 0x1FE));
    wr16(0xCB50, (uint16_t)((rd16(0xCB50) + 4) & 0x1FC));
    wr16(0xCAF0, (uint16_t)((rd16(0xCAF0) - 4) & 0x1FC));
    if (rd16(0xCB50) == 0x30) {
        add16(obj + O_STATE, 4);
        wr16(obj + 0x50, 0xCEC0);
    }
    giant_limbs(obj);
}

/* $04318E. Bent back until $CB50 is 0: standing again (8 frames) */
void DiceGiantStepB(uint16_t obj)
{
    wr16(0xCA90, (uint16_t)((rd16(0xCA90) + 8) & 0x1FE));
    wr16(0xCA30, (uint16_t)((rd16(0xCA30) - 8) & 0x1FE));
    wr16(0xCB50, (uint16_t)((rd16(0xCB50) - 4) & 0x1FC));
    wr16(0xCAF0, (uint16_t)((rd16(0xCAF0) + 4) & 0x1FC));
    if (!rd16(0xCB50)) {
        wr16(obj + O_STATE, 8);
        wr16(obj + 0x52, 8);
    }
    giant_limbs(obj);
}

/* loc_431E8: standing again (8 frames), the stand's pose */
static void giant_turned(uint16_t obj)
{
    wr16(obj + O_STATE, 8);
    wr16(obj + 0x52, 8);
    wr16(obj + 0x50, obj);
    giant_pose(0x043A16);
    giant_limbs(obj);
}

/* $0431D8. Turned to face left */
void DiceGiantFaceLeft(uint16_t obj)
{
    clr8(obj + 0x54, 0);
    giant_turned(obj);
}

/* $0431E2. Turned to face right */
void DiceGiantFaceRight(uint16_t obj)
{
    set8(obj + 0x54, 0);
    giant_turned(obj);
}

/* $043206. A hop forward ($4000 the way it faces, up $2.8), the left leg's collisions on */
void DiceGiantHop(uint16_t obj)
{
    if (!bit8(obj + 0x54, 0)) wr32(obj + 0x18, 0xFFFFC000);
    else wr32(obj + 0x18, 0x4000);
    add16(obj + O_STATE, 4);
    wr32(obj + 0x1C, 0xFFFD8000);
    wr16(obj + 0x50, obj);
    giant_pose(0x043A24);
    giant_collide(0xCCE0, 0x10);
    giant_limbs(obj);
}

/* $04324A. A higher hop (up $4) */
void DiceGiantHighHop(uint16_t obj)
{
    DiceGiantHop(obj);
    wr32(obj + 0x1C, 0xFFFC0000);
}

/* $043264. Rising (gravity $4000), its joints swinging until $CB50 is $D0 */
void DiceGiantRise(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    wr16(0xCA90, (uint16_t)((rd16(0xCA90) + 6) & 0x1FE));
    wr16(0xCA30, (uint16_t)((rd16(0xCA30) - 8) & 0x1F8));
    wr16(0xCB50, (uint16_t)((rd16(0xCB50) + 0x10) & 0x1F0));
    wr16(0xCAF0, (uint16_t)((rd16(0xCAF0) - 0x10) & 0x1F0));
    if (rd16(0xCB50) == 0xD0) add16(obj + O_STATE, 4);
    giant_limbs(obj);
}

/* $043258. The same, the group turning ($10 a frame) */
void DiceGiantSpinRise(uint16_t obj)
{
    add16(obj + 0x56, 0x10);
    wr16(obj + 0x56, rd16(obj + 0x56) & 0x1FE);
    DiceGiantRise(obj);
}

/* $0432B6. Falling; landing on y $11E: sound $A0, the screen shaken, at y $12C for $20 frames,
 * the legs' collisions off, the landing's pose (state $28) */
void DiceGiantFall(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if (!neg32(rd32(obj + 0x1C)) && !below16(obj + O_Y, 0x11E)) {
        QueueSound(0xA0);
        wr16(obj + O_STATE, 0x28);
        wr16(obj + 0x56, 0);
        wr16(v_shakeSprites, 0x10);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr16(obj + O_Y, 0x12C);
        wr16(obj + 0x52, 0x20);
        giant_collide(0xCCE0, 0);
        giant_collide(0xCE60, 0);
        giant_pose(0x043A32);
    }
    giant_limbs(obj);
}

/* $0432AA. The same, the group turning ($10 a frame) */
void DiceGiantSpinFall(uint16_t obj)
{
    add16(obj + 0x56, 0x10);
    wr16(obj + 0x56, rd16(obj + 0x56) & 0x1FE);
    DiceGiantFall(obj);
}

/* $043322. Landed: the arms swung to $180 / $80; after +$52 frames: up at y $118, then by
 * chance a stomp (state $64, its left leg and its own box hurting) or standing again */
void DiceGiantLanded(uint16_t obj)
{
    if (rd16(0xCA90) != 0x180) {
        wr16(0xCA90, (uint16_t)((rd16(0xCA90) - 8) & 0x1FC));
        wr16(0xCA30, (uint16_t)((rd16(0xCA30) + 8) & 0x1FC));
    }
    if (count_out(obj + 0x52)) {
        wr8(obj + 0x21, 0x22);
        wr16(obj + O_Y, 0x118);
        if (!(rd16(v_randomSeed) & 3)) {
            wr16(obj + O_STATE, 0x64);
            giant_collide(0xCCE0, 0x10);
            giant_stomp_box(obj);
            giant_limbs(obj);
            return;
        }
        wr16(obj + O_STATE, 8);
        wr16(obj + 0x52, 8);
        giant_pose(0x043A16);
    }
    giant_limbs(obj);
}

/* $043390. Struck: once +$23 bit 1 is clear (cleared now), up $4 without collisions */
void DiceGiantStruck(uint16_t obj)
{
    int was = bit8(obj + 0x23, 1);
    clr8(obj + 0x23, 1);
    if (!was) {
        add16(obj + O_STATE, 4);
        wr8(obj + 0x21, 0);
        wr32(obj + 0x1C, 0xFFFC0000);
    }
    giant_limbs(obj);
}

/* $0433B0. Tumbling (gravity $2000, turning $10 a frame); landing on y $120: sound $A0, at
 * $128, the screen shaken, bounced up $2, $64 off the body's vitality */
void DiceGiantTumble(uint16_t obj)
{
    giant_walls(obj);
    add32(obj + 0x1C, 0x2000);
    if (!neg32(rd32(obj + 0x1C)) && !below16(obj + O_Y, 0x120)) {
        QueueSound(0xA0);
        add16(obj + O_STATE, 4);
        wr16(obj + O_Y, 0x128);
        wr16(v_shakeSprites, 0x10);
        wr32(obj + 0x1C, 0xFFFE0000);
        wr16(0xCA04, (uint16_t)(rd16(0xCA04) - 0x64));
    }
    add16(obj + 0x56, 0x10);
    wr16(obj + 0x56, rd16(obj + 0x56) & 0x1FE);
    giant_limbs(obj);
}

/* $043402. Bouncing (gravity $3400, turning 8 a frame); landing on y $120: sound $A0, still at
 * $128 for $40 frames */
void DiceGiantBounce(uint16_t obj)
{
    giant_walls(obj);
    add32(obj + 0x1C, 0x3400);
    if (!neg32(rd32(obj + 0x1C)) && !below16(obj + O_Y, 0x120)) {
        QueueSound(0xA0);
        add16(obj + O_STATE, 4);
        wr16(obj + 0x52, 0x40);
        wr16(obj + O_Y, 0x128);
        wr16(v_shakeSprites, 8);
        wr32(obj + 0x1C, 0);
        wr32(obj + 0x18, 0);
    }
    add16(obj + 0x56, 8);
    wr16(obj + 0x56, rd16(obj + 0x56) & 0x1FE);
    giant_limbs(obj);
}

/* $04347A. Lying for +$52 frames; then up $2.8 in the landing's pose (state $24) */
void DiceGiantLie(uint16_t obj)
{
    if (count_out(obj + 0x52)) {
        wr16(obj + O_STATE, 0x24);
        giant_pose(0x043A32);
        wr16(obj + 0x56, 0);
        wr32(obj + 0x1C, 0xFFFD8000);
    }
    giant_limbs(obj);
}

/* $04345C. The same, the legs bending and the group turning */
void DiceGiantRoll(uint16_t obj)
{
    wr16(0xCB50, (uint16_t)((rd16(0xCB50) - 4) & 0x1FE));
    wr16(0xCAF0, (uint16_t)((rd16(0xCAF0) + 2) & 0x1FE));
    add16(obj + 0x56, 8);
    wr16(obj + 0x56, rd16(obj + 0x56) & 0x1FE);
    DiceGiantLie(obj);
}

/* $0434A4. The stomp: flipping, falling (gravity $3400) to y $138: the screen shaken, sound
 * $A0, rolling (state $60) */
void DiceGiantStomp(uint16_t obj)
{
    wr16(0xC9EE, rd16(0xC9EE) ^ 0x800);
    giant_walls(obj);
    add32(obj + 0x1C, 0x3400);
    if (!neg32(rd32(obj + 0x1C)) && !below16(obj + O_Y, 0x138)) {
        wr16(obj + O_STATE, 0x60);
        wr16(obj + 0x52, 0x20);
        wr16(obj + O_Y, 0x138);
        wr16(v_shakeSprites, 0x16);
        wr32(obj + 0x1C, 0);
        wr32(obj + 0x18, 0);
        QueueSound(0xA0);
    }
    giant_limbs(obj);
}

/* $0434FA. A crouch for $40 frames, the arms' collisions on */
void DiceGiantCrouch(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    wr16(obj + 0x50, obj);
    wr16(obj + 0x52, 0x40);
    wr16(obj + O_Y, 0x118);
    giant_pose(0x043A4E);
    giant_collide(0xCAA0, 0x10);
    giant_limbs(obj);
}

/* $043524. Its arms flailing (a table by the frame); then the arms' collisions off, up $2.8
 * in the landing's pose (state $24) */
void DiceGiantFlail(uint16_t obj)
{
    if (count_out(obj + 0x52)) {
        wr16(obj + O_STATE, 0x24);
        wr16(obj + 0x52, 0x20);
        giant_collide(0xCAA0, 0);
        giant_limbs(obj);
        return;
    }
    uint16_t a = rom16(0x04356A + (uint16_t)((uint16_t)(rd16(v_gameFrames) << 1) & 0xE));
    wr16(0xCA30, a);
    wr16(0xCA90, (uint16_t)(0x1FE - a));
    giant_limbs(obj);
}

/* $04357A. Beaten: blinking (y 0 or $118 by +$52's bit 0) for +$52 frames; then sound $CF,
 * the members shattered, it thrown up $5 and away from the middle with a box that hurts */
void DiceGiantBeaten(uint16_t obj)
{
    if (count_out(obj + 0x52)) {
        QueueSound(0xCF);
        wr16(v_shakeSprites, 8);
        add16(obj + O_STATE, 4);
        wr32(obj + 0x1C, 0xFFFB0000);
        wr8(obj + 0x21, 0x10);
        wr32(obj + 0x2C, 0xF808F808);
        wr16(obj + 0x26, 0x14);
        giant_shatter();
        if (below16(obj + O_X, 0x120)) wr32(obj + 0x18, 0x18000);
        else wr32(obj + 0x18, 0xFFFE8000);
        return;
    }
    if (bit8(obj + 0x53, 0)) wr16(obj + O_Y, 0);
    else wr16(obj + O_Y, 0x118);
    giant_limbs(obj);
}

/* $0435F2. Thrown (gravity $2000), flashing, smoking every 4 frames; struck ($22 bit 4) or past
 * y $148 falling: hidden, a burst ($8C) where it is, the screen shaken, $80 frames left */
void DiceGiantThrown(uint16_t obj)
{
    if (!bit8(obj + 0x22, 4)) {
        add32(obj + 0x1C, 0x2000);
        if (neg32(rd32(obj + 0x1C)) || below16(obj + O_Y, 0x148)) {
            giant_flash(obj);
            if (rd16(v_gameFrames) & 3) return;
            uint16_t a0, t;
            if (!SlotFreeHigh(&a0, &t)) return;
            wr16(a0 + O_TYPE, 0x100);
            wr32(a0 + 0x48, 0x0564FC);
            wr16(a0 + O_X, rd16(obj + O_X));
            wr16(a0 + O_Y, rd16(obj + O_Y));
            return;
        }
    }
    add16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0);
    wr8(obj + 0x21, 0);
    wr16(obj + 0x48, 0x80);
    wr16(v_shakeSprites, 0x10);
    uint16_t a0, t;
    if (!SlotFreeHigh(&a0, &t)) return;
    wr16(a0 + O_TYPE, 0x8C);
    wr16(a0 + 0x48, 0x60);
    wr16(a0 + 0x4A, 0x10);
    wr16(a0 + 0x4C, 8);
    wr16(a0 + O_X, rd16(obj + O_X));
    wr16(a0 + O_Y, rd16(obj + O_Y));
}

/* $043634. After +$48 frames the room ends, its player's +$72 given back */
void DiceGiantGone(uint16_t obj)
{
    if (!count_out(obj + 0x48)) return;
    DiceRoomEnd(obj);
    uint16_t p = rd16(0x80D8);
    wr16(p + 0x72, rd16(obj + 0x5E));
}

/* $04364E. Its entrance: the entering pose, placed from the right foot ($CEC0) */
void DiceGiantEnter(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    wr16(obj + 0x58, 0);
    wr16(obj + 0x50, obj);
    giant_pose(0x043A5C);
    wr16(obj + 0x50, 0xCEC0);
    giant_limbs(obj);
}

/* loc_43692: the joints swinging back and forth ($CB50 between 0 and $38) */
static void giant_swing(uint16_t obj)
{
    uint16_t d0, d1;
    if (!rd16(obj + 0x58)) {
        d0 = 8;
        d1 = 0xFFFC;
        if (rd16(0xCB50) == 0x38) wr16(obj + 0x58, 2);
    } else {
        d0 = 0xFFF8;
        d1 = 4;
        if (rd16(0xCB50) == 0) wr16(obj + 0x58, 0);
    }
    wr16(0xCA90, (uint16_t)((rd16(0xCA90) - d0) & 0x1FC));
    wr16(0xCA30, (uint16_t)((rd16(0xCA30) - d0) & 0x1FC));
    wr16(0xCB50, (uint16_t)((rd16(0xCB50) - d1) & 0x1FC));
    wr16(0xCAF0, (uint16_t)((rd16(0xCAF0) - d1) & 0x1FC));
    giant_limbs(obj);
}

/* $04367E. Swinging until the sines are free ($80C2 clear; then taken, $10) */
void DiceGiantEntering(uint16_t obj)
{
    if (!rd16(0x80C2)) {
        add16(obj + O_STATE, 4);
        wr16(0x80C2, 0x10);
    }
    giant_swing(obj);
}

/* $043676. Swinging, watching its player */
void DiceGiantSwing(uint16_t obj)
{
    giant_choose(obj);
    giant_swing(obj);
}

/* $042F66. The room ended if its player left; the body flipped as it faces; the body's
 * vitality spent: 25 points, blinking (state $48); shot: knocked away (sound $AC); struck on a
 * limb (+$22 bit 1, cleared): state $2C */
void DiceGiant(uint16_t obj)
{
    if (room_player_gone(obj)) return;
    if (rd16(0xCA04)) {
        if (bit8(obj + 0x54, 0)) wr16(0xC9EE, rd16(0xC9EE) | 0x800);
        else wr16(0xC9EE, rd16(0xC9EE) & 0xF7FF);
    }
    if (neg16(rd16(0xCA04))) {
        AddScore(obj, 37);
        wr8(obj + 0x21, 0);
        wr16(0xCA04, 0);
        wr16(obj + O_STATE, 0x48);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr16(obj + O_Y, 0x118);
        wr16(obj + 0x50, obj);
        wr16(obj + 0x52, 0x40);
        wr16(obj + 0x56, 0);
        giant_pose(0x043A16);
    } else if (bit8(obj + 0x22, 5)) {
        giant_knocked(obj);
        QueueSound(0xAC);
    } else if (bit8(obj + 0x22, 1)) {
        clr8(obj + 0x22, 1);
        giant_struck(obj);
    }
    dispatch(0x04305C, obj);
}

/* ======================================================================================== */
/* ---- type $2FC: the dice palace's machine room: a machine drawn in plane A (its chunks from
 * $FF4000 on, PlaneAChunk) with $34E of vitality, its pieces: two floors ($DE80, $DEE0), two
 * panels (type $300 at $DF40, $DFA0), a guard (type $304 at $C9E0) and a counter (type $31C
 * at $CAA0, whose +$4E set loses the room). Each $258, $190, $C8 of vitality lost: a falling
 * part ($308) and the machine redrawn more broken (on an odd frame); at 0: collapsing, smoking
 * until the sines are free, the room ends ---- */

/* $0444C8. A floor: plain, solid ($46 $8F) */
static void machine_floor(uint16_t a0)
{
    wr16(a0 + O_TYPE, 0x10);
    wr8(a0 + 0x21, 4);
    wr8(a0 + 0x46, 0x8F);
    wr32(a0 + 0x2C, 0xF010807F);
    wr32(a0 + 0x28, 0xF010807F);
    wr16(a0 + O_Y, 0x145);
}

/* $04450C. A panel (type $300) */
static void machine_panel(uint16_t a0)
{
    wr16(a0 + O_TYPE, 0x300);
    wr16(a0 + O_VITALITY, 0x118);
    wr8(a0 + 0x21, 0x20);
    wr32(a0 + 0x28, 0xF010F010);
    wr16(a0 + O_Y, 0x120);
}

/* $0446BE. A chunk of plane A blanked ($FF4000) */
static void machine_blank(uint16_t vram)
{
    PlaneAChunk(0xFFFF4000, vram);
}

/* $04421C. The counter's +$4E set (the room lost): the sines' work ($80C2 $28, $80C4 8, $80C6
 * $100, $80C8 $3A42D), state $28 for $40 frames, every object from $C9E0 but the counter and
 * the plain ones vanished; Z clear then (the caller returns) */
static int machine_lost(uint16_t obj)
{
    if (!rd16(0xCAEE)) return 0;
    wr16(0x80C2, 0x28);
    wr16(0x80C4, 8);
    wr16(0x80C6, 0x100);
    wr32(0x80C8, 0x03A42D);
    wr16(obj + O_STATE, 0x28);
    wr16(obj + 0x48, 0x40);
    wr8(obj + 0x21, 0);
    ObjectsVanishFrom(0xC9E0, 0x31C, 0x10);
    return 1;
}

/* $044396. Struck (once +$4A is negative; then 1): a spark ($B8) flying from what struck it
 * (+$42) at a random angle */
static void machine_spark(uint16_t obj)
{
    if (!neg16(rd16(obj + 0x4A))) return;
    wr16(obj + 0x4A, 1);
    uint16_t a0, t;
    if (!SlotFreeHigh(&a0, &t)) return;
    wr16(a0 + O_TYPE, 0xB8);
    wr16(a0 + O_FLAGS, 0x8C00);
    uint16_t a1 = rd16(obj + 0x42);
    wr16(a0 + O_X, rd16(a1 + O_X));
    wr16(a0 + O_Y, rd16(a1 + O_Y));
    wr16(a0 + 0x48, 0x28);
    wr16(a0 + 0x4A, 1);
    uint16_t a = (uint16_t)((rd16(v_randomSeed) & 0xF0) + 0xFE);
    wr32(a0 + 0x1C, (uint32_t)(int32_t)sine(a) << 4);
    wr32(a0 + 0x18, (uint32_t)(int32_t)cosine(a) << 4);
    if (!bit8(v_gameFrames + 1, 0)) {
        wr16(a0 + 0x0E, 0x2300);
        wr16(a0 + 0x08, 0x0500);
        wr16(a0 + 0x0A, 0xF8F8);
        return;
    }
    wr16(a0 + 0x0E, 0x2304);
    wr16(a0 + 0x08, 0);
    wr16(a0 + 0x0A, 0xFCFC);
}

/* $0445A6. Every $40 frames a drop ($1B0) at $F8,$F6 */
static void machine_drip(void)
{
    if (rd16(v_gameFrames) & 0x3F) return;
    uint16_t a0, t;
    if (!SlotFreeHigh(&a0, &t)) return;
    wr16(a0 + O_TYPE, 0x1B0);
    wr16(a0 + 0x0E, 0x87B4);
    wr32(a0 + 0x08, 0x000E0BAC);
    wr16(a0 + O_X, 0xF8);
    wr16(a0 + O_Y, 0xF6);
    wr16(a0 + 0x48, 0x2C);
}

/* loc_44266. Fighting: struck, sparks and a flashing face for $800 frames (with the drops),
 * else its face at rest */
static void machine_fight(uint16_t obj)
{
    if (!neg16(rd16(obj + 0x4A))) wr16(obj + 0x4A, (uint16_t)(rd16(obj + 0x4A) - 1));
    if (bit8(obj + 0x22, 5)) {
        machine_spark(obj);
        wr16(obj + 0x48, 0x800);
        wr16(v_shakeSprites, 6);
        wr16(v_shakePlane, 6);
    }
    uint16_t t = rd16(obj + 0x48);
    if (t && !neg16(t)) {
        machine_drip();
        wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
        if (!bit8(v_gameFrames + 1, 3)) {
            PlaneAChunk(0xFFFF4820, 0xC620);
            PlaneAChunk(0xFFFF48C0, 0xC820);
            return;
        }
        PlaneAChunk(0xFFFF4840, 0xC620);
        PlaneAChunk(0xFFFF48E0, 0xC820);
        return;
    }
    PlaneAChunk(0xFFFF4800, 0xC620);
    PlaneAChunk(0xFFFF48A0, 0xC820);
}

/* loc_44316. A stage of damage: the screen shaken, the next state, a falling part ($308) from
 * the tables by `i` (0, 2, 4) */
static void machine_breaks(uint16_t obj, uint16_t i)
{
    wr16(v_shakeSprites, 0x10);
    wr16(v_shakePlane, 0x10);
    add16(obj + O_STATE, 4);
    uint16_t a0, t;
    if (!SlotFreeHigh(&a0, &t)) return;
    wr16(a0 + O_TYPE, 0x308);
    wr16(a0 + 0x4C, rom16(0x04435A + i));
    wr16(a0 + O_X, rom16(0x044360 + i));
    wr16(a0 + O_Y, rom16(0x044366 + i));
}

/* a fighting state: the room lost, or fighting until the vitality is below `limit`, then
 * damage `i` */
static void machine_stage(uint16_t obj, uint16_t limit, uint16_t i)
{
    if (machine_lost(obj)) return;
    if (!below16(obj + O_VITALITY, limit)) { machine_fight(obj); return; }
    machine_breaks(obj, i);
}

/* $043F64. Set up at $120,$110 with its pieces; the sines taken ($80C2 $10, $80D6 $C980); the
 * RAM at $FF7800 cleared ($304 bytes, loc_44498) */
void DiceMachineStart(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    wr16(0x80C2, 0x10);
    wr16(0x80D6, 0xC980);
    wr16(obj + O_VITALITY, 0x34E);
    wr8(obj + 0x21, 0x20);
    wr32(obj + 0x28, 0xE020D030);
    wr16(obj + O_Y, 0x110);
    wr16(obj + O_X, 0x120);
    wr16(0xDE80 + O_X, 0xC0);
    machine_floor(0xDE80);
    wr16(0xDEE0 + O_X, 0x180);
    machine_floor(0xDEE0);
    wr16(0xDF40 + O_X, 0xD0);
    machine_panel(0xDF40);
    wr16(0xDFA0 + O_X, 0x190);
    machine_panel(0xDFA0);
    uint16_t a0 = 0xC9E0;
    wr16(a0 + O_X, 0xA0);
    wr16(a0 + O_TYPE, 0x304);
    wr16(a0 + O_FLAGS, 0x8000);
    wr16(a0 + 0x0E, 0x2305);
    wr16(a0 + 0x08, 0x0E00);
    wr16(a0 + 0x0A, 0xF0F4);
    wr8(a0 + 0x20, 0xB8);
    wr16(a0 + O_VITALITY, 0x28);
    wr8(a0 + 0x21, 0x20);
    wr32(a0 + 0x28, 0xF010F010);
    wr16(a0 + O_Y, 0x128);
    a0 = 0xCAA0;
    wr16(a0 + O_TYPE, 0x31C);
    wr16(a0 + 0x48, 0x400);
    wr16(a0 + 0x4A, 1);
    wr16(a0 + 0x4C, 0);
    wr16(a0 + 0x4E, 0);
    wr16(a0 + 0x50, 0xF0);
    wr16(a0 + 0x52, 0xA8);
    for (uint16_t k = 0; k <= 0xC0; k++) wr32((uint16_t)(0x7800 + 4 * k), 0);
}

/* $043FA8. Fighting to $258 */
void DiceMachineFight1(uint16_t obj)
{
    machine_stage(obj, 0x258, 0);
}

/* $043FC0. On an odd frame: redrawn broken (a piece more while the panel at $DF40 stands) */
void DiceMachineBroken1(uint16_t obj)
{
    if (machine_lost(obj)) return;
    if (!bit8(v_gameFrames + 1, 0)) return;
    add16(obj + O_STATE, 4);
    PlaneAChunk(0xFFFF4C20, 0xC618);
    PlaneAChunk(0xFFFF4C60, 0xC818);
    PlaneAChunk(0xFFFF4CE0, 0xCA20);
    if (!rd16(0xDF40)) return;
    if (neg16(rd16(0xDF64))) return;
    PlaneAChunk(0xFFFF4DA0, 0xCA18);
}

/* $044028. Fighting to $190 */
void DiceMachineFight2(uint16_t obj)
{
    machine_stage(obj, 0x190, 2);
}

/* $044040. On an odd frame: redrawn more broken (the panel at $DFA0's chunk as it stands) */
void DiceMachineBroken2(uint16_t obj)
{
    if (machine_lost(obj)) return;
    if (!bit8(v_gameFrames + 1, 0)) return;
    add16(obj + O_STATE, 4);
    PlaneAChunk(0xFFFF4C40, 0xC630);
    PlaneAChunk(0xFFFF4000, 0xC638);
    PlaneAChunk(0xFFFF4CC0, 0xC830);
    PlaneAChunk(0xFFFF4000, 0xC838);
    PlaneAChunk(0xFFFF4D20, 0xCA30);
    if (rd16(0xDFA0) && !neg16(rd16(0xDFC4))) {
        PlaneAChunk(0xFFFF4D40, 0xCA38);
        return;
    }
    PlaneAChunk(0xFFFF4000, 0xCA38);
}

/* $0440DA. Fighting to $C8 */
void DiceMachineFight3(uint16_t obj)
{
    machine_stage(obj, 0xC8, 4);
}

/* $0440F2. On an odd frame: redrawn more broken */
void DiceMachineBroken3(uint16_t obj)
{
    if (machine_lost(obj)) return;
    if (!bit8(v_gameFrames + 1, 0)) return;
    add16(obj + O_STATE, 4);
    PlaneAChunk(0xFFFF4CA0, 0xC828);
}

/* $04411A. Fighting to 0; then the sines' work ($80C2 $3C, $80C8 $3A438, the counter's +$4A
 * and +$4C to $80CE), the counter stopped (state 8), the sines to its player; the screen
 * shaken, sounds $BD and $82 (loc_4436C) */
void DiceMachineFight4(uint16_t obj)
{
    if (machine_lost(obj)) return;
    if (!neg16(rd16(obj + O_VITALITY))) { machine_fight(obj); return; }
    wr16(0x80C2, 0x3C);
    wr32(0x80C8, 0x03A438);
    wr16(0xCAA0 + O_STATE, 8);
    wr32(0x80CE, (uint32_t)rd16(0xCAA0 + 0x4A) << 16 | rd16(0xCAA0 + 0x4C));
    wr16(0x80D6, rd16(0x80D8));
    wr16(v_shakeSprites, 0x10);
    wr16(v_shakePlane, 0x10);
    add16(obj + O_STATE, 4);
    wr8(obj + 0x21, 0);
    QueueSound(0xBD);
    QueueSound(0x82);
}

/* $04415C. Collapsing: the screen shaken; on an odd frame redrawn as a wreck */
void DiceMachineCollapse(uint16_t obj)
{
    wr16(v_shakeSprites, 0x10);
    wr16(v_shakePlane, 0x10);
    if (!bit8(v_gameFrames + 1, 0)) return;
    add16(obj + O_STATE, 4);
    PlaneAChunk(0xFFFF4AA0, 0xC420);
    PlaneAChunk(0xFFFF4AC0, 0xC428);
    PlaneAChunk(0xFFFF4000, 0xC618);
    PlaneAChunk(0xFFFF4BE0, 0xC620);
    PlaneAChunk(0xFFFF4C00, 0xC628);
    PlaneAChunk(0xFFFF4000, 0xC630);
    PlaneAChunk(0xFFFF4C80, 0xC820);
    PlaneAChunk(0xFFFF4D00, 0xCA28);
}

/* $044436. On an even frame a puff of smoke ($100, $564FC) rising from a random point of it */
static void machine_smoke(uint16_t obj)
{
    if (bit8(v_gameFrames + 1, 0)) return;
    uint16_t a0, t;
    if (!SlotFreeHigh(&a0, &t)) return;
    wr16(a0 + O_TYPE, 0x100);
    wr32(a0 + 0x48, 0x0564FC);
    uint16_t dx = rd8(v_randomSeed) & 0x1E, dy = rd8(v_randomSeed + 1) & 0x1E;
    wr16(a0 + O_X, (uint16_t)(rd16(obj + O_X) + 0xA + dx));
    wr16(a0 + O_Y, (uint16_t)(rd16(obj + O_Y) - 0x24 + dy));
    wr32(a0 + 0x18, 0x8000);
    wr32(a0 + 0x1C, 0xFFFE8000);
}

/* $0441F8. Smoking until the sines are free; then the room ends */
void DiceMachineSmoke(uint16_t obj)
{
    machine_smoke(obj);
    if (!rd16(0x80C2)) DiceRoomEnd(obj);
}

/* $04420C. The room lost: after +$48 frames it ends */
void DiceMachineLost(uint16_t obj)
{
    if (count_out(obj + 0x48)) DiceRoomEnd(obj);
}

/* $043EEE. Type $2FC: the room ended if its player left; else its step */
void DiceMachine(uint16_t obj)
{
    if (room_player_gone(obj)) return;
    dispatch(0x043F38, obj);
}

/* ---- type $300: the machine's panel: struck, the screen shakes; spent: the screen shaken,
 * then on an even frame a blast ($2F4) $10 lower and its part of plane A blanked (the left
 * panel's, x below $120, or the right's, with a chunk by the guard's state) ---- */

/* $0445E2. Type $300: as the section says */
void DiceMachinePanel(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        if (bit8(obj + 0x22, 5)) {
            wr16(v_shakeSprites, 3);
            wr16(v_shakePlane, 3);
        }
        if (!neg16(rd16(obj + O_VITALITY))) return;
        add16(obj + O_STATE, 4);
        wr8(obj + 0x21, 0);
        wr16(v_shakeSprites, 0x10);
        wr16(v_shakePlane, 0x10);
        return;
    }
    if (bit8(v_gameFrames + 1, 0)) return;
    wr16(obj + O_TYPE, 0x2F4);
    add16(obj + O_Y, 0x10);
    wr8(obj + 0x21, 0);
    if (below16(obj + O_X, 0x120)) {
        machine_blank(0xC808);
        machine_blank(0xC810);
        machine_blank(0xCA08);
        machine_blank(0xCA10);
        machine_blank(0xCA18);
        PlaneAChunk(0xFFFF4CE0, 0xCA20);
        return;
    }
    machine_blank(0xC840);
    machine_blank(0xC848);
    machine_blank(0xCA40);
    machine_blank(0xCA48);
    if (below16(0xC984, 0x10)) {
        PlaneAChunk(0xFFFF4DC0, 0xCA38);
        return;
    }
    machine_blank(0xCA38);
}

/* ---- type $304: the machine's guard: spent, an item ($54) ---- */

/* $0446CC. Type $304: its vitality spent, an item */
void DiceMachineGuard(uint16_t obj)
{
    if (!neg16(rd16(obj + O_VITALITY))) return;
    wr16(obj + O_TYPE, 0x54);
    wr16(obj + O_STATE, 0);
    wr8(obj + 0x21, 0);
    DropItem(obj);
}

/* ======================================================================================== */
/* ---- type $270: the dice palace's peg room: seven pegs (type $10 from $C9E0), the boss (this
 * one at $C980, $AF1 of vitality, its colours at $FFE330 flashing when struck) walking under
 * them (its scroll in $FFE6C0) and sending runners (type $274) from peg to peg along scripts
 * chosen at random; its vitality spent: a fuse ($278) runs the pegs and burns it ---- */

/* $044E28. Three colours to $FFE330 */
static void pegs_colours(uint32_t src)
{
    for (uint16_t k = 0; k <= 2; k++) wr16((uint16_t)(0xE330 + 2 * k), rom16(src + 2 * k));
}

/* $044CF4. The time to speed up (+$48) by the difficulty */
static void pegs_push_time(uint16_t obj)
{
    wr16(obj + 0x48, rom16(0x044D06 + (uint32_t)(int16_t)rd16(v_difficulty)));
}

/* $044CA6. The angle (+$4C) to the peg in +$4A */
static void pegs_aim(uint16_t obj)
{
    uint16_t a0 = rd16(obj + 0x4A);
    int16_t dx = (int16_t)(rd16(a0 + O_X) - rd16(obj + O_X));
    uint32_t d1 = (uint16_t)(rd16(a0 + O_Y) - rd16(obj + O_Y));
    wr16(obj + 0x4C, (uint16_t)(((int16_t)Atan2(dx, &d1) >> 7) & 0x1FE));
}

/* $044BDC. The next peg of its script (+$50: the script of the two the boss drew, words at
 * $C9D6; +$52 the place in it): a peg other than the one it is at becomes +$4A and it aims at
 * it; a script's end ($40): the next script, after the second the next state. (The 68k loops
 * by branching back to the routine's start.) */
static void pegs_next(uint16_t obj)
{
    for (;;) {
        uint16_t s = rd16((uint16_t)(0xC9D6 + rd16(obj + 0x50)));
        uint32_t script = rom32(0x044C40 + (uint32_t)(int16_t)s);
        uint16_t i = rd16(obj + 0x52);
        add16(obj + 0x52, 1);
        uint16_t b = bus_rd8(script + (uint32_t)(int16_t)i);
        if (b & 0x40) {
            add16(obj + 0x50, 2);
            if (!below16(obj + 0x50, 2)) {
                add16(obj + O_STATE, 4);
                return;
            }
            wr16(obj + 0x52, 0);
            continue;
        }
        uint16_t peg = rom16(0x044C98 + b);
        if (peg == rd16(obj + 0x4A)) continue;
        wr16(obj + 0x4A, peg);
        pegs_aim(obj);
        return;
    }
}

/* $044BD8. The same, its time to speed up first */
static void pegs_next_timed(uint16_t obj)
{
    pegs_push_time(obj);
    pegs_next(obj);
}

/* $044CCC. Its frame and box by its angle (the table at $5647C, 8 bytes each) */
static void pegs_frame(uint16_t obj)
{
    uint16_t t = (uint16_t)(rd16(obj + 0x4C) + 8);
    uint16_t i = (uint16_t)((uint16_t)(((int16_t)t >> 2) & 0x3C) << 1);
    uint32_t a = 0x05647C + i;
    wr16(obj + 0x0E, rom16(a));
    wr16(obj + 0x08, rom16(a + 2));
    wr16(obj + 0x0A, rom16(a + 4));
}

/* $044D0E. While +$48 lasts: sped up toward its angle (the sine and cosine added) */
static void pegs_push(uint16_t obj)
{
    uint16_t a = rd16(obj + 0x4C);
    if (neg16(rd16(obj + 0x48))) return;
    add32(obj + 0x1C, (uint32_t)(int32_t)sine(a));
    add32(obj + 0x18, (uint32_t)(int32_t)cosine(a));
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
}

/* $044D40. At its peg (within 8 both ways): the N flag of the last compare */
static int pegs_at(uint16_t obj)
{
    uint16_t a0 = rd16(obj + 0x4A);
    uint16_t dx = (uint16_t)(rd16(a0 + O_X) - rd16(obj + O_X));
    if (neg16(dx)) dx = (uint16_t)(~dx + 1);
    if (!lt16(dx, 8)) return 0;
    uint16_t dy = (uint16_t)(rd16(a0 + O_Y) - rd16(obj + O_Y));
    if (neg16(dy)) dy = (uint16_t)(~dy + 1);
    return lt16(dy, 8);
}

/* $044E46. Every 4 frames a puff ($100, $563EE) where it is, sound $CE */
static void pegs_spark(uint16_t obj)
{
    if (rd16(v_gameFrames) & 3) return;
    uint16_t a0, t;
    if (!SlotFreeHigh(&a0, &t)) return;
    wr16(a0 + O_TYPE, 0x100);
    wr32(a0 + 0x48, 0x0563EE);
    wr16(a0 + O_X, rd16(obj + O_X));
    wr16(a0 + O_Y, rd16(obj + O_Y));
    QueueSound(0xCE);
}

/* $044B9E. A runner ($274) let out under it at y $140 (sound $B6 every 4 frames) */
static void pegs_runner(uint16_t obj)
{
    uint16_t x = rd16(obj + O_X);
    if (!(rd16(v_gameFrames) & 3)) QueueSound(0xB6);
    uint16_t a0, t;
    if (!SlotFreeHigh(&a0, &t)) return;
    wr16(a0 + O_TYPE, 0x274);
    wr16(a0 + O_X, x);
    wr16(a0 + O_Y, 0x140);
}

/* $044D7E. Walking left to x $E0 and right to $160 */
static void pegs_walk(uint16_t obj)
{
    if (!rd16(obj + 0x5A)) {
        wr16(obj + O_X, (uint16_t)(rd16(obj + O_X) - 1));
        if (!below16(obj + O_X, 0xE0)) return;
        wr16(obj + 0x5A, 2);
        return;
    }
    add16(obj + O_X, 1);
    if (below16(obj + O_X, 0x160)) return;
    wr16(obj + 0x5A, 0);
}

/* $0447E6. Set up at $120,$140 with its pegs ($C9E0, seven) and floors ($DE80, $DEE0, $DF40);
 * $FFF7E7 = 2; its plane's scroll begun as the furnace's (loc_3A79A: $A95A bit 7, the rows of
 * $FFE400 at -camera x) */
void DicePegsStart(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0x0800);
    wr32(obj + 0x28, 0xF040F010);
    wr16(obj + O_VITALITY, 0xAF1);
    wr16(obj + 0x54, 0);
    wr16(obj + O_X, 0x120);
    wr16(obj + O_Y, 0x140);
    uint16_t a0 = 0xC9E0;
    for (uint16_t k = 0; k <= 6; k++) {
        wr16(a0 + O_TYPE, 0x10);
        wr16(a0 + O_FLAGS, 0x8000);
        wr16(a0 + 0x0E, 0x2300);
        wr16(a0 + 0x08, 0x0A00);
        wr16(a0 + 0x0A, 0xF4F4);
        wr16(a0 + O_X, rom16(0x044B5A + 2 * k));
        wr16(a0 + O_Y, rom16(0x044B68 + 2 * k));
        a0 = (uint16_t)(a0 + 0x60);
    }
    a0 = 0xDE80;
    for (int k = 0; k <= 1; k++) {
        wr16(a0 + O_TYPE, 0x10);
        wr8(a0 + 0x21, 4);
        wr8(a0 + 0x46, 0x8F);
        wr32(a0 + 0x2C, 0xE07F807F);
        wr16(a0 + O_Y, 0x140);
        a0 = (uint16_t)(a0 + 0x60);
    }
    wr16(0xDE90, 0xA0);
    wr16(0xDEF0, 0x1A0);
    wr16(a0 + O_TYPE, 0x10);
    wr8(a0 + 0x21, 0x20);
    wr32(a0 + 0x28, 0x007F807F);
    wr16(a0 + O_Y, 0x140);
    wr16(a0 + O_X, 0x120);
    wr8(0xF7E7, 2);
    wr8(v_planeFlagsA, (uint8_t)(rd8(v_planeFlagsA) | 0x80));
    uint16_t v = (uint16_t)-rd16(v_camX);
    for (uint16_t a = 0xE400, k = 0; k < 28; k++, a = (uint16_t)(a + 0x20)) wr16(a, v);
}

/* $044824. Waiting for the sines to be free; then taken ($10, $C980), its collisions on */
void DicePegsWait(uint16_t obj)
{
    if (rd16(0x80C2)) return;
    add16(obj + O_STATE, 4);
    wr16(0x80C2, 0x10);
    wr16(0x80D6, 0xC980);
    wr8(obj + 0x21, 0x20);
}

/* $044846. After +$4E frames: a script drawn (+$56 = a random multiple of 4 below $20, then
 * Random; loc_44B76, its loop run once; the 68k's a5 left at $C980) */
void DicePegsDraw(uint16_t obj)
{
    if (!count_out(obj + 0x4E)) return;
    add16(obj + O_STATE, 4);
    wr16(obj + 0x50, 0);
    wr16(obj + 0x52, 0);
    wr16(obj + 0x56, rd16(v_randomSeed) & 0x1C);
    Random();
    reg_a5_left = 0xC980;
}

/* $044864. The next peg of the script; the time to show it by the difficulty; sound $A6
 * unless the script ended */
void DicePegsNext(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    pegs_next_timed(obj);
    wr16(obj + 0x4E, rom16(0x044892 + (uint32_t)(int16_t)rd16(v_difficulty)));
    if (below16(obj + O_STATE, 0x14)) QueueSound(0xA6);
}

/* $04489A. The peg (+$4A) lit ($4300) for +$4E frames, then back ($2300) and the next */
void DicePegsShow(uint16_t obj)
{
    uint16_t a0 = rd16(obj + 0x4A);
    if (count_out(obj + 0x4E)) {
        wr16(a0 + 0x0E, 0x2300);
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) - 4));
        return;
    }
    wr16(a0 + 0x0E, 0x4300);
}

/* $0448BA. A pause of $20 frames */
void DicePegsPause(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    wr16(obj + 0x4E, 0x20);
}

/* $0448C6. Then 12 runners to let out ($40 frames to the first), +$5E -1 */
void DicePegsReady(uint16_t obj)
{
    if (!count_out(obj + 0x4E)) return;
    add16(obj + O_STATE, 4);
    add16(obj + 0x48, 4);
    wr16(obj + 0x48, rd16(obj + 0x48) & 0xC);
    wr16(obj + 0x4A, 0xB);
    wr16(obj + 0x4C, 0x40);
    wr16(obj + 0x5E, 0xFFFF);
}

/* $0448F0. A runner each few frames (by the difficulty); the last out: +$5E 0, $100 frames */
void DicePegsRunners(uint16_t obj)
{
    if (!count_out(obj + 0x4C)) return;
    wr16(obj + 0x4C, rom16(0x044924 + (uint32_t)(int16_t)rd16(v_difficulty)));
    pegs_runner(obj);
    if (!count_out(obj + 0x4A)) return;
    add16(obj + O_STATE, 4);
    add16(obj + 0x5E, 1);
    wr16(obj + 0x4E, 0x100);
}

/* $04492C. Walking while the runners run (each at its end takes 1 off +$5E); all done: a new
 * script after $40 frames (state 8); its scroll's rows at $FFE6C0 at x - $120 (loc_44DB2) */
void DicePegsWalk(uint16_t obj)
{
    if (neg16(rd16(obj + 0x5E))) {
        wr16(obj + O_STATE, 8);
        wr16(obj + 0x4E, 0x40);
    }
    pegs_walk(obj);
    uint16_t v = (uint16_t)(rd16(obj + O_X) + 0xFEE0);
    for (uint16_t a = 0xE6C0, k = 0; k <= 5; k++, a = (uint16_t)(a + 0x20)) wr16(a, v);
}

/* $044944. Beaten: the screen shaking (the fuse, $278, moves it on) */
void DicePegsBeaten(uint16_t obj)
{
    (void)obj;
    wr16(v_shakeSprites, 4);
}

/* $04494C. After +$48 frames the room ends */
void DicePegsEnd(uint16_t obj)
{
    if (count_out(obj + 0x48)) DiceRoomEnd(obj);
}

/* $0446F6. The room ended if its player left; its vitality spent: state $24, a fuse ($278, in
 * $DFA0 if no slot is free) at its x on the floor; its colours flashing every 4 frames while
 * struck or beaten */
void DicePegs(uint16_t obj)
{
    if (room_player_gone(obj)) return;
    if (neg16(rd16(obj + O_VITALITY))) {
        wr16(obj + O_STATE, 0x24);
        wr8(obj + 0x21, 0);
        wr16(obj + O_VITALITY, 0);
        wr16(obj + 0x56, 0x10);
        uint16_t a0, t;
        if (!SlotFreeHigh(&a0, &t)) a0 = 0xDFA0;
        wr16(a0 + O_TYPE, 0x278);
        wr16(a0 + O_STATE, 0);
        wr16(a0 + O_X, rd16(obj + O_X));
        wr16(a0 + O_Y, 0x140);
    }
    uint32_t colours = 0x044E3A;
    if (!below16(obj + O_STATE, 0x24) || bit8(obj + 0x22, 5)) {
        if (count_out(obj + 0x54)) {
            wr16(obj + 0x54, 3);
            colours = 0x044E40;
        }
    }
    pegs_colours(colours);
    dispatch(0x0447BA, obj);
}

/* ---- type $274: a runner along the script's pegs (its own copy: +$50, +$52 from 0), sped
 * toward each in turn; at the end: removed, the boss's +$5E counted down ---- */

/* $044984. Its set-up and its first peg */
void PegRunnerStart(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0x8C00);
    wr8(obj + 0x21, 0x30);
    wr32(obj + 0x2C, 0xFA06FA06);
    wr32(obj + 0x28, 0xF808F808);
    wr16(obj + 0x26, 4);
    wr16(obj + 0x4A, 1);
    pegs_next_timed(obj);
    pegs_frame(obj);
    pegs_push_time(obj);
}

/* $0449BC. Sped toward its peg; there: stopped on it, the next */
void PegRunnerRun(uint16_t obj)
{
    pegs_push(obj);
    if (!pegs_at(obj)) return;
    uint16_t a0 = rd16(obj + 0x4A);
    wr16(obj + O_X, rd16(a0 + O_X));
    wr16(obj + O_Y, rd16(a0 + O_Y));
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x4E, 0);
    pegs_next_timed(obj);
    pegs_frame(obj);
    pegs_push_time(obj);
}

/* $0449FC. At the script's end: removed, the boss's +$5E counted down */
void PegRunnerEnd(uint16_t obj)
{
    wr16(obj + O_FLAGS, 0x1000);
    wr16(0xC9DE, (uint16_t)(rd16(0xC9DE) - 1));
}

/* $04495C. Removed once the boss ($C980) is beaten */
void PegRunner(uint16_t obj)
{
    if (!below16(0xC984, 0x24)) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    dispatch(0x044978, obj);
}

/* ---- type $278: the fuse: sparking along the script's pegs, each burst ($8C) on reaching
 * it; then to the boss ($C980): a burst there, the boss to its end ($28, $20 frames) ---- */

/* $044A22. Its set-up and its first peg */
void PegFuseStart(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0x0C00);
    wr16(obj + 0x48, 0x20);
    wr16(obj + 0x4A, 1);
    wr16(obj + 0x52, 0);
    wr16(obj + 0x50, 0);
    pegs_next(obj);
}

/* $044A48. Sparking toward its peg; there: the peg burst, the next */
void PegFuseRun(uint16_t obj)
{
    pegs_spark(obj);
    pegs_push(obj);
    if (!pegs_at(obj)) return;
    uint16_t a0 = rd16(obj + 0x4A);
    wr16(obj + O_X, rd16(a0 + O_X));
    wr16(obj + O_Y, rd16(a0 + O_Y));
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x4E, 0);
    wr16(a0 + O_TYPE, 0x8C);
    wr16(a0 + 0x48, 0x60);
    wr16(a0 + 0x4A, 4);
    wr16(a0 + 0x4C, 0x20);
    pegs_next(obj);
    wr16(obj + 0x48, 0x20);
}

/* $044AA0. The script done: aiming at the boss */
void PegFuseToBoss(uint16_t obj)
{
    pegs_spark(obj);
    add16(obj + O_STATE, 4);
    wr16(obj + 0x48, 0x20);
    wr16(obj + 0x4A, 0xC980);
    pegs_aim(obj);
}

/* $044ABA. Sparking to the boss; there: a burst, the boss to its end */
void PegFuseBurn(uint16_t obj)
{
    pegs_spark(obj);
    pegs_push(obj);
    if (!pegs_at(obj)) return;
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + O_X, rd16(0xC990));
    wr16(obj + O_Y, rd16(0xC994));
    wr16(obj + O_TYPE, 0x8C);
    wr16(obj + O_STATE, 0);
    wr16(obj + 0x48, 0x60);
    wr16(obj + 0x4A, 8);
    wr16(obj + 0x4C, 0x10);
    wr16(0xC984, 0x28);
    wr16(0xC9C8, 0x20);
}

/* $044A08. Type $278: its step */
void PegFuse(uint16_t obj)
{
    dispatch(0x044A12, obj);
}

/* ======================================================================================== */
/* ---- type $2D4: the dice palace's race: soldiers ($48, 1-8 by the difficulty) on the floor,
 * the timer ($31C at $DE80, as the switch room's); its player standing on y $110 between x
 * $158 and $177 before the time is out: won (the sines' work, the timer stopped, sound $82);
 * the time out ($DECE set): lost; either way every object from $C9E0 but the timer vanishes
 * and the room ends ---- */

/* $045000. Every object from $C9E0 but the timer ($31C) vanished */
static void race_vanish(void)
{
    ObjectsVanishFrom(0xC9E0, 0x31C, 0);
}

/* $044FA4. The soldiers ($48) along the floor, their +$06 from a table */
static void race_soldiers(void)
{
    uint16_t i = 0, n = (uint16_t)((int16_t)rd16(0xA20C) >> 1);
    for (;;) {
        uint16_t a0, t;
        if (!SlotFreeHigh(&a0, &t)) return;
        wr16(a0 + O_TYPE, 0x48);
        wr16(a0 + O_X, rom16(0x044FE0 + i));
        wr16(a0 + O_Y, 0x140);
        wr16(a0 + 0x06, rom16(0x044FF0 + i));
        i = (uint16_t)(i + 2);
        if (n-- == 0) return;
    }
}

/* $044EDA. $8080 cleared, the soldiers, the timer (SwitchRoomTimer's code: $31C at $DE80) */
void DiceRaceStart(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    wr16(0x8080, 0);
    race_soldiers();
    uint16_t a0 = 0xDE80;
    wr16(a0 + O_TYPE, 0x31C);
    wr16(a0 + 0x48, 0x400);
    wr16(a0 + 0x4A, 1);
    wr16(a0 + 0x4C, 0);
    wr16(a0 + 0x4E, 0);
    wr16(a0 + 0x50, 0xF0);
    wr16(a0 + 0x52, 0xA8);
}

/* $044EEE. The time out: lost; its player at the goal: won */
void DiceRaceRun(uint16_t obj)
{
    if (rd16(0xDECE)) {
        race_vanish();
        wr16(obj + O_STATE, 0x0C);
        wr16(obj + 0x48, 0x40);
        wr32(0x80C8, 0x03A42D);
        wr16(0x80C2, 0x28);
        wr16(0x80C4, 8);
        wr16(0x80C6, 0x100);
        return;
    }
    uint16_t p = rd16(0x80D8);
    if (rd16(p + O_Y) != 0x110) return;
    if (!below16(p + O_X, 0x178)) return;
    if (below16(p + O_X, 0x158)) return;
    race_vanish();
    add16(obj + O_STATE, 4);
    wr16(0x80C2, 0x3C);
    wr32(0x80C8, 0x03A438);
    wr16(0xDE80 + O_STATE, 8);
    wr32(0x80CE, (uint32_t)rd16(0xDE80 + 0x4A) << 16 | rd16(0xDE80 + 0x4C));
    wr16(0x80D6, rd16(0x80D8));
    QueueSound(0x82);
}

/* $044F84. Won: once the sines are free the room ends */
void DiceRaceWon(uint16_t obj)
{
    if (!rd16(0x80C2)) DiceRoomEnd(obj);
}

/* $044F94. Lost: after +$48 frames the room ends */
void DiceRaceLost(uint16_t obj)
{
    if (count_out(obj + 0x48)) DiceRoomEnd(obj);
}

/* $044E80. Type $2D4: the room ended if its player left; else its step */
void DiceRace(uint16_t obj)
{
    if (room_player_gone(obj)) return;
    dispatch(0x044ECA, obj);
}

/* ======================================================================================== */
/* ---- type $2D8: the dice palace's wheel: eight cars ($C9E0 on, $60 apart; +$06 each one's
 * place on the wheel, $300 round) turning past the front ($9008 the wheel's turn, $9004 its
 * speed, $E460 its scroll, its picture by ChunkDma); it turns each target car (types $2DC,
 * $2E4, $2E8, $2EC) to the front in turn; the lever car ($2E0) sets the floor's band ($9006,
 * $900C: speed and bit 0 turning) moving; no target left: spinning faster until the room
 * ends ---- */

/* $045134. In states 4 and 8: no target car left (each is $2E0 or gone): the camera stopped,
 * state $C; 1 then (the 68k's Z clear: the caller returns) */
static int wheel_cleared(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) return 0;
    if (rd16(obj + O_STATE) == 0x0C) return 0;
    uint16_t a2 = 0xC9E0;
    for (int k = 0; k <= 7; k++) {
        uint16_t t = rd16(a2 + O_TYPE);
        if (t != 0x2E0 && (t == 0x2DC || t == 0x2E4 || t == 0x2EC || t == 0x2E8)) return 0;
        a2 = (uint16_t)(a2 + 0x60);
    }
    wr16(v_camSpeedX, 0);
    wr16(obj + O_STATE, 0x0C);
    return 1;
}

/* $04518C. A target car drawn at random (from the random seed on, the next until one is):
 * the turn that brings it to the front (+$5E); there already: 4 frames more; else turning the
 * shorter way ($9004 2 or -2), the next state */
static void wheel_pick(uint16_t obj)
{
    uint16_t i = rd16(v_randomSeed), car;
    for (;;) {
        i &= 0xE;
        car = rom16(0x045226 + i);
        uint16_t t = rd16(car + O_TYPE);
        if (t != 0x2E0 && (t == 0x2DC || t == 0x2E4 || t == 0x2EC || t == 0x2E8)) break;
        i = (uint16_t)(i + 2);
    }
    uint16_t d = (uint16_t)(0x480 - (uint16_t)(rd16(car + 0x06) + 0x60));
    if (!lt16(d, 0x300)) d = (uint16_t)(d - 0x300);
    wr16(obj + 0x5E, d);
    d = (uint16_t)(d - rd16(0x9008));
    if (!d) {
        wr16(obj + 0x5C, 4);
        return;
    }
    add16(obj + O_STATE, 4);
    if (neg16(d)) {
        wr16(0x9004, lt16(d, 0xFE80) ? 2 : 0xFFFE);
        return;
    }
    wr16(0x9004, lt16(d, 0x180) ? 2 : 0xFFFE);
}

/* $045332. The wheel picture's three chunks for a column (its x mod $18 of 0, 8 or $10) */
static void wheel_chunks(uint16_t col, uint32_t *a, uint32_t *b, uint32_t *c)
{
    uint16_t r = (uint16_t)((col & 0xFFF8) % 0x18);
    if (r == 0x10) { *a = 0xFFFF4600; *b = 0xFFFF4660; *c = 0xFFFF46C0; return; }
    if (r == 0x08) { *a = 0xFFFF45E0; *b = 0xFFFF4640; *c = 0xFFFF46A0; return; }
    *a = 0xFFFF45C0; *b = 0xFFFF4620; *c = 0xFFFF4680;
}

/* $045248. The wheel turned by its speed */
static void wheel_roll(void)
{
    uint16_t v = rd16(0x9004);
    wr16(0xE460, (uint16_t)(rd16(0xE460) + v));
    wr16(0x9008, (uint16_t)(rd16(0x9008) + v));
}

/* $0452A2. The turn kept in 0-$2FF, the column coming into view redrawn (three chunks by
 * ChunkDma, $200 apart) */
static void wheel_draw(void)
{
    uint16_t at = (uint16_t)((int16_t)(uint16_t)(0x200 - (rd16(0xE460) & 0x1FF)) >> 2);
    at = (uint16_t)(at + (bit8(0x9004, 7) ? 0x58 : 0x80));
    at = (uint16_t)((at & 0x78) + 0xC000);
    uint16_t turn = rd16(0x9008);
    if (neg16(turn)) turn = (uint16_t)(turn + 0x300);
    else if (!lt16(turn, 0x300)) turn = (uint16_t)(turn - 0x300);
    wr16(0x9008, turn);
    uint16_t col = (uint16_t)((int16_t)(uint16_t)(0x300 - turn) >> 2);
    col = (uint16_t)(col + (bit8(0x9004, 7) ? 0x58 : 0x78));
    uint32_t a, b, c;
    wheel_chunks(col, &a, &b, &c);
    ChunkDma(a, at);
    ChunkDma(b, (uint16_t)(at + 0x200));
    ChunkDma(c, (uint16_t)(at + 0x400));
}

/* $04538A. The floor's band: while $900C bit 0 is set, moved by $900C (less bit 0) as the
 * camera's speed, a column of it redrawn at each 32 ($FF47A0); stopped at the next 32 */
static void wheel_band(void)
{
    if (!bit8(0x900D, 0)) {
        wr16(v_camSpeedX, 0);
        return;
    }
    uint16_t band = rd16(0x9006);
    if (!(band & 0x1E)) {
        uint16_t at = (uint16_t)((int16_t)(uint16_t)(0x200 - (band & 0x1FE)) >> 2);
        at = (uint16_t)(at + (bit8(0x900C, 7) ? 0x78 : 0x58));
        at = (uint16_t)((at & 0x78) + 0xCC00);
        PlaneAChunk(0xFFFF47A0, at);
    }
    uint16_t v = rd16(0x900C) & 0xFFFE;
    wr16(v_camSpeedX, v);
    wr16(0x9006, (uint16_t)(rd16(0x9006) - v));
    wr16(0x9006, rd16(0x9006) & 0x1FE);
    if (!(rd16(0x9006) & 0x1E)) clr8(0x900D, 0);
}

/* $045256. The rows' scroll: the wheel's 7 at $E460, 9 at half, 4 at a quarter, the band's 4 */
static void wheel_scroll(void)
{
    uint16_t a0 = 0xE480, v = rd16(0xE460);
    for (int k = 0; k <= 6; k++, a0 = (uint16_t)(a0 + 0x20)) wr16(a0, v);
    v = (uint16_t)((int16_t)v >> 1);
    for (int k = 0; k <= 8; k++, a0 = (uint16_t)(a0 + 0x20)) wr16(a0, v);
    v = (uint16_t)((int16_t)v >> 1);
    for (int k = 0; k <= 3; k++, a0 = (uint16_t)(a0 + 0x20)) wr16(a0, v);
    a0 = 0xE700;
    for (int k = 0; k <= 3; k++, a0 = (uint16_t)(a0 + 0x20)) wr16(a0, rd16(0x9006));
}

/* $045236. The wheel turned: rolled, drawn, its band, its rows' scroll */
static void wheel_turn(void)
{
    wheel_roll();
    wheel_draw();
    wheel_band();
    wheel_scroll();
}

/* $045442. A puff ($100, $563EE) at a random place, drifting against the turn */
static void wheel_puff(void)
{
    uint16_t a0, t;
    if (!SlotFreeHigh(&a0, &t)) return;
    wr16(a0 + O_TYPE, 0x100);
    wr32(a0 + 0x48, 0x0563EE);
    wr16(a0 + O_X, (uint16_t)((rd16(v_randomSeed) & 0xFF) + 0xA0));
    wr16(a0 + O_Y, (uint16_t)(rd8(v_randomSeed) + 0x90));
    wr16(a0 + 0x18, (uint16_t)((int16_t)(uint16_t)~rd16(0x9004) >> 2));
}

/* $045072. The planes' flags, the wheel at rest turning 1, the cars ($C9E0, their types from
 * $45432, places $C0 on, $60 apart; loc_4540C) */
void DiceWheelStart(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    wr8(v_planeFlagsB, 0);
    wr8(0xF7E7, 2);
    wr16(obj + 0x5C, 0x40);
    wr16(0x9004, 1);
    wr16(0xE460, 0);
    wr16(0x9008, 0);
    wr16(0x900A, 0);
    wr16(0x9006, 0);
    wr16(0x900C, 0);
    uint16_t a0 = 0xC9E0, place = 0xC0;
    for (uint16_t k = 0; k <= 7; k++) {
        wr16(a0 + O_TYPE, rom16(0x045432 + 2 * k));
        wr16(a0 + 0x06, place);
        place = (uint16_t)(place + 0x60);
        a0 = (uint16_t)(a0 + 0x60);
    }
}

/* $0450A8. After +$5C frames a target drawn; turning */
void DiceWheelWait(uint16_t obj)
{
    if (count_out(obj + 0x5C)) wheel_pick(obj);
    wheel_turn();
}

/* $0450BA. Turning until the target is at the front; stopped there for a time by the
 * difficulty */
void DiceWheelSeek(uint16_t obj)
{
    wheel_turn();
    if (rd16(0x9008) != rd16(obj + 0x5E)) return;
    wr16(obj + O_STATE, 4);
    wr16(0x9004, 0);
    wr16(obj + 0x5C, by_difficulty(0x0450E8));
}

/* $0450F8. Cleared: sound $BE every 8 frames, puffs, faster each 8 frames; at $28 the room
 * ends */
void DiceWheelSpin(uint16_t obj)
{
    SoundBEEvery8();
    wheel_puff();
    if (!(rd16(v_gameFrames) & 7)) add16(0x9004, 1);
    wheel_turn();
    if (bit8(0x9004, 7)) return;
    if (below16(0x9004, 0x28)) return;
    DiceRoomEnd(obj);
}

/* $045010. Type $2D8: the room ended if its player left; the wheel cleared; else its step */
void DiceWheel(uint16_t obj)
{
    if (room_player_gone(obj)) return;
    if (wheel_cleared(obj)) return;
    dispatch(0x045062, obj);
}

/* ---- the wheel's cars: shown and hit only between $70 and $1CF of the turn (at the front,
 * $AC-$193, they act) ---- */

/* $045868. Spent: sound $BE, a burst ($8C), 710 points; 1 then */
static int car_dead(uint16_t obj)
{
    if (!bit8(obj + O_VITALITY, 7)) return 0;
    QueueSound(0xBE);
    wr16(obj + O_TYPE, 0x8C);
    wr16(obj + 0x48, 0x60);
    wr16(obj + 0x4A, 8);
    wr16(obj + 0x4C, 0x10);
    wr16(obj + O_STATE, 0);
    wr16(obj + O_FLAGS, 0);
    wr16(obj + O_VITALITY, 0);
    wr8(obj + 0x21, 0);
    AddScore(obj, 0x710);
    return 1;
}

/* $0458B2. Struck: the sines to it ($80C2 $20) */
static void car_struck(uint16_t obj)
{
    if (!bit8(obj + 0x22, 5)) return;
    wr16(0x80C2, 0x20);
    wr16(0x80D6, obj);
}

/* $0458C8. A car's set-up: its box, $17C of vitality, at y $B8 */
static void car_setup(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    wr16(obj + 0x08, 0x0F00);
    wr16(obj + 0x0A, 0xF0F0);
    wr16(obj + O_VITALITY, 0x17C);
    wr32(obj + 0x28, 0xF40CF40C);
    wr16(obj + O_Y, 0xB8);
    wr8(obj + 0x20, 0xA0);
}

/* $045902 (loc_45902): its delay by the difficulty, + `extra` */
static void car_delay_plus(uint16_t obj, uint16_t extra)
{
    wr16(obj + 0x48, by_difficulty(0x045918));
    add16(obj + 0x48, extra);
}

/* $0458F4. Its delay by the difficulty, up to $1F more at random */
static void car_delay(uint16_t obj)
{
    Random();
    car_delay_plus(obj, rd16(v_randomSeed) & 0x1F);
}

/* $045928. Its x from its place on the wheel: out of view ($70-$1CF) hidden without
 * collisions; else shown; 0 (Z set) only at the front ($AC-$193) */
static int car_place(uint16_t obj)
{
    uint16_t at = (uint16_t)(rd16(0x9008) + rd16(obj + 0x06));
    if (!lt16(at, 0x300)) at = (uint16_t)(at - 0x300);
    if (!lt16(at, 0x1D0) || lt16(at, 0x70)) {
        wr16(obj + O_FLAGS, 0);
        wr8(obj + 0x21, 0);
        return 1;
    }
    wr16(obj + O_X, at);
    wr16(obj + O_FLAGS, 0x8000);
    wr8(obj + 0x21, 0x20);
    if (!lt16(at, 0x194) || lt16(at, 0xAC)) return 1;
    return 0;
}

/* ---- type $2DC: the gunner car: at the front, blinking while its delay runs, then soldiers
 * ($1A0) out of it, three from the second difficulty on ---- */

/* $0454AA. Its set-up, its delay */
void WheelGunnerStart(uint16_t obj)
{
    car_setup(obj);
    wr16(obj + 0x0E, 0x312);
    car_delay(obj);
}

/* $0454B8. At the front its delay runs (blinking); then firing */
void WheelGunnerWait(uint16_t obj)
{
    if (!car_place(obj) && count_out(obj + 0x48)) {
        add16(obj + O_STATE, 4);
        wr16(obj + 0x4A, 0x40);
        wr16(obj + 0x0E, 0x312);
        return;
    }
    if (bit8(v_gameFrames + 1, 2)) wr16(obj + 0x0E, 0x312);
    else wr16(obj + 0x0E, 0x322);
}

/* $0454EC. Firing for $40 frames: soldiers at $20 left; out of the front: waiting */
void WheelGunnerFire(uint16_t obj)
{
    uint16_t slot;
    if (car_place(obj) || count_out(obj + 0x4A)) {
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) - 4));
        car_delay(obj);
        return;
    }
    if (rd16(obj + 0x4A) != 0x20) return;
    SpawnAt(obj, 0x1A0, 0xC, 1, 0x20, &slot);
    if (!rd16(v_difficulty)) return;
    SpawnAt(obj, 0x1A0, 0xA, 9, 0x10, &slot);
    SpawnAt(obj, 0x1A0, 0xA, 0xFFF9, 0x30, &slot);
}

/* $045488. Type $2DC: spent or struck as the cars; else its step */
void WheelGunner(uint16_t obj)
{
    if (car_dead(obj)) return;
    car_struck(obj);
    dispatch(0x04549E, obj);
}

/* ---- type $2E8: the dropper car: at the front after its delay, a drop ($310) at $DE80 when
 * that is free ---- */

/* $04556E. Its set-up, its delay */
void WheelDropperStart(uint16_t obj)
{
    car_setup(obj);
    wr16(obj + 0x0E, 0x362);
    car_delay(obj);
}

/* $04557C. Its delay run out: at the front, a drop at $DE80 */
void WheelDropperRun(uint16_t obj)
{
    if (!count_out(obj + 0x48)) {
        car_place(obj);
        return;
    }
    car_delay(obj);
    if (car_place(obj)) return;
    if (rd16(0xDE80)) return;
    uint16_t a0 = 0xDE80;
    wr16(a0 + O_TYPE, 0x310);
    wr16(a0 + O_STATE, 0);
    wr16(a0 + O_X, rd16(obj + O_X));
    wr16(a0 + O_Y, (uint16_t)(rd16(obj + O_Y) + 8));
}

/* $045550. Type $2E8: spent or struck as the cars; else its step */
void WheelDropper(uint16_t obj)
{
    if (car_dead(obj)) return;
    car_struck(obj);
    dispatch(0x045566, obj);
}

/* ---- type $2E4: the shooter car: turning toward its player (from the second difficulty),
 * firing flames ($2F8) every second frame, then a ball ($10) its way ---- */

/* loc_456BA. Its frame and way (+$56: 0 down, 2 right, 4 left) by its player's side */
static void shooter_face(uint16_t obj)
{
    if (rd16(v_difficulty)) {
        uint16_t p = rd16(0x80D8);
        uint16_t d = (uint16_t)(rd16(p + O_X) - rd16(obj + O_X));
        if (neg16(d)) {
            if (lt16(d, 0xFFD0)) {
                wr16(obj + 0x0E, 0x342);
                wr16(obj + 0x56, 4);
                return;
            }
        } else if (!lt16(d, 0x30)) {
            wr16(obj + 0x0E, 0xB42);
            wr16(obj + 0x56, 2);
            return;
        }
    }
    wr16(obj + 0x0E, 0x332);
    wr16(obj + 0x56, 0);
}

/* loc_45696: back to waiting */
static void shooter_rest(uint16_t obj)
{
    wr16(obj + O_STATE, 4);
    car_delay(obj);
}

/* $0455E2. Its set-up, its delay */
void WheelShooterStart(uint16_t obj)
{
    car_setup(obj);
    wr16(obj + 0x0E, 0x332);
    car_delay(obj);
}

/* $0455F0. Waiting, facing its player; then flames */
void WheelShooterWait(uint16_t obj)
{
    car_place(obj);
    if (count_out(obj + 0x48)) {
        add16(obj + O_STATE, 4);
        wr16(obj + 0x48, 0x30);
    }
    shooter_face(obj);
}

/* $04560A. Flames ($2F8) on even frames for $30 frames, then the ball */
void WheelShooterFlame(uint16_t obj)
{
    if (car_place(obj)) {
        shooter_rest(obj);
        return;
    }
    if (count_out(obj + 0x48)) {
        add16(obj + O_STATE, 4);
        wr16(obj + 0x48, 0x20);
        return;
    }
    if (bit8(v_gameFrames + 1, 0)) return;
    uint16_t a0, t;
    if (!SlotFreeHigh(&a0, &t)) return;
    wr16(a0 + O_TYPE, 0x2F8);
    uint16_t w = rd16(obj + 0x56);
    wr16(a0 + 0x48, (uint16_t)(rd16(obj + O_X) + rom16(0x045674 + (uint32_t)(int16_t)w)));
    wr16(a0 + 0x4A, (uint16_t)(rd16(obj + O_Y) + rom16(0x04567A + (uint32_t)(int16_t)w)));
    wr16(a0 + 0x52, 1);
    wr16(a0 + 0x54, 1);
}

/* $045680. Shown: under $10 frames left, a ball ($10, colliding) each frame its way (speed
 * << 6, from where it is); at $10 sound $B9; out: back to waiting (loc_45C94 the ball) */
void WheelShooterBall(uint16_t obj)
{
    car_place(obj);
    if (!bit8(obj + O_FLAGS, 7) || count_out(obj + 0x48)) {
        shooter_rest(obj);
        return;
    }
    if (below16(obj + 0x48, 0x10)) {
        uint16_t a0, t;
        if (!SlotFreeHigh(&a0, &t)) return;
        wr16(a0 + O_TYPE, 0x10);
        wr16(a0 + O_FLAGS, 0x8E00);
        wr16(a0 + 0x0E, 0x64B9);
        wr16(a0 + 0x08, 0x0A00);
        wr16(a0 + 0x0A, 0xF4F4);
        wr32(a0 + 0x2C, 0xFC04FC04);
        wr8(a0 + 0x21, 0x10);
        wr16(a0 + 0x26, 4);
        uint16_t a = rom16(0x045D12 + (uint32_t)(int16_t)rd16(obj + 0x56));
        uint32_t vy = (uint32_t)(int32_t)sine(a) << 6, vx = (uint32_t)(int32_t)cosine(a) << 6;
        wr32(a0 + 0x1C, vy);
        wr32(a0 + 0x18, vx);
        wr32(a0 + O_Y, vy + rd32(obj + O_Y));
        wr32(a0 + O_X, vx + rd32(obj + O_X));
        return;
    }
    if (rd16(obj + 0x48) != 0x10) return;
    QueueSound(0xB9);
}

/* $0455BC. Type $2E4: spent or struck as the cars; else its step */
void WheelShooter(uint16_t obj)
{
    if (car_dead(obj)) return;
    car_struck(obj);
    dispatch(0x0455D2, obj);
}

/* ---- type $2E0: the lever car: struck at the front, the floor's band set moving away from
 * its player ($900C -1 or 3); removed once the wheel is cleared ---- */

/* $04573E. Its set-up */
void WheelLeverStart(uint16_t obj)
{
    car_setup(obj);
    wr16(obj + 0x0E, 0x300);
    wr16(obj + 0x08, 0x0A00);
    wr16(obj + 0x0A, 0xF4F4);
}

/* $045756. Struck while the band is still: the band set moving away from its player */
void WheelLeverWait(uint16_t obj)
{
    if (!bit8(0x900D, 0)) {
        if (!bit8(obj + 0x22, 5)) {
            wr16(obj + 0x0E, 0x300);
            return;
        }
        uint16_t p = rd16(0x80D8);
        if (below16(p + O_X, 0x120)) wr16(0x900C, 0xFFFF);
        else wr16(0x900C, 3);
    }
    wr16(obj + 0x0E, 0x309);
}

/* $04570C. Placed (its result unused); shown only: removed once the wheel ($C984) is past
 * state $C */
void WheelLever(uint16_t obj)
{
    car_place(obj);
    if (!bit8(obj + O_FLAGS, 7)) return;
    if (!below16(0xC984, 0x0C)) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    dispatch(0x045736, obj);
}

/* ---- type $2EC: the mortar car: at the front after its delay, recoiling ($18 frames, frames
 * from $45858), a bomb ($2F0) at the sixth, sound $CB ---- */

/* $0457D0. Its delay by the difficulty */
static void mortar_delay(uint16_t obj)
{
    car_delay_plus(obj, 0);
}

/* $0457BA. Its set-up, its delay */
void WheelMortarStart(uint16_t obj)
{
    car_setup(obj);
    wr16(obj + 0x0E, 0x372);
    wr16(obj + 0x08, 0x0A00);
    wr16(obj + 0x0A, 0xF4F4);
    mortar_delay(obj);
}

/* $0457D6. Waiting; then recoiling */
void WheelMortarWait(uint16_t obj)
{
    car_place(obj);
    if (!count_out(obj + 0x48)) return;
    add16(obj + O_STATE, 4);
    wr16(obj + 0x48, 0x18);
}

/* $0457EE. Recoiling; at the sixth frame a bomb */
void WheelMortarFire(uint16_t obj)
{
    if (count_out(obj + 0x48)) {
        wr16(obj + O_STATE, 4);
        wr16(obj + 0x0E, 0x372);
        mortar_delay(obj);
        car_place(obj);
        return;
    }
    wr16(obj + 0x0E, rom16(0x045858 + ((rd16(obj + 0x48) & 0x1C) >> 1)));
    if (car_place(obj)) return;
    if (rd16(obj + 0x48) != 0x0C) return;
    uint16_t a0, t;
    if (!SlotFreeHigh(&a0, &t)) return;
    wr16(a0 + O_TYPE, 0x2F0);
    wr16(a0 + O_X, rd16(obj + O_X));
    wr16(a0 + O_Y, rd16(obj + O_Y));
    QueueSound(0xCB);
}

/* $045798. Type $2EC: spent or struck as the cars; else its step */
void WheelMortar(uint16_t obj)
{
    if (car_dead(obj)) return;
    car_struck(obj);
    dispatch(0x0457AE, obj);
}

/* ---- type $2F0: the mortar's bomb: falling (gravity $2000); shot: a puff ($100, $56450)
 * bouncing up; past y $158: removed; on the floor (y $140 on, MapAttribute): a blast ($2F4)
 * and the floor's band redrawn broken there (its tiles by the walls beside it) ---- */

/* $045994. Type $2F0: as the section says */
void WheelBomb(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        add16(obj + O_STATE, 4);
        wr16(obj + O_VITALITY, 8);
        wr16(obj + O_FLAGS, 0x8400);
        wr16(obj + 0x0E, 0x65BC);
        wr16(obj + 0x08, 0x0500);
        wr16(obj + 0x0A, 0xF8F8);
        wr8(obj + 0x21, 0x20);
        wr32(obj + 0x28, 0xF808F808);
        return;
    }
    if (neg16(rd16(obj + O_VITALITY))) {
        wr16(obj + O_TYPE, 0x100);
        wr32(obj + 0x48, 0x056450);
        wr16(obj + O_STATE, 0);
        wr32(obj + 0x1C, 0xFFFE8000);
        return;
    }
    add32(obj + 0x1C, 0x2000);
    if (!below16(obj + O_Y, 0x158)) {
        wr16(obj + O_FLAGS, 0x1000);
        return;
    }
    if (below16(obj + O_Y, 0x140)) return;
    if (!MapAttribute(rd16(obj + O_X), rd16(obj + O_Y))) return;
    wr16(obj + O_TYPE, 0x2F4);
    wr16(obj + O_FLAGS, 0);
    wr8(obj + 0x21, 0);
    uint16_t at = (uint16_t)((int16_t)(uint16_t)(0x200 - (rd16(0x9006) & 0x1E0)) >> 2);
    at = (uint16_t)(at + (uint16_t)((int16_t)(uint16_t)((rd16(obj + O_X) - 0x80) & 0x1E0) >> 2));
    at = (uint16_t)(at + 0xCC00);
    uint16_t sides = 0, x = rd16(obj + O_X), y = rd16(obj + O_Y);
    if (!MapAttribute((uint16_t)(x - 0x20), y)) {
        sides |= 2;
        uint16_t far = MapAttribute((uint16_t)(rd16(obj + O_X) - 0x40), rd16(obj + O_Y));
        uint16_t v = (uint16_t)((at - 8) & 0xCC78);
        PlaneAChunk(far ? 0xFFFF4EE0 : 0xFFFF4F20, v);
    }
    x = rd16(obj + O_X);
    y = rd16(obj + O_Y);
    if (!MapAttribute((uint16_t)(x + 0x20), y)) {
        sides |= 1;
        uint16_t far = MapAttribute((uint16_t)(rd16(obj + O_X) + 0x40), rd16(obj + O_Y));
        uint16_t v = (uint16_t)((at + 8) & 0xCC78);
        PlaneAChunk(far ? 0xFFFF4F00 : 0xFFFF4F20, v);
    }
    uint16_t v = at & 0xCC78;
    if (sides == 3) PlaneAChunk(0xFFFF4F20, v);
    else if (sides == 2) PlaneAChunk(0xFFFF4F00, v);
    else if (sides == 1) PlaneAChunk(0xFFFF4EE0, v);
    else PlaneAChunk(0xFFFF47C0, v);
}

/* ---- type $310: the dropper's drop: lowered with the car at $CBC0 to y $E8 (sound $C3 at
 * $E7), then falling (gravity $2800) smoking; resting on the floor up to +$48 times; past y
 * $158 or the rests spent: removed ---- */

/* $045C50. Every 8 frames a smoke ($220, $567AC) rising */
static void drop_smoke(uint16_t obj)
{
    if (rd16(v_gameFrames) & 7) return;
    uint16_t a0, t;
    if (!SlotFreeHigh(&a0, &t)) return;
    wr16(a0 + O_TYPE, 0x220);
    wr32(a0 + 0x48, 0x0567AC);
    wr16(a0 + O_X, rd16(obj + O_X));
    wr16(a0 + O_Y, rd16(obj + O_Y));
    wr32(a0 + 0x1C, 0xFFFE8000);
    wr16(a0 + O_FLAGS, 0x8400);
    wr16(a0 + 0x4E, 0x20);
}

/* $045B7A. Its set-up */
void WheelDropStart(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0x8400);
    wr16(obj + 0x0E, 0x6572);
    wr16(obj + 0x08, 0x0A00);
    wr16(obj + 0x0A, 0xF4F4);
    wr8(obj + 0x21, 0x10);
    wr32(obj + 0x2C, 0xFC04FC04);
    wr16(obj + 0x26, 2);
    wr8(obj + 0x20, 0xBC);
    wr16(obj + 0x48, 0x40);
}

/* $045BB8. Lowered, then falling smoking, resting on the floor */
void WheelDropFall(uint16_t obj)
{
    if (below16(obj + O_Y, 0xE8)) {
        if (rd16(obj + O_Y) == 0xE7) QueueSound(0xC3);
        wr16(obj + O_X, rd16(0xCBC0 + O_X));
        if (!bit8(v_gameFrames + 1, 0)) add16(obj + O_Y, 1);
    } else {
        drop_smoke(obj);
        add32(obj + 0x1C, 0x2800);
        if (!below16(obj + O_Y, 0x158)) {
            wr16(obj + O_FLAGS, 0x1000);
            return;
        }
        if (MapAttribute(rd16(obj + O_X), (uint16_t)(rd16(obj + O_Y) + 0xC))) {
            if (count_out(obj + 0x48)) {
                wr16(obj + O_FLAGS, 0x1000);
                return;
            }
            wr32(obj + 0x1C, 0);
        }
    }
    if (bit8(v_gameFrames + 1, 1)) wr16(obj + 0x0E, 0x657B);
    else wr16(obj + 0x0E, 0x6572);
}

/* $045B68. Type $310: its step */
void WheelDrop(uint16_t obj)
{
    dispatch(0x045B72, obj);
}

/* ======================================================================================== */
/* ---- type $4C8: a piece wobbling round the object in +$5C (radius +$5E, its three angles
 * +$56, +$58, +$5A stepping 8, 4, 2 a frame) ---- */

/* $0468BA. Type $4C8: placed round its centre object as the section says */
void OrbitWobble(uint16_t obj)
{
    uint16_t centre = rd16(obj + 0x5C);
    if (!centre) return;
    uint16_t a = rd16(obj + 0x56) & 0x1FE;
    add16(obj + 0x56, 8);
    int16_t r = (int16_t)rd16(obj + 0x5E);
    uint32_t d1 = (uint32_t)((int32_t)sine(a) * r) << 2;
    uint32_t d2 = (uint32_t)((int32_t)cosine(a) * r) << 2;
    uint16_t b = rd16(obj + 0x58) & 0x1FE;
    add16(obj + 0x58, 4);
    d2 = (uint32_t)((int32_t)(int16_t)(d2 >> 16) * cosine(b)) << 2;
    uint16_t c = rd16(obj + 0x5A) & 0x1FE;
    add16(obj + 0x5A, 2);
    d1 = (uint32_t)((int32_t)(int16_t)(d1 >> 16) * sine(c)) << 2;
    wr32(obj + O_Y, d1 + rd32(centre + O_Y));
    wr32(obj + O_X, d2 + rd32(centre + O_X));
}

/* ======================================================================================== */
/* ---- type $32C: the dice palace's maw: a head of 16 parts (from $C9E0; each at its own
 * angles +$54 / +$56 round it, turned by the head's +$54 / +$56), its jaws ($CDF4 the lower
 * jaw's y, the upper's mirrored round $40), two eyes ($CFE0 / $D040) following its turn
 * (+$5C / +$5E eased to it); it bobs (+$5A's step), sways or follows its player (+$4E's), opens
 * and snaps its jaws (+$4C's), now and then spits a ball ($33C). Its colours ($E318 ...) flash
 * when struck; $1F7 of vitality: then flames from its mouth and the room ends ---- */

/* $046CC6. The 18 parts: plain, their frames and boxes from a table by a list of offsets */
static void maw_parts(void)
{
    uint16_t a0 = 0xC9E0;
    for (uint16_t k = 0; k <= 0x11; k++) {
        wr16(a0 + O_TYPE, 0x10);
        wr16(a0 + O_FLAGS, 0x8000);
        wr8(a0 + 0x20, 0x10);
        uint16_t i = rom8(0x046D16 + k);
        wr16(a0 + 0x0E, rom16(0x046D28 + i));
        wr16(a0 + 0x08, rom16(0x046D28 + i + 2));
        wr16(a0 + 0x0A, rom16(0x046D28 + i + 4));
        a0 = (uint16_t)(a0 + 0x60);
    }
}

/* $046D46. The 16 parts' angles from a list of words (high byte +$54, low +$56, as indices) */
static void maw_angles(void)
{
    uint16_t a0 = 0xC9E0;
    for (uint16_t k = 0; k <= 0xF; k++) {
        uint16_t w = rom16(0x046D76 + 2 * k);
        wr16(a0 + 0x54, (uint16_t)(((int16_t)w >> 7) & 0x1FE));
        wr16(a0 + 0x56, (uint16_t)((uint16_t)(w << 1) & 0x1FE));
        a0 = (uint16_t)(a0 + 0x60);
    }
}

/* $046E9E. An eye's angle `a` eased by 4 toward its target (`diff` = target - a, whose sign
 * the 68k's caller left in N): unmoved within 4, the other way round past $100 */
static uint16_t maw_ease(uint16_t diff, uint16_t a)
{
    if (neg16(diff)) {
        if (!lt16(diff, 0xFFFC)) return a;
        if (lt16(diff, 0xFF00)) return (uint16_t)((a + 4) & 0x1FE);
        return (uint16_t)((a - 4) & 0x1FE);
    }
    if (lt16(diff, 4)) return a;
    if (!lt16(diff, 0x100)) return (uint16_t)((a - 4) & 0x1FE);
    return (uint16_t)((a + 4) & 0x1FE);
}

/* loc_46D96. The 16 parts placed round it; the eyes' angles eased to the head's (to 0 while
 * +$5B bit 4 is set) and the eyes placed $30 out from the parts at $CA40 / $CAA0 */
static void maw_place(uint16_t obj)
{
    uint16_t a0 = 0xC9E0, y = rd16(obj + O_Y), x = rd16(obj + O_X);
    for (int k = 0; k <= 0xF; k++) {
        int32_t p = (int32_t)sine((uint16_t)((rd16(obj + 0x54) + rd16(a0 + 0x54)) & 0x1FE))
                    * cosine(rd16(a0 + 0x56));
        wr16(a0 + O_Y, (uint16_t)((uint16_t)((uint32_t)(p >> 6) >> 16) + y));
        p = (int32_t)sine((uint16_t)((rd16(obj + 0x56) + rd16(a0 + 0x56)) & 0x1FE))
            * cosine(rd16(a0 + 0x54));
        wr16(a0 + O_X, (uint16_t)((uint16_t)((uint32_t)(p >> 6) >> 16) + x));
        a0 = (uint16_t)(a0 + 0x60);
    }
    uint16_t e;
    if (!bit8(obj + 0x5B, 4)) {
        e = rd16(obj + 0x5C);
        wr16(obj + 0x5C, maw_ease((uint16_t)(rd16(obj + 0x54) - e), e));
        e = rd16(obj + 0x5E);
        wr16(obj + 0x5E, maw_ease((uint16_t)(rd16(obj + 0x56) - e), e));
    } else {
        e = rd16(obj + 0x5C);
        wr16(obj + 0x5C, maw_ease((uint16_t)(0 - e), e));
        e = rd16(obj + 0x5E);
        wr16(obj + 0x5E, maw_ease((uint16_t)(0 - e), e));
    }
    uint16_t dy = (uint16_t)((uint32_t)((int32_t)sine(rd16(obj + 0x5C)) * 0x30) >> 16);
    uint16_t dx = (uint16_t)((uint32_t)((int32_t)sine(rd16(obj + 0x5E)) * 0x30) >> 16);
    wr16(0xCFF4, (uint16_t)(dy + rd16(0xCA54)));
    wr16(0xCFF0, (uint16_t)(dx + rd16(0xCA50)));
    wr16(0xD054, (uint16_t)(dy + rd16(0xCAB4)));
    wr16(0xD050, (uint16_t)(dx + rd16(0xCAB0)));
}

/* $046ED2. Its bob: a step of the table at $46EE2 by +$5A's bits 2-3 */
static void maw_bob(uint16_t obj)
{
    call_object(rom32(0x046EE2 + (rd16(obj + 0x5A) & 0xC)), obj);
}

/* $046F64. Its head: a step of the table at $46F6E by +$4E */
static void maw_head(uint16_t obj)
{
    call_object(rom32(0x046F6E + (uint32_t)(int16_t)rd16(obj + 0x4E)), obj);
}

/* $0471E4. Its jaw: a step of the table at $471EE by +$4C */
static void maw_jaw_step(uint16_t obj)
{
    call_object(rom32(0x0471EE + (uint32_t)(int16_t)rd16(obj + 0x4C)), obj);
}

/* $047196. The jaw stepped; the lower jaw's five parts at $CDF4, the upper's seven mirrored
 * ((-($CDF4 - $40) >> 1) + $40, even) */
static void maw_jaw(uint16_t obj)
{
    maw_jaw_step(obj);
    uint16_t v = rd16(0xCDF4);
    wr16(0xCE54, v);
    wr16(0xCEB4, v);
    wr16(0xCF14, v);
    wr16(0xCF74, v);
    wr16(0xCFD4, v);
    v = (uint16_t)(((int16_t)(uint16_t)(0 - (uint16_t)(v - 0x40)) >> 1) + 0x40);
    v &= 0xFFFE;
    wr16(0xCB54, v);
    wr16(0xCBB4, v);
    wr16(0xCC14, v);
    wr16(0xCC74, v);
    wr16(0xCCD4, v);
    wr16(0xCD34, v);
    wr16(0xCD94, v);
}

/* $047178. Drifting toward the middle ($120) */
static void maw_drift(uint16_t obj)
{
    if (below16(obj + O_X, 0x120)) wr32(obj + 0x18, 0x8000);
    else wr32(obj + 0x18, 0xFFFF8000);
}

/* $0472C6. Dying: sound $AC every 8 frames, flames ($1A4) from its mouth ($CE10) on even
 * frames */
static void maw_flames(void)
{
    if (!(rd16(v_gameFrames) & 7)) QueueSound(0xAC);
    if (bit8(v_gameFrames + 1, 0)) return;
    uint16_t a0, t;
    if (!SlotFreeHigh(&a0, &t)) return;
    wr16(a0 + O_TYPE, 0x1A4);
    wr16(a0 + O_Y, (uint16_t)(rd16(0xCE14) + 4));
    wr16(a0 + O_X, (uint16_t)((rd16(v_randomSeed) & 0x1F) + 0xFFF1 + rd16(0xCE10)));
}

/* $047316. A ball ($33C) spat the way it faces */
static void maw_spit(uint16_t obj)
{
    uint16_t a0, t;
    if (!SlotFreeHigh(&a0, &t)) return;
    wr16(a0 + O_TYPE, 0x33C);
    wr16(a0 + O_X, rd16(obj + O_X));
    wr16(a0 + O_Y, rd16(obj + O_Y));
    if (below16(obj + 0x56, 0x100)) wr32(a0 + 0x18, 0x8000);
    else wr32(a0 + 0x18, 0xFFFF8000);
}

/* $046AE4. Set up at $120,$100 with its parts; the sines taken */
void DiceMawStart(uint16_t obj)
{
    wr16(0x80C2, 0x10);
    wr16(0x80D6, obj);
    add16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0x0C00);
    wr8(obj + 0x21, 0x20);
    wr32(obj + 0x28, 0xD030D030);
    wr16(obj + O_VITALITY, 0x51D);
    wr16(obj + O_X, 0x120);
    wr16(obj + O_Y, 0x100);
    wr16(obj + 0x4A, 0x200);
    wr16(obj + 0x4C, 0x10);
    wr16(obj + 0x4E, 0);
    wr16(obj + 0x5A, 0);
    wr16(obj + 0x5C, 0);
    wr16(obj + 0x5E, 0);
    maw_parts();
    maw_angles();
    wr8(0xD000, 0);
    wr8(0xD060, 0);
}

/* $046B52. Swaying for +$4A frames (the eyes looking ahead the last $68); then following its
 * player for $180-$1BF frames, the bob's fourth step */
void DiceMawSway(uint16_t obj)
{
    if (below16(obj + 0x4A, 0x68)) set8(obj + 0x5B, 4);
    if (count_out(obj + 0x4A)) {
        add16(obj + O_STATE, 4);
        wr16(obj + 0x4A, (uint16_t)((rd16(v_gameFrames) & 0x3F) + 0x180));
        wr16(obj + 0x4C, 0);
        wr16(obj + 0x4E, 4);
        wr16(obj + 0x5A, (uint16_t)((rd16(obj + 0x5A) & 0xFFF0) | 0xC));
    }
    maw_bob(obj);
    maw_head(obj);
    maw_jaw(obj);
    maw_place(obj);
}

/* $046BA6. Following its player (the eyes ahead between $48 and $7F, and $160 on); then
 * straightening, its jaw wide */
void DiceMawFollow(uint16_t obj)
{
    if (!below16(obj + 0x4A, 0x80)) {
        if (below16(obj + 0x4A, 0x160)) clr8(obj + 0x5B, 4);
    } else if (below16(obj + 0x4A, 0x48)) {
        clr8(obj + 0x5B, 4);
    } else {
        set8(obj + 0x5B, 4);
    }
    if (count_out(obj + 0x4A)) {
        add16(obj + O_STATE, 4);
        wr16(obj + 0x4C, 0xC);
        wr16(obj + 0x4E, 8);
    }
    maw_bob(obj);
    maw_head(obj);
    maw_jaw(obj);
    maw_place(obj);
}

/* $046BFC. Rising 2 a frame to y $E8 */
void DiceMawRise(uint16_t obj)
{
    wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 2));
    if (below16(obj + O_Y, 0xE8)) {
        add16(obj + O_STATE, 4);
        wr16(obj + 0x4A, 0x20);
        wr32(obj + 0x1C, 0);
    }
    maw_head(obj);
    maw_jaw(obj);
    maw_place(obj);
}

/* $046C28. A pause of +$4A frames; then the jaw opened wide, the eyes ahead */
void DiceMawPause(uint16_t obj)
{
    if (count_out(obj + 0x4A)) {
        add16(obj + O_STATE, 4);
        wr16(obj + 0x4A, 0x30);
        wr16(obj + 0x4C, 8);
        set8(obj + 0x5B, 4);
    }
    maw_head(obj);
    maw_jaw(obj);
    maw_place(obj);
}

/* $046C52. At $24 frames left a ball spat; then swaying again ($180-$1BF frames) */
void DiceMawSpit(uint16_t obj)
{
    if (rd16(obj + 0x4A) == 0x24) maw_spit(obj);
    if (count_out(obj + 0x4A)) {
        wr16(obj + O_STATE, 4);
        wr16(obj + 0x4A, (uint16_t)((rd16(v_randomSeed) & 0x3F) + 0x180));
        wr16(obj + 0x4C, 0x10);
        wr16(obj + 0x4E, 0);
        clr8(obj + 0x5B, 4);
        wr16(obj + 0x5A, rd16(obj + 0x5A) & 0xFFF0);
    }
    maw_head(obj);
    maw_jaw(obj);
    maw_place(obj);
}

/* $046CA2. Beaten: flames for +$4A frames, then the room ends */
void DiceMawBeaten(uint16_t obj)
{
    if (count_out(obj + 0x4A)) {
        DiceRoomEnd(obj);
        return;
    }
    maw_flames();
    maw_bob(obj);
    maw_head(obj);
    maw_jaw(obj);
    maw_place(obj);
}

/* $046EF2. The bob: up to y $E0, then down */
void DiceMawBobUp(uint16_t obj)
{
    wr32(obj + 0x1C, 0xFFFFE000);
    if (!below16(obj + O_Y, 0xE0)) return;
    wr16(obj + 0x5A, (uint16_t)((rd16(obj + 0x5A) & 0xFFF0) | 4));
}

/* $046F12. Down to y $F4, then up */
void DiceMawBobDown(uint16_t obj)
{
    wr32(obj + 0x1C, 0x2000);
    if (below16(obj + O_Y, 0xF4)) return;
    wr16(obj + 0x5A, rd16(obj + 0x5A) & 0xFFF0);
}

/* $046F2C. Rising to y $E0 and stopping */
void DiceMawBobRise(uint16_t obj)
{
    wr32(obj + 0x1C, 0xFFFF8000);
    if (below16(obj + O_Y, 0xE0)) wr32(obj + 0x1C, 0);
}

/* $046F48. Sinking to y $100 and stopping */
void DiceMawBobSink(uint16_t obj)
{
    wr32(obj + 0x1C, 0x4000);
    if (!below16(obj + O_Y, 0x100)) wr32(obj + 0x1C, 0);
}

/* $046F7A. The head swaying at random: +$54 turned 2 a frame (+$50 the time left each way, a
 * new random time at the ends of $20-$1DF), +$56 by 4 (+$52, $48-$1B7); drifting to the
 * middle */
void DiceMawHeadSway(uint16_t obj)
{
    maw_drift(obj);
    if (!neg16(rd16(obj + 0x50))) {
        if (count_out(obj + 0x50) || (!below16(obj + 0x54, 0x100) && below16(obj + 0x54, 0x1E0)))
            wr16(obj + 0x50, (uint16_t)((rd16(v_randomSeed) & 0xF) + 0xFFEC));
        wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) - 2));
    } else {
        add16(obj + 0x50, 1);
        if (!neg16(rd16(obj + 0x50)) || (below16(obj + 0x54, 0x100) && !below16(obj + 0x54, 0x20)))
            wr16(obj + 0x50, (uint16_t)((rd16(v_randomSeed) & 0xF) + 5));
        add16(obj + 0x54, 2);
    }
    if (!neg16(rd16(obj + 0x52))) {
        if (count_out(obj + 0x52) || (!below16(obj + 0x56, 0x100) && below16(obj + 0x56, 0x1B8)))
            wr16(obj + 0x52, (uint16_t)((rd16(v_randomSeed) & 0x1F) + 0xFFD0));
        wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 4));
    } else {
        add16(obj + 0x52, 1);
        if (!neg16(rd16(obj + 0x52)) || (below16(obj + 0x56, 0x100) && !below16(obj + 0x56, 0x48)))
            wr16(obj + 0x52, (uint16_t)((rd16(v_randomSeed) & 0x1F) + 0x11));
        add16(obj + 0x56, 4);
    }
    wr16(obj + 0x54, rd16(obj + 0x54) & 0x1FE);
    wr16(obj + 0x56, rd16(obj + 0x56) & 0x1FE);
}

/* $047064. The head nodding (+$54 by 4 between $50 and $160) and turned toward its player
 * (by the body's x, $C9F0), moving that way within $E0-$15F */
void DiceMawHeadFollow(uint16_t obj)
{
    if (!neg16(rd16(obj + 0x50))) {
        if (!below16(obj + 0x54, 0x160) || below16(obj + 0x54, 0x50)) wr16(obj + 0x50, 0xFFFF);
        wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) - 4));
    } else {
        if (below16(obj + 0x54, 0x160) && !below16(obj + 0x54, 0x60)) wr16(obj + 0x50, 0);
        add16(obj + 0x54, 4);
    }
    wr16(obj + 0x54, rd16(obj + 0x54) & 0x1FE);
    uint16_t p = rd16(0x80D8);
    int turn_down;
    if (lt16(rd16(p + O_X), rd16(0xC9F0))) {
        wr32(obj + 0x18, 0xFFFF2000);
        if (below16(obj + O_X, 0xE0)) wr32(obj + 0x18, 0);
        turn_down = !below16(obj + 0x56, 0x1C8) || below16(obj + 0x56, 0x100);
    } else {
        wr32(obj + 0x18, 0xE000);
        if (!below16(obj + O_X, 0x160)) wr32(obj + 0x18, 0);
        turn_down = !below16(obj + 0x56, 0x38) && below16(obj + 0x56, 0x100);
    }
    if (turn_down) wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 2));
    else add16(obj + 0x56, 2);
    wr16(obj + 0x56, rd16(obj + 0x56) & 0x1FE);
}

/* $047132. The head straightened (+$54 by 8 to 0), still */
void DiceMawHeadStraight(uint16_t obj)
{
    wr32(obj + 0x18, 0);
    if (!below16(obj + 0x54, 0x100)) {
        if (!below16(obj + 0x54, 0x1F8)) { wr16(obj + 0x54, 0); return; }
        add16(obj + 0x54, 8);
        wr16(obj + 0x54, rd16(obj + 0x54) & 0x1FE);
        return;
    }
    if (below16(obj + 0x54, 8)) { wr16(obj + 0x54, 0); return; }
    wr16(obj + 0x54, (uint16_t)(rd16(obj + 0x54) - 8));
    wr16(obj + 0x54, rd16(obj + 0x54) & 0x1FE);
}

/* $04720A. The jaw opening 2 a frame to $50 */
void DiceMawJawOpen(uint16_t obj)
{
    add16(0xCDF4, 2);
    if (!below16(0xCDF4, 0x50)) add16(obj + 0x4C, 4);
}

/* $04721E. Closing 4 a frame below $40, then opening again */
void DiceMawJawClose(uint16_t obj)
{
    wr16(0xCDF4, (uint16_t)(rd16(0xCDF4) - 4));
    if (below16(0xCDF4, 0x40)) wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) - 4));
}

/* $047232. Opening wide (6 a frame to $68) */
void DiceMawJawWide(uint16_t obj)
{
    (void)obj;
    if (below16(0xCDF4, 0x68)) { add16(0xCDF4, 6); return; }
    wr16(0xCDF4, 0x68);
}

/* $04724A. Shutting (6 a frame to $40) */
void DiceMawJawShut(uint16_t obj)
{
    (void)obj;
    if (!below16(0xCDF4, 0x44)) { wr16(0xCDF4, (uint16_t)(rd16(0xCDF4) - 6)); return; }
    wr16(0xCDF4, 0x40);
}

/* $047262. Opening 4 a frame: from $70 by chance, at $80 surely, snapping shut */
void DiceMawJawGape(uint16_t obj)
{
    add16(0xCDF4, 4);
    if (below16(0xCDF4, 0x80)) {
        if (below16(0xCDF4, 0x70)) return;
        if (bit8(v_randomSeed + 1, 0)) return;
    }
    add16(obj + 0x4C, 4);
}

/* $04728A. Snapping shut 8 a frame; at $40 again open, or (one in eight) held $38 frames */
void DiceMawJawSnap(uint16_t obj)
{
    wr16(0xCDF4, (uint16_t)(rd16(0xCDF4) - 8));
    if (!below16(0xCDF4, 0x40)) return;
    if (!(rd16(v_randomSeed) & 7)) {
        add16(obj + 0x4C, 4);
        wr16(obj + 0x58, 0x38);
        return;
    }
    wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) - 4));
}

/* $0472B6. Held shut for +$58 frames, then gaping again */
void DiceMawJawHold(uint16_t obj)
{
    if (count_out(obj + 0x58)) wr16(obj + 0x4C, 0x10);
}

/* $0469D4. The room ended if its player left; its vitality spent: beaten (state $18, $100
 * frames, the jaw opening, the bob's third step), 139,000 points; its colours white while
 * struck; its player drawn in front ($E bit 15) */
void DiceMaw(uint16_t obj)
{
    if (room_player_gone(obj)) return;
    if (neg16(rd16(obj + O_VITALITY))) {
        wr16(obj + O_VITALITY, 0);
        wr8(obj + 0x21, 0);
        wr16(obj + O_STATE, 0x18);
        wr16(obj + 0x4A, 0x100);
        wr16(obj + 0x4C, 8);
        wr16(obj + 0x4E, 8);
        clr8(obj + 0x5B, 4);
        wr16(obj + 0x5A, rd16(obj + 0x5A) & 0xFFF0);
        wr16(obj + 0x5A, rd16(obj + 0x5A) | 8);
        AddScore(obj, 0x13900);
    }
    if (bit8(obj + 0x22, 5)) {
        wr16(0xE318, 0xEEE);
        wr16(0xE398, 0xEEE);
        wr16(0xE31A, 0xCCC);
        wr16(0xE39A, 0xCCC);
        wr16(0xE31C, 0xAAA);
        wr16(0xE39C, 0xAAA);
    } else {
        wr16(0xE318, 0x46E);
        wr16(0xE398, 0x46E);
        wr16(0xE31A, 0xE);
        wr16(0xE39A, 0xE);
        wr16(0xE31C, 0xA);
        wr16(0xE39C, 0xA);
    }
    set8((uint16_t)(rd16(0x80D8) + 0x0E), 7);
    dispatch(0x046AC8, obj);
}

/* ---- type $33C: the maw's ball: bouncing on the floor (y $148) three times, off the room's
 * sides ($98, $1A8); struck or done: a puff ($100, $563EE), sound $CE ---- */

/* $047410. Turned back at the sides */
static void maw_ball_walls(uint16_t obj)
{
    if (!bit8(obj + 0x18, 7)) {
        if (below16(obj + O_X, 0x1A8)) return;
    } else if (!below16(obj + O_X, 0x98)) {
        return;
    }
    wr32(obj + 0x18, ~rd32(obj + 0x18));
    add32(obj + 0x18, 1);
}

/* $04734E. Type $33C: as the section says */
void MawBall(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        add16(obj + O_STATE, 4);
        wr16(obj + O_FLAGS, 0x8C00);
        wr16(obj + 0x08, 0x0500);
        wr16(obj + 0x0A, 0xF8F8);
        wr8(obj + 0x21, 0x10);
        wr32(obj + 0x2C, 0xF808F808);
        wr16(obj + 0x26, 0x19);
        wr16(obj + 0x48, 3);
    }
    maw_ball_walls(obj);
    if (!bit8(obj + 0x22, 4)) {
        if (bit8(v_gameFrames + 1, 1)) wr16(obj + 0x0E, 0x6546);
        else wr16(obj + 0x0E, 0x546);
        add32(obj + 0x1C, 0x4000);
        if (bit8(obj + 0x1C, 7)) return;
        if (below16(obj + O_Y, 0x148)) return;
        if (!count_out(obj + 0x48)) {
            wr32(obj + 0x1C, 0xFFFD8000);
            QueueSound(0xE6);
            return;
        }
    }
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x0563EE);
    wr16(obj + O_STATE, 0);
    wr8(obj + 0x21, 0);
    wr32(obj + 0x1C, 0xFFFF0000);
    QueueSound(0xCE);
}

/* ======================================================================================== */
/* ---- type $35C: the dice palace's brawler boss (at $C980; as type $4C's brawler, with the
 * shared Brawler* helpers): it drops in, walks at its player ($80D8, in a6), crouches and
 * leaps, grabs and throws, roars sparks, charges with flames and throws a fire ($360);
 * $7D9 of vitality: 840,040 points, puffs, the room ends. The brawler's inner steps
 * (loc_47F56 ...) are copied from objects48.c's ---- */

/* loc_47F56: from the start, facing right */
static void brawler_anim_start_flipped(uint16_t obj)
{
    BrawlerAnimStart(obj);
    wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x800);
}

/* loc_47F62: stepped, the way it faces */
static void brawler_anim_on(uint16_t obj)
{
    if (bit8(obj + 0x0E, 3)) BrawlerAnimOnFlipped(obj);
    else BrawlerAnim(obj);
}

/* loc_47C40: crouched before a jump (8 frames) */
static void brawler_crouch(uint16_t obj)
{
    wr8(obj + 0x50, 0);
    wr8(obj + 0x21, 0x22);
    wr8(obj + 0x23, 0x20);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x5E, 8);
    wr32(obj + 0x48, 0x047FF4);
    BrawlerAnimFacing(obj);
}

/* loc_47912: landed: standing ($10 frames) */
static void brawler_rest(uint16_t obj)
{
    wr16(obj + 0x5C, 0x10);
    wr8(obj + 0x21, 0x22);
    wr8(obj + 0x50, 0);
    wr32(obj + 0x48, 0x047FF4);
    BrawlerAnimFacing(obj);
    wr32(obj + 0x18, 0);
    wr16(obj + 0x4C, 0x7FFF);
    wr16(obj + O_STATE, obj == 0xC980 ? 0x18 : 4);
}

/* loc_47DE0: standing up (still) */
static void brawler_stand(uint16_t obj)
{
    wr8(obj + 0x21, 0x22);
    wr8(obj + 0x50, 0);
    wr32(obj + 0x48, 0x047FA2);
    BrawlerAnimBySpeed(obj);
    wr32(obj + 0x18, 0);
    wr16(obj + O_STATE, obj == 0xC980 ? 0x18 : 4);
}

/* loc_47DDA: at a wall: standing ($20 frames) */
static void brawler_stop(uint16_t obj)
{
    wr16(obj + 0x5C, 0x20);
    brawler_stand(obj);
}

/* loc_47E18: landed from a knock: lying ($20 frames) */
static void brawler_lie(uint16_t obj)
{
    wr16(obj + 0x5C, 0x20);
    wr8(obj + 0x50, 1);
    wr32(obj + 0x48, 0x04802C);
    BrawlerAnimBySpeed(obj);
    wr32(obj + 0x18, 0);
    wr16(obj + O_STATE, obj == 0xC980 ? 0x28 : 0x14);
}

/* the room's player (a6) */
static uint16_t brawler_player(void)
{
    return (uint16_t)reg_a6;
}

/* loc_47C90. A roar: sound $D4, sparks for a time by the difficulty (state $58) */
static void brawler_roar(uint16_t obj)
{
    QueueSound(0xD4);
    wr16(obj + O_STATE, 0x58);
    wr8(obj + 0x50, 1);
    wr8(obj + 0x21, 0x20);
    wr32(obj + 0x18, 0);
    wr16(obj + 0x5E, by_difficulty(0x047CD0));
    wr32(obj + 0x48, 0x047FB8);
    BrawlerAnimFacing(obj);
}

/* loc_47B66. Walking, `d` the distance ahead to its player: every $400 frames a roar; close
 * (below $12, its player low or by chance from the second difficulty) a grab (state $34);
 * below $48 a leap (state $48); else now and then (by the frame and chance) a charge (state
 * $5C; long, $40 frames, from the fifth difficulty by chance) */
static void brawler_decide(uint16_t obj, uint16_t d)
{
    uint16_t p = brawler_player();
    if (!(rd16(v_gameFrames) & 0x3FF)) {
        brawler_roar(obj);
        return;
    }
    if (rd16(v_difficulty) && bit8(v_gameFrames, 2) && !bit8(v_gameFrames, 1)) {
        if (!lt16(d, 0x12)) return;
        goto grab;
    }
    if (!below16(p + O_Y, 0x130) && lt16(d, 0x12)) goto grab;
    if (lt16(d, 0x48)) {
        wr16(obj + O_STATE, 0x48);
        brawler_crouch(obj);
        return;
    }
    if (bit8(v_gameFrames, 1)) return;
    if (bit8(v_gameFrames, 2)) {
        if (rd16(v_gameFrames) & 0x7F) return;
    } else if ((rd16(v_gameFrames) & 0xFF) && (rd8(v_randomSeed) & 0xF)) {
        return;
    }
    wr16(obj + O_STATE, 0x5C);
    wr8(obj + 0x50, 1);
    wr8(obj + 0x21, 0x20);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x48, 0x04803C);
    BrawlerAnimFacing(obj);
    if (!below16(0xA20C, 8) && !bit8(v_randomSeed + 1, 1)) {
        wr16(obj + 0x5E, 0x40);
        wr16(obj + 0x5C, 1);
        return;
    }
    wr16(obj + 0x5E, 0x18);
    wr16(obj + 0x5C, 0);
    return;
grab:
    wr16(obj + O_STATE, 0x34);
    wr8(obj + 0x50, 1);
    wr8(obj + 0x21, 0x23);
}

/* $047B16. A spark ($4C) flung up at a random angle (from $D400, nine slots) */
static void brawler_spark(uint16_t obj)
{
    uint16_t a0 = 0xD400, t;
    if (!SlotFree9(&a0, &t)) return;
    wr16(a0 + O_TYPE, 0x4C);
    wr16(a0 + O_X, rd16(obj + O_X));
    wr16(a0 + O_Y, rd16(obj + O_Y));
    uint16_t a = (uint16_t)((rd16(v_randomSeed) & 0x7E) + 0x140);
    wr32(a0 + 0x1C, (uint32_t)(int32_t)sine(a) << 4);
    wr32(a0 + 0x18, (uint32_t)(int32_t)cosine(a) << 4);
}

/* $047D8C. On even frames a flame ($2F8) where it is */
static void brawler_flame(uint16_t obj)
{
    if (bit8(v_gameFrames + 1, 0)) return;
    uint16_t a0, t;
    if (!SlotFreeHigh(&a0, &t)) return;
    wr16(a0 + O_TYPE, 0x2F8);
    wr16(a0 + 0x48, rd16(obj + O_X));
    wr16(a0 + 0x4A, rd16(obj + O_Y));
    wr16(a0 + 0x52, 2);
    wr16(a0 + 0x54, 2);
}

/* $047DBE. Facing its player */
static void brawler_face(uint16_t obj)
{
    uint16_t p = brawler_player();
    if (lt16(rd16(p + O_X), rd16(obj + O_X))) clr8(obj + 0x0E, 3);
    else set8(obj + 0x0E, 3);
}

/* $0475B6. Its set-up off the screen right */
void DiceBrawlerStart(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0x8C00);
    wr8(obj + 0x50, 8);
    wr16(obj + O_VITALITY, 0xFFFF);
    wr16(obj + O_X, 0x1E0);
    wr16(obj + O_Y, 0x100);
    wr16(obj + 0x26, 5);
    wr32(obj + 0x2C, 0xFC04F010);
    wr32(obj + 0x28, 0xFD04F40C);
}

/* $0475F0. Once the sines are free: leaping in from the right */
void DiceBrawlerWait(uint16_t obj)
{
    if (rd16(0x80C2)) return;
    add16(obj + O_STATE, 4);
    wr16(obj + 0x5E, 0x40);
    wr16(obj + O_X, 0x1B0);
    wr32(obj + 0x18, 0xFFFAC000);
    wr32(obj + 0x48, 0x048014);
    BrawlerAnimStart(obj);
}

/* $04761E. Falling in; landed: sliding at half speed */
void DiceBrawlerEnter(uint16_t obj)
{
    if (!BrawlerFall(obj)) return;
    add16(obj + O_STATE, 4);
    wr32(obj + 0x18, (uint32_t)((int32_t)rd32(obj + 0x18) >> 1));
    wr32(obj + 0x48, 0x047FF4);
    brawler_anim_start_flipped(obj);
}

/* $047642. Sliding, slowing by $2000 */
void DiceBrawlerSlide(uint16_t obj)
{
    add32(obj + 0x18, 0x2000);
    if (neg32(rd32(obj + 0x18))) return;
    add16(obj + O_STATE, 4);
    wr32(obj + 0x48, 0x047FA2);
    brawler_anim_start_flipped(obj);
}

/* $047660. The same, then still for $40 frames */
void DiceBrawlerSlow(uint16_t obj)
{
    add32(obj + 0x18, 0x2000);
    if (neg32(rd32(obj + 0x18))) return;
    add16(obj + O_STATE, 4);
    wr16(obj + 0x5E, 0x40);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x48, 0x047FA2);
    brawler_anim_start_flipped(obj);
}

/* $04768C. Posing for +$5E frames; then fighting ($7D9 of vitality), the sines taken */
void DiceBrawlerPose(uint16_t obj)
{
    if (count_out(obj + 0x5E)) {
        add16(obj + O_STATE, 4);
        wr8(obj + 0x50, 0);
        wr16(obj + O_VITALITY, 0x7D9);
        wr16(obj + 0x5C, 0xFFFF);
        wr16(0x80C2, 0x10);
        wr16(0x80D6, obj);
        wr8(obj + 0x21, 0x22);
    }
    BrawlerAnimOnFlipped(obj);
}

/* $0476BE. Standing for +$5C frames; then walking at its player */
void DiceBrawlerStand(uint16_t obj)
{
    if (!count_out(obj + 0x5C)) {
        brawler_anim_on(obj);
        return;
    }
    wr32(obj + 0x48, 0x047FCE);
    uint16_t p = brawler_player();
    if (!lt16(rd16(p + O_X), rd16(obj + O_X))) {
        add16(obj + O_STATE, 8);
        wr32(obj + 0x18, 0x22000);
        brawler_anim_start_flipped(obj);
        return;
    }
    add16(obj + O_STATE, 4);
    wr32(obj + 0x18, 0xFFFDE000);
    BrawlerAnimStart(obj);
}

/* $0476FE. Walking left (at the wall it stops) */
void DiceBrawlerWalkLeft(uint16_t obj)
{
    if (bit8(obj + 0x50, 2)) {
        brawler_stop(obj);
        return;
    }
    BrawlerAnim(obj);
    uint16_t p = brawler_player();
    brawler_decide(obj, (uint16_t)~(uint16_t)(rd16(p + O_X) - rd16(obj + O_X)));
}

/* $04771C. Walking right */
void DiceBrawlerWalkRight(uint16_t obj)
{
    if (bit8(obj + 0x50, 1)) {
        brawler_stop(obj);
        return;
    }
    BrawlerAnimOnFlipped(obj);
    uint16_t p = brawler_player();
    brawler_decide(obj, (uint16_t)(rd16(p + O_X) - rd16(obj + O_X)));
}

/* $047736. Knocked up: falling, then lying */
void DiceBrawlerKnocked(uint16_t obj)
{
    if (BrawlerFall(obj)) brawler_lie(obj);
}

/* $047740. Lying for +$5C frames; then a roar (one in four) or standing */
void DiceBrawlerLying(uint16_t obj)
{
    if (!count_out(obj + 0x5C)) return;
    if (!(rd16(v_gameFrames) & 3)) brawler_roar(obj);
    else brawler_stand(obj);
}

/* $04775A. Held: while +$23 bit 1 stays set (cleared now); then thrown */
void DiceBrawlerHeld(uint16_t obj)
{
    int was = bit8(obj + 0x23, 1);
    clr8(obj + 0x23, 1);
    if (was) return;
    add16(obj + O_STATE, 4);
    BrawlerAnimOnBySpeed(obj);
}

/* $04776E. Thrown: landing, 4 off its vitality, sound $AC, lying */
void DiceBrawlerThrown(uint16_t obj)
{
    if (!BrawlerFall(obj)) return;
    wr16(obj + O_VITALITY, (uint16_t)(rd16(obj + O_VITALITY) - 4));
    QueueSound(0xAC);
    brawler_lie(obj);
}

/* $04778A. A grab: holding what it touched (+$40, +$22 bit 0), both still, facing apart;
 * nothing: standing */
void DiceBrawlerGrab(uint16_t obj)
{
    wr16(obj + 0x5C, 0x10);
    uint16_t held = rd16(obj + 0x40);
    if (!held || !bit8(obj + 0x22, 0)) {
        brawler_stand(obj);
        return;
    }
    add16(obj + O_STATE, 4);
    wr8(obj + 0x21, 1);
    set8(held + 0x23, 1);
    wr32(held + 0x18, 0);
    wr32(held + 0x1C, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + 0x5E, 8);
    wr32(obj + 0x48, 0x047FF4);
    if (below16(obj + O_X, 0x120)) {
        set8(obj + 0x0E, 3);
        clr8(held + 0x0E, 3);
    } else {
        clr8(obj + 0x0E, 3);
        set8(held + 0x0E, 3);
    }
    BrawlerAnimFacing(obj);
}

/* $04780C. Lifting it ($C above) for +$5E frames; then jumping up $7, $8000 the way it faces */
void DiceBrawlerLift(uint16_t obj)
{
    if (count_out(obj + 0x5E)) {
        add16(obj + O_STATE, 4);
        wr32(obj + 0x1C, 0xFFF90000);
        if (!bit8(obj + 0x0E, 3)) wr32(obj + 0x18, 0x8000);
        else wr32(obj + 0x18, 0xFFFF8000);
    }
    wr8(obj + 0x21, 0x22);
    uint16_t held = rd16(obj + 0x40);
    set8(held + 0x23, 1);
    wr16(held + O_X, rd16(obj + O_X));
    wr16(held + O_Y, (uint16_t)(rd16(obj + O_Y) - 0xC));
    brawler_anim_on(obj);
}

/* $047864. Up with it ($10 above); at the top: it thrown down and away ($4.8) as a blow */
void DiceBrawlerToss(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    uint16_t held = rd16(obj + 0x40);
    if (neg32(rd32(obj + 0x1C))) {
        set8(held + 0x23, 1);
        wr16(held + O_X, rd16(obj + O_X));
        wr16(held + O_Y, (uint16_t)(rd16(obj + O_Y) - 0x10));
        brawler_anim_on(obj);
        return;
    }
    add16(obj + O_STATE, 4);
    wr16(obj + 0x5E, 8);
    wr32(obj + 0x48, 0x048024);
    BrawlerAnimFacing(obj);
    wr8(obj + 0x22, 0);
    wr8(obj + 0x23, 0);
    held = rd16(obj + 0x40);
    wr16(obj + 0x40, 0);
    clr8(held + 0x23, 1);
    set8(held + 0x23, 4);
    wr32(held + 0x1C, 0x4000);
    if (!bit8(obj + 0x0E, 3)) wr32(held + 0x18, 0xFFFB8000);
    else wr32(held + 0x18, 0x48000);
}

/* $0478E8. Coming down for +$5E frames */
void DiceBrawlerRecover(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if (!count_out(obj + 0x5E)) return;
    add16(obj + O_STATE, 4);
    wr32(obj + 0x48, 0x048034);
    BrawlerAnimFacing(obj);
}

/* $04790A. Falling; landed: standing */
void DiceBrawlerFall(uint16_t obj)
{
    if (BrawlerFall(obj)) brawler_rest(obj);
}

/* $047956. Crouched for +$5E frames; then a leap at random (up $4.4 or $6.8 less a random
 * 0-7 quarter by bit 8 of the frame, across -2 plus a random eighth), facing its player. As
 * Brawler4CCrouch, the speed's fraction takes d0's high word as the 68k's caller left it
 * (the 68k swaps d0 having set its low word only): d0 comes in */
void DiceBrawlerLeap(uint16_t obj, uint32_t d0)
{
    if (count_out(obj + 0x5E)) {
        add16(obj + O_STATE, 4);
        d0 = (uint32_t)(rd16(v_randomSeed) & 7) << 16 | d0 >> 16;
        d0 = (uint32_t)((int32_t)d0 >> 2);
        if (!bit8(v_gameFrames, 0)) d0 += 0xFFFDC000;
        d0 += 0xFFFBC000;
        wr32(obj + 0x1C, d0);
        d0 = (uint32_t)(rd8(v_randomSeed) & 0x3F) << 16;
        d0 = (uint32_t)((int32_t)d0 >> 4) - 0x20000;
        wr32(obj + 0x18, d0);
        uint16_t p = brawler_player();
        if (lt16(rd16(p + O_X), rd16(obj + O_X))) clr8(obj + 0x0E, 3);
        else set8(obj + 0x0E, 3);
    }
    brawler_anim_on(obj);
}

/* $0479C0. Leaping: near its player it attacks; on the way down, falling */
void DiceBrawlerJump(uint16_t obj)
{
    BrawlerFall(obj);
    if (BrawlerNear(obj)) return;
    if (bit8(obj + 0x1C, 7)) return;
    add16(obj + O_STATE, 4);
    wr32(obj + 0x48, 0x048034);
    BrawlerAnimFacing(obj);
}

/* $0479E8. Falling from the leap: landed, standing; else near its player, attacking */
void DiceBrawlerJumpFall(uint16_t obj)
{
    if (BrawlerFall(obj)) {
        brawler_rest(obj);
        return;
    }
    BrawlerNear(obj);
}

/* $0479F6. Attacking for +$5E frames (falling); then falling (state $44) */
void DiceBrawlerAttack(uint16_t obj)
{
    if (BrawlerFall(obj)) {
        brawler_rest(obj);
        return;
    }
    if (!count_out(obj + 0x5E)) {
        brawler_anim_on(obj);
        return;
    }
    wr16(obj + O_STATE, 0x44);
    wr8(obj + 0x21, 0x22);
    wr32(obj + 0x48, 0x048034);
    BrawlerAnimFacing(obj);
}

/* $047A22. Roaring sparks for +$5E frames */
void DiceBrawlerRoar(uint16_t obj)
{
    brawler_spark(obj);
    if (count_out(obj + 0x5E)) brawler_stop(obj);
    else brawler_anim_on(obj);
}

/* $047A32. Charging up in flames, facing its player; then a jump up $3.C, across $1.1 (the
 * way it faces when +$5C, else the other way) */
void DiceBrawlerCharge(uint16_t obj)
{
    brawler_flame(obj);
    brawler_face(obj);
    if (!count_out(obj + 0x5E)) return;
    add16(obj + O_STATE, 4);
    wr16(obj + 0x5E, 8);
    wr32(obj + 0x1C, 0xFFFC4000);
    wr32(obj + 0x48, 0x04801C);
    BrawlerAnimFacing(obj);
    int right = bit8(obj + 0x0E, 3);
    if (!rd16(obj + 0x5C)) right = !right;
    wr32(obj + 0x18, right ? 0x11000 : 0xFFFEF000);
}

/* $047A90. In the air; at the end of +$5E: a fire ($360, its kind +$5C) thrown the way it
 * faces, sound $CA; landed: standing */
void DiceBrawlerFire(uint16_t obj)
{
    if (BrawlerFall(obj)) {
        brawler_rest(obj);
        return;
    }
    wr16(obj + 0x5E, (uint16_t)(rd16(obj + 0x5E) - 1));
    if (rd16(obj + 0x5E)) {
        brawler_anim_on(obj);
        return;
    }
    uint16_t a0, t;
    if (!SlotFreeHigh(&a0, &t)) return;
    QueueSound(0xCA);
    wr16(a0 + O_TYPE, 0x360);
    wr16(a0 + O_X, rd16(obj + O_X));
    wr16(a0 + O_Y, rd16(obj + O_Y));
    wr16(a0 + 0x48, rd16(obj + 0x5C));
    if (!bit8(obj + 0x0E, 3)) wr16(a0 + 0x18, 0xFF00);
    else wr16(a0 + 0x18, 0);
}

/* $047AE4. Beaten: shaking, flickering between its x (+$5C) and off the screen with puffs, for
 * +$5E frames; then the room ends */
void DiceBrawlerBeaten(uint16_t obj)
{
    wr16(v_shakeSprites, 2);
    if (count_out(obj + 0x5E)) {
        DiceRoomEnd(obj);
        return;
    }
    if (bit8(v_gameFrames + 1, 0)) {
        wr16(obj + O_X, 0x220);
        BrawlerPuff(obj);
        return;
    }
    wr16(obj + O_X, rd16(obj + 0x5C));
}

/* $04743E. The room ended if its player left; fighting, its vitality spent: 840,040 points,
 * the boss timer 4, beaten (state $64, $100 frames, sound $A3); else kept in the room, grabbed
 * by a player (+$23 bit 1, cleared): held (state $2C); or knocked by a blow; then its step,
 * the room's player in a6 */
void DiceBrawler(uint16_t obj)
{
    if (room_player_gone(obj)) return;
    uint32_t d0 = 0;
    if (!below16(obj + O_STATE, 0x18) && (d0 = rd16(obj + O_VITALITY), neg16((uint16_t)d0))) {
        AddScore(obj, 0x84004);
        wr16(v_bossTimer, 4);
        wr16(obj + O_STATE, 0x64);
        wr16(obj + O_VITALITY, 0);
        wr8(obj + 0x21, 0);
        wr8(obj + 0x50, 1);
        wr8(obj + 0x20, 0xBC);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr32(obj + 0x48, 0x048024);
        BrawlerAnimFacing(obj);
        wr16(obj + 0x5C, rd16(obj + O_X));
        wr16(obj + 0x5E, 0x100);
        QueueSound(0xA3);
    } else {
        BrawlerWalls(obj);
        if (bit8(obj + 0x23, 1)) {
            clr8(obj + 0x23, 1);
            wr16(obj + O_STATE, 0x2C);
            wr8(obj + 0x21, 0x20);
            wr8(obj + 0x50, 1);
            wr32(obj + 0x18, 0);
            wr32(obj + 0x1C, 0);
            wr32(obj + 0x48, 0x048024);
            BrawlerAnimFacing(obj);
        } else {
            BrawlerHitJump(obj);
        }
    }
    reg_a6 = (uint32_t)(int32_t)(int16_t)rd16(0x80D8);
    uint32_t fn = rom32(0x04754E + (uint32_t)(int16_t)rd16(obj + O_STATE));
    /* d0's high word is the previous object's (ObjectsUpdate's loop keeps none): taken as 0 */
    if (fn == 0x047956) DiceBrawlerLeap(obj, d0 & 0xFFFF);
    else call_object(fn, obj);
}

/* ======================================================================================== */
/* ---- type $364: the dice palace's spring: a body on a coiled neck (+$4C its stretch, a
 * 16.16; +$50 its speed; the neck's lines drawn by the H-interrupt effect $10 from the
 * tables at $9400 and $E402), its three flames ($DE80, $DEE0, $DF40) on top; it bobs, leaps
 * at its player throwing a pod ($368); $DB5 of vitality: 88,880 points, sinking with smoke,
 * the room ends ---- */

/* $048A24. Its colours ($E316 / $E396, four) flickering every 8 frames */
static void spring_colours(void)
{
    uint32_t src = bit8(v_gameFrames + 1, 3) ? 0x048A50 : 0x048A58;
    for (uint16_t k = 0; k <= 3; k++) {
        wr16((uint16_t)(0xE316 + 2 * k), rom16(src + 2 * k));
        wr16((uint16_t)(0xE396 + 2 * k), rom16(src + 2 * k));
    }
}

/* $048722. The bobs before a leap (+$5E): by the difficulty, plus 0-3 */
static void spring_count(uint16_t obj)
{
    wr16(obj + 0x5E, by_difficulty(0x048740));
    add16(obj + 0x5E, rd16(v_randomSeed) & 3);
}

/* $048750. A flame on it (plain, $8300) */
static void spring_flame(uint16_t a0)
{
    wr16(a0 + O_TYPE, 0x10);
    wr16(a0 + O_FLAGS, 0x8000);
    wr16(a0 + 0x0E, 0x8300);
    wr16(a0 + 0x08, 0);
    wr16(a0 + 0x0A, 0xFCFC);
}

/* $04882A. The neck's rows ($9400, 64 words from $80 down by 4) */
static void spring_neck_clear(void)
{
    uint16_t v = 0x80;
    for (uint16_t k = 0; k <= 0x3F; k++) {
        wr16((uint16_t)(0x9400 + 2 * k), v);
        v = (uint16_t)(v - 4);
    }
}

/* $048868. Its stretch's step: the table at $48872 by +$48 */
static void spring_step_jump(uint16_t obj)
{
    call_object(rom32(0x048872 + (uint32_t)(int16_t)rd16(obj + 0x48)), obj);
}

/* $048840. The stretch stepped; the neck's lean (+$54) from it, the way +$58 bit 0 says */
static void spring_step(uint16_t obj)
{
    spring_step_jump(obj);
    uint32_t d0 = (uint32_t)((int32_t)(0x8000 - rd32(obj + 0x4C)) >> 4);
    if (!bit8(obj + 0x59, 0)) d0 = ~d0 + 1;
    wr32(obj + 0x54, d0);
}

/* $0487C6. The neck's rows above the body (from $9400 + its y's): each row's y as the
 * stretch draws it (none while the stretch's whole part leaves no rows); the rows' count
 * times 2 (-1: none) */
static uint16_t spring_neck(uint16_t obj)
{
    spring_neck_clear();
    spring_step(obj);
    uint16_t d0 = (uint16_t)((int16_t)(uint16_t)(rd16(obj + O_Y) - 0x80) >> 1);
    uint16_t n = (uint16_t)((int16_t)d0 >> 1);
    uint16_t a0 = (uint16_t)((d0 & 0xFFFE) + 0x9400);
    uint16_t whole = rd16(obj + 0x4C);
    if (!neg16(whole)) {
        if (!whole) n = 0x1F;
        n = (uint16_t)(n - (uint16_t)(whole * 0x20));
        if (neg16(n)) return 0xFFFF;
    }
    uint16_t rows = (uint16_t)(n << 1);
    uint32_t acc = (uint32_t)(uint16_t)(0xFFF0 - rd16(obj + O_Y)) << 16;
    uint32_t step = rd32(obj + 0x4C) << 2;
    for (;;) {
        acc -= step;
        a0 = (uint16_t)(a0 - 2);
        wr16(a0, (uint16_t)(acc >> 16));
        if (n-- == 0) break;
    }
    return rows;
}

/* $048970. The neck's lines' x (two words each, 4 apart, down from $E402 + its y's) leaning
 * by +$54; the flames placed at its top */
static void spring_lines(uint16_t obj, uint16_t rows)
{
    if (neg16(rows)) return;
    uint16_t a0 = (uint16_t)(((uint16_t)((rd16(obj + O_Y) - 0x7E) << 2) & 0xFFFC) + 0xE402);
    uint32_t d0 = (uint32_t)(uint16_t)(0xFFD0 + rd16(obj + O_X)) << 16;
    uint32_t d1 = 0, d2 = rd32(obj + 0x54);
    for (;;) {
        d1 += d2;
        d0 += d1;
        wr16(a0, (uint16_t)(d0 >> 16));
        a0 = (uint16_t)(a0 - 4);
        wr16(a0, (uint16_t)(d0 >> 16));
        a0 = (uint16_t)(a0 - 4);
        if (rows-- == 0) break;
    }
    uint16_t x = (uint16_t)((uint16_t)((d1 << 4) >> 16) + rd16(obj + O_X));
    x = (uint16_t)(x - 7);
    wr16(0xDE90, x);
    x = (uint16_t)(x + 7);
    wr16(0xDF50, x);
    x = (uint16_t)(x + 7);
    wr16(0xDEF0, x);
}

/* $0487BC. The neck drawn: its rows, then its lines */
static void spring_shape(uint16_t obj)
{
    spring_lines(obj, spring_neck(obj));
}

/* loc_487A2. The middle flame's frame flickering */
static void spring_flicker(void)
{
    if (bit8(v_gameFrames + 1, 4)) wr16(0xDF4E, 0x8301);
    else wr16(0xDF4E, 0x8302);
}

/* loc_4876E. The flames' y at the neck's top ($30 times the stretch less $5C), its box's top
 * there; the middle flame flickering */
static void spring_top(uint16_t obj)
{
    int32_t d1 = (int32_t)(int16_t)(uint16_t)((uint32_t)((int32_t)rd32(obj + 0x4C) >> 8)) * 0x30;
    d1 >>= 8;
    uint16_t y = (uint16_t)(rd16(obj + O_Y) - 0x5C + (uint16_t)d1);
    wr16(0xDE94, y);
    wr16(0xDEF4, y);
    y = (uint16_t)(y + 0xA);
    wr16(0xDF54, y);
    y = (uint16_t)(y - rd16(obj + O_Y));
    wr8(obj + 0x2C, (uint8_t)y);
    wr8(obj + 0x28, (uint8_t)y);
    spring_flicker();
}

/* $0489D2. Smoke: sound $CA every 8 frames, a puff ($100, $563EE) rising from a random point */
static void spring_smoke(uint16_t obj)
{
    BlastSound8();
    uint16_t a0, t;
    if (!SlotFreeHigh(&a0, &t)) return;
    wr16(a0 + O_TYPE, 0x100);
    wr32(a0 + 0x48, 0x0563EE);
    wr32(a0 + 0x1C, 0xFFFFC000);
    wr16(a0 + O_X, (uint16_t)((rd8(v_randomSeed) & 0x3F) + (uint16_t)(rd16(obj + O_X) - 0x20)));
    wr16(a0 + O_Y, (uint16_t)((rd8(v_randomSeed + 1) & 0x7F) + (uint16_t)(rd16(obj + O_Y) - 0x68)));
}

/* $048526. Once the sines are free: set up at $D0,$150 ($DB5 of vitality), the neck's
 * H-interrupt effect ($10), $FFF7E7 3, plane B's flag, sound $B5 */
void DiceSpringStart(uint16_t obj)
{
    if (rd16(0x80C2)) return;
    add16(obj + O_STATE, 4);
    wr16(0x80C2, 0x10);
    wr16(0x80D6, obj);
    wr16(obj + O_FLAGS, 0x0C00);
    wr16(obj + O_X, 0xD0);
    wr16(obj + O_Y, 0x150);
    wr32(obj + 0x2C, 0xFF01E020);
    wr32(obj + 0x28, 0xFF01E020);
    wr8(obj + 0x46, 0x8F);
    wr16(obj + O_VITALITY, 0xDB5);
    wr16(obj + 0x48, 0x10);
    wr32(obj + 0x4C, 0xFFFF0000);
    wr32(obj + 0x50, 0);
    wr16(obj + 0x58, 1);
    wr16(v_hintEffect, 0x10);
    wr16(v_hintState, 0);
    wr8(0xF7E7, 3);
    set8(v_planeFlagsB, 7);
    QueueSound(0xB5);
}

/* $0485AA. Rising out (the screen shaking) until the stretch's step is 0: the flames lit,
 * its collisions on */
void DiceSpringRise(uint16_t obj)
{
    wr16(v_shakeSprites, 2);
    if (!rd16(obj + 0x48)) {
        add16(obj + O_STATE, 4);
        wr8(obj + 0x21, 0x24);
        spring_flame(0xDE80);
        spring_flame(0xDEE0);
        spring_flame(0xDF40);
        wr16(0xDF40 + 0x0E, 0x8301);
        spring_count(obj);
    }
    spring_shape(obj);
    spring_top(obj);
}

/* $0485EC. Bobbing until its count (+$5E) runs out: then stretching to leap */
void DiceSpringBob(uint16_t obj)
{
    if (neg16(rd16(obj + 0x5E))) {
        add16(obj + O_STATE, 4);
        wr16(obj + 0x48, 4);
    }
    spring_shape(obj);
    spring_top(obj);
}

/* $048606. Stretched (step 8): a leap at its player ($1.2 or $1.A across, up $3.8 or by
 * chance from the second difficulty $5.8), a pod ($368) thrown the other way from $C9E0 if
 * free */
void DiceSpringLeap(uint16_t obj)
{
    if (rd16(obj + 0x48) == 8) {
        add16(obj + O_STATE, 4);
        uint16_t p = rd16(0x80D8);
        uint32_t d1 = 0x12000, d2 = 0xFFFE6000;
        wr16(obj + 0x58, 0);
        uint16_t d = (uint16_t)(rd16(p + O_X) - rd16(obj + O_X));
        if (neg16(d)) {
            d1 = 0xFFFE6000;
            d2 = 0x14000;
            wr16(obj + 0x58, 1);
        }
        wr32(obj + 0x18, d1);
        if (rd16(v_difficulty) && !bit8(v_randomSeed + 1, 3)) wr32(obj + 0x1C, 0xFFFA8000);
        else wr32(obj + 0x1C, 0xFFFC8000);
        if (!rd16(0xC9E0)) {
            uint16_t a0 = 0xC9E0;
            wr16(a0 + O_TYPE, 0x368);
            wr16(a0 + O_X, rd16(obj + O_X));
            wr16(a0 + O_Y, (uint16_t)(rd16(obj + O_Y) - 0x10));
            wr32(a0 + 0x18, d2);
            wr32(a0 + 0x1C, 0xFFFC8000);
        }
    }
    spring_shape(obj);
    spring_top(obj);
}

/* $0486B6. Falling (gravity $4000) to y $150; landed: bobbing again (state 8) */
void DiceSpringFall(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if (!bit8(obj + 0x1C, 7) && !below16(obj + O_Y, 0x150)) {
        wr16(obj + O_Y, 0x150);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) - 8));
        spring_count(obj);
    }
    spring_shape(obj);
    spring_top(obj);
}

/* $0486F8. Beaten: the flames sinking, the screen shaking, smoke; after +$48 frames the room
 * ends */
void DiceSpringSink(uint16_t obj)
{
    wr16(0xDE94, (uint16_t)(rd16(0xDE94) - 2));
    wr16(0xDEF4, (uint16_t)(rd16(0xDEF4) - 2));
    wr16(0xDF54, (uint16_t)(rd16(0xDF54) - 2));
    wr16(v_shakeSprites, 0xA);
    if (count_out(obj + 0x48)) {
        DiceRoomEnd(obj);
        return;
    }
    spring_smoke(obj);
    spring_flicker();
}

/* $048886. The stretch bouncing (+$50 + $200 a frame); at the top ($8000, $18000 while
 * touching +$47 bit 4): back down, the lean turned, a bob counted */
void DiceSpringBounce(uint16_t obj)
{
    add32(obj + 0x50, 0x200);
    add32(obj + 0x4C, rd32(obj + 0x50));
    if (bit8(obj + 0x50, 7)) return;
    uint32_t top = bit8(obj + 0x47, 4) ? 0x18000 : 0x8000;
    if (neg32(rd32(obj + 0x4C) - top)) return;
    wr32(obj + 0x50, 0xFFFFE800);
    wr16(obj + 0x58, rd16(obj + 0x58) ^ 1);
    wr16(obj + 0x5E, (uint16_t)(rd16(obj + 0x5E) - 1));
}

/* $0488D6. Stretching to $14000, then the next step */
void DiceSpringStretch(uint16_t obj)
{
    add32(obj + 0x50, 0x200);
    add32(obj + 0x4C, rd32(obj + 0x50));
    if (bit8(obj + 0x50, 7)) return;
    if (below32(obj + 0x4C, 0x14000)) return;
    add16(obj + 0x48, 4);
    wr32(obj + 0x4C, 0x14000);
}

/* $04890A. Shrinking ($2000 a frame to 0) while rising; falling: the next step */
void DiceSpringShrink(uint16_t obj)
{
    if (rd32(obj + 0x4C)) wr32(obj + 0x4C, rd32(obj + 0x4C) - 0x2000);
    if (!bit8(obj + 0x1C, 7)) add16(obj + 0x48, 4);
}

/* $04892E. Stretching $800 a frame while falling; landed: bouncing from $14000 */
void DiceSpringLand(uint16_t obj)
{
    add32(obj + 0x4C, 0x800);
    if (rd32(obj + 0x1C)) return;
    wr16(obj + 0x48, 0);
    wr32(obj + 0x4C, 0x14000);
    wr32(obj + 0x50, 0xFFFFDC00);
    wr16(obj + 0x58, rd16(obj + 0x58) ^ 1);
}

/* $04895C. Rising out ($400 a frame); at 0 bouncing */
void DiceSpringRecoil(uint16_t obj)
{
    add32(obj + 0x4C, 0x400);
    if (!rd32(obj + 0x4C)) wr16(obj + 0x48, 0);
}

/* $04841A. The room ended if its player left; its count (+$5C); its colours; kept between x
 * $B0 and $18F; struck: its flash colours ($E302 / $E382, white then three frames back);
 * its vitality spent: 88,880 points, the effects off, sinking ($100 frames) */
void DiceSpring(uint16_t obj)
{
    if (room_player_gone(obj)) return;
    add16(obj + 0x5C, 1);
    spring_colours();
    if (below16(obj + O_X, 0xB0)) wr16(obj + O_X, 0xB2);
    if (!below16(obj + O_X, 0x190)) wr16(obj + O_X, 0x18E);
    uint32_t src = 0;
    if (!neg16(rd16(obj + 0x5A))) {
        wr16(obj + 0x5A, (uint16_t)(rd16(obj + 0x5A) - 1));
        src = 0x048A60;
    } else if (bit8(obj + 0x22, 5)) {
        wr16(obj + 0x5A, 2);
        src = 0x048A68;
    }
    if (src) {
        for (uint16_t k = 0; k <= 3; k++) {
            wr16((uint16_t)(0xE302 + 2 * k), rom16(src + 2 * k));
            wr16((uint16_t)(0xE382 + 2 * k), rom16(src + 2 * k));
        }
    }
    if (neg16(rd16(obj + O_VITALITY))) {
        AddScore(obj, 0x8888);
        wr16(obj + O_STATE, 0x14);
        wr8(obj + 0x21, 0);
        wr16(obj + O_VITALITY, 0);
        wr16(v_hintEffect, 0);
        wr16(v_hintState, 0);
        wr8(0xF7E7, 0);
        wr16(0xE402, 0);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr16(obj + 0x48, 0x100);
    }
    dispatch(0x04850E, obj);
}

/* ---- type $368: the spring's pod: falling with its three flames (from the slot after it);
 * landed (y $142): after a time by the difficulty (flickering off the screen the last $18
 * frames) it becomes type $354 where it landed, its flames removed ---- */

/* loc_48B10. The flames placed on it, wobbling */
static void pod_flames(uint16_t obj)
{
    uint16_t a0 = (uint16_t)(obj + 0x60), a1 = (uint16_t)(a0 + 0x60), a2 = (uint16_t)(a1 + 0x60);
    uint16_t f = rd16(v_gameFrames);
    uint16_t d1 = (uint16_t)(((int16_t)f >> 2) & 3), d2 = (uint16_t)(((int16_t)f >> 3) & 1);
    uint16_t x = (uint16_t)(rd16(obj + O_X) + 7);
    wr16(a0 + O_X, x);
    x = (uint16_t)(x - 7);
    wr16(a2 + O_X, x);
    x = (uint16_t)(x - 7);
    wr16(a1 + O_X, x);
    uint16_t y = (uint16_t)(rd16(obj + O_Y) - 6 + d1);
    wr16(a0 + O_Y, y);
    wr16(a1 + O_Y, y);
    wr16(a2 + O_Y, (uint16_t)(rd16(obj + O_Y) + 4 - d2));
}

/* $048A86. Its set-up, its three flames */
void SpringPodStart(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    wr16(obj + O_FLAGS, 0x8C00);
    wr16(obj + 0x0E, 0x84C2);
    wr16(obj + 0x08, 0x0F00);
    wr16(obj + 0x0A, 0xF0F0);
    wr8(obj + 0x20, 0x20);
    uint16_t a0 = (uint16_t)(obj + 0x60);
    spring_flame(a0);
    a0 = (uint16_t)(a0 + 0x60);
    spring_flame(a0);
    a0 = (uint16_t)(a0 + 0x60);
    spring_flame(a0);
    wr16(a0 + 0x0E, 0x8301);
}

/* $048ACA. Falling to y $142; landed: its time by the difficulty */
void SpringPodFall(uint16_t obj)
{
    add32(obj + 0x1C, 0x4000);
    if (!bit8(obj + 0x1C, 7) && !below16(obj + O_Y, 0x142)) {
        add16(obj + O_STATE, 4);
        wr16(obj + 0x48, by_difficulty(0x048B68));
        wr16(obj + 0x4A, rd16(obj + O_X));
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
    }
    pod_flames(obj);
}

/* $048B78. Its time run out: it becomes type $354, its flames removed */
void SpringPodWait(uint16_t obj)
{
    if (count_out(obj + 0x48)) {
        wr16(obj + O_TYPE, 0x354);
        wr16(obj + O_STATE, 0);
        wr16(obj + O_X, rd16(obj + 0x4A));
        uint16_t a0 = (uint16_t)(obj + 0x60);
        wr16(a0 + O_FLAGS, 0x1000);
        a0 = (uint16_t)(a0 + 0x60);
        wr16(a0 + O_FLAGS, 0x1000);
        a0 = (uint16_t)(a0 + 0x60);
        wr16(a0 + O_FLAGS, 0x1000);
        return;
    }
    if (below16(obj + 0x48, 0x18)) {
        if (!bit8(v_gameFrames + 1, 0)) wr16(obj + O_X, 0x200);
        else wr16(obj + O_X, rd16(obj + 0x4A));
    }
    pod_flames(obj);
}

/* $048A70. Type $368: its step */
void SpringPod(uint16_t obj)
{
    dispatch(0x048A7A, obj);
}

/* ---- type $43C: the dice palace's captain room: the colours, once the sines are free the
 * captain (type $1F8 at $FFA800, its +$06 $90) and on the fourth difficulty a wisp ($658 at
 * $DE80); the captain gone: the room ends ---- */

/* $048BDA. Type $43C: as the section says */
void CaptainRoom(uint16_t obj)
{
    if (room_player_gone(obj)) return;
    uint16_t a0 = 0xA800;
    if (!rd16(obj + O_STATE)) {
        wr16(0xE320, 8);
        wr16(0xE33C, 0xC);
        if (rd16(0x80C2)) return;
        add16(obj + O_STATE, 4);
        wr16(a0 + O_TYPE, 0x1F8);
        wr16(a0 + O_STATE, 0);
        wr16(a0 + 0x06, 0x90);
        if (rd16(v_difficulty) != 4) return;
        wr16(0xDE80, 0x658);
        wr16(0xDE84, 0);
    }
    if (!rd16(a0 + O_TYPE)) DiceRoomEnd(obj);
}

/* ---- type $658: the wisp: drifting at random speeds (up to 7 across, 3 up or down), turned
 * back into the middle of the screen ($80-$1BF, $90-$BF) ---- */

/* $048C6E. Type $658: as the section says */
void CaptainWisp(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        add16(obj + O_STATE, 4);
        wr16(obj + O_FLAGS, 0x8C00);
        wr16(obj + 0x0E, 0x7FB);
        wr16(obj + 0x08, 0);
        wr16(obj + 0x0A, 0xFCFC);
        wr8(obj + 0x20, 0xBC);
        wr16(obj + O_X, 0x120);
        wr16(obj + O_Y, 0x40);
    }
    uint16_t a = rd16(v_randomSeed) & 0x7E;
    if (below16(obj + O_Y, 0x90)) a = (uint16_t)(a + 0x40);
    else if (!below16(obj + O_Y, 0xC0)) a = (uint16_t)(a + 0x140);
    else if (below16(obj + O_X, 0x80)) a = (uint16_t)(a + 0x1C0);
    else if (!below16(obj + O_X, 0x1C0)) a = (uint16_t)(a + 0xC0);
    else {
        if (!count_out(obj + 0x48)) return;
        a = rd16(v_randomSeed);
    }
    a &= 0x1FE;
    int16_t s = sine(a), c = cosine(a);
    int16_t v = (int16_t)(rd8(v_randomSeed) & 7);
    wr32(obj + 0x18, (uint32_t)((int32_t)c * v));
    wr32(obj + 0x1C, (uint32_t)((int32_t)s * (int16_t)(v >> 1)));
    wr16(obj + 0x48, rd16(v_randomSeed + 2) & 0x3F);
}

/* ======================================================================================== */
/* ---- type $440: a quake director: the ground's rows waving (the H-scroll words from $FFE47E,
 * 4 apart, by the amplitude +$54 and a table; past row $14E a slope; once +$4A, past $16A the
 * shake +$54 - +$58), the planes' speeds ($A91C / $A93C) stepped by the planes' position
 * ($A92C), soldiers ($48) and riders ($144) let in, then the quake dying down ---- */

/* $049B0A. Tiles to VRAM: a command list at $FF9000 of the 71 blocks from $BCA98 (each its
 * byte length first) to VRAM $6000 on, run */
static void quake_tiles(void)
{
    uint32_t src = 0x0BCA98;
    uint16_t a1 = 0x9000, vram = 0x6000;
    for (int k = 0; k <= 0x46; k++) {
        wr16(a1, 1);
        wr32((uint16_t)(a1 + 2), src);
        wr16((uint16_t)(a1 + 6), vram);
        a1 = (uint16_t)(a1 + 8);
        uint16_t len = rom16(src);
        src += 2 + (uint32_t)len;
        vram = (uint16_t)(vram + len);
    }
    wr16(a1, 0xFFFF);
    RunCommands(0xFFFF9000);
}

/* $049912. The planes' speed up by $400; its sign */
static int quake_speed_up(void)
{
    uint32_t v = rd32(0xA93C) + 0x400;
    wr32(0xA93C, v);
    wr32(0xA91C, v);
    return neg32(v);
}

/* $049926. Plane B's other speed stopped past $480 */
static void quake_stop_b(void)
{
    if (!below16(0xA928, 0x480)) wr32(0xA938, 0);
}

/* $049936. The shaking by its count (+$48 / 32); sound $CA every 8 frames from $80 */
static void quake_rumble(uint16_t obj)
{
    if (!below16(obj + 0x48, 0x80)) BlastSound8();
    wr16(v_shakeSprites, (uint16_t)((int16_t)rd16(obj + 0x48) >> 5));
}

/* $049952. The amplitude's speed (+$4C) eased to 0 */
static void quake_ease(uint16_t obj)
{
    if (!neg32(rd32(obj + 0x4C))) {
        wr32(obj + 0x4C, 0);
        return;
    }
    add32(obj + 0x4C, 0xA00);
}

/* $049A24. Three rows at the amplitude times `m` (<< 2), a0 left at the third */
static uint16_t quake_wave_triple(uint16_t obj, uint16_t a0, uint16_t m)
{
    int32_t p = (int32_t)(int16_t)rd16(obj + 0x54) * (int16_t)m;
    uint16_t w = (uint16_t)(((uint32_t)p << 2) >> 16);
    wr16(a0, w);
    a0 = (uint16_t)(a0 + 4);
    wr16(a0, w);
    a0 = (uint16_t)(a0 + 4);
    wr16(a0, w);
    return a0;
}

/* $04996A. The amplitude moved by its speed; the rows waved */
static void quake_wave(uint16_t obj)
{
    add32(obj + 0x54, rd32(obj + 0x4C));
    uint16_t i = (uint16_t)(rd16(0xA92C) - 0x100), row = i;
    i = (uint16_t)((i << 1) & 0x1E);
    uint16_t a0 = 0xE47E;
    for (;;) {
        int32_t p = (int32_t)(int16_t)rd16(obj + 0x54) * (int16_t)rom16(0x049A3E + i);
        wr16(a0, (uint16_t)((uint32_t)p >> 16));
        row = (uint16_t)(row + 1);
        if (!lt16(row, 0x14E)) break;
        i = (uint16_t)((i + 2) & 0x1E);
        a0 = (uint16_t)(a0 + 4);
        if (!lt16(a0, 0xE686)) return;
    }
    uint16_t m = 0x800;
    a0 = quake_wave_triple(obj, a0, m);
    for (;;) {
        int32_t p = (int32_t)(int16_t)rd16(obj + 0x54) * (int16_t)m;
        wr16(a0, (uint16_t)(((uint32_t)p << 2) >> 16));
        m = (uint16_t)(m + 0x140);
        row = (uint16_t)(row + 1);
        a0 = (uint16_t)(a0 + 4);
        if (rd16(obj + 0x4A) && !lt16(row, 0x16A)) break;
        if (!lt16(a0, 0xE686)) return;
    }
    uint16_t hi = (uint16_t)((rd32(obj + 0x54) - rd32(obj + 0x58)) >> 16);
    uint16_t d2 = (uint16_t)(((uint32_t)((int32_t)(int16_t)hi * 0x7FFF) << 2) >> 16);
    uint16_t d3 = (uint16_t)((int16_t)d2 >> 1);
    if (bit8(v_gameFrames + 1, 0)) {
        uint16_t t = d2;
        d2 = d3;
        d3 = t;
    }
    do {
        wr16(a0, d3);
        a0 = (uint16_t)(a0 + 4);
        wr16(a0, d2);
        a0 = (uint16_t)(a0 + 4);
    } while (lt16(a0, 0xE686));
}

/* $049A5E. Between planes' x $480 and $69F, a soldier ($48, +$06 $88) at x $1C0 every $40-$7F
 * frames (from $CC80 below $CE00) */
static void quake_soldiers(uint16_t obj)
{
    if (below16(v_planes, 0x480) || !below16(v_planes, 0x6A0)) return;
    if (!count_out(obj + 0x5C)) return;
    uint16_t a0 = 0xCC80, t;
    if (!SlotFreeBelowCE(&a0, &t)) return;
    wr16(a0 + O_TYPE, 0x48);
    wr16(a0 + O_X, 0x1C0);
    wr16(a0 + 0x06, 0x88);
    wr16(obj + 0x5C, (uint16_t)((rd16(v_randomSeed) & 0x3F) + 0x40));
}

/* $049AAA. Two riders ($144 at $1C0,$100): the first past planes' x $240 (+$06 $8000), the
 * second past $500 ($8004) */
static void quake_riders(uint16_t obj)
{
    uint16_t kind;
    if (!rd16(obj + 0x5E)) {
        if (below16(v_planes, 0x240)) return;
        kind = 0x8000;
    } else {
        if (rd16(obj + 0x5E) == 2) return;
        if (below16(v_planes, 0x500)) return;
        kind = 0x8004;
    }
    add16(obj + 0x5E, 1);
    uint16_t a0;
    if (!SlotFreeLow(&a0)) return;
    wr16(a0 + O_TYPE, 0x144);
    wr16(a0 + O_X, 0x1C0);
    wr16(a0 + O_Y, 0x100);
    wr16(a0 + 0x06, kind);
}

/* $049800. Set up: the amplitude's speed -16, the planes at $1.8, $FFF7E7 3, plane B's flag,
 * the tiles loaded, three colours ($49848; StageColoursCopy's d0 is the command list's end
 * mark, $FFFF, as RunCommands leaves it) */
void QuakeStart(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    wr16(obj + 0x4A, 0);
    wr32(obj + 0x4C, 0xFFF00000);
    wr32(obj + 0x54, 0);
    wr16(obj + 0x5C, 0);
    wr16(obj + 0x5E, 0);
    wr32(0xA93C, 0x18000);
    wr32(0xA91C, 0x18000);
    wr8(0xF7E7, 3);
    set8(v_planeFlagsB, 7);
    quake_tiles();
    StageColoursCopy(0x049848, 0xFFFF);
}

/* $04984E. Waving until the planes reach $200: their speed -$1.4, a shake, sound $BD */
void QuakeRoll(uint16_t obj)
{
    quake_wave(obj);
    if (below16(0xA92C, 0x200)) return;
    add16(obj + O_STATE, 4);
    wr32(0xA93C, 0xFFFEC000);
    wr32(0xA91C, 0xFFFEC000);
    wr16(v_shakeSprites, 0x10);
    QueueSound(0xBD);
}

/* $049878. The speed rising back; once not negative: plane B's other speed 8, +$4A set */
void QuakeBrake(uint16_t obj)
{
    quake_wave(obj);
    if (quake_speed_up()) return;
    add16(obj + O_STATE, 4);
    wr32(0xA938, 0x80000);
    add16(obj + 0x4A, 1);
}

/* $049896. Rolling on to $200 again: the speed -$0.8, a shake, sound $BD */
void QuakeRoll2(uint16_t obj)
{
    quake_wave(obj);
    quake_stop_b();
    quake_speed_up();
    if (below16(0xA92C, 0x200)) return;
    add16(obj + O_STATE, 4);
    wr32(0xA93C, 0xFFFF8000);
    wr32(0xA91C, 0xFFFF8000);
    wr16(v_shakeSprites, 0xA);
    QueueSound(0xBD);
}

/* $0498C8. The speed rising; not negative and past $200: stopped, $200 frames of rumbling */
void QuakeBrake2(uint16_t obj)
{
    quake_wave(obj);
    if (quake_speed_up()) return;
    if (below16(0xA92C, 0x200)) return;
    add16(obj + O_STATE, 4);
    wr16(obj + 0x48, 0x200);
    wr32(0xA93C, 0);
    wr32(0xA91C, 0);
}

/* $0498F2. Rumbling, the waves easing, for +$48 frames */
void QuakeRumble(uint16_t obj)
{
    quake_ease(obj);
    quake_wave(obj);
    quake_rumble(obj);
    if (count_out(obj + 0x48)) add16(obj + O_STATE, 4);
}

/* $04990C. Waving on */
void QuakeCalm(uint16_t obj)
{
    quake_wave(obj);
}

/* $0497C2. After its set-up: the shake's phase (+$58) on by $8000, soldiers and riders */
void Quake(uint16_t obj)
{
    if (rd16(obj + O_STATE)) {
        add32(obj + 0x58, 0x8000);
        quake_soldiers(obj);
        quake_riders(obj);
    }
    dispatch(0x0497E4, obj);
}

/* ======================================================================================== */
/* ---- stage 5's space pieces (as objects4c.c's) ---- */

/* loc_4C6C6 (Space460's): struck by a player: a puff ($100, $565F6), sound $CA */
static void space_struck(uint16_t obj)
{
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x0565F6);
    wr8(obj + 0x21, 0);
    wr16(obj + O_STATE, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    QueueSound(0xCA);
}

/* ---- type $478: a shot bouncing off the screen's edges ($8C-$1B3, $A0-$153) +$48 times,
 * then left to fly off; struck by a player: a puff ---- */

/* $04C6EE. Type $478: as the section says */
void SpaceBouncer(uint16_t obj)
{
    wr16(obj + 0x0E, rd16(0xCA3A));
    if (bit8(obj + 0x22, 4) && (rd16(obj + 0x40) == 0xA400 || rd16(obj + 0x40) == 0xA600)) {
        space_struck(obj);
        return;
    }
    if (neg16(rd16(obj + 0x48))) {
        ObjectRemoveOffscreen(obj);
        return;
    }
    int bounce;
    if (bit8(obj + 0x18, 7)) bounce = below16(obj + O_X, 0x8C);
    else bounce = !below16(obj + O_X, 0x1B4);
    if (bounce) {
        wr16(obj + 0x18, (uint16_t)-rd16(obj + 0x18));
        wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
    }
    if (bit8(obj + 0x1C, 7)) bounce = below16(obj + O_Y, 0xA0);
    else bounce = !below16(obj + O_Y, 0x154);
    if (!bounce) return;
    wr32(obj + 0x1C, (uint32_t)-rd32(obj + 0x1C));
    wr16(obj + 0x48, (uint16_t)(rd16(obj + 0x48) - 1));
}

/* ---- type $468: a homing missile: blinking for +$48 frames, then flying at its angle
 * (+$4C, from $180) turning 4 a frame toward its player; after +$4A frames a burst ($100,
 * $5639C), sound $A5 ---- */

/* $04C906. Type $468: as the section says */
void SpaceHomer(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        if (count_out(obj + 0x48)) {
            add16(obj + O_STATE, 4);
            wr16(obj + 0x4C, 0x180);
        }
        if (bit8(obj + 0x49, 0)) set8(obj + O_FLAGS, 7);
        else clr8(obj + O_FLAGS, 7);
        return;
    }
    if (count_out(obj + 0x4A)) {
        wr16(obj + O_TYPE, 0x100);
        wr32(obj + 0x48, 0x05639C);
        wr16(obj + O_STATE, 0);
        wr8(obj + 0x21, 0);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        QueueSound(0xA5);
        return;
    }
    uint16_t a = rd16(obj + 0x4C);
    add32(obj + O_Y, (uint32_t)(int32_t)sine(a) << 1);
    add32(obj + O_X, (uint32_t)(int32_t)cosine(a) << 1);
    uint16_t d = (uint16_t)(SpaceAim(obj) - rd16(obj + 0x4C));
    int up;
    if (neg16(d)) up = lt16(d, 0xFF00);
    else up = lt16(d, 0x100);
    if (up) add16(obj + 0x4C, 4);
    else wr16(obj + 0x4C, (uint16_t)(rd16(obj + 0x4C) - 4));
    wr16(obj + 0x4C, rd16(obj + 0x4C) & 0x1FE);
}

/* ---- type $504: a charge: set up from $4C5FA; for +$56 frames its look from $C9DC (struck by
 * a shot: a puff, loc_4C662); then shrinking ($18 frames, 2 a frame: three frames from
 * $5663C) and fired right as a shot of type $10 ($4.8, from 16 right of it; loc_4C358) ---- */

/* loc_4C358: object a0 made a shot (type $10) at (x, y) with x speed `vx` */
static void space_shot10(uint16_t a0, uint32_t vx, uint16_t x, uint16_t y)
{
    wr16(a0 + O_TYPE, 0x10);
    wr16(a0 + O_FLAGS, 0xCA00);
    wr16(a0 + 0x0E, 0xE000);
    wr32(a0 + 0x08, 0x000E09C6);
    wr8(a0 + 0x20, 0x80);
    wr8(a0 + 0x21, 0x10);
    wr32(a0 + 0x2C, 0xFF01F20E);
    wr8(a0 + 0x23, 0x20);
    wr16(a0 + 0x26, 2);
    wr16(a0 + O_X, x);
    wr16(a0 + O_Y, y);
    wr32(a0 + 0x18, vx);
}

/* $04CB4A. Type $504: as the section says */
void SpaceCharge(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        uint32_t data = 0x04C5FA;
        uint16_t n;
        ObjectSetupAll(obj, &data, &n);
        return;
    }
    if (rd16(obj + O_STATE) == 4) {
        if (count_out(obj + 0x56)) {
            add16(obj + O_STATE, 4);
            wr16(obj + 0x56, 0x18);
            wr8(obj + 0x21, 0);
            wr32(obj + 0x18, 0);
            wr32(obj + 0x1C, 0);
            wr16(obj + 0x0E, rd16(0xC9DC));
            QueueSound(0xC7);
            return;
        }
        wr16(obj + 0x0E, rd16(0xC9DC));
        if (!bit8(obj + 0x22, 4)) return;
        wr16(obj + O_TYPE, 0x100);
        wr32(obj + 0x48, 0x05663A);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr16(obj + O_STATE, 0);
        wr8(obj + 0x21, 0);
        QueueSound(0xCA);
        return;
    }
    wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - 2));
    if (neg16(rd16(obj + 0x56))) {
        space_shot10(obj, 0x48000, (uint16_t)(rd16(obj + O_X) + 0x10), rd16(obj + O_Y));
        return;
    }
    uint32_t look;
    if (!below16(obj + 0x56, 0xF)) look = 0x05663C;
    else if (!below16(obj + 0x56, 7)) look = 0x056644;
    else look = 0x05664C;
    wr16(obj + 0x0E, rom16(look));
    wr16(obj + 0x08, rom16(look + 2));
    wr16(obj + 0x0A, rom16(look + 4));
    wr16(obj + 0x0E, rd16(obj + 0x0E) | 0x8000);
}

/* ======================================================================================== */
/* ---- type $488: the boss of seven forms again (stage 6's, with objects26.c's Seven* code):
 * it loads its tiles, then takes its forms at random (+$5D the forms used, +$5C the last):
 * A ($4D324, bobbing, firing shards $48C), B (the chain), C (the beam), D (the spokes, then
 * pellets $490 / $498), E (its mines $494, charging), F (legs swinging, shots $498), G
 * (spinning, blasts $454); between forms it vanishes; at the start it rises and flies in.
 * Its vitality ($1690): 777,770 points, the last form A falling off the screen, the stage's
 * camera let go ---- */

/* the bobbing (loc_4E15C: as SevenPoseBob's, objects26.c): while the pose runs (+$4A not
 * negative) the y speed eased to 0 by $1400; after it, up (to -$1.2, above y $C0 turned back)
 * or down (to $1.2, below $140 turned back) by $1000 ($8050 bit 1 the way, changed now and
 * then by chance at full speed) */
static void seven_bob(uint16_t obj)
{
    if (!neg16(rd16(obj + 0x4A))) {
        if (!bit8(obj + 0x1C, 7)) {
            if (below32(obj + 0x1C, 0x2000)) wr32(obj + 0x1C, 0);
            else wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x1400);
        } else {
            if (!below32(obj + 0x1C, 0xFFFFE000)) wr32(obj + 0x1C, 0);
            else add32(obj + 0x1C, 0x1400);
        }
        return;
    }
    int full;
    if (bit8(0x8050, 1)) {
        if (!below16(obj + O_Y, 0x140)) { wr8(0x8050, rd8(0x8050) ^ 2); return; }
        full = !below32(obj + 0x1C, 0x12000);
        if (!full) { add32(obj + 0x1C, 0x1000); return; }
    } else {
        if (below16(obj + O_Y, 0xC0)) { wr8(0x8050, rd8(0x8050) ^ 2); return; }
        full = below32(obj + 0x1C, 0xFFFEE000);
        if (!full) { wr32(obj + 0x1C, rd32(obj + 0x1C) - 0x1000); return; }
    }
    if (rd16(v_randomSeed) & 0x3F) return;
    if (bit8(v_randomSeed, 7)) clr8(0x8050, 1);
    else set8(0x8050, 1);
}

/* loc_4DA32: the pose stepped, then the bobbing */
static void seven_run_bob(uint16_t obj)
{
    SevenAPoseRun(obj);
    seven_bob(obj);
}

/* $04DFAE. A free slot of 17 from $D820 (loc_56032: five, then SlotFree12's) */
static int seven_slot(uint16_t *slot)
{
    uint16_t a = 0xD820, t;
    for (int k = 0; k < 5; k++) {
        if (!rd16(a)) {
            *slot = a;
            return 1;
        }
        a = (uint16_t)(a + 0x60);
    }
    *slot = a;
    return SlotFree12(slot, &t);
}

/* $04DEB8. Its type, flags ($8C80), boxes and power from a list */
static void seven_box(uint16_t obj, uint32_t list)
{
    wr16(obj + O_TYPE, 0x488);
    wr16(obj + O_FLAGS, 0x8C80);
    wr32(obj + 0x2C, rom32(list));
    wr32(obj + 0x28, rom32(list + 4));
    wr16(obj + 0x26, rom16(list + 8));
    wr8(obj + 0x21, 0x30);
    wr8(obj + 0x23, 0x20);
}

/* $04DF2C. A part's boxes and power from a list */
static void seven_part_box(uint32_t list, uint16_t a1)
{
    wr32(a1 + 0x2C, rom32(list));
    wr32(a1 + 0x28, rom32(list + 4));
    wr16(a1 + 0x26, rom16(list + 8));
    wr8(a1 + 0x21, 0x30);
    wr8(a1 + 0x23, 0x20);
}

/* $04DFC4. +$54 $100 when its player is to the left, else 0 */
static void seven_face(uint16_t obj)
{
    uint16_t p = PlayerTarget();
    if (neg16((uint16_t)(rd16(p + O_X) - rd16(obj + O_X)))) wr16(obj + 0x54, 0x100);
    else wr16(obj + 0x54, 0);
}

/* $04DFB8. The same every 16 frames */
static void seven_face_now(uint16_t obj)
{
    if (rd16(v_gameFrames) & 0xF) return;
    seven_face(obj);
}

/* $04DFA0. +$50 counted up to 0 */
static void seven_recover(uint16_t obj)
{
    if (neg16(rd16(obj + 0x50))) add16(obj + 0x50, 1);
}

/* $04DDAA. The angle to its player (an index of 256) */
static uint16_t seven_aim(uint16_t obj)
{
    uint16_t p = PlayerTarget();
    int16_t dx = (int16_t)(rd16(p + O_X) - rd16(obj + O_X));
    uint32_t d1 = (uint16_t)(rd16(p + O_Y) - rd16(obj + O_Y));
    return (uint16_t)(((int16_t)Atan2(dx, &d1) >> 7) & 0x1FE);
}

/* $04DDCE. Its player close behind it (left, within $18 up or down): vanishing to form $30 */
static void seven_catch(uint16_t obj)
{
    uint16_t p = PlayerTarget();
    if (!neg16((uint16_t)(rd16(p + O_X) - rd16(obj + O_X)))) return;
    uint16_t dy = (uint16_t)(rd16(p + O_Y) - rd16(obj + O_Y));
    if (neg16(dy)) {
        if (lt16(dy, 0xFFE8)) return;
    } else if (!lt16(dy, 0x18)) {
        return;
    }
    wr16(obj + O_STATE, 0x0C);
    wr16(0x8040, 0x30);
    wr8(obj + 0x5C, rd8(obj + 0x5D));
}

/* loc_4DE12. Its player in front (right, below $80) within $30 up or down: vanishing to form
 * $28 */
static void seven_catch_front(uint16_t obj)
{
    uint16_t p = PlayerTarget();
    uint16_t dx = (uint16_t)(rd16(p + O_X) - rd16(obj + O_X));
    if (neg16(dx) || !lt16(dx, 0x80)) return;
    uint16_t dy = (uint16_t)(rd16(p + O_Y) - rd16(obj + O_Y));
    if (neg16(dy)) {
        if (lt16(dy, 0xFFD0)) return;
    } else if (!lt16(dy, 0x30)) {
        return;
    }
    wr16(obj + O_STATE, 0x0C);
    wr16(0x8040, 0x28);
    wr8(obj + 0x5C, rd8(obj + 0x5D));
}

/* loc_4DE5E. The next form at random, one not used since the last pick (+$5D's bits; the
 * last's kept in +$5C): vanishing to it (state $C, $8040 its state) */
static void seven_choose(uint16_t obj)
{
    uint16_t i = rd16(v_randomSeed) & 0xE, b;
    for (;;) {
        while (!lt16(i, 0xE)) i = (uint16_t)(i - 0xE);
        b = (uint16_t)((int16_t)i >> 1);
        if (!bit8(obj + 0x5D, b & 7)) break;
        i = (uint16_t)(i + 2);
    }
    wr8(obj + 0x5C, rd8(obj + 0x5D));
    wr8(obj + 0x5D, 0);
    set8(obj + 0x5D, b & 7);
    wr16(obj + O_STATE, 0x0C);
    wr16(0x8040, rom16(0x04DEAA + i));
}

/* $04DD86. The 29 parts from $C980: +$20 moved by `d`, their sprite word's bit 15 cleared
 * then `bits` or'd in */
static void seven_layer(uint8_t d, uint16_t bits)
{
    uint16_t a0 = 0xC980;
    for (int k = 0; k <= 28; k++) {
        wr8(a0 + 0x20, (uint8_t)(rd8(a0 + 0x20) + d));
        clr8(a0 + 0x0E, 7);
        wr16(a0 + 0x0E, rd16(a0 + 0x0E) | bits);
        a0 = (uint16_t)(a0 + 0x60);
    }
}

/* $04DBEE. Its angle (+$56) turned by `step` toward `target` (the shorter way; not within
 * 4) */
static void seven_turn(uint16_t obj, uint16_t step, uint16_t target)
{
    uint16_t d = (uint16_t)((target & 0x1FE) - rd16(obj + 0x56));
    int up;
    if (neg16(d)) {
        if (lt16(d, 0xFF00)) up = 1;
        else if (!lt16(d, 0xFFFC)) return;
        else up = 0;
    } else {
        if (!lt16(d, 0x100)) up = 0;
        else if (lt16(d, 4)) return;
        else up = 1;
    }
    if (up) add16(obj + 0x56, step);
    else wr16(obj + 0x56, (uint16_t)(rd16(obj + 0x56) - step));
    wr16(obj + 0x56, rd16(obj + 0x56) & 0x1FE);
}

/* $04DF5A. On even frames a flame ($250) at the chain's end ($CF20), from $D820 */
static void seven_flame(void)
{
    if (bit8(v_gameFrames + 1, 0)) return;
    uint16_t a0;
    if (!seven_slot(&a0)) return;
    uint16_t a1 = 0xCF20;
    wr16(a0 + O_TYPE, 0x250);
    wr16(a0 + 0x48, 1);
    wr16(a0 + 0x4A, 0x20);
    wr16(a0 + 0x0E, 0xEB18);
    wr16(a0 + 0x08, 0x0500);
    wr16(a0 + 0x0A, 0xF8F8);
    wr16(a0 + O_X, rd16(a1 + O_X));
    wr16(a0 + O_Y, rd16(a1 + O_Y));
}

/* loc_4DFE4. Every 32 frames of +$8048: a ring of pellets ($490, $498 from the fifth
 * difficulty; 4, or 8 above the first) about it, sound $C7 */
static void seven_pellets(uint16_t obj)
{
    if (rd16(0x8048) & 0x1F) return;
    uint16_t x = rd16(obj + O_X), y = rd16(obj + O_Y);
    uint16_t a = rd16(v_randomSeed) & 0x7C, type = 0x490, n, step;
    if (!rd16(v_difficulty)) {
        n = 3;
        step = 32;
    } else {
        n = 7;
        step = 16;
        if (!below16(v_difficulty, 4)) type = 0x498;
    }
    for (;;) {
        uint16_t a0;
        if (!seven_slot(&a0)) return;
        wr16(a0 + O_TYPE, type);
        wr16(a0 + O_X, x);
        wr16(a0 + O_Y, y);
        wr16(a0 + 0x48, a);
        a = (uint16_t)((a + step) & 0x7C);
        if (n-- == 0) break;
    }
    QueueSound(0xC7);
}

/* $04E052. Every 32 frames (16 from the fifth difficulty) a shot ($498, state 1) from $30
 * ahead of it (its angle + $180), its way, sound $B6 */
static void seven_shot(uint16_t obj)
{
    uint16_t mask = below16(v_difficulty, 4) ? 31 : 15;
    if (rd16(v_gameFrames) & mask) return;
    uint16_t a0;
    if (!seven_slot(&a0)) return;
    wr16(a0 + O_TYPE, 0x498);
    wr16(a0 + O_STATE, 1);
    wr16(a0 + 0x4A, 0xFFFF);
    uint16_t a = (uint16_t)((rd16(obj + 0x56) + 0x180) & 0x1FE);
    uint16_t dy = (uint16_t)(((uint32_t)((int32_t)sine(a) * 0x30) << 2) >> 16);
    uint16_t dx = (uint16_t)(((uint32_t)((int32_t)cosine(a) * 0x30) << 2) >> 16);
    wr16(a0 + O_Y, (uint16_t)(dy + rd16(obj + O_Y)));
    wr16(a0 + O_X, (uint16_t)(dx + rd16(obj + O_X)));
    wr16(a0 + 0x48, (uint16_t)(((int16_t)a >> 2) & 0x7C));
    QueueSound(0xB6);
}

/* $04E0D8. Every 32 frames (16 above the first difficulty while $804A runs) a mine ($494)
 * left behind, flung back against its speed */
static void seven_mine(uint16_t obj)
{
    uint16_t f = rd16(v_gameFrames);
    if (rd16(v_difficulty) && !neg16(rd16(0x804A))) f &= 0xF;
    else f &= 0x1F;
    if (f) return;
    uint16_t a0;
    if (!seven_slot(&a0)) return;
    wr16(a0 + O_TYPE, 0x494);
    wr16(a0 + O_FLAGS, 0x8C00);
    wr16(a0 + 0x0E, 0xEB18);
    wr16(a0 + 0x08, 0x0500);
    wr16(a0 + 0x0A, 0xF8F8);
    wr8(a0 + 0x21, 0x10);
    wr32(a0 + 0x2C, 0xFA06FA06);
    wr16(a0 + 0x26, 0x1E);
    wr16(a0 + 0x4A, 0x40);
    wr32(a0 + 0x1C, (uint32_t)-rd32(obj + 0x1C));
    wr32(a0 + 0x18, (uint32_t)-rd32(obj + 0x18));
    wr16(a0 + O_Y, rd16(obj + O_Y));
    wr16(a0 + O_X, rd16(obj + O_X));
}

/* $04E218. A blast ($454) from the part at $CB10, its way (+$56 + $100), sound $CA */
static void seven_blast(uint16_t obj)
{
    uint16_t a0;
    if (seven_slot(&a0)) {
        wr16(a0 + O_TYPE, 0x454);
        uint16_t a = (uint16_t)((rd16(obj + 0x56) + 0x100) & 0x1FE);
        uint32_t vy = (uint32_t)(int32_t)sine(a) << 5, vx = (uint32_t)(int32_t)cosine(a) << 5;
        wr32(a0 + 0x1C, vy);
        wr32(a0 + 0x18, vx);
        wr16(a0 + O_Y, (uint16_t)((vy >> 16) + rd16(0xCB14)));
        wr16(a0 + O_X, (uint16_t)((vx >> 16) + rd16(0xCB10)));
    }
    QueueSound(0xCA);
}

/* $04E2BE (4, 8) and loc_4E2C6 (-4, -8): a leg's five segments from a0 bent */
static void seven_leg_step(uint16_t a0, uint16_t d1, uint16_t d2)
{
    wr16(a0 + 0x56, (uint16_t)((rd16(a0 + 0x56) - d1) & 0x1FE));
    a0 = (uint16_t)(a0 + 0x60);
    for (int k = 0; k <= 3; k++) {
        wr16(a0 + 0x56, (uint16_t)((rd16(a0 + 0x56) - d2) & 0x1FE));
        a0 = (uint16_t)(a0 + 0x60);
    }
}

/* $04E272. The legs ($CC80, $CE60) swinging in turn, $20 frames each ($804A, $804C the leg) */
static void seven_legs(void)
{
    if (!rd16(0x804C)) {
        if (count_out(0x804A)) {
            wr16(0x804A, 0x1F);
            add16(0x804C, 1);
        }
        seven_leg_step(0xCC80, 4, 8);
        seven_leg_step(0xCE60, 0xFFFC, 0xFFF8);
        return;
    }
    if (count_out(0x804A)) {
        wr16(0x804A, 0x1F);
        wr16(0x804C, 0);
    }
    seven_leg_step(0xCE60, 4, 8);
    seven_leg_step(0xCC80, 0xFFFC, 0xFFF8);
}

/* $04E2F2 ($FF5A40 on) and loc_4E33C ($FF5B40 on): five chunks of plane A at $C630 on */
static void seven_chunks(uint32_t src)
{
    uint16_t vram = 0xC630;
    for (int k = 0; k < 5; k++) {
        ChunkDma(src, vram);
        vram = (uint16_t)(vram + 8);
        src += 0x20;
    }
}

/* $04E386. Every 16 frames a puff of smoke ($100, $563EE) blown back and up */
static void seven_smoke(uint16_t obj)
{
    if (rd16(v_gameFrames) & 0xF) return;
    uint16_t a0;
    if (!seven_slot(&a0)) return;
    wr16(a0 + O_TYPE, 0x100);
    wr32(a0 + 0x48, 0x0563EE);
    wr16(a0 + O_X, rd16(obj + O_X));
    wr16(a0 + O_Y, rd16(obj + O_Y));
    wr32(a0 + 0x18, 0xFFFCE000);
    wr32(a0 + 0x1C, 0xFFFF8000);
    wr16(a0 + O_Y, (uint16_t)(rd16(a0 + O_Y) - (rd16(v_randomSeed) & 0xF)));
}

/* loc_25D9E (SevenEBreathe's, objects26.c): some segments' radii (from their +$52) pushed out
 * and drawn in ($9212: up by 2 to $40, then down by 8 to 0) */
static void seven_breathe(void)
{
    if (!rd16(0x9210)) {
        add16(0x9212, 2);
        if (!below16(0x9212, 0x40)) {
            wr16(0x9210, 2);
            wr16(0x9212, 0x40);
        }
    } else {
        add16(0x9212, (uint16_t)-8);
        if (neg16(rd16(0x9212))) {
            wr16(0x9210, 0);
            wr16(0x9212, 0);
        }
    }
    int16_t d0 = (int16_t)((int16_t)rd16(0x9212) >> 2), d1 = (int16_t)(d0 >> 1);
    int16_t d2 = (int16_t)(d1 >> 1), d3 = (int16_t)(d2 >> 1);
    uint16_t v = (uint16_t)(rd16(0xCA92) + d1);
    wr16(0xCA94, v);
    wr16(0xCEB4, v);
    v = (uint16_t)(rd16(0xCB52) + d2);
    wr16(0xCB54, v);
    wr16(0xCF74, v);
    v = (uint16_t)(rd16(0xCAF2) - d0);
    wr16(0xCAF4, v);
    wr16(0xCF14, v);
    v = (uint16_t)(rd16(0xCBB2) - d1);
    wr16(0xCBB4, v);
    wr16(0xCFD4, v);
    v = (uint16_t)(rd16(0xCDF2) - d0);
    wr16(0xCDF4, v);
    wr16(0xD214, v);
    v = (uint16_t)(rd16(0xCC12) - d3);
    wr16(0xCC14, v);
    wr16(0xD034, v);
    v = (uint16_t)(rd16(0xCCD2) - d2);
    wr16(0xCCD4, v);
    wr16(0xD0F4, v);
    v = (uint16_t)(rd16(0xCD92) - d1);
    wr16(0xCD94, v);
    wr16(0xD1B4, v);
}

/* $04D33A. Form A: its colours, 17 + 12 segments from itself (sprite word $8000), its boxes,
 * bobbing (the pose $4E57E over 18), still */
static void seven_form_a(uint16_t obj)
{
    BossColours(0x027FC2);
    uint32_t look = 0xFFFFE3F8;     /* a3 as BossColours leaves it */
    wr16(0x8040, 0x8000);
    uint16_t slot = OrbiterSetupGrow(obj, 0x04E528, 0x04E56C, 0x027B30, 16, obj, &look);
    OrbiterSetupGrow(obj, 0x04E53C, 0x04E571, 0x027B52, 11, slot, &look);
    seven_box(obj, 0x04DEDC);
    wr16(0x8048, 0x30);
    SevenPoseBob(obj, 0x04E57E, 18);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
}

/* loc_4D584. Form B: its colours, 10 + 4 segments ($CF80 on the second), the chain, its
 * boxes, the chain's end's; moving right ($1 or $2 by its x) */
static void seven_form_b(uint16_t obj)
{
    wr16(obj + O_STATE, 0x2C);
    BossColours(0x02800A);
    uint32_t look = 0xFFFFE3F8;     /* a3 as BossColours leaves it */
    wr16(0x8040, 0x8000);
    OrbiterSetupGrow(obj, 0x04E5B4, 0x04E5EC, 0x027C4C, 9, obj, &look);
    OrbiterSetupGrow(obj, 0x04E5DC, 0x04E5F6, 0x027C60, 3, 0xCF80, &look);
    wr16(0x8040, 0xC);
    SevenChainSetup(0x027ACE, 0x027AC8);
    wr16(obj + O_TYPE, 0x488);
    seven_box(obj, 0x04DEE6);
    seven_part_box(0x04DF46, 0xCF20);
    wr16(0x8048, 0x50);
    wr16(obj + 0x54, 0x100);
    wr32(obj + 0x58, 0x027C68);
    wr16(obj + 0x5E, 0);
    wr32(obj + 0x1C, 0);
    QueueSound(0xB7);
    if (!below16(obj + O_X, 0xE0)) wr32(obj + 0x18, 0x10000);
    else wr32(obj + 0x18, 0x20000);
}

/* loc_4D658. Form C: its colours, 11 + 6 + 2 segments, at rest, its boxes; leaping up-left,
 * sound $A3 */
static void seven_form_c(uint16_t obj)
{
    wr16(obj + O_STATE, 0x34);
    BossColours(0x027FE6);
    uint32_t look = 0xFFFFE3F8;     /* a3 as BossColours leaves it */
    wr16(0x8040, 0x8000);
    uint16_t slot = OrbiterSetupGrow(obj, 0x04E5FA, 0x04E62E, 0x027CCE, 10, obj, &look);
    slot = OrbiterSetupGrow(obj, 0x04E60E, 0x04E633, 0x027CE4, 5, slot, &look);
    OrbiterSetupGrow(obj, 0x04E626, 0x04E639, 0x027CF0, 1, slot, &look);
    SevenCReset(obj);
    seven_box(obj, 0x04DEF0);
    wr16(0x8048, 0x28);
    wr16(obj + O_TYPE, 0x488);
    wr16(obj + 0x54, 0x100);
    wr16(obj + 0x56, 0);
    wr32(obj + 0x18, 0xFFFA8000);
    wr32(obj + 0x1C, 0xFFFE8000);
    QueueSound(0xA3);
}

/* $04D2C4. Once no job runs and the sines are free: its tiles' job ($4D2F6) queued */
void SevenSpaceLoad(uint16_t obj)
{
    wr16(0x8050, 0);
    wr16(obj + 0x5C, 0);
    if (rd16(v_jobKind)) return;
    if (rd16(0x80C2)) return;
    add16(obj + O_STATE, 4);
    uint32_t list = 0x04D2F6;
    NextJob(&list);
}

/* $04D300. Loaded: plane A's chunks, $20 frames (state $5C), sound $B5 */
void SevenSpaceLoaded(uint16_t obj)
{
    if (rd16(v_jobKind)) return;
    wr16(obj + O_STATE, 0x5C);
    wr16(0x8048, 0x20);
    seven_chunks(0xFFFF5A40);
    QueueSound(0xB5);
}

/* $04D324. Form A, facing its player, sound $A6 */
void SevenSpaceFormA(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    seven_form_a(obj);
    seven_face(obj);
    QueueSound(0xA6);
}

/* $04D9BE. Vanishing (turning 8 a frame) while +$50 counts below -$10; then its parts gone,
 * its joints cleared */
void SevenSpaceVanish(uint16_t obj)
{
    wr16(obj + 0x56, (uint16_t)((rd16(obj + 0x56) - 8) & 0x1FE));
    if (!count_out(obj + 0x50)) return;
    if (!below16(obj + 0x50, 0xFFF0)) return;
    add16(obj + O_STATE, 4);
    ObjectsVanishFrom(0xC9E0, 0, 0);
    SevenJointsClear();
}

/* $04D9F4. The next form's state ($8040) */
void SevenSpaceNext(uint16_t obj)
{
    wr16(obj + O_STATE, rd16(0x8040));
}

/* $04D9FC. Form A bobbing for $8048 frames (catching its player behind it unless +$5C bit
 * 2); then by chance another form, or the pose $4E590 over 40 to fire */
void SevenSpaceABob(uint16_t obj)
{
    seven_recover(obj);
    if (neg16(rd16(obj + 0x4A))) seven_face_now(obj);
    if (count_out(0x8048)) {
        if (bit8(v_randomSeed + 1, 0)) {
            wr32(obj + 0x1C, 0);
            seven_choose(obj);
            return;
        }
        add16(obj + O_STATE, 4);
        SevenPoseBob(obj, 0x04E590, 40);
        return;
    }
    if (neg16(rd16(obj + 0x50))) {
        seven_run_bob(obj);
        return;
    }
    if (!bit8(obj + 0x5C, 2)) seven_catch(obj);
    seven_run_bob(obj);
}

/* $04DA64. The pose run out: the firing pose ($4E5A2 over 12), 4 frames; catching */
void SevenSpaceAPose(uint16_t obj)
{
    if (!neg16(rd16(obj + 0x4A))) {
        seven_run_bob(obj);
        return;
    }
    add16(obj + O_STATE, 4);
    wr16(0x8048, 4);
    SevenPoseBob(obj, 0x04E5A2, 12);
    seven_catch(obj);
    seven_catch_front(obj);
}

/* $04DA8A. After $8048 frames a shard ($48C, its kind 2) $20 ahead of it the way it faces
 * (in the last four slots); the pose run out: bobbing again ($40 frames, the pose $4E57E over
 * 32) */
void SevenSpaceAFire(uint16_t obj)
{
    wr16(0x8048, (uint16_t)(rd16(0x8048) - 1));
    if (rd16(0x8048)) {
        if (!neg16(rd16(obj + 0x4A))) {
            seven_run_bob(obj);
            return;
        }
        wr16(obj + O_STATE, (uint16_t)(rd16(obj + O_STATE) - 8));
        wr16(0x8048, 0x40);
        SevenPoseBob(obj, 0x04E57E, 32);
        seven_catch_front(obj);
        return;
    }
    uint16_t a0;
    if (!SlotFreeLast(&a0)) {
        seven_run_bob(obj);
        return;
    }
    wr16(a0 + O_TYPE, 0x48C);
    wr16(a0 + O_FLAGS, 0x8000);
    wr16(a0 + 0x0E, 0xC4B9);
    wr16(a0 + 0x08, 0x0A00);
    wr16(a0 + 0x0A, 0xF4F4);
    wr16(a0 + 0x4A, 2);
    wr16(a0 + O_Y, rd16(obj + O_Y));
    uint16_t x = rd16(obj + O_X);
    if (rd16(obj + 0x54)) {
        wr16(a0 + O_X, (uint16_t)(x - 0x20));
        wr16(a0 + 0x48, 0x100);
        return;
    }
    wr16(a0 + O_X, (uint16_t)(x + 0x20));
}

/* $04D566. Form B (or, unless +$5C bit 0, at x $E0 on: form C, +$5C $402) */
void SevenSpaceFormB(uint16_t obj)
{
    if (!bit8(obj + 0x5C, 0) && !below16(obj + O_X, 0xE0)) {
        wr16(obj + 0x5C, 0x402);
        seven_form_c(obj);
        return;
    }
    seven_form_b(obj);
}

/* $04DB10. Form B: flames from the chain's end, turning $C a frame, its pose; then another
 * form */
void SevenSpaceBRun(uint16_t obj)
{
    seven_flame();
    seven_recover(obj);
    wr16(obj + 0x56, (uint16_t)((rd16(obj + 0x56) - 0xC) & 0x1FE));
    SevenBPose(obj);
    if (!count_out(0x8048)) return;
    seven_choose(obj);
}

/* $04D644. Form C (or, left of x $100: form B, +$5C $204) */
void SevenSpaceFormC(uint16_t obj)
{
    if (below16(obj + O_X, 0x100)) {
        wr16(obj + 0x5C, 0x204);
        seven_form_b(obj);
        return;
    }
    seven_form_c(obj);
}

/* $04DB36. Form C: falling ($1000), its beam aimed, walking; then still, another form */
void SevenSpaceCRun(uint16_t obj)
{
    add32(obj + 0x1C, 0x1000);
    seven_recover(obj);
    SevenCBeamAim();
    SevenCGait(obj);
    SevenCLimbs();
    if (!count_out(0x8048)) return;
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    seven_choose(obj);
}

/* $04D3A8. Form D: its colours, 9 + 8 segments, its boxes, sound $B9; $80 frames after forms
 * $3400 of +$5C, else $180 */
void SevenSpaceFormD(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    BossColours(0x027FFE);
    uint32_t look = 0xFFFFE3F8;     /* a3 as BossColours leaves it */
    wr16(0x8040, 0x8000);
    uint16_t slot = OrbiterSetupGrow(obj, 0x04E6D2, 0x04E6F6, 0x027EB6, 8, obj, &look);
    OrbiterSetupGrow(obj, 0x04E6D6, 0x04E6F7, 0x027EC8, 7, slot, &look);
    wr16(obj + O_TYPE, 0x488);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    seven_box(obj, 0x04DEFA);
    QueueSound(0xB9);
    if (rd16(obj + 0x5C) & 0x3400) wr16(0x8048, 0x80);
    else wr16(0x8048, 0x180);
}

/* $04DB68. Form D: spinning ($10 a frame), its spokes; every 32 frames pellets; then another
 * form */
void SevenSpaceDRun(uint16_t obj)
{
    wr16(obj + 0x56, (uint16_t)((rd16(obj + 0x56) - 0x10) & 0x1FE));
    seven_recover(obj);
    SevenDSpokes();
    SevenDSpokes2();
    wr16(0x8048, (uint16_t)(rd16(0x8048) - 1));
    if (rd16(0x8048) && !neg16(rd16(0x8048))) {
        seven_pellets(obj);
        return;
    }
    seven_choose(obj);
}

/* $04D430. Form G: its colours, 16 segments, its boxes, facing its player, at rest, sound
 * $BA; spinning first ($40 frames) on the first difficulty, or above it while its vitality is
 * $578 on below the fifth; $804A $80 after forms $3200 of +$5C, else $180 */
void SevenSpaceFormG(uint16_t obj)
{
    uint16_t diff = rd16(v_difficulty);
    if (diff && (below16(obj + O_VITALITY, 0x578) || !lt16(diff, 4))) {
        add16(obj + O_STATE, 8);
    } else {
        add16(obj + O_STATE, 4);
        wr16(0x8048, 0x40);
    }
    BossColours(0x027FF2);
    uint32_t look = 0xFFFFE3F8;     /* a3 as BossColours leaves it */
    wr16(0x8040, 0x8000);
    OrbiterSetupGrow(obj, 0x04E700, 0x04E740, 0x027D7A, 15, obj, &look);
    wr16(obj + O_TYPE, 0x488);
    wr16(obj + 0x54, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    seven_box(obj, 0x04DF04);
    wr16(obj + 0x56, (uint16_t)((seven_aim(obj) + 0x80) & 0x1FE));
    SevenGRest();
    QueueSound(0xBA);
    if (rd16(obj + 0x5C) & 0x3200) wr16(0x804A, 0x80);
    else wr16(0x804A, 0x180);
}

/* $04DB94. Form G spinning ($18 a frame) for $8048 frames */
void SevenSpaceGSpin(uint16_t obj)
{
    seven_recover(obj);
    wr16(obj + 0x56, (uint16_t)((rd16(obj + 0x56) - 0x18) & 0x1FE));
    if (count_out(0x8048)) add16(obj + O_STATE, 4);
}

/* $04DBB2. Form G for $804A frames: every 32 a blast and a quarter turn; else turning (6) to
 * face away from its player; then another form */
void SevenSpaceGRun(uint16_t obj)
{
    if (count_out(0x804A)) {
        seven_choose(obj);
        return;
    }
    seven_recover(obj);
    if (!neg16(rd16(obj + 0x50)) && !(rd16(0x804A) & 0x1F)) {
        seven_blast(obj);
        wr16(obj + 0x56, (uint16_t)((rd16(obj + 0x56) + 0x40) & 0x1FE));
        return;
    }
    seven_turn(obj, 6, (uint16_t)(seven_aim(obj) + 0x100));
}

/* $04D6FE. Form F: its colours, 30 segments, its boxes and its feet's, at rest, facing its
 * player, sound $B2; $180 frames ($80 after form 0 of +$5C) */
void SevenSpaceFormF(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    BossColours(0x027FDA);
    uint32_t look = 0xFFFFE3F8;     /* a3 as BossColours leaves it */
    wr16(0x8040, 0x8000);
    OrbiterSetupGrow(obj, 0x04E63C, 0x04E6B4, 0x027E30, 29, obj, &look);
    seven_box(obj, 0x04DF18);
    wr16(obj + O_TYPE, 0x488);
    wr16(obj + 0x54, 0);
    wr16(0x804A, 0xF);
    wr16(0x804C, 0);
    SevenFRest(obj);
    wr16(obj + 0x56, (uint16_t)((seven_aim(obj) + 0x80) & 0x1FE));
    seven_part_box(0x04DF50, 0xCCE0);
    seven_part_box(0x04DF50, 0xCEC0);
    QueueSound(0xB2);
    wr16(0x8048, 0x180);
    if (bit8(obj + 0x5C, 0)) wr16(0x8048, 0x80);
}

/* $04DC36. Form F: turning (2) toward its player, walking ($4 up, $8 across its way), legs
 * swinging, shots; catching its player after forms $4800 of +$5C; then another form */
void SevenSpaceFRun(uint16_t obj)
{
    seven_recover(obj);
    if (count_out(0x8048)) {
        seven_choose(obj);
        return;
    }
    if (!neg16(rd16(obj + 0x50))) seven_shot(obj);
    seven_turn(obj, 2, (uint16_t)(seven_aim(obj) + 0x80));
    uint16_t a = (uint16_t)((rd16(obj + 0x56) + 0x180) & 0x1FE);
    wr32(obj + 0x1C, (uint32_t)(int32_t)sine(a) << 2);
    wr32(obj + 0x18, (uint32_t)(int32_t)cosine(a) << 3);
    seven_legs();
    if (neg16(rd16(obj + 0x50))) return;
    if (!(rd16(obj + 0x5C) & 0x4800)) return;
    seven_catch(obj);
}

/* $04D4E0. Form E: its colours, 23 segments, its boxes, its pose, facing its player, sound
 * $C7; $180 frames ($80 after forms $700 of +$5C) */
void SevenSpaceFormE(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    BossColours(0x027FCE);
    uint32_t look = 0xFFFFE3F8;     /* a3 as BossColours leaves it */
    wr16(0x8040, 0x8000);
    OrbiterSetupGrow(obj, 0x04E750, 0x04E7AC, 0x027F4C, 22, obj, &look);
    seven_box(obj, 0x04DF0E);
    wr16(obj + O_TYPE, 0x488);
    wr16(obj + 0x54, 0);
    wr16(0x804A, 0xFFFF);
    SevenEPose(obj);
    wr16(obj + 0x56, (uint16_t)((seven_aim(obj) + 0x80) & 0x1FE));
    QueueSound(0xC7);
    if (!(rd16(obj + 0x5C) & 0x700)) wr16(0x8048, 0x180);
    else wr16(0x8048, 0x80);
}

/* $04DCAC. Form E: charging at its player ($8 a frame its way; once close across and up or
 * down, turned away for $40 frames, unless after forms $700 of +$5C), mines behind it,
 * breathing; then another form */
void SevenSpaceERun(uint16_t obj)
{
    seven_recover(obj);
    if (count_out(0x8048)) {
        seven_choose(obj);
        return;
    }
    if (!neg16(rd16(obj + 0x50))) {
        if (!(rd16(obj + 0x5C) & 0x700)) {
            uint16_t p = PlayerTarget(), near = 0;
            uint16_t dx = (uint16_t)(rd16(p + O_X) - rd16(obj + O_X));
            if (!neg16(dx)) {
                if (lt16(dx, 0x44)) near++;
            } else if (!lt16(dx, 0xFFBC)) {
                near++;
            }
            uint16_t dy = (uint16_t)(rd16(p + O_Y) - rd16(obj + O_Y));
            int close = neg16(dy) ? !lt16(dy, 0xFFC8) : lt16(dy, 0x38);
            if (close && near) wr16(0x804A, 0x40);
        }
        seven_mine(obj);
    }
    uint16_t to = (uint16_t)(seven_aim(obj) + 0x80);
    if (!neg16(rd16(0x804A))) {
        wr16(0x804A, (uint16_t)(rd16(0x804A) - 1));
        to = (uint16_t)(to + 0x100);
    }
    seven_turn(obj, 4, to);
    uint16_t a = (uint16_t)((rd16(obj + 0x56) + 0x180) & 0x1FE);
    wr32(obj + 0x1C, (uint32_t)(int32_t)sine(a) << 3);
    wr32(obj + 0x18, (uint32_t)(int32_t)cosine(a) << 3);
    seven_breathe();
}

/* $04D7A0. After $8048 frames: plane A's other chunks, $20 frames */
void SevenSpaceWait(uint16_t obj)
{
    if (!count_out(0x8048)) return;
    add16(obj + O_STATE, 4);
    wr16(0x8048, 0x20);
    seven_chunks(0xFFFF5B40);
}

/* $04D7B6. Then form A rising from below (y $730 less the camera's), its parts behind the
 * planes ($20 + $88), without collisions */
void SevenSpaceRise(uint16_t obj)
{
    if (count_out(0x8048)) {
        add16(obj + O_STATE, 4);
        seven_form_a(obj);
        seven_layer(0x88, 0);
        wr16(obj + O_X, 0x110);
        wr16(obj + O_Y, (uint16_t)(0x730 - rd16(v_camY)));
        wr32(obj + 0x1C, 0xFFF98000);
        wr32(obj + 0x18, 0xFFFF0000);
        wr8(obj + 0x21, 0);
        wr16(obj + 0x54, 0x100);
    }
    seven_recover(obj);
    SevenAPoseRun(obj);
}

/* $04D806. Flying off right (turning 2 every other frame, slowing up); its parts back in
 * front; past x $3C0: still, the sines' work ($80C2 8, $80C8 $39AB0) */
void SevenSpaceFly(uint16_t obj)
{
    if (!bit8(v_gameFrames + 1, 0)) wr16(obj + 0x56, (uint16_t)((rd16(obj + 0x56) + 2) & 0x1FE));
    add32(obj + 0x18, 0x1800);
    add32(obj + 0x1C, 0x3000);
    if (!neg32(rd32(obj + 0x1C)) || !below32(obj + 0x1C, 0xFFFFC000)) {
        wr32(obj + 0x1C, 0);
        seven_layer(0, 0);
    }
    if (!below16(obj + O_X, 0x3C0)) {
        add16(obj + O_STATE, 4);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr16(0x80C2, 8);
        wr32(0x80C8, 0x039AB0);
    }
    seven_recover(obj);
    SevenAPoseRun(obj);
}

/* $04D874. Once the sines are free: coming back from the right at $1E0,$100, its parts behind
 * ($88, bit 15) */
void SevenSpaceReturn(uint16_t obj)
{
    if (rd16(0x80C2)) return;
    add16(obj + O_STATE, 4);
    wr16(obj + O_X, 0x1E0);
    wr16(obj + O_Y, 0x100);
    wr32(obj + 0x18, 0xFFFE8000);
    wr16(obj + 0x56, 0);
    seven_layer(0x88, 0x8000);
}

/* $04D8A6. Stopping at x $1A0, $20 frames */
void SevenSpaceArrive(uint16_t obj)
{
    if (below16(obj + O_X, 0x1A0)) {
        add16(obj + O_STATE, 4);
        wr16(0x804E, 0x20);
        wr32(obj + 0x18, 0);
    }
    SevenAPoseRun(obj);
}

/* $04D8C4. Then fighting: the sines taken, $1690 of vitality, form A bobbing (state $1C, form
 * 0 used), kept on the screen, beatable ($8050 bits 2, 3) */
void SevenSpaceReady(uint16_t obj)
{
    if (count_out(0x804E)) {
        wr16(0x80C2, 0x10);
        wr16(0x80D6, obj);
        wr16(obj + O_VITALITY, 0x1690);
        wr8(obj + 0x21, 0x30);
        wr16(obj + O_STATE, 0x1C);
        set8(obj + 0x5D, 0);
        set8(0x8050, 2);
        set8(0x8050, 3);
        wr16(0x8048, 0x40);
    }
    SevenAPoseRun(obj);
}

/* $04D906. Beaten: blasts, form A again without collisions, drifting left and down */
void SevenSpaceBeaten(uint16_t obj)
{
    add16(obj + O_STATE, 4);
    BlastSound8();
    seven_form_a(obj);
    wr8(obj + 0x21, 0);
    wr16(obj + 0x54, 0);
    wr16(obj + 0x56, 0x20);
    wr32(obj + 0x18, 0xFFFF8000);
    wr16(obj + 0x1E, 0x8000);
}

/* $04D932. Falling off with blasts and smoke, shaking; off the screen: its parts gone, $80
 * frames */
void SevenSpaceFalls(uint16_t obj)
{
    BlastSound8();
    if (below16(obj + O_X, 0x40) || !below16(obj + O_Y, 0x1A0)) {
        add16(obj + O_STATE, 4);
        wr16(0x8048, 0x80);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        ObjectsVanishFrom(0xC9E0, 0, 0);
        return;
    }
    seven_recover(obj);
    seven_smoke(obj);
    SevenAPoseRun(obj);
    if (rd16(v_gameFrames) & 3) return;
    if (!bit8(v_gameFrames + 1, 2)) wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 2));
    else add16(obj + O_Y, 2);
}

/* $04D99C. After $8048 frames: the camera let go, it removed, $FF8000-$FF9FFF cleared
 * (loc_2A322) */
void SevenSpaceGone(uint16_t obj)
{
    BlastSound8();
    if (!count_out(0x8048)) return;
    clr8(v_cameraHold, 7);
    wr16(obj + O_FLAGS, 0x1000);
    for (uint16_t k = 0; k <= 0x7FF; k++) wr32((uint16_t)(0x8000 + 4 * k), 0);
}

/* $04D196. Its colours and flash; beatable ($8050 bit 3) and its vitality spent: 777,770
 * points, vanishing to the beaten state ($74); kept on the screen ($8050 bit 2: x $90-$1AF, y
 * $A0-$14F) */
void SevenSpace(uint16_t obj)
{
    SpaceColours();
    SpaceColoursFlash();
    SevenFlash(obj);
    if (bit8(0x8050, 3) && neg16(rd16(obj + O_VITALITY))) {
        AddScore(obj, 0x77777);
        wr16(obj + O_STATE, 0x0C);
        wr16(0x8040, 0x74);
        wr16(obj + O_VITALITY, 0);
        wr8(obj + 0x21, 0);
        wr8(0x8050, 0);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
    }
    if (bit8(0x8050, 2)) {
        if (!below16(obj + O_X, 0x1B0)) wr16(obj + O_X, 0x1AE);
        else if (below16(obj + O_X, 0x90)) wr16(obj + O_X, 0x92);
        if (!below16(obj + O_Y, 0x150)) wr16(obj + O_Y, 0x14E);
        else if (below16(obj + O_Y, 0xA0)) wr16(obj + O_Y, 0xA2);
    }
    dispatch(0x04D244, obj);
}

/* ---- type $48C: form A's shard: after +$4A frames, sound $D4 and three shards flying apart
 * ($40 either side of +$48; the others in the last slots), each at half the sine and cosine as
 * its pull; struck: a puff; off x $80-$1BF removed ---- */

/* $04E428. Object a0 made a shard at it, flying at angle `a` */
static void shard_make(uint16_t obj, uint16_t a0, uint16_t a)
{
    add16(a0 + O_STATE, 4);
    wr16(a0 + O_TYPE, 0x48C);
    wr16(a0 + O_FLAGS, 0x8E00);
    wr16(a0 + 0x08, 0x0F00);
    wr16(a0 + 0x0A, 0xF0F0);
    wr16(a0 + 0x26, 0xD);
    wr8(a0 + 0x21, 0x10);
    wr32(a0 + 0x2C, 0xF40CF808);
    wr16(a0 + O_X, rd16(obj + O_X));
    wr16(a0 + O_Y, rd16(obj + O_Y));
    a &= 0x1FE;
    uint16_t s = (uint16_t)(sine(a) >> 1), c = (uint16_t)(cosine(a) >> 1);
    wr16(a0 + 0x48, s);
    wr16(a0 + 0x4A, c);
    wr16(a0 + 0x0E, neg16(c) ? 0xC36A : 0xCB6A);
}

/* $04E3D0. Type $48C: as the section says */
void SevenSpaceShard(uint16_t obj)
{
    if (!rd16(obj + O_STATE)) {
        if (!count_out(obj + 0x4A)) return;
        QueueSound(0xD4);
        uint16_t a0;
        if (SlotFreeLast(&a0)) {
            shard_make(obj, a0, (uint16_t)(rd16(obj + 0x48) - 0x40));
            if (SlotFreeLast(&a0)) shard_make(obj, a0, (uint16_t)(rd16(obj + 0x48) + 0x40));
        }
        shard_make(obj, obj, rd16(obj + 0x48));
        return;
    }
    if (bit8(obj + 0x22, 4)) {
        wr16(obj + O_TYPE, 0x100);
        wr32(obj + 0x48, 0x0563EE);
        wr8(obj + 0x21, 0);
        wr32(obj + 0x18, 0);
        wr32(obj + 0x1C, 0);
        wr16(obj + O_STATE, 0);
        QueueSound(0xCE);
        return;
    }
    if (!below16(obj + O_X, 0x1C0) || below16(obj + O_X, 0x80)) wr16(obj + O_FLAGS, 0x1000);
    add32(obj + 0x1C, (uint32_t)(int32_t)(int16_t)rd16(obj + 0x48));
    add32(obj + 0x18, (uint32_t)(int32_t)(int16_t)rd16(obj + 0x4A));
}

/* ---- type $494: form E's mine: after +$4A frames, or struck, a puff, sound $CA ---- */

/* $04E4F0. Type $494: as the section says */
void SevenSpaceMine(uint16_t obj)
{
    if (!count_out(obj + 0x4A) && !bit8(obj + 0x22, 4)) return;
    wr16(obj + O_TYPE, 0x100);
    wr32(obj + 0x48, 0x0563EE);
    wr8(obj + 0x21, 0);
    wr32(obj + 0x18, 0);
    wr32(obj + 0x1C, 0);
    wr16(obj + O_STATE, 0);
    QueueSound(0xCA);
}
