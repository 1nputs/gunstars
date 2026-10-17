/* The players' states (PlayerStates, the table at $0F72C, a routine per state times 4): standing,
 * walking, crouching, jumping, sliding, hanging, throwing and being thrown, hit, dying, and the
 * scenes'. A state's routine often starts another's: the "start_" functions set a state up and
 * most go straight on into its routine, as the 68k falls into it. */
#include "game.h"

/* a player's fields (player.c has the input's) */
#define P_SPRITE   0x0E          /* byte: bit 3 flipped (facing left), bit 4 upside down */
#define P_VX       0x18          /* long: the speeds, 16.16 */
#define P_VY       0x1C
#define P_HURT     0x21          /* byte: bit 7 can be hurt; bit 0 cleared each frame */
#define P_HIT      0x22          /* byte: bit 7 hit, bit 0 throwing what it holds, bit 1 grabbed */
#define P_STATUS   0x23          /* byte: bit 2 blinking, bit 3 touching an item, bit 4 knocked down */
#define P_BOTTOM   0x29
#define P_HOLDING  0x40          /* the object it carries */
#define P_TOUCHING 0x42          /* the object it touches (an item, what hit it) */
#define P_HITS     0x47
#define P_HELD     0x69
#define P_PRESSED  0x6A
#define P_WEAPON_A 0x6E
#define P_WEAPON_B 0x70
#define P_WEAPON   0x72
#define P_FLOOR_KIND 0x76
#define P_JUMP     0x78          /* the jump's way: 0 none yet, 4 up, 8 forward, $C back */
#define P_KNOCK_TIME 0x7E        /* frames left of being knocked back */
#define P_DISPLAY  0x81          /* byte: bit 2 the vitality's display is to be redrawn */
#define P_FLOOR    0x82
#define P_WALL     0x84
#define P_ANIM_SET 0x90          /* the animation to start (sub_16BF6) */
#define P_TIMER    0x92
#define P_SHOT     0x94          /* the weapon's: the shot's word and byte */
#define P_SHOT_B   0x96
#define P_KNOCK    0x97          /* byte: bit 1 knocked from behind, bit 2 from below */
#define P_THROWN   0xAE          /* the object last thrown */
#define P_VITALITY_SHOWN 0xBE
#define P_CHARACTER 0xCA         /* 0 Red, else Blue */
#define P_PAUSE    0xD2          /* frames an attack that hit holds still */


#define P_AIM      0x06          /* the aim's direction (a byte, $00 right ... $80 left) and 0 */
#define P_TURNED   0x87          /* byte: bit 7 turned round this frame (PlayerFace) */
#define P_ATTACK_DIR 0xAC        /* the melee attack's direction */
#define P_MOTION   0xCE

static void start_knocked(uint16_t obj);
static void fall_out(uint16_t obj);
static void throw_held(uint16_t obj, uint16_t state);
static void grabbed(uint16_t obj);
static void start_shoot(uint16_t obj);
static void start_wait(uint16_t obj);
static void start_crouch(uint16_t obj);
static void start_walk(uint16_t obj);
static void start_fall(uint16_t obj);
static void start_shoot_fall(uint16_t obj);
static void start_jump(uint16_t obj);
static void start_shoot_jump(uint16_t obj);
static void start_slide(uint16_t obj);
static void start_drop(uint16_t obj);
static void start_crouch_shoot(uint16_t obj);
static void start_walk_shoot(uint16_t obj);
static void start_wait_crouch(uint16_t obj);
static void start_special(uint16_t obj);
static void start_punch(uint16_t obj);
static void start_guard(uint16_t obj);
static void attack_pause_hold(uint16_t obj);
static void start_punch_air(uint16_t obj);
static void start_fall_on(uint16_t obj);
static void start_shoot_fall_on(uint16_t obj);
static void start_air_attack(uint16_t obj);
static void start_wall(uint16_t obj);
static void start_hang(uint16_t obj);
static void start_hang_shoot(uint16_t obj);
static void start_climb(uint16_t obj);
static void start_hurt_hanging(uint16_t obj);
static void start_air_attack_up(uint16_t obj);
static void die(uint16_t obj);
static void start_get_up(uint16_t obj);
static void land_knocked(uint16_t obj);
static void start_thrown_by_player(uint16_t obj);
static void start_thrown_by_enemy(uint16_t obj);
static void start_hurt_air(uint16_t obj);
static void start_hurt(uint16_t obj);
static void start_hurt_slope(uint16_t obj);
static void start_down(uint16_t obj);

/* the shot cleared */
static void no_shot(uint16_t obj)
{
    wr8(obj + P_SHOT_B, 0);
    wr16(obj + P_SHOT, 0);
}

static void still(uint16_t obj)
{
    wr32(obj + P_VX, 0);
    wr32(obj + P_VY, 0);
}

static void flags_or(uint16_t obj, uint16_t f) { wr16(obj + O_FLAGS, (uint16_t)(rd16(obj + O_FLAGS) | f)); }

/* $00F64E. The state's routine, after what happened to the player since: its vitality changed
 * (the display redrawn), a fall below the level, a hit (knocked back, or down when it was against
 * two walls or a floor and a bar), what it carried thrown, a grab, an item touched with Down and
 * B. */
void PlayerStates(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) != rd16(obj + P_VITALITY_SHOWN)) {
        wr16(obj + P_VITALITY_SHOWN, rd16(obj + O_VITALITY));
        set8(obj + P_DISPLAY, 2);
    }
    uint16_t state = rd16(obj + O_STATE);
    if (state != 0x64 && state != 0x2C && rd16(v_deathLine)
        && (int16_t)((int8_t)rd8(obj + P_BOTTOM) + rd16(obj + O_Y)) >= (int16_t)rd16(v_deathLine)) {
        fall_out(obj);
        return;
    }
    if (!bit8(obj + P_STATUS, 2) && bit8(obj + P_HURT, 7)) {
        uint8_t hits = rd8(obj + P_HITS);
        if (((hits & 0x08) && (hits & 0x04)) || ((hits & 0x02) && (hits & 0x01))) {
            set8(obj + P_HIT, 7);
            wr16(obj + O_VITALITY, (uint16_t)(rd16(obj + O_VITALITY) - 0x14));
            set8(obj + P_STATUS, 4);
            wr8(obj + P_KNOCK, 0);
            start_knocked(obj);
            return;
        }
    }
    clr8(obj + P_HURT, 0);
    if (bit8(obj + P_HIT, 0)) {
        clr8(obj + P_HIT, 0);
        uint16_t held = rd16(obj + P_HOLDING);
        wr32(held + O_X, rd32(obj + O_X));
        wr32(held + O_Y, rd32(obj + O_Y));
        wr32(held + P_VX, 0);
        wr32(held + P_VY, 0);
        state = rd16(obj + O_STATE);
        throw_held(obj, state == 0x18 || state == 0x1C || state == 0x28 ? 0x50 : 0x4C);
        return;
    }
    if (bit8(obj + P_HIT, 1)) {
        clr8(obj + P_HIT, 1);
        grabbed(obj);
        return;
    }
    if (bit8(obj + P_HIT, 7)) PlayerKnockTimer(obj);
    else if (bit8(obj + P_STATUS, 3)) {
        clr8(obj + P_STATUS, 3);
        PlayerPickUp(obj);
    }
    uint32_t at = 0x00F72C + (uint32_t)((int16_t)rd16(obj + O_STATE) >> 1);
    uint32_t fn = at + (uint32_t)(int16_t)rom16(at);
    switch (fn) {
    case 0x00F97C: PlayerStart(obj); return;
    case 0x00F9AC: PlayerStand(obj); return;
    case 0x00FABC: PlayerShootStand(obj); return;
    case 0x00FD0A: PlayerCrouch(obj); return;
    case 0x00FDCC: PlayerShootCrouch(obj); return;
    case 0x00FFD2: PlayerWalk(obj); return;
    case 0x0104D6: PlayerJump(obj); return;
    case 0x0105E0: PlayerFall(obj); return;
    case 0x0106F2: PlayerShootJump(obj); return;
    case 0x01078C: PlayerShootFall(obj); return;
    case 0x01115A: PlayerDead(obj); return;
    case 0x010F56: PlayerDown(obj); return;
    case 0x010874: PlayerAirAttack(obj); return;
    case 0x010B08: PlayerSlide(obj); return;
    case 0x010BE2: PlayerHang(obj); return;
    case 0x010C92: PlayerHangShoot(obj); return;
    case 0x010D4A: PlayerClimb(obj); return;
    case 0x010DFE: PlayerDrop(obj); return;
    case 0x0112AC: PlayerThrow(obj); return;
    case 0x011350: PlayerThrowAir(obj); return;
    case 0x0113C2: PlayerHeldByEnemy(obj); return;
    case 0x0114CC: PlayerThrownByPlayer(obj); return;
    case 0x011534: PlayerThrownByEnemy(obj); return;
    case 0x0100FA: PlayerShootWalk(obj); return;
    case 0x0111E8: PlayerFallenOut(obj); return;
    case 0x011240: PlayerComingBack(obj); return;
    case 0x010E6A: PlayerHurt(obj); return;
    case 0x00F94E: PlayerHidden(obj); return;
    case 0x010A50: PlayerOnWall(obj); return;
    case 0x010380: PlayerPunch(obj); return;
    case 0x01042E: PlayerPunchAir(obj); return;
    case 0x00FC0E: PlayerTurning(obj); return;
    case 0x00FF0A: PlayerTurningCrouched(obj); return;
    case 0x01106C: PlayerKnocked(obj); return;
    case 0x010F00: PlayerHurtHanging(obj); return;
    case 0x010EBE: PlayerHurtSlope(obj); return;
    case 0x01110C: PlayerGettingUp(obj); return;
    case 0x0110B2: PlayerLying(obj); return;
    case 0x0102A8: PlayerGuard(obj); return;
    case 0x0101EA: PlayerDash(obj); return;
    case 0x010FDA: PlayerHurtAir(obj); return;
    case 0x011440: PlayerHeldByPlayer(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $00F782. Knocked back: down for good when out of vitality or when the time is up; which side
 * it was hit from. */
void PlayerKnockTimer(uint16_t obj)
{
    int16_t v = (int16_t)rd16(obj + O_VITALITY);
    if (v <= 0) set8(obj + P_STATUS, 4);
    else {
        wr16(obj + P_KNOCK_TIME, (uint16_t)(rd16(obj + P_KNOCK_TIME) - 1));
        if (rd16(obj + P_KNOCK_TIME) & 0x8000) set8(obj + P_STATUS, 4);
    }
    uint16_t by = rd16(obj + P_TOUCHING);
    PlayerKnockSide(obj, by);
    if (bit8(obj + P_STATUS, 4) && (int16_t)rd16(obj + O_Y) > (int16_t)rd16(by + O_Y)) set8(obj + P_KNOCK, 2);
    else clr8(obj + P_KNOCK, 2);
}

/* $00F7D4. P_KNOCK bit 1: the hit came from behind (by its speed, or still, by where it is). */
void PlayerKnockSide(uint16_t obj, uint16_t by)
{
    int16_t vx = (int16_t)rd16(by + P_VX), vy = (int16_t)rd16(by + P_VY);
    int16_t x = (int16_t)rd16(obj + O_X), bx = (int16_t)rd16(by + O_X);
    int behind;
    if (!bit8(obj + P_SPRITE, 3)) {
        if (vx) behind = vx > 0;
        else if (vy) behind = vy > 0;
        else behind = x > bx;
    } else {
        if (vx) behind = vx < 0;
        else if (vy) behind = vy > 0;
        else behind = x < bx;
    }
    if (behind) set8(obj + P_KNOCK, 1);
    else clr8(obj + P_KNOCK, 1);
}

/* $00F86A. Down and B on an item: vitality (types $68: its amount, $6C: 50), or a weapon, the
 * one it replaces dropped where the item was (with the combined weapon, the second goes). */
void PlayerPickUp(uint16_t obj)
{
    if (!bit8(obj + P_HELD, 1) || !bit8(obj + P_PRESSED, 4)) return;
    uint16_t item = rd16(obj + P_TOUCHING);
    uint16_t type = rd16(item + O_TYPE);
    if (type == 0x68) {
        QueueSound(0xA7);
        wr16(obj + O_VITALITY, (uint16_t)(rd16(obj + O_VITALITY) + rd16(item + 0x50)));
        clr8(item + P_STATUS, 3);
        return;
    }
    if (type == 0x6C) {
        wr16(obj + O_VITALITY, (uint16_t)(rd16(obj + O_VITALITY) + 50));
        clr8(item + P_STATUS, 3);
        return;
    }
    QueueSound(0xA7);
    no_shot(obj);
    uint16_t old;
    uint8_t w = rd8(obj + P_WEAPON);
    if (w != 3 && (w & 2)) {
        old = rd16(obj + P_WEAPON_B);
        wr16(obj + P_WEAPON_B, (uint16_t)(rd16(item + O_TYPE) - 0x54));
    } else if (w != 3 && (w & 1)) {
        old = rd16(obj + P_WEAPON_A);
        wr16(obj + P_WEAPON_A, (uint16_t)(rd16(item + O_TYPE) - 0x54));
    } else {
        old = rd16(obj + P_WEAPON_B);
        wr16(obj + P_WEAPON_B, rd16(obj + P_WEAPON_A));
        wr16(obj + P_WEAPON_A, (uint16_t)(rd16(item + O_TYPE) - 0x54));
        if (!old) { clr8(item + P_STATUS, 3); return; }
    }
    wr16(item + O_TYPE, rom16(0x00F944 + (uint32_t)((int16_t)old >> 1)));
    wr32(item + O_X, rd32(obj + O_X));
    wr32(item + O_Y, rd32(obj + O_Y));
    wr32(item + P_VX, 0);
    wr32(item + P_VY, 0xFFFC0000u);
}

/* $00F94E. State $70: out of the game, nowhere. */
void PlayerHidden(uint16_t obj)
{
    wr16(obj + O_STATE, 0x70);
    wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) & 0x73FF);
    wr8(obj + P_HURT, 0);
    wr8(obj + P_HIT, 0);
    wr32(obj + O_X, 0);
    wr32(obj + O_Y, 0);
    still(obj);
    no_shot(obj);
}

/* $00F98E: standing (state 4) */
static void start_stand(uint16_t obj)
{
    wr16(obj + O_STATE, 4);
    flags_or(obj, 0xC00);
    wr8(obj + P_HURT, 0x8A);
    PlayerStandAnim(obj);
    still(obj);
    PlayerStand(obj);
}

/* $00F97C. State 0: the player starts, standing. */
void PlayerStart(uint16_t obj)
{
    flags_or(obj, 0x8C00);
    no_shot(obj);
    wr32(obj + 0x8C, 0);
    start_stand(obj);
}

/* bclr #7 of P_HIT: whether it was hit, the flag taken */
static int took_hit(uint16_t obj)
{
    if (!bit8(obj + P_HIT, 7)) return 0;
    clr8(obj + P_HIT, 7);
    return 1;
}

/* a hit taken on the ground: knocked back (on a slope, the slope's), or down */
static void hit_taken(uint16_t obj)
{
    if (bit8(obj + P_STATUS, 4)) start_down(obj);
    else if ((int16_t)rd16(obj + P_FLOOR_KIND) < 8) start_hurt(obj);
    else start_hurt_slope(obj);
}

/* C pressed on the ground: a jump (jump), or with Down a slide (with a side, or on a floor that
 * cannot be dropped through) or a drop */
static int c_pressed(uint16_t obj, void (*jump)(uint16_t))
{
    if (!bit8(obj + P_PRESSED, 5)) return 0;
    if (!bit8(obj + P_HELD, 1)) jump(obj);
    else if ((rd8(obj + P_HELD) & 0xC) || rd16(obj + P_FLOOR) >= 0x38) start_slide(obj);
    else start_drop(obj);
    return 1;
}

/* B held: the special move after the motion input, a melee attack with a direction pressed,
 * else the weapon (or a punch without one) */
static int b_held(uint16_t obj)
{
    if (!bit8(obj + P_HELD, 4)) return 0;
    if (rd16(obj + P_MOTION) == 8) { start_special(obj); return 1; }
    if (PlayerAttackDir(obj)) return 0;
    if (!rd16(obj + P_WEAPON) || !(rd16(obj + P_WEAPON_A) | rd16(obj + P_WEAPON_B))) start_punch(obj);
    else start_shoot(obj);
    return 1;
}

/* $00F9AC. State 4: standing. */
void PlayerStand(uint16_t obj)
{
    if (!rd16(obj + P_FLOOR)) { start_fall(obj); return; }
    if (took_hit(obj)) { hit_taken(obj); return; }
    if (rd16(obj + P_FLOOR_KIND) == 4 && bit8(obj + P_PRESSED, 4) && bit8(obj + P_HELD, 5)) {
        start_guard(obj);
        return;
    }
    if (c_pressed(obj, start_jump)) return;
    if (b_held(obj)) return;
    if (bit8(obj + P_HELD, 1) && rd16(obj + P_FLOOR_KIND) == 4) { start_crouch(obj); return; }
    PlayerAim(obj);
    if (rd16(obj + P_FLOOR_KIND) == 4 && (rd8(obj + P_TURNED) & 0x80)) {
        wr8(obj + P_TURNED, rd8(obj + P_TURNED) & 0x7F);
        wr16(obj + P_TIMER, 4);
        start_wait(obj);
        return;
    }
    if (rd8(obj + P_HELD) & 0xC) start_walk(obj);
}

/* $00FA9C: shooting, standing (state 8) */
static void start_shoot(uint16_t obj)
{
    wr16(obj + O_STATE, 8);
    wr8(obj + P_HURT, 0x8A);
    clr8(obj + P_SHOT_B, 6);
    flags_or(obj, 0xC00);
    still(obj);
    PlayerShootStand(obj);
}

/* $00FABC. State 8: shooting, standing; the free shot walks on, crouches. */
void PlayerShootStand(uint16_t obj)
{
    if (!rd16(obj + P_FLOOR)) { start_shoot_fall(obj); return; }
    if (took_hit(obj)) { hit_taken(obj); return; }
    if (c_pressed(obj, start_shoot_jump)) return;
    if (!rd16(obj + P_CHARACTER)) {
        if (bit8(obj + P_HELD, 1) && rd16(obj + P_FLOOR_KIND) == 4) { start_crouch_shoot(obj); return; }
        if (!bit8(obj + P_SHOT_B, 4) && (rd8(obj + P_HELD) & 0xC)) { start_walk_shoot(obj); return; }
    }
    sub_1776A(obj);
    PlayerAttackDir(obj);
    if (!rd16(obj + P_SHOT)) start_stand(obj);
}

/* $00FBEE: turned round, a few frames (state $80) */
static void start_wait(uint16_t obj)
{
    wr16(obj + O_STATE, 0x80);
    wr8(obj + P_HURT, 0x8A);
    wr16(obj + P_ANIM_SET, 0x7A);
    flags_or(obj, 0xC00);
    still(obj);
    PlayerTurning(obj);
}

/* $00FC0E. State $80: turning round (P_TIMER frames), then standing. */
void PlayerTurning(uint16_t obj)
{
    uint16_t t = rd16(obj + P_TIMER);
    if (!t) { start_stand(obj); return; }
    wr16(obj + P_TIMER, (uint16_t)(t - 1));
    if (!rd16(obj + P_FLOOR)) { start_fall(obj); return; }
    if (took_hit(obj)) { hit_taken(obj); return; }
    if (rd16(obj + P_FLOOR_KIND) == 4 && bit8(obj + P_PRESSED, 4) && bit8(obj + P_HELD, 5)) {
        start_guard(obj);
        return;
    }
    if (c_pressed(obj, start_jump)) return;
    if (b_held(obj)) return;
    if (bit8(obj + P_HELD, 1) && rd16(obj + P_FLOOR_KIND) == 4) start_wait_crouch(obj);
}

/* $00FCEA: crouching (state $C) */
static void start_crouch(uint16_t obj)
{
    wr16(obj + O_STATE, 0xC);
    wr8(obj + P_HURT, 0x8A);
    wr16(obj + P_ANIM_SET, 0x13);
    flags_or(obj, 0xC00);
    still(obj);
    PlayerCrouch(obj);
}

/* $012922. B pressed with a direction: a melee attack that way (P_ATTACK_DIR, its box); whether
 * it started (Z clear). */
int PlayerAttackDir(uint16_t obj)
{
    if (!bit8(obj + P_PRESSED, 4)) return 0;
    uint16_t dir = rd8(obj + P_HELD) & 0xF;
    if (!dir) return 0;
    wr16(obj + P_ATTACK_DIR, rom16(0x012950 + (uint32_t)(dir << 1)));
    set8(obj + P_HURT, 0);
    wr32(obj + 0x2C, 0xF010F018u);
    return 1;
}

/* $012A96. Facing and aim from the pad. */
void PlayerAim(uint16_t obj)
{
    PlayerAimStraight(obj);
    PlayerAimPad(obj);
}

/* $012AD6. Facing from the pad, the aim straight ahead. */
void PlayerAimStraight(uint16_t obj)
{
    PlayerFace(obj);
    wr16(obj + P_AIM, bit8(obj + P_SPRITE, 3) ? 0x8000 : 0);
}

/* $012A9A. The aim the pad's directions give (none: unchanged). */
void PlayerAimPad(uint16_t obj)
{
    uint16_t a = rom16(0x012AB6 + (uint32_t)((rd8(obj + P_HELD) & 0xF) << 1));
    if (a & 0x8000) return;
    wr8(obj + P_AIM, (uint8_t)a);
    wr8(obj + P_AIM + 1, 0);
}

/* $012AF2. Facing the side held (P_TURNED when it changes: $80 to the left, $88 to the right); on
 * a slope with neither, Up and Down face up or down it. */
void PlayerFace(uint16_t obj)
{
    wr8(obj + P_TURNED, 0);
    uint8_t side = rd8(obj + P_HELD) & 0xC;
    if (side) {
        if (side == 4) {
            if (!bit8(obj + P_SPRITE, 3)) { set8(obj + P_SPRITE, 3); wr8(obj + P_TURNED, 0x80); }
        } else if (bit8(obj + P_SPRITE, 3)) {
            clr8(obj + P_SPRITE, 3);
            wr8(obj + P_TURNED, 0x88);
        }
        return;
    }
    uint16_t kind = rd16(obj + P_FLOOR_KIND);
    int up = bit8(obj + P_HELD, 0), down = bit8(obj + P_HELD, 1);
    if (kind == 8) {
        if (up) clr8(obj + P_SPRITE, 3);
        else if (down) set8(obj + P_SPRITE, 3);
    } else if (kind == 0xC) {
        if (up) set8(obj + P_SPRITE, 3);
        else if (down) clr8(obj + P_SPRITE, 3);
    }
}

/* $0115F0. Standing's animation, by the floor and the facing. */
void PlayerStandAnim(uint16_t obj)
{
    uint16_t i = (uint16_t)(((rd8(obj + P_SPRITE) >> 2) & 2) + rd16(obj + P_FLOOR_KIND));
    wr16(obj + P_ANIM_SET, rom16(0x011606 + (uint32_t)(int16_t)i));
}

/* $011AD4. Walking's animation, by the floor and the facing (turning on the flat: $7A). */
void PlayerWalkAnim(uint16_t obj)
{
    uint16_t kind = rd16(obj + P_FLOOR_KIND);
    if (kind == 4 && rd8(obj + P_TURNED)) { wr16(obj + P_ANIM_SET, 0x7A); return; }
    uint16_t i = (uint16_t)(((rd8(obj + P_SPRITE) >> 2) & 2) + kind);
    if (rd8(obj + 0x86) & 0x80) i = (uint16_t)(i + 0x10);
    wr16(obj + P_ANIM_SET, rom16(0x011B10 + (uint32_t)(int16_t)i));
}

/* $0100C2. Walking's speed: -1.75 left, else 1.75. */
void PlayerWalkSpeed(uint16_t obj)
{
    wr32(obj + P_VX, bit8(obj + P_HELD, 2) ? 0xFFFE4000u : 0x1C000u);
}

/* B held crouching: a melee attack with a direction, the weapon, a slide without one */
static int b_held_crouching(uint16_t obj)
{
    if (!bit8(obj + P_HELD, 4) || PlayerAttackDir(obj)) return 0;
    if (!rd16(obj + P_WEAPON) || !(rd16(obj + P_WEAPON_A) | rd16(obj + P_WEAPON_B))) start_slide(obj);
    else start_crouch_shoot(obj);
    return 1;
}

/* $00FD0A. State $C: crouching. */
void PlayerCrouch(uint16_t obj)
{
    if (!rd16(obj + P_FLOOR)) { start_fall(obj); return; }
    if (took_hit(obj)) { hit_taken(obj); return; }
    if (!bit8(obj + P_HELD, 1)) { start_stand(obj); return; }
    if (c_pressed(obj, start_jump)) return;
    if (b_held_crouching(obj)) return;
    PlayerAim(obj);
    if (rd8(obj + P_TURNED) & 0x80) {
        wr8(obj + P_TURNED, rd8(obj + P_TURNED) & 0x7F);
        wr16(obj + P_TIMER, 4);
        start_wait_crouch(obj);
    }
}

/* $00FDB2: shooting, crouched (state $10) */
static void start_crouch_shoot(uint16_t obj)
{
    wr16(obj + O_STATE, 0x10);
    wr8(obj + P_HURT, 0x8A);
    flags_or(obj, 0xC00);
    still(obj);
    PlayerShootCrouch(obj);
}

/* $00FDCC. State $10: shooting, crouched; the free shot stands up with Down let go. */
void PlayerShootCrouch(uint16_t obj)
{
    if (!rd16(obj + P_FLOOR)) { start_shoot_fall(obj); return; }
    if (took_hit(obj)) { hit_taken(obj); return; }
    if (c_pressed(obj, start_shoot_jump)) return;
    if (!rd16(obj + P_CHARACTER) && !bit8(obj + P_HELD, 1)) { start_shoot(obj); return; }
    set8(obj + P_SHOT_B, 6);
    sub_1776A(obj);
    PlayerAttackDir(obj);
    if (!rd16(obj + P_SHOT)) start_crouch(obj);
}

/* $00FEEA: turned round crouching (state $84) */
static void start_wait_crouch(uint16_t obj)
{
    wr16(obj + O_STATE, 0x84);
    wr8(obj + P_HURT, 0x8A);
    wr16(obj + P_ANIM_SET, 0x7B);
    flags_or(obj, 0xC00);
    still(obj);
    PlayerTurningCrouched(obj);
}

/* $00FF0A. State $84: turning round crouched, then crouching. */
void PlayerTurningCrouched(uint16_t obj)
{
    uint16_t t = rd16(obj + P_TIMER);
    if (!t) { start_crouch(obj); return; }
    wr16(obj + P_TIMER, (uint16_t)(t - 1));
    if (!rd16(obj + P_FLOOR)) { start_fall(obj); return; }
    if (took_hit(obj)) { hit_taken(obj); return; }
    if (!bit8(obj + P_HELD, 1)) { start_wait(obj); return; }
    if (c_pressed(obj, start_jump)) return;
    b_held_crouching(obj);
}

/* $00FFB4: walking (state $14) */
static void start_walk(uint16_t obj)
{
    wr16(obj + O_STATE, 0x14);
    wr8(obj + P_HURT, 0x8A);
    PlayerWalkAnim(obj);
    flags_or(obj, 0xC00);
    PlayerWalkSpeed(obj);
    wr32(obj + P_VY, 0);
    PlayerWalk(obj);
}

/* $00FFD2. State $14: walking. */
void PlayerWalk(uint16_t obj)
{
    if (!rd16(obj + P_FLOOR)) { start_fall(obj); return; }
    if (took_hit(obj)) { hit_taken(obj); return; }
    if (rd16(obj + P_FLOOR_KIND) == 4 && bit8(obj + P_PRESSED, 4) && bit8(obj + P_HELD, 5)) {
        start_guard(obj);
        return;
    }
    if (c_pressed(obj, start_jump)) return;
    if (b_held(obj)) return;
    if (bit8(obj + P_HELD, 1) && rd16(obj + P_FLOOR_KIND) == 4) { start_crouch(obj); return; }
    if (!(rd8(obj + P_HELD) & 0xC)) { start_stand(obj); return; }
    PlayerAim(obj);
    if (rd16(obj + P_FLOOR_KIND) == 4 && (rd8(obj + P_TURNED) & 0x80)) {
        wr8(obj + P_TURNED, rd8(obj + P_TURNED) & 0x7F);
        wr16(obj + P_TIMER, 4);
        start_wait(obj);
    }
}

/* $0100E0: shooting, walking (state $60) */
static void start_walk_shoot(uint16_t obj)
{
    wr16(obj + O_STATE, 0x60);
    wr8(obj + P_HURT, 0x8A);
    PlayerWalkAnim(obj);
    flags_or(obj, 0xC00);
    wr32(obj + P_VY, 0);
    PlayerShootWalk(obj);
}

/* $0100FA. State $60: shooting, walking (the free shot's). */
void PlayerShootWalk(uint16_t obj)
{
    if (!rd16(obj + P_FLOOR)) { start_fall(obj); return; }
    if (took_hit(obj)) { hit_taken(obj); return; }
    if (c_pressed(obj, start_shoot_jump)) return;
    if (bit8(obj + P_HELD, 1) && rd16(obj + P_FLOOR_KIND) == 4) { start_crouch_shoot(obj); return; }
    if (bit8(obj + P_SHOT_B, 4) || !(rd8(obj + P_HELD) & 0xC)) { start_shoot(obj); return; }
    sub_1776A(obj);
    PlayerAttackDir(obj);
    if (!rd16(obj + P_SHOT)) start_walk(obj);
}

/* $01019E: the motion input's move, a dash (state $A0) */
static void start_special(uint16_t obj)
{
    QueueSound(0xA1);
    wr16(obj + O_STATE, 0xA0);
    wr8(obj + P_HURT, 0x4A);
    set8(obj + P_STATUS, 5);
    wr32(obj + 0x2C, 0xF010F810u);
    PlayerAttackPower(obj);
    no_shot(obj);
    flags_or(obj, 0xC00);
    PlayerDashSpeed(obj);
    wr32(obj + P_VY, 0);
    wr16(obj + P_PAUSE, 0);
    wr8(obj + P_TURNED, 0);
    wr16(obj + P_TIMER, 0x10);
    PlayerDash(obj);
}

/* $0101EA. State $A0: the dash, 16 frames, then slowing to a stop. */
void PlayerDash(uint16_t obj)
{
    if (!rd16(obj + P_FLOOR)) { start_fall(obj); return; }
    if (took_hit(obj)) { hit_taken(obj); return; }
    wr16(obj + P_TIMER, (uint16_t)(rd16(obj + P_TIMER) - 1));
    uint16_t t = rd16(obj + P_TIMER);
    if (!(t & 0x8000)) {
        if (t) {
            PlayerWalkAnim(obj);
            wr8(obj + P_HURT, rd8(obj + P_HURT) & 0x7D);
            attack_pause_hold(obj);
            return;
        }
        wr16(obj + P_ANIM_SET, 0x8E);
        wr8(obj + P_HURT, rd8(obj + P_HURT) | 0x82);
    }
    PlayerAttackPause(obj);
    if (!rd32(obj + P_VX)) start_stand(obj);
}

/* $010256. The dash's speed, the way the motion started (its first side's opposite... the side
 * of P_MOTION_DIR's bit 2: left 4). */
void PlayerDashSpeed(uint16_t obj)
{
    if (bit8(obj + 0xD0, 2)) {
        wr32(obj + P_VX, 0xFFFC0000u);
        set8(obj + P_SPRITE, 3);
    } else {
        wr32(obj + P_VX, 0x40000);
        clr8(obj + P_SPRITE, 3);
    }
}

/* $010280: guarding, B and C (state $9C) */
static void start_guard(uint16_t obj)
{
    wr16(obj + O_STATE, 0x9C);
    wr8(obj + P_HURT, 0x8A);
    wr16(obj + P_ANIM_SET, 0x8C);
    flags_or(obj, 0xC00);
    still(obj);
    wr8(obj + 0xCC, 1);
}

/* $0102A8. State $9C: guarding. P_GUARD 1 while the animation starts, $80 held (a hit then only
 * blinks the player), back to standing when B or C is let go. */
void PlayerGuard(uint16_t obj)
{
    if (!rd16(obj + P_FLOOR)) { wr8(obj + 0xCC, 0); start_fall(obj); return; }
    if (rd16(obj + P_FLOOR_KIND) != 4) { wr8(obj + 0xCC, 0); start_stand(obj); return; }
    if (bit8(obj + P_HIT, 7)) {
        if ((int16_t)rd16(obj + O_VITALITY) > 0 && (rd8(obj + 0xCC) & 0x80)) {
            wr16(obj + 0x7C, 0x10);
        } else {
            if (!bit8(obj + P_STATUS, 4)) start_hurt(obj);
            else start_down(obj);
            return;
        }
    }
    if (!(rd8(obj + 0x0D) & 0x80)) return;           /* the animation not at its end */
    uint8_t g = rd8(obj + 0xCC);
    if (!g) { start_stand(obj); return; }
    if (!(g & 0x80)) { wr8(obj + 0xCC, 0x80); return; }
    if ((rd8(obj + P_HELD) & 0x30) == 0x30) return;
    wr16(obj + P_ANIM_SET, 0x8D);
    wr8(obj + 0xCC, 0);
}

/* $010334: a punch, without a weapon (state $78) */
static void start_punch(uint16_t obj)
{
    QueueSound(0xA4);
    wr16(obj + O_STATE, 0x78);
    wr8(obj + P_HURT, 0x8A);
    set8(obj + P_STATUS, 5);
    wr32(obj + 0x2C, 0xF808F830u);
    PlayerAttackPower(obj);
    wr16(obj + P_ANIM_SET, 0xAE);
    wr16(obj + 0xB8, 0);
    flags_or(obj, 0xC00);
    still(obj);
    wr16(obj + P_TIMER, 0xC);
    wr16(obj + P_PAUSE, 0);
}

/* $012C66. The attack's strength by the difficulty. */
void PlayerAttackPower(uint16_t obj)
{
    wr16(obj + 0x26, rom16(0x012C72 + (uint32_t)(int16_t)rd16(v_difficulty)));
}

/* $010380. State $78: the punch, P_TIMER frames; hurting while its animation's frame is not 0. */
void PlayerPunch(uint16_t obj)
{
    if (!rd16(obj + P_FLOOR)) { start_fall(obj); return; }
    if (took_hit(obj)) { hit_taken(obj); return; }
    wr16(obj + P_TIMER, (uint16_t)(rd16(obj + P_TIMER) - 1));
    if (rd16(obj + P_TIMER) & 0x8000) { start_stand(obj); return; }
    if (!rd8(obj + 0x0C)) {
        wr8(obj + P_HURT, rd8(obj + P_HURT) & 0x7D);
        PlayerAttackPause(obj);
        return;
    }
    flags_or(obj, 0xC00);
    wr8(obj + P_HURT, (uint8_t)((rd8(obj + P_HURT) | 0x82) & ~0x40));
}

/* $0103EA: a punch in the air (state $7C), once a jump */
static void start_punch_air(uint16_t obj)
{
    QueueSound(0xA4);
    wr16(obj + O_STATE, 0x7C);
    wr8(obj + P_HURT, 0x8A);
    flags_or(obj, 0xC00);
    set8(obj + P_STATUS, 5);
    wr32(obj + 0x2C, 0xF808F830u);
    PlayerAttackPower(obj);
    wr16(obj + P_ANIM_SET, 0xAD);
    wr16(obj + 0xB8, 0);
    wr8(obj + 0xAA, 1);
    wr16(obj + P_PAUSE, 0);
}

/* a hit in the air: knocked away, or down */
static void hit_air(uint16_t obj)
{
    if (!bit8(obj + P_STATUS, 4)) start_hurt_air(obj);
    else start_knocked(obj);
}

/* $01042E. State $7C: the punch in the air. */
void PlayerPunchAir(uint16_t obj)
{
    if (rd16(obj + P_FLOOR)) {
        if (bit8(obj + P_HITS, 1)) start_stand(obj);
        else start_hang(obj);
        return;
    }
    if (took_hit(obj)) { hit_air(obj); return; }
    if (rd8(obj + 0x0D) & 0x80) {
        if (rd16(obj + P_VY) & 0x8000) start_jump(obj);
        else start_fall_on(obj);
        return;
    }
    if (rd8(obj + 0x0C) == 8) {
        wr8(obj + P_HURT, rd8(obj + P_HURT) & 0x7D);
        attack_pause_hold(obj);
        return;
    }
    flags_or(obj, 0xC00);
    wr8(obj + P_HURT, (uint8_t)((rd8(obj + P_HURT) | 0x82) & ~0x40));
}

/* B held in the air: a melee attack with a direction, the weapon (shoot), else the punch, once */
static void air_b(uint16_t obj, void (*shoot)(uint16_t))
{
    if (!bit8(obj + P_HELD, 4) || PlayerAttackDir(obj)) return;
    if (rd16(obj + P_WEAPON) && (rd16(obj + P_WEAPON_A) | rd16(obj + P_WEAPON_B))) shoot(obj);
    else if (!rd8(obj + 0xAA)) start_punch_air(obj);
}

/* C pressed in the air: off a wall, or the air attack (once a jump) */
static int air_c(uint16_t obj)
{
    if (!bit8(obj + P_PRESSED, 5)) return 0;
    if (rd16(obj + P_WALL)) { start_wall(obj); return 1; }
    if (!rd8(obj + 0xAA)) { start_air_attack(obj); return 1; }
    return 0;
}

/* $01049C: jumping (state $18); from the ground the jump's push (-6.5) and its way */
static void start_jump(uint16_t obj)
{
    if (!rd16(obj + P_JUMP)) {
        QueueSound(0xAB);
        PlayerJumpDir(obj);
        wr32(obj + P_VY, 0xFFF98000u);
        wr8(obj + 0xAB, 0);
    }
    wr16(obj + O_STATE, 0x18);
    PlayerJumpHurt(obj);
    flags_or(obj, 0xC00);
    sub_11CD0(obj);
    air_b(obj, start_shoot_jump);
}

/* $0104D6. State $18: going up; falling from the top. */
void PlayerJump(uint16_t obj)
{
    if (took_hit(obj)) { hit_air(obj); return; }
    if (!(rd16(obj + P_VY) & 0x8000)) { start_fall_on(obj); return; }
    if (air_c(obj)) return;
    air_b(obj, start_shoot_jump);
}

/* $010540. The hurt flags a jump of its kind has. */
void PlayerJumpHurt(uint16_t obj)
{
    wr8(obj + P_HURT, rom8(0x01054E + (uint32_t)((int16_t)rd16(obj + P_JUMP) >> 2)));
}

/* $010552. The jump's way from the pad: up (4), forward (8, 1 a frame) or back ($C). */
void PlayerJumpDir(uint16_t obj)
{
    uint8_t held = rd8(obj + P_HELD);
    int left = bit8(obj + P_SPRITE, 3);
    if (held & 4) {
        wr32(obj + P_VX, 0xFFFF0000u);
        wr16(obj + P_JUMP, left ? 8 : 0xC);
    } else if (held & 8) {
        wr32(obj + P_VX, 0x10000);
        wr16(obj + P_JUMP, left ? 0xC : 8);
    } else {
        wr32(obj + P_VX, 0);
        wr16(obj + P_JUMP, 4);
    }
}

/* $0105A4: falling off a ledge (state $28), still */
static void start_fall(uint16_t obj)
{
    wr16(obj + O_STATE, 0x28);
    wr8(obj + P_HURT, 0x8A);
    flags_or(obj, 0xC00);
    still(obj);
    PlayerFallDir(obj);
    air_b(obj, start_shoot_fall_on);
}

/* $0105C6: falling (state $1C), at the speed it has */
static void start_fall_on(uint16_t obj)
{
    wr16(obj + O_STATE, 0x1C);
    wr8(obj + P_HURT, 0x8A);
    flags_or(obj, 0xC00);
    PlayerFallDir(obj);
    air_b(obj, start_shoot_fall_on);
}

/* $0105E0. States $1C and $28: falling. */
void PlayerFall(uint16_t obj)
{
    if (rd16(obj + P_FLOOR)) {
        if (bit8(obj + P_HITS, 1)) start_stand(obj);
        else start_hang(obj);
        return;
    }
    if (took_hit(obj)) { hit_air(obj); return; }
    if (air_c(obj)) return;
    air_b(obj, start_shoot_fall_on);
}

/* $010658. The fall's way and animation from the pad. */
void PlayerFallDir(uint16_t obj)
{
    uint8_t held = rd8(obj + P_HELD);
    int left = bit8(obj + P_SPRITE, 3);
    int back;
    if (held & 4) back = !left;
    else if (held & 8) back = left;
    else {
        wr16(obj + P_JUMP, 4);
        wr16(obj + P_ANIM_SET, 0xB);
        return;
    }
    wr16(obj + P_JUMP, back ? 0xC : 8);
    wr16(obj + P_ANIM_SET, back ? 0x12 : 0x11);
    wr8(obj + 0xAB, 1);
}

/* $0106B4: shooting, jumping (state $20) */
static void start_shoot_jump(uint16_t obj)
{
    if (!rd16(obj + P_JUMP)) {
        QueueSound(0xAB);
        PlayerJumpDir(obj);
        wr32(obj + P_VY, 0xFFF98000u);
        wr8(obj + 0xAB, 0);
    }
    wr16(obj + O_STATE, 0x20);
    wr8(obj + P_HURT, 0x8A);
    flags_or(obj, 0xC00);
    clr8(obj + P_SHOT_B, 6);
    sub_1776A(obj);
    PlayerAttackDir(obj);
    if (!rd16(obj + P_SHOT)) start_jump(obj);
}

/* $0106F2. State $20: shooting, going up. */
void PlayerShootJump(uint16_t obj)
{
    if (took_hit(obj)) { hit_air(obj); return; }
    if (!(rd16(obj + P_VY) & 0x8000)) { start_shoot_fall_on(obj); return; }
    if (air_c(obj)) return;
    sub_1776A(obj);
    PlayerAttackDir(obj);
    if (!rd16(obj + P_SHOT)) start_jump(obj);
}

static void shooting_fall(uint16_t obj)
{
    sub_1776A(obj);
    PlayerAttackDir(obj);
    if (!rd16(obj + P_SHOT)) start_fall_on(obj);
}

/* $010740: shooting, off a ledge (state $24), still, its animation at once */
static void start_shoot_fall(uint16_t obj)
{
    wr16(obj + O_STATE, 0x24);
    wr8(obj + P_HURT, 0x8A);
    wr16(obj + P_ANIM_SET, 0x11);
    sub_16BF6(obj);
    flags_or(obj, 0xC00);
    still(obj);
    wr16(obj + P_JUMP, 4);
    shooting_fall(obj);
}

/* $010770: shooting, falling (state $24) */
static void start_shoot_fall_on(uint16_t obj)
{
    wr16(obj + O_STATE, 0x24);
    wr8(obj + P_HURT, 0x8A);
    flags_or(obj, 0xC00);
    clr8(obj + P_SHOT_B, 6);
    shooting_fall(obj);
}

/* $01078C. State $24: shooting, falling. */
void PlayerShootFall(uint16_t obj)
{
    if (rd16(obj + P_FLOOR)) {
        if (bit8(obj + P_HITS, 1)) start_shoot(obj);
        else start_hang_shoot(obj);
        return;
    }
    if (took_hit(obj)) { hit_air(obj); return; }
    if (air_c(obj)) return;
    shooting_fall(obj);
}

/* the air attack's start (state $34) */
static void air_attack(uint16_t obj)
{
    QueueSound(0xA1);
    wr16(obj + O_STATE, 0x34);
    wr8(obj + P_HURT, 0x8A);
    flags_or(obj, 0xC00);
    set8(obj + P_STATUS, 5);
    no_shot(obj);
    wr8(obj + 0xAA, 1);
    PlayerAttackPower(obj);
}

/* $0107E8: the air attack, a somersault (state $34), once a jump */
static void start_air_attack(uint16_t obj)
{
    air_attack(obj);
    PlayerAirAttackSpeed(obj);
    PlayerAirAttackBox(obj);
    wr16(obj + P_PAUSE, 0);
}

/* $01082A: the same off a wall, up (-5) */
static void start_air_attack_up(uint16_t obj)
{
    air_attack(obj);
    PlayerWallJumpSpeed(obj);
    wr32(obj + P_VY, 0xFFFB0000u);
    PlayerAirAttackBox(obj);
    wr16(obj + P_PAUSE, 0);
}

/* $010874. State $34: the air attack. */
void PlayerAirAttack(uint16_t obj)
{
    if (rd16(obj + P_FLOOR)) {
        if (bit8(obj + P_HITS, 1)) start_stand(obj);
        else start_hang(obj);
        return;
    }
    if (took_hit(obj)) { hit_air(obj); return; }
    if (rd8(obj + 0x0D) & 0x80) {
        if (rd16(obj + P_VY) & 0x8000) start_jump(obj);
        else start_fall_on(obj);
        return;
    }
    if (air_c(obj)) return;
    if (rd8(obj + 0x0C) != 4) {
        flags_or(obj, 0xC00);
        wr8(obj + P_HURT, (uint8_t)((rd8(obj + P_HURT) | 0x82) & ~0x40));
        return;
    }
    wr8(obj + P_HURT, rd8(obj + P_HURT) & 0x7D);
    attack_pause_hold(obj);
}

/* the frames an attack that hit holds still (4, its animation's frame kept), with the hit's
 * spark; otherwise hurting (P_HURT bit 6) */
static void attack_pause(uint16_t obj, int hold)
{
    uint16_t n = rd16(obj + P_PAUSE);
    if (!n) {
        if (!bit8(obj + P_HIT, 6)) {
            flags_or(obj, 0xC00);
            set8(obj + P_HURT, 6);
            return;
        }
        clr8(obj + P_HIT, 6);
        PlayerHitSpark(obj);
        n = 4;
    }
    wr16(obj + P_PAUSE, (uint16_t)(n - 1));
    wr16(obj + O_FLAGS, rd16(obj + O_FLAGS) & 0xF3FF);
    if (hold) wr8(obj + 0x0D, (uint8_t)(rd8(obj + 0x0D) + 1));
    clr8(obj + P_HURT, 6);
}

/* $0108F8 */
static void attack_pause_hold(uint16_t obj) { attack_pause(obj, 1); }

/* $010B7C. The same, the animation going on. */
void PlayerAttackPause(uint16_t obj) { attack_pause(obj, 0); }

/* $01A1FE (a jump into the effects' code): a spark (type $430) at x, y, made by obj */
static void spawn_spark(uint16_t obj, uint16_t x, uint16_t y)
{
    uint16_t left, e;
    if (!ObjectFindEffect(obj, &left, &e)) return;
    wr16(e + O_STATE, 0);
    wr16(e + O_TYPE, 0x430);
    wr16(e + O_X, x);
    wr16(e + O_Y, y);
    wr16(e + 0x4C, obj);
}

/* $010938. The hit's spark, between the player and what it hit (an object of type $430). */
void PlayerHitSpark(uint16_t obj)
{
    QueueSound(0xAC);
    uint16_t hit = rd16(obj + P_HOLDING);
    uint16_t x = (uint16_t)((int16_t)(rd16(hit + O_X) + rd16(obj + O_X)) >> 1);
    uint16_t y = (uint16_t)((int16_t)(rd16(hit + O_Y) + rd16(obj + O_Y)) >> 1);
    spawn_spark(obj, x, y);
}

/* $010960. The air attack's animation and box: the somersault, or for the fixed shot's
 * character down (Down, or falling) or up. */
void PlayerAirAttackBox(uint16_t obj)
{
    int up;
    if (!rd16(obj + P_CHARACTER)) {
        wr16(obj + P_ANIM_SET, 0x2B);
        wr32(obj + 0x2C, 0xF808E820u);
        return;
    }
    if (bit8(obj + P_HELD, 0)) up = 1;
    else if (bit8(obj + P_HELD, 1)) up = 0;
    else up = (rd16(obj + P_VY) & 0x8000) != 0;
    if (up) {
        wr16(obj + P_ANIM_SET, 0x2B);
        wr32(obj + 0x2C, 0xF008F81Cu);
    } else {
        wr16(obj + P_ANIM_SET, 0x91);
        wr32(obj + 0x2C, 0x0018F81Cu);
    }
}

/* $0109B4. The air attack's speed: 3 the side held (none: as it was). */
void PlayerAirAttackSpeed(uint16_t obj)
{
    uint8_t held = rd8(obj + P_HELD);
    if (held & 4) wr32(obj + P_VX, 0xFFFD0000u);
    else if (held & 8) wr32(obj + P_VX, 0x30000);
}

/* $0109DC. Off a wall: 3 the way it faces. */
void PlayerWallJumpSpeed(uint16_t obj)
{
    wr32(obj + P_VX, bit8(obj + P_SPRITE, 3) ? 0xFFFD0000u : 0x30000);
}

/* $010A18: against a wall in the air (state $74), facing away */
static void start_wall(uint16_t obj)
{
    QueueSound(0xB0);
    wr16(obj + O_STATE, 0x74);
    wr8(obj + P_HURT, 0x8A);
    flags_or(obj, 0xC00);
    no_shot(obj);
    wr16(obj + P_ANIM_SET, 0x71);
    PlayerFaceFromWall(obj);
    still(obj);
}

/* $010A50. State $74: on the wall, its animation; then off it up. */
void PlayerOnWall(uint16_t obj)
{
    if (rd16(obj + P_FLOOR)) {
        if (bit8(obj + P_HITS, 1)) start_stand(obj);
        else start_hang(obj);
        return;
    }
    if (took_hit(obj)) { hit_air(obj); return; }
    if (rd8(obj + 0x0D) & 0x80) start_air_attack_up(obj);
}

/* $010A88. Facing away from the wall (its attribute's bit 2). */
void PlayerFaceFromWall(uint16_t obj)
{
    if (bit8(obj + P_WALL + 1, 2)) clr8(obj + P_SPRITE, 3);
    else set8(obj + P_SPRITE, 3);
}

/* $010AA2: the slide (state $38) */
static void start_slide(uint16_t obj)
{
    QueueSound(0xA1);
    wr16(obj + O_STATE, 0x38);
    wr8(obj + P_HURT, 0x8A);
    flags_or(obj, 0xC00);
    set8(obj + P_STATUS, 5);
    wr32(obj + 0x2C, 0x0010F818u);
    PlayerAttackPower(obj);
    wr16(obj + P_ANIM_SET, 0x15);
    no_shot(obj);
    wr32(obj + P_VY, 0);
    wr16(obj + P_PAUSE, 0);
    PlayerAim(obj);
    PlayerSlideSpeed(obj);
}

/* $010AEA. 5 the way it faces. */
void PlayerSlideSpeed(uint16_t obj)
{
    wr32(obj + P_VX, bit8(obj + P_SPRITE, 3) ? 0xFFFB0000u : 0x50000);
}

/* $010B08. State $38: sliding; at the animation's end crouching (standing with bit 7 of $86). */
void PlayerSlide(uint16_t obj)
{
    if (!rd16(obj + P_FLOOR)) { start_fall(obj); return; }
    if (took_hit(obj)) { hit_taken(obj); return; }
    if (rd8(obj + 0x0D) & 0x80) {
        if (!(rd8(obj + 0x86) & 0x80)) start_crouch(obj);
        else start_stand(obj);
        return;
    }
    int8_t frame = (int8_t)rd8(obj + 0x0C);
    if (frame < 4) {
        flags_or(obj, 0xC00);
        clr8(obj + P_HURT, 6);
        return;
    }
    if (frame > 4) wr32(obj + 0x2C, 0xF010F810u);
    PlayerAttackPause(obj);
}

/* $010BB8: hanging under a floor (state $3C) */
static void start_hang(uint16_t obj)
{
    QueueSound(0xB1);
    wr16(obj + O_STATE, 0x3C);
    wr8(obj + P_HURT, 0x8A);
    flags_or(obj, 0xC00);
    wr16(obj + P_ANIM_SET, 0x16);
    still(obj);
    PlayerHang(obj);
}

/* C hanging: Down lets go; on a bar ($30-$37) Up climbs, else a jump; elsewhere only with Up
 * (else it lets go) */
static int hang_c(uint16_t obj, void (*let_go)(uint16_t), void (*jump)(uint16_t))
{
    if (!bit8(obj + P_PRESSED, 5)) return 0;
    if (bit8(obj + P_HELD, 1)) { let_go(obj); return 1; }
    if ((rd16(obj + P_FLOOR) & 0x78) == 0x30) {
        if (bit8(obj + P_HELD, 0)) start_climb(obj);
        else jump(obj);
        return 1;
    }
    if (!bit8(obj + P_HELD, 0)) { let_go(obj); return 1; }
    return 0;
}

/* $010BE2. State $3C: hanging. */
void PlayerHang(uint16_t obj)
{
    if (!rd16(obj + P_FLOOR)) { start_fall(obj); return; }
    if (bit8(obj + P_HITS, 1)) { start_stand(obj); return; }
    if (took_hit(obj)) {
        if (!bit8(obj + P_STATUS, 4)) start_hurt_hanging(obj);
        else start_knocked(obj);
        return;
    }
    if (hang_c(obj, start_fall_on, start_jump)) return;
    if (bit8(obj + P_HELD, 4)) {
        if (rd16(obj + P_WEAPON) && (rd16(obj + P_WEAPON_A) | rd16(obj + P_WEAPON_B))) {
            start_hang_shoot(obj);
            return;
        }
        if (!rd8(obj + 0xAA)) { start_punch_air(obj); return; }
    }
    PlayerAim(obj);
}

/* $010C72: shooting, hanging (state $40) */
static void start_hang_shoot(uint16_t obj)
{
    wr16(obj + O_STATE, 0x40);
    wr8(obj + P_HURT, 0x8A);
    flags_or(obj, 0xC00);
    clr8(obj + P_SHOT_B, 6);
    still(obj);
    PlayerHangShoot(obj);
}

/* $010C92. State $40: shooting, hanging. */
void PlayerHangShoot(uint16_t obj)
{
    if (!rd16(obj + P_FLOOR)) { start_shoot_fall(obj); return; }
    if (bit8(obj + P_HITS, 1)) { start_shoot(obj); return; }
    if (took_hit(obj)) {
        if (!bit8(obj + P_STATUS, 4)) start_hurt_hanging(obj);
        else start_knocked(obj);
        return;
    }
    if (hang_c(obj, start_shoot_fall_on, start_shoot_jump)) return;
    sub_1776A(obj);
    if (!rd16(obj + P_SHOT)) start_hang(obj);
}

/* $010D08: climbing onto the floor hung from (state $44) */
static void start_climb(uint16_t obj)
{
    QueueSound(0xA1);
    wr16(obj + O_STATE, 0x44);
    wr8(obj + P_HURT, 8);
    flags_or(obj, 0xC00);
    set8(obj + P_STATUS, 5);
    PlayerAttackPower(obj);
    wr16(obj + P_ANIM_SET, 0x6E);
    still(obj);
    no_shot(obj);
    wr16(obj + P_PAUSE, 0);
}

/* $01A21C. A free effect slot (of ten, below $C380, or below $C740 for an object with bit 6 of
 * $23): whether there is one (Z), the count left (d2), the slot (a4). */
int ObjectFindEffect(uint16_t obj, uint16_t *left, uint16_t *slot)
{
    uint16_t at = bit8(obj + P_STATUS, 6) ? 0xC740 : 0xC380;
    uint16_t n = 9;
    for (;;) {
        at = (uint16_t)(at - 0x60);
        if (!rd16(at)) { *left = n; *slot = at; return 1; }
        if (n-- == 0) { *left = n; *slot = at; return 0; }
    }
}

/* $010D4A. State $44: climbing up onto the floor (up $2A at the end); its kicks hurt, with the
 * attack's pause (the animation held, the flags left as they are). */
void PlayerClimb(uint16_t obj)
{
    if (!rd16(obj + P_FLOOR)) { start_fall(obj); return; }
    if (bit8(obj + P_HITS, 1)) { start_stand(obj); return; }
    if (rd8(obj + 0x0D) & 0x80) {
        wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) - 0x2A));
        start_stand(obj);
        return;
    }
    uint8_t frame = rd8(obj + 0x0C);
    if (frame == 8) wr32(obj + 0x2C, 0x0028F030u);
    else if (frame == 0xC) wr32(obj + 0x2C, 0xD800F030u);
    else { clr8(obj + P_HURT, 6); return; }
    uint16_t n = rd16(obj + P_PAUSE);
    if (!n) {
        if (!bit8(obj + P_HIT, 6)) { set8(obj + P_HURT, 6); return; }
        clr8(obj + P_HIT, 6);
        PlayerHitSpark(obj);
        n = 4;
    }
    wr16(obj + P_PAUSE, (uint16_t)(n - 1));
    wr8(obj + 0x0D, (uint8_t)(rd8(obj + 0x0D) + 1));
    clr8(obj + P_HURT, 6);
}

/* $010DCA: dropping through a thin floor (state $48) */
static void start_drop(uint16_t obj)
{
    QueueSound(0xA4);
    wr16(obj + O_STATE, 0x48);
    wr8(obj + P_HURT, 8);
    flags_or(obj, 0xC00);
    wr16(obj + P_ANIM_SET, 0x6F);
    still(obj);
    no_shot(obj);
}

/* $010DFE. State $48: dropping; at the end, down $2A, hanging under it. */
void PlayerDrop(uint16_t obj)
{
    if (!rd16(obj + P_FLOOR)) { start_fall(obj); return; }
    if (!(rd8(obj + 0x0D) & 0x80)) return;
    wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) + 0x2A));
    clr8(obj + P_HITS, 1);
    start_hang(obj);
}

/* hurt: the sound, the state, no shot, still */
static void hurt(uint16_t obj, uint16_t state)
{
    QueueSound(0xE7);
    wr16(obj + O_STATE, state);
    no_shot(obj);
    flags_or(obj, 0xC00);
    wr8(obj + P_HURT, 8);
    wr8(obj + P_HIT, 0);
}

/* $010E20: hurt standing (state $6C), the animation by the side hit */
static void start_hurt(uint16_t obj)
{
    hurt(obj, 0x6C);
    still(obj);
    wr16(obj + P_ANIM_SET, bit8(obj + P_KNOCK, 1) ? 0x81 : 0x7E);
}

/* $010E6A. States $6C, $90: hurt, the animation, then standing. */
void PlayerHurt(uint16_t obj)
{
    if (rd8(obj + 0x0D) & 0x80) start_stand(obj);
}

/* $010E74: hurt on a slope (state $90) */
static void start_hurt_slope(uint16_t obj)
{
    hurt(obj, 0x90);
    still(obj);
    wr16(obj + P_ANIM_SET, bit8(obj + P_KNOCK, 1) ? 0x88 : 0x89);
}

/* $010EBE */
void PlayerHurtSlope(uint16_t obj)
{
    if (rd8(obj + 0x0D) & 0x80) start_stand(obj);
}

/* $010EC8: hurt hanging (state $8C) */
static void start_hurt_hanging(uint16_t obj)
{
    hurt(obj, 0x8C);
    wr16(obj + P_ANIM_SET, 0x7C);
    still(obj);
}

/* $010F00. State $8C: hurt hanging, then hanging. */
void PlayerHurtHanging(uint16_t obj)
{
    if (rd8(obj + 0x0D) & 0x80) start_hang(obj);
}

/* $010F0A: knocked down (state $30), thrown up and away */
static void start_down(uint16_t obj)
{
    hurt(obj, 0x30);
    PlayerKnockSpeed(obj);
    wr32(obj + P_VY, 0xFFFB0000u);
    wr16(obj + P_ANIM_SET, rom16(0x010F4E + (uint32_t)(rd8(obj + P_KNOCK) & 6)));
}

/* $010F56. State $30: down; on the floor, dead or up again. */
void PlayerDown(uint16_t obj)
{
    if (!rd16(obj + P_FLOOR)) return;
    if ((int16_t)rd16(obj + O_VITALITY) <= 0) die(obj);
    else start_get_up(obj);
}

/* $010F70. Away from the hit, 1 a frame. */
void PlayerKnockSpeed(uint16_t obj)
{
    int away_left = bit8(obj + P_SPRITE, 3) ? bit8(obj + P_KNOCK, 1) : !bit8(obj + P_KNOCK, 1);
    wr32(obj + P_VX, away_left ? 0xFFFF0000u : 0x10000);
}

/* the knock's animation and speed, by the side it came from */
static void knocked_away(uint16_t obj)
{
    int behind = bit8(obj + P_KNOCK, 1), left = bit8(obj + P_SPRITE, 3);
    wr16(obj + P_ANIM_SET, behind ? 0x83 : 0x84);
    wr32(obj + P_VX, (behind ? left : !left) ? 0xFFFF0000u : 0x10000);
}

/* $010FA0: hurt in the air (state $A4), 16 frames */
static void start_hurt_air(uint16_t obj)
{
    hurt(obj, 0xA4);
    wr32(obj + P_VY, 0xFFFB0000u);
    wr16(obj + P_TIMER, 0x10);
    knocked_away(obj);
}

/* $010FDA. State $A4: hurt in the air; landing, or after the time going on as it was. */
void PlayerHurtAir(uint16_t obj)
{
    if (rd16(obj + P_FLOOR) && !(rd16(obj + P_VY) & 0x8000)) { land_knocked(obj); return; }
    wr16(obj + P_TIMER, (uint16_t)(rd16(obj + P_TIMER) - 1));
    if (!(rd16(obj + P_TIMER) & 0x8000)) return;
    if (rd16(obj + P_VY) & 0x8000) start_jump(obj);
    else start_fall_on(obj);
}

/* $011000: knocked flying (state $88) */
static void start_knocked(uint16_t obj)
{
    hurt(obj, 0x88);
    wr32(obj + P_VY, 0xFFFB0000u);
    knocked_away(obj);
}

/* $01106C. State $88: flying, until it lands. */
void PlayerKnocked(uint16_t obj)
{
    if (rd16(obj + P_FLOOR) && !(rd16(obj + P_VY) & 0x8000)) land_knocked(obj);
}

/* $01107E: landed knocked (state $98) */
static void land_knocked(uint16_t obj)
{
    wr16(obj + O_STATE, 0x98);
    no_shot(obj);
    flags_or(obj, 0xC00);
    wr8(obj + P_HURT, 8);
    wr8(obj + P_HIT, 0);
    still(obj);
    wr16(obj + P_ANIM_SET, 0x85);
    set8(obj + P_KNOCK, 1);
}

/* $0110B2. State $98: lying; then dead or up. */
void PlayerLying(uint16_t obj)
{
    if (!(rd8(obj + 0x0D) & 0x80)) return;
    if ((int16_t)rd16(obj + O_VITALITY) <= 0) die(obj);
    else start_get_up(obj);
}

/* $0110CC: getting up (state $94) */
static void start_get_up(uint16_t obj)
{
    wr16(obj + O_STATE, 0x94);
    no_shot(obj);
    flags_or(obj, 0xC00);
    wr8(obj + P_HURT, 8);
    wr8(obj + P_HIT, 0);
    still(obj);
    wr16(obj + P_ANIM_SET, bit8(obj + P_KNOCK, 1) ? 0x87 : 0x86);
}

/* $01110C. State $94: getting up, then standing. */
void PlayerGettingUp(uint16_t obj)
{
    if (!(rd8(obj + 0x0D) & 0x80)) return;
    sub_12B78(obj);
    start_stand(obj);
}

/* $01111E: dead (state $2C), $80 frames */
static void die(uint16_t obj)
{
    QueueSound(0xBC);
    no_shot(obj);
    wr8(obj + P_HURT, 8);
    flags_or(obj, 0xC00);
    still(obj);
    wr16(obj + O_VITALITY, 0);
    wr8(obj + 0x6C, 0);
    wr16(obj + O_STATE, 0x2C);
    wr16(obj + P_TIMER, 0x80);
}

/* $01115A. State $2C: dead; then the object goes and its player is out (v_players, the pause). */
void PlayerDead(uint16_t obj)
{
    wr16(obj + P_TIMER, (uint16_t)(rd16(obj + P_TIMER) - 1));
    if (!(rd16(obj + P_TIMER) & 0x8000)) return;
    wr16(obj + O_FLAGS, 0x1000);
    set8(obj + P_DISPLAY, 6);
    int bit = obj == v_player1 ? 0 : 1;
    clr8(v_players, bit);
    clr8(v_pauseFlags, bit);
}

/* $011192: fallen out of the level: 20 vitality lost (dead with none left), then back up */
static void fall_out(uint16_t obj)
{
    wr16(obj + O_Y, (uint16_t)(rd16(v_deathLine) + 0x10));
    wr16(obj + O_VITALITY, (uint16_t)(rd16(obj + O_VITALITY) - 0x14));
    if ((int16_t)rd16(obj + O_VITALITY) <= 0) { die(obj); return; }
    QueueSound(0xE7);
    wr16(obj + O_STATE, 0x64);
    wr16(obj + P_TIMER, 0x20);
    wr8(obj + P_HURT, 0);
    wr8(obj + P_HIT, 0);
    no_shot(obj);
    flags_or(obj, 0xC00);
    wr16(obj + P_ANIM_SET, 0x2A);
    still(obj);
}

/* $0111E8. State $64: fallen out, $20 frames; then a jump up from y $160, blinking. */
void PlayerFallenOut(uint16_t obj)
{
    wr16(obj + P_TIMER, (uint16_t)(rd16(obj + P_TIMER) - 1));
    if (!(rd16(obj + P_TIMER) & 0x8000)) return;
    QueueSound(0xA1);
    wr16(obj + O_STATE, 0x68);
    wr8(obj + P_HURT, 0);
    no_shot(obj);
    flags_or(obj, 0xC00);
    wr8(obj + 0xAA, 0);
    wr16(obj + 0x7C, 0x40);
    wr16(obj + O_Y, 0x160);
    wr16(obj + O_Y + 2, 0);
    PlayerAim(obj);
    wr16(obj + P_ANIM_SET, 8);
    PlayerJumpDir(obj);
    wr32(obj + P_VY, 0xFFF40000u);
}

/* $011240. State $68: back up, until it falls. */
void PlayerComingBack(uint16_t obj)
{
    if (!(rd16(obj + P_VY) & 0x8000)) start_fall_on(obj);
}

/* $01124A (state $4C) and $0112EE (state $50, from a jump or a fall): throwing what it holds */
static void throw_held(uint16_t obj, uint16_t state)
{
    QueueSound(rd16(obj + P_CHARACTER) ? 0xE8 : 0xE2);
    wr16(obj + O_STATE, state);
    wr16(obj + P_THROWN, rd16(obj + P_HOLDING));
    uint16_t held = rd16(obj + P_THROWN);
    set8(held + P_STATUS, 1);
    clr8(held + P_STATUS, 4);
    no_shot(obj);
    flags_or(obj, 0xC00);
    wr8(obj + P_HURT, 8);
    wr8(obj + P_HIT, 0);
    wr16(obj + P_ANIM_SET, 0x70);
    wr16(obj + P_TIMER, 0x11);
    still(obj);
    PlayerAim(obj);
}

/* $0112AC. State $4C: throwing, the animation, then standing. */
void PlayerThrow(uint16_t obj)
{
    if (!rd16(obj + P_FLOOR)) { start_fall(obj); return; }
    if (took_hit(obj)) { hit_taken(obj); return; }
    if (rd8(obj + 0x0D) & 0x80) start_stand(obj);
}

/* $011350. State $50: throwing in the air, then falling. */
void PlayerThrowAir(uint16_t obj)
{
    if (rd16(obj + P_FLOOR)) {
        if (bit8(obj + P_HITS, 1)) start_stand(obj);
        else start_hang(obj);
        return;
    }
    if (took_hit(obj)) { hit_air(obj); return; }
    if (rd8(obj + 0x0D) & 0x80) start_fall(obj);
}

/* held: by a player (state $A8) or by an enemy ($54) */
static void held(uint16_t obj, uint16_t state)
{
    wr16(obj + O_STATE, state);
    no_shot(obj);
    flags_or(obj, 0xC00);
    wr8(obj + P_HURT, 0x88);
    wr8(obj + P_HIT, 0);
    wr16(obj + P_ANIM_SET, 0x72);
}

/* $011388 */
static void grabbed(uint16_t obj)
{
    uint16_t by = rd16(obj + P_TOUCHING);
    held(obj, by == v_player1 || by == v_player2 ? 0xA8 : 0x54);
}

/* held: a hit; struggling (a direction pressed: the other struggle animation) while it holds
 * on; let go, thrown (knocked down: P_STATUS bit 4) or standing */
static void being_held(uint16_t obj, void (*thrown)(uint16_t))
{
    if (took_hit(obj)) { hit_air(obj); return; }
    if (bit8(obj + P_STATUS, 1)) {
        clr8(obj + P_STATUS, 1);
        if (!(rd8(obj + P_PRESSED) & 0xF)) return;
        wr16(obj + P_ANIM_SET, rd16(obj + 0xB8) == 0x72 ? 0xAC : 0x72);
        return;
    }
    if (!bit8(obj + P_STATUS, 4)) start_stand(obj);
    else thrown(obj);
}

/* $0113C2. State $54: held by an enemy. */
void PlayerHeldByEnemy(uint16_t obj) { being_held(obj, start_thrown_by_enemy); }

/* $011440. State $A8: held by the other player. */
void PlayerHeldByPlayer(uint16_t obj) { being_held(obj, start_thrown_by_player); }

/* $011498: thrown by the other player (state $58), hurting what it hits */
static void start_thrown_by_player(uint16_t obj)
{
    wr16(obj + O_STATE, 0x58);
    no_shot(obj);
    flags_or(obj, 0xC00);
    wr8(obj + P_HURT, 0x4A);
    set8(obj + P_STATUS, 5);
    wr32(obj + 0x2C, 0xF010F010u);
    PlayerAttackPower(obj);
    wr16(obj + P_ANIM_SET, 0x84);
}

/* $0114CC. State $58: thrown, until it lands (standing) or hangs. */
void PlayerThrownByPlayer(uint16_t obj)
{
    if (!rd16(obj + P_FLOOR)) return;
    if (bit8(obj + P_HITS, 1)) start_stand(obj);
    else start_hang(obj);
}

/* $0114E4: thrown by an enemy (state $5C), facing back the way it flies */
static void start_thrown_by_enemy(uint16_t obj)
{
    QueueSound(0xE7);
    wr16(obj + O_STATE, 0x5C);
    no_shot(obj);
    flags_or(obj, 0xC00);
    wr8(obj + P_HURT, 0x9A);
    wr32(obj + 0x2C, 0xF010F010u);
    wr16(obj + 0x26, 4);
    wr16(obj + P_ANIM_SET, 0x84);
    if (rd16(obj + P_VX) & 0x8000) clr8(obj + P_SPRITE, 3);
    else set8(obj + P_SPRITE, 3);
}

/* $011534. State $5C: thrown; landing costs 20 vitality. */
void PlayerThrownByEnemy(uint16_t obj)
{
    if (rd16(obj + P_FLOOR)) {
        wr16(obj + O_VITALITY, (uint16_t)(rd16(obj + O_VITALITY) - 0x14));
        land_knocked(obj);
        return;
    }
    if (took_hit(obj)) hit_air(obj);
}
