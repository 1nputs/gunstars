/* The players riding the mine cart in stage 1 (v_playerMode 4: types $3EC and $3F0, CartPlayer,
 * $12C7A-$1567B). Fighters built like the players (they run the players' weapon, pick-up, limits,
 * aim and animation code) that stand on any of the cart's four sides, the box v_boundsX0/Y0 -
 * v_boundsX1/Y1 (MapBoundsY, MapBoundsX): the floor, the ceiling (upside down) or either wall. C
 * jumps across to the opposite side; in the air C turns back (at some places of the box).
 * Each side has the same states: standing, shooting, crouched (toward the side), crouched and
 * shooting, in the air toward it (and shooting there), hurt on it and knocked off it; the floor
 * and the ceiling also a slide. The states' table is at $12CFC, their movement's at $1479C. A
 * state's routine often starts another's: the "start_" functions set it up and most go straight
 * on into its routine, as the 68k falls into it. */
#include "game.h"

/* a player's fields (as the players' own: player_states.c, player_move.c) */
#define P_ANIM_END 0x0D          /* byte: bit 7 the animation at its end */
#define P_SPRITE   0x0E          /* byte: bit 3 flipped (facing left), bit 4 upside down */
#define P_VX       0x18          /* longs: the speeds, 16.16 */
#define P_VY       0x1C
#define P_HURT     0x21          /* byte: bit 7 can be hurt */
#define P_HIT      0x22          /* byte: bit 7 hit */
#define P_STATUS   0x23          /* byte: bit 2 blinking, bit 3 touching an item, bit 4 knocked
                                    down, bit 5 the slide's attack */
#define P_POWER    0x26          /* the attack's strength */
#define P_BOTTOM   0x29          /* signed bytes: the box's bottom from y, its left and right from x */
#define P_LEFT     0x2A
#define P_RIGHT    0x2B
#define P_ATTACK_BOX 0x2C        /* long: the attack's box */
#define P_TOUCHING 0x42          /* the object that hit it */
#define P_HELD     0x69          /* bytes: the buttons held, pressed this frame */
#define P_PRESSED  0x6A
#define P_JUMP     0x78          /* the jump's way: 4 straight, 8 forward, $C back */
#define P_BLINK    0x7C          /* frames of blinking left (hurt) */
#define P_KNOCK_TIME 0x7E        /* hits left before one knocks it down */
#define P_SIDE     0x80          /* byte, low 5 bits: the side it is on or falls toward (2 the
                                    floor, 1 the ceiling, 8 the right wall, 4 the left), bit 4 on it */
#define P_DISPLAY  0x81          /* byte: bit 2 the vitality's display to redraw, bit 6 out */
#define P_FLOOR    0x82          /* the last side touched (MapBoundsY, MapBoundsX) */
#define P_WATER    0x86
#define P_PUSH_X   0x88          /* longs: the cart's push, copied while on a side */
#define P_PUSH_Y   0x8C
#define P_ANIM_SET 0x90          /* the animation to start (PlayerSetAnim) */
#define P_TIMER    0x92
#define P_SHOT     0x94          /* the weapon's: the shot's word and byte */
#define P_SHOT_B   0x96          /* byte: bit 4 the free aim, bit 6 crouched */
#define P_KNOCK    0x97          /* byte: bit 1 hit from the left, bit 2 from above */
#define P_AIM_DIR  0x9A          /* byte: the shot's direction (256 to the turn) */
#define P_GUN_X    0x9E          /* the muzzle's place from the player */
#define P_GUN_Y    0xA0
#define P_VITALITY_SHOWN 0xBE
#define P_CHARACTER 0xCA         /* 0 Red, else Blue (4) */
#define P_PAUSE    0xD2          /* frames an attack that hit holds still */

#define CART_X 0xA970            /* the cart's push on the players (longs), x and y */
#define CART_Y 0xA974

/* the pad's bits in P_HELD and P_PRESSED */
#define PAD_UP    0
#define PAD_DOWN  1
#define PAD_LEFT  2
#define PAD_RIGHT 3
#define PAD_B     4
#define PAD_C     5

/* the sides' attributes (MapBoundsY, MapBoundsX) */
#define AT_RIGHT   0x10
#define AT_LEFT    0x14
#define AT_FLOOR   0x20
#define AT_CEILING 0x28

typedef void (*StateFn)(uint16_t obj);

static void flags_or(uint16_t obj, uint16_t f) { wr16(obj + O_FLAGS, (uint16_t)(rd16(obj + O_FLAGS) | f)); }
static void add16(uint16_t a, uint16_t v) { wr16(a, (uint16_t)(rd16(a) + v)); }
static void add32(uint16_t a, uint32_t v) { wr32(a, rd32(a) + v); }

static void still(uint16_t obj)
{
    wr32(obj + P_VX, 0);
    wr32(obj + P_VY, 0);
}

/* the shot cleared */
static void no_shot(uint16_t obj)
{
    wr8(obj + P_SHOT_B, 0);
    wr16(obj + P_SHOT, 0);
}

/* bclr #7 of P_HIT: whether it was hit, the flag taken */
static int took_hit(uint16_t obj)
{
    if (!bit8(obj + P_HIT, 7)) return 0;
    clr8(obj + P_HIT, 7);
    return 1;
}

static int held(uint16_t obj, int b) { return bit8(obj + P_HELD, b); }
static int pressed(uint16_t obj, int b) { return bit8(obj + P_PRESSED, b); }
static int red(uint16_t obj) { return !rd16(obj + P_CHARACTER); }
static int anim_ended(uint16_t obj) { return (rd8(obj + P_ANIM_END) & 0x80) != 0; }

/* P_SIDE's low bits; upside down on the ceiling and toward it */
static void set_side(uint16_t obj, uint8_t side)
{
    wr8(obj + P_SIDE, (uint8_t)((rd8(obj + P_SIDE) & 0xE0) | side));
    if (side & 1) set8(obj + P_SPRITE, 4);
    else clr8(obj + P_SPRITE, 4);
}

/* the animation by the facing: `right` facing right, `left` flipped */
static void anim_by_facing(uint16_t obj, uint16_t right, uint16_t left)
{
    wr16(obj + P_ANIM_SET, bit8(obj + P_SPRITE, 3) ? left : right);
}

static void start_floor(uint16_t obj);
static void start_floor_shoot(uint16_t obj);
static void start_floor_crouch(uint16_t obj);
static void start_floor_crouch_shoot(uint16_t obj);
static void start_to_floor(uint16_t obj);
static void start_to_floor_shoot(uint16_t obj);
static void jump_floor(uint16_t obj);
static void jump_floor_shoot(uint16_t obj);
static void start_floor_slide(uint16_t obj);
static void start_ceiling(uint16_t obj);
static void start_ceiling_shoot(uint16_t obj);
static void start_ceiling_crouch(uint16_t obj);
static void start_ceiling_crouch_shoot(uint16_t obj);
static void start_to_ceiling(uint16_t obj);
static void start_to_ceiling_shoot(uint16_t obj);
static void jump_ceiling(uint16_t obj);
static void jump_ceiling_shoot(uint16_t obj);
static void start_ceiling_slide(uint16_t obj);
static void start_wall_r(uint16_t obj);
static void start_wall_r_shoot(uint16_t obj);
static void start_wall_r_crouch(uint16_t obj);
static void start_wall_r_crouch_shoot(uint16_t obj);
static void start_to_wall_r(uint16_t obj);
static void start_to_wall_r_shoot(uint16_t obj);
static void jump_wall_r(uint16_t obj);
static void jump_wall_r_shoot(uint16_t obj);
static void start_wall_l(uint16_t obj);
static void start_wall_l_shoot(uint16_t obj);
static void start_wall_l_crouch(uint16_t obj);
static void start_wall_l_crouch_shoot(uint16_t obj);
static void start_to_wall_l(uint16_t obj);
static void start_to_wall_l_shoot(uint16_t obj);
static void jump_wall_l(uint16_t obj);
static void jump_wall_l_shoot(uint16_t obj);
static void floor_hurt(uint16_t obj);
static void floor_knocked(uint16_t obj);
static void ceiling_hurt(uint16_t obj);
static void ceiling_knocked(uint16_t obj);
static void wall_r_hurt(uint16_t obj);
static void wall_r_knocked(uint16_t obj);
static void wall_l_hurt(uint16_t obj);
static void wall_l_knocked(uint16_t obj);
static void start_dead(uint16_t obj);

/* ---- the type's routine ---- */

/* the blinking after a hit (loc_0F61A, PlayerFrame's end): shown on odd frames, then for good */
static void cart_blink(uint16_t obj)
{
    if (!rd16(obj + P_BLINK)) return;
    add16(obj + P_BLINK, 0xFFFF);
    if (rd16(obj + P_BLINK)) {
        set8(obj + P_STATUS, 2);
        wr16(obj + O_FLAGS, (uint16_t)((rd16(obj + O_FLAGS) & 0x7FFF) | (rd16(v_frameCount) & 1) << 15));
        return;
    }
    clr8(obj + P_STATUS, 2);
    set8(obj + O_FLAGS, 7);
}

/* $012C7A. Types $3EC and $3F0, a player in the mine cart: the weapon button, nothing more while
 * paused; the push it was given, the screen's limits, its state and movement, the blinking after
 * a hit. */
void CartPlayer(uint16_t obj)
{
    PlayerWeaponButton(obj);
    if (rd8(v_pauseFlags) & 0x80) return;
    PlayerPushed(obj);
    PlayerLimits(obj);
    PlayerFloorKind(obj);
    CartStates(obj);
    PlayerSetAnim(obj);
    CartMove(obj);
    PlayerSetAnim(obj);
    cart_blink(obj);
}

/* $012CB6. The vitality's display redrawn when it changed; a hit (the knock's sides), an item
 * touched; then the state's routine (the table at $12CFC, relative words, by the state / 2). */
void CartStates(uint16_t obj)
{
    if (rd16(obj + O_VITALITY) != rd16(obj + P_VITALITY_SHOWN)) {
        wr16(obj + P_VITALITY_SHOWN, rd16(obj + O_VITALITY));
        set8(obj + P_DISPLAY, 2);
    }
    if (bit8(obj + P_HIT, 7)) CartHit(obj);
    else if (bit8(obj + P_STATUS, 3)) {
        clr8(obj + P_STATUS, 3);
        PlayerPickUp(obj);
    }
    uint32_t at = 0x012CFC + (uint32_t)((int16_t)rd16(obj + O_STATE) >> 1);
    call_object(at + (uint32_t)(int16_t)rom16(at), obj);
}

/* $012D48. A hit: knocked down when out of vitality or out of the hits it takes standing
 * (P_KNOCK_TIME); the sides the hitter (P_TOUCHING) came from: by its speeds, or still, by where it
 * is (P_KNOCK bit 1 from the left, CartHitSideX; bit 2 from above: moving down, or still and
 * above it). */
void CartHit(uint16_t obj)
{
    if ((int16_t)rd16(obj + O_VITALITY) <= 0) set8(obj + P_STATUS, 4);
    else {
        add16(obj + P_KNOCK_TIME, 0xFFFF);
        if (rd16(obj + P_KNOCK_TIME) & 0x8000) set8(obj + P_STATUS, 4);
    }
    uint16_t by = rd16(obj + P_TOUCHING);
    CartHitSideX(obj, by);
    int16_t vy = (int16_t)rd16(by + P_VY);
    if (vy > 0 || (!vy && (int16_t)rd16(obj + O_Y) > (int16_t)rd16(by + O_Y))) set8(obj + P_KNOCK, 2);
    else clr8(obj + P_KNOCK, 2);
}

/* $012D9C. P_KNOCK bit 1, hit from the left: the hitter (by) moving right, or still and left of
 * it. */
void CartHitSideX(uint16_t obj, uint16_t by)
{
    int16_t vx = (int16_t)rd16(by + P_VX);
    if (vx > 0 || (!vx && (int16_t)rd16(obj + O_X) > (int16_t)rd16(by + O_X))) set8(obj + P_KNOCK, 1);
    else clr8(obj + P_KNOCK, 1);
}

/* ---- the sides reached ---- */

/* standing on the floor or the ceiling: a wall touched, onto it (y moved a pixel off the floor
 * or ceiling, `step`); then the floor and ceiling: none, into the air (`air`); the opposite one
 * (`other`), onto it. 1: the state changed */
static int level_sides(uint16_t obj, uint16_t step, StateFn wall_r, StateFn wall_l, StateFn air,
                       uint16_t other, StateFn onto)
{
    uint16_t at = CartWalls(obj);
    if (at == AT_RIGHT) { add16(obj + O_Y, step); wall_r(obj); return 1; }
    if (at == AT_LEFT) { add16(obj + O_Y, step); wall_l(obj); return 1; }
    at = CartFloors(obj);
    if (!at) { air(obj); return 1; }
    if (at == other) { onto(obj); return 1; }
    return 0;
}

/* standing on a wall: the floor or the ceiling touched, onto it (x moved a pixel off the wall,
 * `step`); then the walls: none, into the air; the opposite one (`other`), onto it */
static int wall_sides(uint16_t obj, uint16_t step, StateFn floor, StateFn ceiling, StateFn air,
                      uint16_t other, StateFn onto)
{
    uint16_t at = CartFloors(obj);
    if (at == AT_FLOOR) { add16(obj + O_X, step); floor(obj); return 1; }
    if (at == AT_CEILING) { add16(obj + O_X, step); ceiling(obj); return 1; }
    at = CartWalls(obj);
    if (!at) { air(obj); return 1; }
    if (at == other) { onto(obj); return 1; }
    return 0;
}

/* in the air: a side reached (CartBounds), onto it, after the landing's sound (or none) */
static int air_sides(uint16_t obj, void (*sound)(void), StateFn floor, StateFn ceiling, StateFn wall_r,
                     StateFn wall_l)
{
    uint16_t at = CartBounds(obj);
    StateFn onto = at == AT_FLOOR ? floor : at == AT_CEILING ? ceiling : at == AT_RIGHT ? wall_r
                 : at == AT_LEFT ? wall_l : 0;
    if (!onto) return 0;
    if (sound) sound();
    onto(obj);
    return 1;
}

/* knocked into the air: a side reached, up again (PlayerUpAgain) on it, standing */
static int knocked_sides(uint16_t obj)
{
    uint16_t at = CartBounds(obj);
    StateFn onto = at == AT_FLOOR ? start_floor : at == AT_CEILING ? start_ceiling
                 : at == AT_RIGHT ? start_wall_r : at == AT_LEFT ? start_wall_l : 0;
    if (!onto) return 0;
    PlayerUpAgain(obj);
    onto(obj);
    return 1;
}

/* a hit standing on a side: hurt, or when knocked down, knocked off it */
static int hit_on_side(uint16_t obj, StateFn hurt, StateFn knocked)
{
    if (!took_hit(obj)) return 0;
    if (!bit8(obj + P_STATUS, 4)) hurt(obj);
    else knocked(obj);
    return 1;
}

/* ---- the states' set-ups ---- */

/* standing on a side (side: P_SIDE's bits; push: the long cleared, the push along it) */
static void on_side(uint16_t obj, uint16_t state, uint16_t push, uint8_t side)
{
    wr16(obj + O_STATE, state);
    still(obj);
    wr32(obj + push, 0);
    flags_or(obj, 0xC00);
    wr8(obj + P_HURT, 0x80);
    set_side(obj, side);
}

/* in the air toward a side */
static void toward(uint16_t obj, uint16_t state, uint8_t side)
{
    flags_or(obj, 0xC00);
    wr16(obj + O_STATE, state);
    set_side(obj, side);
}

/* loc_12DE2: standing on the floor (state 4) */
static void start_floor(uint16_t obj)
{
    on_side(obj, 4, P_PUSH_Y, 0x12);
    CartFloor(obj);
}

/* loc_12E8A: shooting on the floor (state $C) */
static void start_floor_shoot(uint16_t obj)
{
    clr8(obj + P_SHOT_B, 6);
    on_side(obj, 0xC, P_PUSH_Y, 0x12);
    CartFloorShoot(obj);
}

/* loc_12FB6: crouched on the floor (state $14) */
static void start_floor_crouch(uint16_t obj)
{
    on_side(obj, 0x14, P_PUSH_Y, 0x12);
    CartFloorCrouch(obj);
}

/* loc_1305E: crouched and shooting on the floor (state $18) */
static void start_floor_crouch_shoot(uint16_t obj)
{
    on_side(obj, 0x18, P_PUSH_Y, 0x12);
    CartFloorCrouchShoot(obj);
}

/* loc_131C2: in the air toward the floor (state 8) */
static void start_to_floor(uint16_t obj)
{
    wr8(obj + P_HURT, 0x80);
    toward(obj, 8, 2);
}

/* loc_13274: in the air toward the floor, shooting (state $10) */
static void start_to_floor_shoot(uint16_t obj)
{
    clr8(obj + P_SHOT_B, 6);
    toward(obj, 0x10, 2);
}

/* loc_131AC: C on the floor: a jump up (its sideways speed by the pad) */
static void jump_floor(uint16_t obj)
{
    QueueSound(0xAB);
    CartJumpX(obj);
    wr32(obj + P_VY, 0xFFF98000);
    start_to_floor(obj);
}

/* loc_1325E: the same, shooting */
static void jump_floor_shoot(uint16_t obj)
{
    QueueSound(0xAB);
    CartJumpX(obj);
    wr32(obj + P_VY, 0xFFF98000);
    start_to_floor_shoot(obj);
}

/* loc_132EA: C crouched on the floor: a slide, an attack (state $64) */
static void start_floor_slide(uint16_t obj)
{
    QueueSound(0xA1);
    no_shot(obj);
    wr32(obj + P_VY, 0);
    PlayerAim(obj);
    PlayerSlideSpeed(obj);
    flags_or(obj, 0xC00);
    wr8(obj + P_HURT, 0xC0);
    set8(obj + P_STATUS, 5);
    wr32(obj + P_ATTACK_BOX, 0xF808F81C);
    PlayerAttackPower(obj);
    wr16(obj + P_ANIM_SET, 0x94);
    wr16(obj + O_STATE, 0x64);
    set_side(obj, 0x12);
    wr16(obj + P_PAUSE, 0);
}

/* loc_133B6: standing on the ceiling (state $1C) */
static void start_ceiling(uint16_t obj)
{
    on_side(obj, 0x1C, P_PUSH_Y, 0x11);
    CartCeiling(obj);
}

/* loc_1345E: shooting on the ceiling (state $24) */
static void start_ceiling_shoot(uint16_t obj)
{
    clr8(obj + P_SHOT_B, 6);
    on_side(obj, 0x24, P_PUSH_Y, 0x11);
    CartCeilingShoot(obj);
}

/* loc_1358A: crouched on the ceiling (state $2C) */
static void start_ceiling_crouch(uint16_t obj)
{
    on_side(obj, 0x2C, P_PUSH_Y, 0x11);
    CartCeilingCrouch(obj);
}

/* loc_13632: crouched and shooting on the ceiling (state $30) */
static void start_ceiling_crouch_shoot(uint16_t obj)
{
    on_side(obj, 0x30, P_PUSH_Y, 0x11);
    CartCeilingCrouchShoot(obj);
}

/* loc_13796: in the air toward the ceiling (state $20) */
static void start_to_ceiling(uint16_t obj)
{
    wr8(obj + P_HURT, 0x80);
    toward(obj, 0x20, 1);
}

/* loc_13844: in the air toward the ceiling, shooting (state $28) */
static void start_to_ceiling_shoot(uint16_t obj)
{
    clr8(obj + P_SHOT_B, 6);
    toward(obj, 0x28, 1);
}

/* loc_13780: C on the ceiling: a jump down */
static void jump_ceiling(uint16_t obj)
{
    QueueSound(0xAB);
    CartJumpX(obj);
    wr32(obj + P_VY, 0x68000);
    start_to_ceiling(obj);
}

/* loc_13832: the same, shooting (no sound) */
static void jump_ceiling_shoot(uint16_t obj)
{
    CartJumpX(obj);
    wr32(obj + P_VY, 0x68000);
    wr8(obj + P_HURT, 0x80);
    start_to_ceiling_shoot(obj);
}

/* loc_138BA: C crouched on the ceiling: a slide (state $68) */
static void start_ceiling_slide(uint16_t obj)
{
    QueueSound(0xA1);
    no_shot(obj);
    wr32(obj + P_VY, 0);
    PlayerAim(obj);
    PlayerSlideSpeed(obj);
    flags_or(obj, 0xC00);
    wr8(obj + P_HURT, 0xC0);
    set8(obj + P_STATUS, 5);
    wr32(obj + P_ATTACK_BOX, 0xF808F81C);
    wr16(obj + P_POWER, 4);
    wr16(obj + P_ANIM_SET, 0x94);
    wr16(obj + O_STATE, 0x68);
    set_side(obj, 0x11);
    wr16(obj + P_PAUSE, 0);
}

/* loc_13986: standing on the right wall (state $34) */
static void start_wall_r(uint16_t obj)
{
    on_side(obj, 0x34, P_PUSH_X, 0x18);
    CartWallR(obj);
}

/* loc_13A24: shooting on the right wall (state $3C) */
static void start_wall_r_shoot(uint16_t obj)
{
    clr8(obj + P_SHOT_B, 6);
    on_side(obj, 0x3C, P_PUSH_X, 0x18);
    CartWallRShoot(obj);
}

/* loc_13ACC: crouched on the right wall (state $44) */
static void start_wall_r_crouch(uint16_t obj)
{
    on_side(obj, 0x44, P_PUSH_X, 0x18);
    CartWallRCrouch(obj);
}

/* loc_13B74: crouched and shooting on the right wall (state $48), its aim set */
static void start_wall_r_crouch_shoot(uint16_t obj)
{
    still(obj);
    wr32(obj + P_PUSH_X, 0);
    flags_or(obj, 0xC00);
    wr8(obj + P_HURT, 0x80);
    CartAimWallRCrouch(obj);
    wr16(obj + O_STATE, 0x48);
    set_side(obj, 0x18);
    CartWallRCrouchShoot(obj);
}

/* loc_13C36: in the air toward the right wall (state $38) */
static void start_to_wall_r(uint16_t obj)
{
    wr8(obj + P_HURT, 0x80);
    toward(obj, 0x38, 8);
}

/* loc_13CE4: in the air toward the right wall, shooting (state $40) */
static void start_to_wall_r_shoot(uint16_t obj)
{
    clr8(obj + P_SHOT_B, 6);
    toward(obj, 0x40, 8);
}

/* loc_13C20: C on the right wall: a jump left (its speed along the wall by the pad) */
static void jump_wall_r(uint16_t obj)
{
    QueueSound(0xAB);
    wr32(obj + P_VX, 0xFFF98000);
    CartJumpY(obj);
    start_to_wall_r(obj);
}

/* loc_13CD2: the same, shooting (no sound) */
static void jump_wall_r_shoot(uint16_t obj)
{
    wr32(obj + P_VX, 0xFFF98000);
    CartJumpY(obj);
    wr8(obj + P_HURT, 0x80);
    start_to_wall_r_shoot(obj);
}

/* loc_13D8C: standing on the left wall (state $4C) */
static void start_wall_l(uint16_t obj)
{
    on_side(obj, 0x4C, P_PUSH_X, 0x14);
    CartWallL(obj);
}

/* loc_13E2A: shooting on the left wall (state $54) */
static void start_wall_l_shoot(uint16_t obj)
{
    clr8(obj + P_SHOT_B, 6);
    on_side(obj, 0x54, P_PUSH_X, 0x14);
    CartWallLShoot(obj);
}

/* loc_13ED2: crouched on the left wall (state $5C) */
static void start_wall_l_crouch(uint16_t obj)
{
    on_side(obj, 0x5C, P_PUSH_X, 0x14);
    CartWallLCrouch(obj);
}

/* loc_13F7A: crouched and shooting on the left wall (state $60) */
static void start_wall_l_crouch_shoot(uint16_t obj)
{
    on_side(obj, 0x60, P_PUSH_X, 0x14);
    CartWallLCrouchShoot(obj);
}

/* loc_14038: in the air toward the left wall (state $50) */
static void start_to_wall_l(uint16_t obj)
{
    wr8(obj + P_HURT, 0x80);
    toward(obj, 0x50, 4);
}

/* loc_140E6: in the air toward the left wall, shooting (state $58) */
static void start_to_wall_l_shoot(uint16_t obj)
{
    clr8(obj + P_SHOT_B, 6);
    toward(obj, 0x58, 4);
}

/* loc_14022: C on the left wall: a jump right */
static void jump_wall_l(uint16_t obj)
{
    QueueSound(0xAB);
    wr32(obj + P_VX, 0x68000);
    CartJumpY(obj);
    start_to_wall_l(obj);
}

/* loc_140D4: the same, shooting (no sound) */
static void jump_wall_l_shoot(uint16_t obj)
{
    wr32(obj + P_VX, 0x68000);
    CartJumpY(obj);
    wr8(obj + P_HURT, 0x80);
    start_to_wall_l_shoot(obj);
}

/* a hit's start: harmless, its shot dropped */
static void hit_start(uint16_t obj, uint16_t state)
{
    QueueSound(0xE7);
    wr16(obj + O_STATE, state);
    flags_or(obj, 0xC00);
    wr8(obj + P_HURT, 0);
    wr8(obj + P_HIT, 0);
    no_shot(obj);
}

/* hurt: the speeds' whole parts cleared (only those, clr.w), facing the side hit from: hit from
 * the left (P_KNOCK bit 1), flipped and `anim_left`, else `anim` */
static void hurt_start(uint16_t obj, uint16_t state, uint16_t anim, uint16_t anim_left)
{
    hit_start(obj, state);
    wr16(obj + P_VX, 0);
    wr16(obj + P_VY, 0);
    if (bit8(obj + P_KNOCK, 1)) {
        wr16(obj + P_ANIM_SET, anim_left);
        set8(obj + P_SPRITE, 3);
    } else {
        wr16(obj + P_ANIM_SET, anim);
        clr8(obj + P_SPRITE, 3);
    }
}

/* loc_14162: hurt on the floor (state $74) */
static void floor_hurt(uint16_t obj) { hurt_start(obj, 0x74, 0x3C, 0x4C); }

/* loc_142E4: hurt on the ceiling (state $78) */
static void ceiling_hurt(uint16_t obj) { hurt_start(obj, 0x78, 0x3C, 0x4C); }

/* loc_143F8: hurt on the right wall (state $7C) */
static void wall_r_hurt(uint16_t obj) { hurt_start(obj, 0x7C, 0x6C, 0x5C); }

/* loc_14576: hurt on the left wall (state $80) */
static void wall_l_hurt(uint16_t obj) { hurt_start(obj, 0x80, 0x5C, 0x6C); }

/* loc_1423E: knocked off the floor or ceiling: the animation and x's speed by the side hit from */
static void knocked_level(uint16_t obj)
{
    if (bit8(obj + P_KNOCK, 1)) {
        wr16(obj + P_ANIM_SET, 0x4D);
        set8(obj + P_SPRITE, 3);
        wr32(obj + P_VX, 0x10000);
        return;
    }
    wr16(obj + P_ANIM_SET, 0x3D);
    clr8(obj + P_SPRITE, 3);
    wr32(obj + P_VX, 0xFFFF0000);
}

/* loc_14502, loc_14680: knocked off a wall: y's speed by the side hit from */
static void knocked_wall_vy(uint16_t obj)
{
    wr32(obj + P_VY, bit8(obj + P_KNOCK, 2) ? 0x10000 : 0xFFFF0000);
}

/* loc_14200: knocked off the floor (state $84; out of vitality, dead) */
static void floor_knocked(uint16_t obj)
{
    if ((int16_t)rd16(obj + O_VITALITY) <= 0) { start_dead(obj); return; }
    hit_start(obj, 0x84);
    wr32(obj + P_VY, 0xFFFB0000);
    CartKnockTimer(obj);
    knocked_level(obj);
}

/* loc_14360: knocked off the ceiling (state $88) */
static void ceiling_knocked(uint16_t obj)
{
    if ((int16_t)rd16(obj + O_VITALITY) <= 0) { start_dead(obj); return; }
    hit_start(obj, 0x88);
    wr32(obj + P_VY, 0x50000);
    CartKnockTimer(obj);
    knocked_level(obj);
}

/* loc_14496: knocked off the right wall (state $8C) */
static void wall_r_knocked(uint16_t obj)
{
    if ((int16_t)rd16(obj + O_VITALITY) <= 0) { start_dead(obj); return; }
    hit_start(obj, 0x8C);
    wr32(obj + P_VX, 0xFFFB0000);
    CartKnockTimer(obj);
    CartKnockAnimR(obj);
    knocked_wall_vy(obj);
}

/* loc_14614: knocked off the left wall (state $90) */
static void wall_l_knocked(uint16_t obj)
{
    if ((int16_t)rd16(obj + O_VITALITY) <= 0) { start_dead(obj); return; }
    hit_start(obj, 0x90);
    wr32(obj + P_VX, 0x50000);
    CartKnockTimer(obj);
    CartKnockAnimL(obj);
    knocked_wall_vy(obj);
}

/* loc_146F4: out of vitality: thrown up (still when it was upside down), harmless (state $94) */
static void start_dead(uint16_t obj)
{
    QueueSound(0xBC);
    wr8(obj + P_WATER, 0);
    no_shot(obj);
    wr16(obj + O_VITALITY, 0);
    wr32(obj + P_VX, 0);
    wr32(obj + P_VY, 0xFFFB0000);
    if (bit8(obj + P_SPRITE, 4)) {
        clr8(obj + P_SPRITE, 4);
        wr32(obj + P_VY, 0);
    }
    flags_or(obj, 0xC00);
    wr8(obj + P_HURT, 0);
    wr8(obj + P_HIT, 0);
    wr8(obj + 0x6C, 0);
    wr16(obj + P_ANIM_SET, 0x93);
    wr16(obj + O_STATE, 0x94);
    wr16(obj + P_TIMER, 0x80);
}

/* ---- the states (the table at $12CFC) ---- */

/* $012DC4. State 0: the player starts, standing on the floor. */
void CartStart(uint16_t obj)
{
    flags_or(obj, 0x8C00);
    no_shot(obj);
    wr16(obj + P_PUSH_Y, 0);
    wr16(obj + P_ANIM_SET, 0x2E);
    PlayerSetAnim(obj);
    start_floor(obj);
}

/* $012E12. State 4: standing on the floor; C jumps up, Down crouches, B shoots. */
void CartFloor(uint16_t obj)
{
    if (level_sides(obj, 0xFFFF, start_wall_r, start_wall_l, start_to_floor, AT_CEILING, start_ceiling))
        return;
    if (hit_on_side(obj, floor_hurt, floor_knocked)) return;
    if (pressed(obj, PAD_C)) jump_floor(obj);
    else if (held(obj, PAD_DOWN)) start_floor_crouch(obj);
    else if (held(obj, PAD_B)) start_floor_shoot(obj);
}

/* $012EC0. State $C: shooting on the floor (Red: Down crouches); the shot over, standing. */
void CartFloorShoot(uint16_t obj)
{
    int r = red(obj);
    if (level_sides(obj, 0xFFFF, start_wall_r_shoot, start_wall_l_shoot, start_to_floor_shoot,
                    AT_CEILING, start_ceiling_shoot))
        return;
    if (hit_on_side(obj, floor_hurt, floor_knocked)) return;
    if (pressed(obj, PAD_C)) { jump_floor_shoot(obj); return; }
    if (r && held(obj, PAD_DOWN)) { start_floor_crouch_shoot(obj); return; }
    PlayerShoot(obj);
    if (!rd16(obj + P_SHOT)) start_floor(obj);
}

/* $012FE6. State $14: crouched on the floor; Down let go, standing; C slides, B shoots. */
void CartFloorCrouch(uint16_t obj)
{
    if (level_sides(obj, 0xFFFF, start_wall_r, start_wall_l, start_to_floor, AT_CEILING, start_ceiling))
        return;
    if (hit_on_side(obj, floor_hurt, floor_knocked)) return;
    if (!held(obj, PAD_DOWN)) start_floor(obj);
    else if (pressed(obj, PAD_C)) start_floor_slide(obj);
    else if (held(obj, PAD_B)) start_floor_crouch_shoot(obj);
}

/* $01308E. State $18: crouched and shooting on the floor (Red: Down let go, shooting standing);
 * C slides (with Down) or jumps; the shot over, crouched. */
void CartFloorCrouchShoot(uint16_t obj)
{
    int r = red(obj);
    if (level_sides(obj, 0xFFFF, start_wall_r_shoot, start_wall_l_shoot, start_to_floor_shoot,
                    AT_CEILING, start_ceiling_shoot))
        return;
    if (hit_on_side(obj, floor_hurt, floor_knocked)) return;
    if (pressed(obj, PAD_C)) {
        if (held(obj, PAD_DOWN)) start_floor_slide(obj);
        else jump_floor_shoot(obj);
        return;
    }
    if (r && !held(obj, PAD_DOWN)) { start_floor_shoot(obj); return; }
    set8(obj + P_SHOT_B, 6);
    PlayerShoot(obj);
    if (!rd16(obj + P_SHOT)) start_floor_crouch(obj);
}

/* $0131E8. State 8: in the air toward the floor: a side reached, onto it (a sound); C turns
 * toward the ceiling when it may (CartCanFlipUp); B shoots. */
void CartToFloor(uint16_t obj)
{
    if (air_sides(obj, CartLandSound, start_floor, start_ceiling, start_wall_r, start_wall_l)) return;
    if (took_hit(obj)) { floor_knocked(obj); return; }
    if (pressed(obj, PAD_C) && CartCanFlipUp(obj)) { start_to_ceiling(obj); return; }
    if (held(obj, PAD_B)) start_to_floor_shoot(obj);
}

/* $01322C. The landing's sound ($B1). */
void CartLandSound(void) { QueueSound(0xB1); }

/* $01329A. State $10: in the air toward the floor, shooting; the shot over, not. */
void CartToFloorShoot(uint16_t obj)
{
    if (air_sides(obj, 0, start_floor_shoot, start_ceiling_shoot, start_wall_r_shoot, start_wall_l_shoot))
        return;
    if (took_hit(obj)) { floor_knocked(obj); return; }
    if (pressed(obj, PAD_C) && CartCanFlipUp(obj)) { start_to_ceiling_shoot(obj); return; }
    PlayerShoot(obj);
    if (!rd16(obj + P_SHOT)) start_to_floor(obj);
}

/* $013350. State $64: sliding on the floor; stopped, crouched; else the attack's pause. */
void CartFloorSlide(uint16_t obj)
{
    if (level_sides(obj, 0xFFFF, start_wall_r, start_wall_l, start_to_floor, AT_CEILING, start_ceiling))
        return;
    if (hit_on_side(obj, floor_hurt, floor_knocked)) return;
    if (!rd16(obj + P_VX)) { start_floor_crouch(obj); return; }
    PlayerAttackPause(obj);
}

/* $0133E6. State $1C: standing on the ceiling; C jumps down, Up crouches, B shoots. */
void CartCeiling(uint16_t obj)
{
    if (level_sides(obj, 1, start_wall_r, start_wall_l, start_to_ceiling, AT_FLOOR, start_floor)) return;
    if (hit_on_side(obj, ceiling_hurt, ceiling_knocked)) return;
    if (pressed(obj, PAD_C)) jump_ceiling(obj);
    else if (held(obj, PAD_UP)) start_ceiling_crouch(obj);
    else if (held(obj, PAD_B)) start_ceiling_shoot(obj);
}

/* $013494. State $24: shooting on the ceiling (Red: Up crouches); the shot over, standing. */
void CartCeilingShoot(uint16_t obj)
{
    int r = red(obj);
    if (level_sides(obj, 1, start_wall_r_shoot, start_wall_l_shoot, start_to_ceiling_shoot, AT_FLOOR,
                    start_floor_shoot))
        return;
    if (hit_on_side(obj, ceiling_hurt, ceiling_knocked)) return;
    if (pressed(obj, PAD_C)) { jump_ceiling_shoot(obj); return; }
    if (r && held(obj, PAD_UP)) { start_ceiling_crouch_shoot(obj); return; }
    PlayerShoot(obj);
    if (!rd16(obj + P_SHOT)) start_ceiling(obj);
}

/* $0135BA. State $2C: crouched on the ceiling; Up let go, standing; C slides, B shoots. */
void CartCeilingCrouch(uint16_t obj)
{
    if (level_sides(obj, 1, start_wall_r, start_wall_l, start_to_ceiling, AT_FLOOR, start_floor)) return;
    if (hit_on_side(obj, ceiling_hurt, ceiling_knocked)) return;
    if (!held(obj, PAD_UP)) start_ceiling(obj);
    else if (pressed(obj, PAD_C)) start_ceiling_slide(obj);
    else if (held(obj, PAD_B)) start_ceiling_crouch_shoot(obj);
}

/* $013662. State $30: crouched and shooting on the ceiling (Red: Up let go, shooting standing);
 * C slides (with Up) or jumps; the shot over, crouched. */
void CartCeilingCrouchShoot(uint16_t obj)
{
    int r = red(obj);
    if (level_sides(obj, 1, start_wall_r_shoot, start_wall_l_shoot, start_to_ceiling_shoot, AT_FLOOR,
                    start_floor_shoot))
        return;
    if (hit_on_side(obj, ceiling_hurt, ceiling_knocked)) return;
    if (pressed(obj, PAD_C)) {
        if (held(obj, PAD_UP)) start_ceiling_slide(obj);
        else jump_ceiling_shoot(obj);
        return;
    }
    if (r && !held(obj, PAD_UP)) { start_ceiling_shoot(obj); return; }
    set8(obj + P_SHOT_B, 6);
    PlayerShoot(obj);
    if (!rd16(obj + P_SHOT)) start_ceiling_crouch(obj);
}

/* $0137BC. State $20: in the air toward the ceiling: a side reached, onto it (a sound); C turns
 * toward the floor when it may (CartCanFlipDown); B shoots. */
void CartToCeiling(uint16_t obj)
{
    if (air_sides(obj, CartLandSound2, start_floor, start_ceiling, start_wall_r, start_wall_l)) return;
    if (took_hit(obj)) { ceiling_knocked(obj); return; }
    if (pressed(obj, PAD_C) && CartCanFlipDown(obj)) { start_to_floor(obj); return; }
    if (held(obj, PAD_B)) start_to_ceiling_shoot(obj);
}

/* $013800. The landing's sound ($B1). */
void CartLandSound2(void) { QueueSound(0xB1); }

/* $01386A. State $28: in the air toward the ceiling, shooting; the shot over, not. */
void CartToCeilingShoot(uint16_t obj)
{
    if (air_sides(obj, 0, start_floor_shoot, start_ceiling_shoot, start_wall_r_shoot, start_wall_l_shoot))
        return;
    if (took_hit(obj)) { ceiling_knocked(obj); return; }
    if (pressed(obj, PAD_C) && CartCanFlipDown(obj)) { start_to_floor_shoot(obj); return; }
    PlayerShoot(obj);
    if (!rd16(obj + P_SHOT)) start_to_ceiling(obj);
}

/* $013920. State $68: sliding on the ceiling; stopped, crouched; else the attack's pause. */
void CartCeilingSlide(uint16_t obj)
{
    if (level_sides(obj, 1, start_wall_r, start_wall_l, start_to_ceiling, AT_FLOOR, start_floor)) return;
    if (hit_on_side(obj, ceiling_hurt, ceiling_knocked)) return;
    if (!rd16(obj + P_VX)) { start_ceiling_crouch(obj); return; }
    PlayerAttackPause(obj);
}

/* $0139B6. State $34: standing on the right wall; C jumps left, B shoots. */
void CartWallR(uint16_t obj)
{
    if (wall_sides(obj, 0xFFFF, start_floor, start_ceiling, start_to_wall_r, AT_LEFT, start_wall_l)) return;
    if (hit_on_side(obj, wall_r_hurt, wall_r_knocked)) return;
    if (pressed(obj, PAD_C)) jump_wall_r(obj);
    else if (held(obj, PAD_B)) start_wall_r_shoot(obj);
}

/* $013A5A. State $3C: shooting on the right wall; the shot over, standing. */
void CartWallRShoot(uint16_t obj)
{
    if (wall_sides(obj, 0xFFFF, start_floor_shoot, start_ceiling_shoot, start_to_wall_r_shoot, AT_LEFT,
                   start_wall_l_shoot))
        return;
    if (hit_on_side(obj, wall_r_hurt, wall_r_knocked)) return;
    if (pressed(obj, PAD_C)) { jump_wall_r_shoot(obj); return; }
    PlayerShoot(obj);
    if (!rd16(obj + P_SHOT)) start_wall_r(obj);
}

/* $013AFC. State $44: crouched on the right wall; Right let go, standing; C jumps, B shoots ($44
 * and $48 start only each other: no other state leads here). */
void CartWallRCrouch(uint16_t obj)
{
    if (wall_sides(obj, 0xFFFF, start_floor, start_ceiling, start_to_wall_r, AT_LEFT, start_wall_l)) return;
    if (hit_on_side(obj, wall_r_hurt, wall_r_knocked)) return;
    if (!held(obj, PAD_RIGHT)) start_wall_r(obj);
    else if (pressed(obj, PAD_C)) jump_wall_r(obj);
    else if (held(obj, PAD_B)) start_wall_r_crouch_shoot(obj);
}

/* $013BA8. State $48: crouched and shooting on the right wall; the shot over, crouched. */
void CartWallRCrouchShoot(uint16_t obj)
{
    if (wall_sides(obj, 0xFFFF, start_floor_shoot, start_ceiling_shoot, start_to_wall_r_shoot, AT_LEFT,
                   start_wall_l_shoot))
        return;
    if (hit_on_side(obj, wall_r_hurt, wall_r_knocked)) return;
    if (pressed(obj, PAD_C)) { jump_wall_r_shoot(obj); return; }
    set8(obj + P_SHOT_B, 6);
    PlayerShoot(obj);
    if (!rd16(obj + P_SHOT)) start_wall_r_crouch(obj);
}

/* $013C5C. State $38: in the air toward the right wall: a side reached, onto it (a sound); C
 * turns toward the left wall when it may (CartCanFlipLeft); B shoots. */
void CartToWallR(uint16_t obj)
{
    if (air_sides(obj, CartLandSound3, start_floor, start_ceiling, start_wall_r, start_wall_l)) return;
    if (took_hit(obj)) { wall_r_knocked(obj); return; }
    if (pressed(obj, PAD_C) && CartCanFlipLeft(obj)) { start_to_wall_l(obj); return; }
    if (held(obj, PAD_B)) start_to_wall_r_shoot(obj);
}

/* $013CA0. The landing's sound ($B1). */
void CartLandSound3(void) { QueueSound(0xB1); }

/* $013D0A. State $40: in the air toward the right wall, shooting; the shot over, not. */
void CartToWallRShoot(uint16_t obj)
{
    if (air_sides(obj, 0, start_floor_shoot, start_ceiling_shoot, start_wall_r_shoot, start_wall_l_shoot))
        return;
    if (took_hit(obj)) { wall_r_knocked(obj); return; }
    if (pressed(obj, PAD_C) && CartCanFlipLeft(obj)) { start_to_wall_l_shoot(obj); return; }
    PlayerShoot(obj);
    if (!rd16(obj + P_SHOT)) start_to_wall_r(obj);
}

/* $013D8A. State $6C: nothing (the bytes before it are code no branch reaches). */
void CartNone6C(uint16_t obj) { (void)obj; }

/* $013DBC. State $4C: standing on the left wall; C jumps right, B shoots. */
void CartWallL(uint16_t obj)
{
    if (wall_sides(obj, 1, start_floor, start_ceiling, start_to_wall_l, AT_RIGHT, start_wall_r)) return;
    if (hit_on_side(obj, wall_l_hurt, wall_l_knocked)) return;
    if (pressed(obj, PAD_C)) jump_wall_l(obj);
    else if (held(obj, PAD_B)) start_wall_l_shoot(obj);
}

/* $013E60. State $54: shooting on the left wall; the shot over, standing. */
void CartWallLShoot(uint16_t obj)
{
    if (wall_sides(obj, 1, start_floor_shoot, start_ceiling_shoot, start_to_wall_l_shoot, AT_RIGHT,
                   start_wall_r_shoot))
        return;
    if (hit_on_side(obj, wall_l_hurt, wall_l_knocked)) return;
    if (pressed(obj, PAD_C)) { jump_wall_l_shoot(obj); return; }
    PlayerShoot(obj);
    if (!rd16(obj + P_SHOT)) start_wall_l(obj);
}

/* $013F02. State $5C: crouched on the left wall; Left let go, standing; C jumps, B shoots ($5C
 * and $60 start only each other: no other state leads here). */
void CartWallLCrouch(uint16_t obj)
{
    if (wall_sides(obj, 1, start_floor, start_ceiling, start_to_wall_l, AT_RIGHT, start_wall_r)) return;
    if (hit_on_side(obj, wall_l_hurt, wall_l_knocked)) return;
    if (!held(obj, PAD_LEFT)) start_wall_l(obj);
    else if (pressed(obj, PAD_C)) jump_wall_l(obj);
    else if (held(obj, PAD_B)) start_wall_l_crouch_shoot(obj);
}

/* $013FAA. State $60: crouched and shooting on the left wall; the shot over, crouched. */
void CartWallLCrouchShoot(uint16_t obj)
{
    if (wall_sides(obj, 1, start_floor_shoot, start_ceiling_shoot, start_to_wall_l_shoot, AT_RIGHT,
                   start_wall_r_shoot))
        return;
    if (hit_on_side(obj, wall_l_hurt, wall_l_knocked)) return;
    if (pressed(obj, PAD_C)) { jump_wall_l_shoot(obj); return; }
    set8(obj + P_SHOT_B, 6);
    PlayerShoot(obj);
    if (!rd16(obj + P_SHOT)) start_wall_l_crouch(obj);
}

/* $01405E. State $50: in the air toward the left wall: a side reached, onto it (a sound); C
 * turns toward the right wall when it may (CartCanFlipRight); B shoots. */
void CartToWallL(uint16_t obj)
{
    if (air_sides(obj, CartLandSound4, start_floor, start_ceiling, start_wall_r, start_wall_l)) return;
    if (took_hit(obj)) { wall_l_knocked(obj); return; }
    if (pressed(obj, PAD_C) && CartCanFlipRight(obj)) { start_to_wall_r(obj); return; }
    if (held(obj, PAD_B)) start_to_wall_l_shoot(obj);
}

/* $0140A2. The landing's sound ($B1). */
void CartLandSound4(void) { QueueSound(0xB1); }

/* $01410C. State $58: in the air toward the left wall, shooting; the shot over, not. */
void CartToWallLShoot(uint16_t obj)
{
    if (air_sides(obj, 0, start_floor_shoot, start_ceiling_shoot, start_wall_r_shoot, start_wall_l_shoot))
        return;
    if (took_hit(obj)) { wall_l_knocked(obj); return; }
    if (pressed(obj, PAD_C) && CartCanFlipRight(obj)) { start_to_wall_r_shoot(obj); return; }
    PlayerShoot(obj);
    if (!rd16(obj + P_SHOT)) start_to_wall_l(obj);
}

/* $014160. State $70: nothing. */
void CartNone70(uint16_t obj) { (void)obj; }

/* $0141B6. State $74: hurt on the floor; its animation over, standing. */
void CartFloorHurt(uint16_t obj)
{
    if (level_sides(obj, 0xFFFF, start_wall_r, start_wall_l, start_to_floor, AT_CEILING, start_ceiling))
        return;
    if (anim_ended(obj)) start_floor(obj);
}

/* $014274. State $84: knocked off the floor: a side reached, up again on it; P_TIMER frames, then
 * just in the air. */
void CartFloorKnocked(uint16_t obj)
{
    if (knocked_sides(obj)) return;
    add16(obj + P_TIMER, 0xFFFF);
    if (rd16(obj + P_TIMER) & 0x8000) start_to_floor(obj);
}

/* $0142CA. The time knocked: $10 frames, $100 knocked down. */
void CartKnockTimer(uint16_t obj)
{
    wr16(obj + P_TIMER, bit8(obj + P_STATUS, 4) ? 0x100 : 0x10);
}

/* $014316. State $78: hurt on the ceiling; its animation over, standing. */
void CartCeilingHurt(uint16_t obj)
{
    if (level_sides(obj, 1, start_wall_r, start_wall_l, start_to_ceiling, AT_FLOOR, start_floor)) return;
    if (anim_ended(obj)) start_ceiling(obj);
}

/* $0143A2. State $88: knocked off the ceiling. */
void CartCeilingKnocked(uint16_t obj)
{
    if (knocked_sides(obj)) return;
    add16(obj + P_TIMER, 0xFFFF);
    if (rd16(obj + P_TIMER) & 0x8000) start_to_ceiling(obj);
}

/* $01444C. State $7C: hurt on the right wall; its animation over, standing. */
void CartWallRHurt(uint16_t obj)
{
    if (wall_sides(obj, 0xFFFF, start_floor, start_ceiling, start_to_wall_r, AT_LEFT, start_wall_l)) return;
    if (anim_ended(obj)) start_wall_r(obj);
}

/* $0144DC. Knocked off the right wall: the animation by the side hit from. */
void CartKnockAnimR(uint16_t obj)
{
    if (bit8(obj + P_KNOCK, 1)) {
        wr16(obj + P_ANIM_SET, 0x5D);
        set8(obj + P_SPRITE, 3);
        return;
    }
    wr16(obj + P_ANIM_SET, 0x6D);
    clr8(obj + P_SPRITE, 3);
}

/* $014520. State $8C: knocked off the right wall. */
void CartWallRKnocked(uint16_t obj)
{
    if (knocked_sides(obj)) return;
    add16(obj + P_TIMER, 0xFFFF);
    if (rd16(obj + P_TIMER) & 0x8000) start_to_wall_r(obj);
}

/* $0145CA. State $80: hurt on the left wall; its animation over, standing. */
void CartWallLHurt(uint16_t obj)
{
    if (wall_sides(obj, 1, start_floor, start_ceiling, start_to_wall_l, AT_RIGHT, start_wall_r)) return;
    if (anim_ended(obj)) start_wall_l(obj);
}

/* $01465A. Knocked off the left wall: the animation by the side hit from. */
void CartKnockAnimL(uint16_t obj)
{
    if (bit8(obj + P_KNOCK, 1)) {
        wr16(obj + P_ANIM_SET, 0x6C);
        set8(obj + P_SPRITE, 3);
        return;
    }
    wr16(obj + P_ANIM_SET, 0x5C);
    clr8(obj + P_SPRITE, 3);
}

/* $01469E. State $90: knocked off the left wall. */
void CartWallLKnocked(uint16_t obj)
{
    if (knocked_sides(obj)) return;
    add16(obj + P_TIMER, 0xFFFF);
    if (rd16(obj + P_TIMER) & 0x8000) start_to_wall_l(obj);
}

/* $01474E. State $94: dead; its animation over and P_TIMER frames more, the object goes and its
 * player is out (v_players, the pause). */
void CartDead(uint16_t obj)
{
    if (!anim_ended(obj)) return;
    add16(obj + P_TIMER, 0xFFFF);
    if (!(rd16(obj + P_TIMER) & 0x8000)) return;
    wr16(obj + O_FLAGS, 0x1000);
    set8(obj + P_DISPLAY, 6);
    int bit = obj == v_player1 ? 0 : 1;
    clr8(v_players, bit);
    clr8(v_pauseFlags, bit);
}

/* ---- the movement (the table at $1479C) ---- */

/* $01478E. The movement of the state (relative words, by the state / 2). */
void CartMove(uint16_t obj)
{
    uint32_t at = 0x01479C + (uint32_t)((int16_t)rd16(obj + O_STATE) >> 1);
    call_object(at + (uint32_t)(int16_t)rom16(at), obj);
}

/* $0147E8. State 0: nothing. */
void CartMoveNone(uint16_t obj) { (void)obj; }

/* $0147EA. On the floor: facing and aim, the animation, walking, the cart's push. */
void CartMoveFloor(uint16_t obj)
{
    PlayerAim(obj);
    CartAnimFloor(obj);
    CartWalkX(obj);
    CartFloorPush(obj);
}

/* $0147F8. The cart's push across (on the floor; hurt there). */
void CartFloorPush(uint16_t obj) { wr32(obj + P_PUSH_X, rd32(CART_X)); }

/* $014800. Standing on the floor or the ceiling. */
void CartAnimFloor(uint16_t obj) { anim_by_facing(obj, 0x2E, 0x3E); }

/* the shot's animation by the aim's eighth of the turn and the facing (anims: 16 words), the
 * muzzle's place from the character's tables (longs: ys by the eighth, xs by it and the facing);
 * with the free aim from the aim's direction (PlayerFreeAimGun) */
static void cart_aim(uint16_t obj, uint32_t anims, uint32_t xs, uint32_t ys)
{
    uint16_t i = (uint16_t)(((uint8_t)(rd8(obj + P_AIM_DIR) + 0x10) & 0xE0) >> 4);
    uint16_t j = (uint16_t)(i + ((rd8(obj + P_SPRITE) & 8) << 1));
    wr16(obj + P_ANIM_SET, rom16(anims + j));
    if (bit8(obj + P_SHOT_B, 4)) { PlayerFreeAimGun(obj); return; }
    uint32_t c = (uint32_t)(int16_t)rd16(obj + P_CHARACTER);
    wr16(obj + P_GUN_Y, rom16(rom32(ys + c) + i));
    wr16(obj + P_GUN_X, rom16(rom32(xs + c) + j));
}

/* $01481A. Shooting on the floor: the aim; Red walks; the cart's push. */
void CartMoveFloorShoot(uint16_t obj)
{
    CartAimFloor(obj);
    if (red(obj)) CartWalkX(obj);
    wr32(obj + P_PUSH_X, rd32(CART_X));
}

/* $014832. Shooting on the floor: the animation and the muzzle by the aim (tables at $1487C). */
void CartAimFloor(uint16_t obj) { cart_aim(obj, 0x01487C, 0x01489C, 0x0148A4); }

/* $01490C. Crouched on the floor: facing and aim, walking, the cart's push. */
void CartMoveFloorCrouch(uint16_t obj)
{
    PlayerAim(obj);
    CartAnimCrouch(obj);
    CartWalkX(obj);
    wr32(obj + P_PUSH_X, rd32(CART_X));
}

/* $014922. Crouched on the floor or the ceiling. */
void CartAnimCrouch(uint16_t obj) { anim_by_facing(obj, 0x34, 0x44); }

/* $01493C. Crouched and shooting on the floor: straight ahead; the cart's push. */
void CartMoveFloorCrouchShoot(uint16_t obj)
{
    CartAimFloorCrouch(obj);
    wr32(obj + P_PUSH_X, rd32(CART_X));
}

/* the crouched shot's animation and muzzle: by the facing (anim, gun x) and the character */
static void crouch_aim(uint16_t obj, uint16_t anim_r, uint16_t anim_l, uint16_t red_x, uint16_t red_y,
                       uint16_t blue_x, uint16_t blue_y)
{
    int left = bit8(obj + P_SPRITE, 3);
    wr16(obj + P_ANIM_SET, left ? anim_l : anim_r);
    int r = red(obj);
    uint16_t x = r ? red_x : blue_x;
    wr16(obj + P_GUN_X, left ? (uint16_t)-x : x);
    wr16(obj + P_GUN_Y, r ? red_y : blue_y);
}

/* $014948. Crouched and shooting on the floor: straight ahead, by the facing and the character. */
void CartAimFloorCrouch(uint16_t obj) { crouch_aim(obj, 0x35, 0x45, 0x1C, 2, 0x1D, 0xFFFF); }

/* $0149A6. In the air toward the floor: facing and aim, steering both ways, falling. */
void CartMoveToFloor(uint16_t obj)
{
    PlayerAim(obj);
    CartAnimToLevel(obj);
    CartSteerX(obj);
    CartSteerYSlow(obj);
    CartFallDown(obj);
}

/* $0149B8. Falling toward the floor ($5000 a frame; knocked off it). */
void CartFallDown(uint16_t obj) { add32(obj + P_VY, 0x5000); }

/* $0149C2. In the air toward the floor or the ceiling. */
void CartAnimToLevel(uint16_t obj) { anim_by_facing(obj, 0x36, 0x46); }

/* $0149DC. In the air toward the floor, shooting: the aim, falling. */
void CartMoveToFloorShoot(uint16_t obj)
{
    CartAimToFloor(obj);
    add32(obj + P_VY, 0x5000);
}

/* $0149EA. In the air toward the floor, shooting: the animation and the muzzle by the aim ($14A34). */
void CartAimToFloor(uint16_t obj) { cart_aim(obj, 0x014A34, 0x014A54, 0x014A5C); }

/* $014AC4. Sliding on the floor: slowing down. */
void CartMoveFloorSlide(uint16_t obj) { PlayerSlideFriction(obj); }

/* $014ACC. On the ceiling: facing and aim, the animation, walking, the cart's push. */
void CartMoveCeiling(uint16_t obj)
{
    PlayerAim(obj);
    CartAnimFloor(obj);
    CartWalkX(obj);
    CartCeilingPush(obj);
}

/* $014ADA. The cart's push across (on the ceiling; hurt there). */
void CartCeilingPush(uint16_t obj) { wr32(obj + P_PUSH_X, rd32(CART_X)); }

/* $014AE2. Shooting on the ceiling: the aim; Red walks; the cart's push. */
void CartMoveCeilingShoot(uint16_t obj)
{
    CartAimCeiling(obj);
    if (red(obj)) CartWalkX(obj);
    wr32(obj + P_PUSH_X, rd32(CART_X));
}

/* $014AFA. Shooting on the ceiling: the animation and the muzzle by the aim ($14B44). */
void CartAimCeiling(uint16_t obj) { cart_aim(obj, 0x014B44, 0x014B64, 0x014B6C); }

/* $014BD4. Crouched on the ceiling: facing and aim, walking, the cart's push. */
void CartMoveCeilingCrouch(uint16_t obj)
{
    PlayerAim(obj);
    CartAnimCrouch(obj);
    CartWalkX(obj);
    wr32(obj + P_PUSH_X, rd32(CART_X));
}

/* $014BEA. Crouched and shooting on the ceiling: straight ahead; the cart's push. */
void CartMoveCeilingCrouchShoot(uint16_t obj)
{
    CartAimCeilingCrouch(obj);
    wr32(obj + P_PUSH_X, rd32(CART_X));
}

/* $014BF6. Crouched and shooting on the ceiling: straight ahead. */
void CartAimCeilingCrouch(uint16_t obj) { crouch_aim(obj, 0x35, 0x45, 0x1C, 0xFFFE, 0x1C, 1); }

/* $014C54. In the air toward the ceiling: facing and aim, steering both ways, falling up. */
void CartMoveToCeiling(uint16_t obj)
{
    PlayerAim(obj);
    CartAnimToLevel(obj);
    CartSteerX(obj);
    CartSteerYSlow(obj);
    CartFallUp(obj);
}

/* $014C66. Falling toward the ceiling (knocked off it). */
void CartFallUp(uint16_t obj) { add32(obj + P_VY, 0xFFFFB000); }

/* $014C70. In the air toward the ceiling, shooting: the aim, falling up. */
void CartMoveToCeilingShoot(uint16_t obj)
{
    CartAimToCeiling(obj);
    add32(obj + P_VY, 0xFFFFB000);
}

/* $014C7E. In the air toward the ceiling, shooting: the aim ($14CC8). */
void CartAimToCeiling(uint16_t obj) { cart_aim(obj, 0x014CC8, 0x014CE8, 0x014CF0); }

/* $014D58. Sliding on the ceiling: slowing down. */
void CartMoveCeilingSlide(uint16_t obj) { PlayerSlideFriction(obj); }

/* $014D5E. On the right wall: facing and aim, the animation, walking along it, the cart's push. */
void CartMoveWallR(uint16_t obj)
{
    PlayerAim(obj);
    CartAnimWallR(obj);
    CartWalkY(obj);
    CartWallRPush(obj);
}

/* $014D6C. The cart's push along the wall (on the right wall; hurt there). */
void CartWallRPush(uint16_t obj) { wr32(obj + P_PUSH_Y, rd32(CART_Y)); }

/* $014D74. Standing on the right wall. */
void CartAnimWallR(uint16_t obj) { anim_by_facing(obj, 0x5E, 0x4E); }

/* $014D8E. Shooting on the right wall: the aim; Red walks; the cart's push. */
void CartMoveWallRShoot(uint16_t obj)
{
    CartAimWallR(obj);
    if (red(obj)) CartWalkY(obj);
    wr32(obj + P_PUSH_Y, rd32(CART_Y));
}

/* $014DA6. Shooting on the right wall: the aim ($14DF0). */
void CartAimWallR(uint16_t obj) { cart_aim(obj, 0x014DF0, 0x014E10, 0x014E18); }

/* $014E80. Crouched on the right wall: facing and aim, walking along it, the cart's push. */
void CartMoveWallRCrouch(uint16_t obj)
{
    PlayerAim(obj);
    CartAnimWallRCrouch(obj);
    CartWalkY(obj);
    wr32(obj + P_PUSH_Y, rd32(CART_Y));
}

/* $014E96. Crouched on the right wall. */
void CartAnimWallRCrouch(uint16_t obj) { anim_by_facing(obj, 0x64, 0x54); }

/* $014EB0. Crouched and shooting on the right wall: straight ahead; the cart's push. */
void CartMoveWallRCrouchShoot(uint16_t obj)
{
    CartAimWallRCrouch(obj);
    wr32(obj + P_PUSH_Y, rd32(CART_Y));
}

/* $014EBC. Crouched and shooting on the right wall: straight ahead. */
void CartAimWallRCrouch(uint16_t obj) { crouch_aim(obj, 0x45, 0x35, 0x1D, 3, 0x1D, 1); }

/* $014F1A. In the air toward the right wall: facing and aim, steering both ways, falling right. */
void CartMoveToWallR(uint16_t obj)
{
    PlayerAim(obj);
    CartAnimToWallR(obj);
    CartSteerY(obj);
    CartSteerXSlow(obj);
    CartFallRight(obj);
}

/* $014F2C. Falling toward the right wall (knocked off it). */
void CartFallRight(uint16_t obj) { add32(obj + P_VX, 0x5000); }

/* $014F36. In the air toward the right wall. */
void CartAnimToWallR(uint16_t obj) { anim_by_facing(obj, 0x66, 0x56); }

/* $014F50. In the air toward the right wall, shooting: the aim, falling right. */
void CartMoveToWallRShoot(uint16_t obj)
{
    CartAimToWallR(obj);
    add32(obj + P_VX, 0x5000);
}

/* $014F5E. In the air toward the right wall, shooting: the aim ($14FA8). */
void CartAimToWallR(uint16_t obj) { cart_aim(obj, 0x014FA8, 0x014FC8, 0x014FD0); }

/* $015038. State $6C: nothing. */
void CartMoveNone6C(uint16_t obj) { (void)obj; }

/* $01503A. On the left wall: facing and aim, the animation, walking along it, the cart's push. */
void CartMoveWallL(uint16_t obj)
{
    PlayerAim(obj);
    CartAnimWallL(obj);
    CartWalkY(obj);
    CartWallLPush(obj);
}

/* $015048. The cart's push along the wall (on the left wall; hurt there). */
void CartWallLPush(uint16_t obj) { wr32(obj + P_PUSH_Y, rd32(CART_Y)); }

/* $015050. Standing on the left wall. */
void CartAnimWallL(uint16_t obj) { anim_by_facing(obj, 0x4E, 0x5E); }

/* $01506A. Shooting on the left wall: the aim; Red walks; the cart's push. */
void CartMoveWallLShoot(uint16_t obj)
{
    CartAimWallL(obj);
    if (red(obj)) CartWalkY(obj);
    wr32(obj + P_PUSH_Y, rd32(CART_Y));
}

/* $015082. Shooting on the left wall: the aim ($150CC). */
void CartAimWallL(uint16_t obj) { cart_aim(obj, 0x0150CC, 0x0150EC, 0x0150F4); }

/* $01515C. Crouched on the left wall: facing and aim, walking along it, the cart's push. */
void CartMoveWallLCrouch(uint16_t obj)
{
    PlayerAim(obj);
    CartAnimWallLCrouch(obj);
    CartWalkY(obj);
    wr32(obj + P_PUSH_Y, rd32(CART_Y));
}

/* $015172. Crouched on the left wall. */
void CartAnimWallLCrouch(uint16_t obj) { anim_by_facing(obj, 0x54, 0x64); }

/* $01518C. Crouched and shooting on the left wall: straight ahead; the cart's push. */
void CartMoveWallLCrouchShoot(uint16_t obj)
{
    CartAimWallLCrouch(obj);
    wr32(obj + P_PUSH_Y, rd32(CART_Y));
}

/* $015198. The same as the right wall's (CartAimWallRCrouch), the animations the other way. */
void CartAimWallLCrouch(uint16_t obj) { crouch_aim(obj, 0x35, 0x45, 0x1D, 3, 0x1D, 1); }

/* $0151F6. In the air toward the left wall: facing and aim, steering both ways, falling left. */
void CartMoveToWallL(uint16_t obj)
{
    PlayerAim(obj);
    CartAnimToWallL(obj);
    CartSteerY(obj);
    CartSteerXSlow(obj);
    CartFallLeft(obj);
}

/* $015208. Falling toward the left wall (knocked off it). */
void CartFallLeft(uint16_t obj) { add32(obj + P_VX, 0xFFFFB000); }

/* $015212. In the air toward the left wall. */
void CartAnimToWallL(uint16_t obj) { anim_by_facing(obj, 0x56, 0x66); }

/* $01522C. In the air toward the left wall, shooting: the aim, falling left. */
void CartMoveToWallLShoot(uint16_t obj)
{
    CartAimToWallL(obj);
    add32(obj + P_VX, 0xFFFFB000);
}

/* $01523A. In the air toward the left wall, shooting: the aim ($15284). */
void CartAimToWallL(uint16_t obj) { cart_aim(obj, 0x015284, 0x0152A4, 0x0152AC); }

/* $015314. State $70: nothing. */
void CartMoveNone70(uint16_t obj) { (void)obj; }

/* $015316. Dead: falling toward the floor. */
void CartMoveDead(uint16_t obj) { add32(obj + P_VY, 0x5000); }

/* the walk along a side: 3 a frame by the pad's two ways (`back`: the left or up bit); none
 * with the free aim */
static void walk(uint16_t obj, uint16_t speed, int back, int on)
{
    wr32(obj + speed, 0);
    if (bit8(obj + P_SHOT_B, 4)) return;
    if (held(obj, back)) wr32(obj + speed, 0xFFFD0000);
    else if (held(obj, on)) wr32(obj + speed, 0x30000);
}

/* $015320. Walking along the floor or the ceiling (Left, Right). */
void CartWalkX(uint16_t obj) { walk(obj, P_VX, PAD_LEFT, PAD_RIGHT); }

/* $015356. Walking along a wall (Up, Down). */
void CartWalkY(uint16_t obj) { walk(obj, P_VY, PAD_UP, PAD_DOWN); }

/* a jump's speed along the side by the pad (3 a frame) and its way: 8 forward, $C back, 4
 * straight */
static void jump_way(uint16_t obj, uint16_t speed, int back, int on)
{
    int left = bit8(obj + P_SPRITE, 3);
    if (held(obj, back)) {
        wr32(obj + speed, 0xFFFD0000);
        wr16(obj + P_JUMP, left ? 8 : 0xC);
    } else if (held(obj, on)) {
        wr32(obj + speed, 0x30000);
        wr16(obj + P_JUMP, left ? 0xC : 8);
    } else {
        wr32(obj + speed, 0);
        wr16(obj + P_JUMP, 4);
    }
}

/* $01538C. A jump off the floor or the ceiling: its speed across. */
void CartJumpX(uint16_t obj) { jump_way(obj, P_VX, PAD_LEFT, PAD_RIGHT); }

/* $0153DE. A jump off a wall: its speed along it. */
void CartJumpY(uint16_t obj) { jump_way(obj, P_VY, PAD_UP, PAD_DOWN); }

/* steering in the air across the fall: $2000 a frame to 3 */
static void steer(uint16_t obj, uint16_t speed, int back, int on)
{
    if (held(obj, back)) {
        add32(obj + speed, 0xFFFFE000);
        if ((int32_t)rd32(obj + speed) < (int32_t)0xFFFD0000) wr32(obj + speed, 0xFFFD0000);
    } else if (held(obj, on)) {
        add32(obj + speed, 0x2000);
        if ((int32_t)rd32(obj + speed) > 0x30000) wr32(obj + speed, 0x30000);
    }
}

/* steering along the fall: $1000 a frame to v_speedLimitY */
static void steer_slow(uint16_t obj, uint16_t speed, int back, int on)
{
    int32_t lim = (int32_t)rd32(v_speedLimitY), neg = (int32_t)(0u - (uint32_t)lim);
    if (held(obj, back)) {
        add32(obj + speed, 0xFFFFF000);
        if (!(neg < (int32_t)rd32(obj + speed))) wr32(obj + speed, (uint32_t)neg);
    } else if (held(obj, on)) {
        add32(obj + speed, 0x1000);
        if (!(lim > (int32_t)rd32(obj + speed))) wr32(obj + speed, (uint32_t)lim);
    }
}

/* $015430. In the air toward the floor or the ceiling: steering across (Left, Right). */
void CartSteerX(uint16_t obj) { steer(obj, P_VX, PAD_LEFT, PAD_RIGHT); }

/* $015480. The same: steering along the fall (Up, Down). */
void CartSteerYSlow(uint16_t obj) { steer_slow(obj, P_VY, PAD_UP, PAD_DOWN); }

/* $0154C8. In the air toward a wall: steering across (Up, Down). */
void CartSteerY(uint16_t obj) { steer(obj, P_VY, PAD_UP, PAD_DOWN); }

/* $015518. The same: steering along the fall (Left, Right). */
void CartSteerXSlow(uint16_t obj) { steer_slow(obj, P_VX, PAD_LEFT, PAD_RIGHT); }

/* ---- the cart's sides ---- */

/* $015560. A side reached: the walls first, then the floor and ceiling. */
uint16_t CartBounds(uint16_t obj)
{
    uint16_t at = CartWalls(obj);
    if (!at) at = CartFloors(obj);
    return at;
}

/* $015570. The floor (the box's bottom + 4 past it) or else the ceiling (as far above): held to
 * it; the attribute (also in P_FLOOR). */
uint16_t CartFloors(uint16_t obj)
{
    uint16_t d = (uint16_t)((int8_t)rd8(obj + P_BOTTOM) + 4);
    uint16_t x = rd16(obj + O_X), y = (uint16_t)(rd16(obj + O_Y) + d);
    uint16_t at = MapBoundsY(x, &y);
    wr16(obj + P_FLOOR, at);
    if (at == AT_FLOOR) {
        wr16(obj + O_X, x);
        wr16(obj + O_Y, (uint16_t)(y - d));
        return at;
    }
    y = (uint16_t)(rd16(obj + O_Y) - d);
    at = MapBoundsY(x, &y);
    wr16(obj + P_FLOOR, at);
    if (at == AT_CEILING) {
        wr16(obj + O_X, x);
        wr16(obj + O_Y, (uint16_t)(y + d));
    }
    return at;
}

/* $0155CC. The right wall (the box's right + $A past it) or else the left (its left - $A): held
 * to it; the attribute (also in P_FLOOR). The second test takes y as the first left it. */
uint16_t CartWalls(uint16_t obj)
{
    uint16_t y = rd16(obj + O_Y);
    uint16_t d = (uint16_t)((int8_t)rd8(obj + P_RIGHT) + 0xA);
    uint16_t x = (uint16_t)(rd16(obj + O_X) + d);
    uint16_t at = MapBoundsX(&x, &y);
    wr16(obj + P_FLOOR, at);
    if (at == AT_RIGHT) {
        wr16(obj + O_X, (uint16_t)(x - d));
        wr16(obj + O_Y, y);
        return at;
    }
    d = (uint16_t)((int8_t)rd8(obj + P_LEFT) - 0xA);
    x = (uint16_t)(rd16(obj + O_X) + d);
    at = MapBoundsX(&x, &y);
    wr16(obj + P_FLOOR, at);
    if (at == AT_LEFT) {
        wr16(obj + O_X, (uint16_t)(x - d));
        wr16(obj + O_Y, y);
    }
    return at;
}

/* $015634. In the air toward the ceiling, C: whether it may turn toward the floor: (x, $160) past
 * the box's floor or ceiling (MapBoundsY not 0). */
int CartCanFlipDown(uint16_t obj)
{
    uint16_t y = 0x160;
    return MapBoundsY(rd16(obj + O_X), &y) != 0;
}

/* $015646. Toward the floor: whether it may turn toward the ceiling: (x, $A0) past one. */
int CartCanFlipUp(uint16_t obj)
{
    uint16_t y = 0xA0;
    return MapBoundsY(rd16(obj + O_X), &y) != 0;
}

/* $015658. Toward the left wall: whether it may turn toward the right one: ($1C0, y) past a wall
 * (MapBoundsX not 0). */
int CartCanFlipRight(uint16_t obj)
{
    uint16_t x = 0x1C0, y = rd16(obj + O_Y);
    return MapBoundsX(&x, &y) != 0;
}

/* $01566A. Toward the right wall: whether it may turn toward the left one: ($80, y) past one. */
int CartCanFlipLeft(uint16_t obj)
{
    uint16_t x = 0x80, y = rd16(obj + O_Y);
    return MapBoundsX(&x, &y) != 0;
}

/* ---- shared helpers ---- */

/* $00C1F6. The box's walls (corners v_boundsX0/Y0 and v_boundsX1/Y1), as MapBoundsY its floor and
 * ceiling: x held to the wall it is past ($10 the right, $14 the left, 0 neither). With the second
 * corner left of the first, x is held to X1 at or left of it, and to X0 at or right of it when y
 * is past Y0 (away from Y1). With it right of the first, y is held too: to Y1 at or past it ($28,
 * $20), and past both X0 and Y0 to the nearer edge; with X1 = X0 only y is ($28, $20). No run
 * has had y moved here. */
uint16_t MapBoundsX(uint16_t *x, uint16_t *y)
{
    int16_t x0 = (int16_t)rd16(v_boundsX0), y0 = (int16_t)rd16(v_boundsY0);
    int16_t x1 = (int16_t)rd16(v_boundsX1), y1 = (int16_t)rd16(v_boundsY1);
    int16_t px = (int16_t)*x, py = (int16_t)*y;
    uint16_t a = 0;
    if (x1 == x0) {
        if (y1 == y0) return 0;
        if (y1 < y0) {
            if (y1 >= py) { *y = (uint16_t)y1; return AT_CEILING; }
            if (y0 > py) return 0;
            *y = (uint16_t)y0;
            return AT_FLOOR;
        }
        if (y1 <= py) { py = y1; *y = (uint16_t)y1; a = AT_FLOOR; }
        if (y0 < py) return a;
        *y = (uint16_t)y0;
        return AT_CEILING;
    }
    if (x1 < x0) {
        if (y1 == y0) {
            if (x1 >= px) { *x = (uint16_t)x1; return AT_LEFT; }
            if (x0 > px) return 0;
            *x = (uint16_t)x0;
            return AT_RIGHT;
        }
        if (x1 >= px) { px = x1; *x = (uint16_t)x1; a = AT_LEFT; }
        if (x0 > px) return a;
        if (y1 < y0 ? y0 > py : y0 < py) return a;
        *x = (uint16_t)x0;
        return AT_RIGHT;
    }
    if (y1 == y0) {
        if (x1 <= px) { *x = (uint16_t)x1; return AT_RIGHT; }
        if (x0 < px) return 0;
        *x = (uint16_t)x0;
        return AT_LEFT;
    }
    if (x1 <= px) { px = x1; *x = (uint16_t)x1; a = AT_RIGHT; }
    if (y1 < y0) {
        if (y1 >= py) { py = y1; *y = (uint16_t)y1; a = AT_CEILING; }
        if (x0 < px || y0 > py) return a;
        if ((int16_t)(py - y0) < (int16_t)(x0 - px)) { *y = (uint16_t)y0; return AT_FLOOR; }
        *x = (uint16_t)x0;
        return AT_LEFT;
    }
    if (y1 <= py) { py = y1; *y = (uint16_t)y1; a = AT_FLOOR; }
    if (x0 < px || y0 < py) return a;
    if ((int16_t)(y0 - py) < (int16_t)(x0 - px)) { *y = (uint16_t)y0; return AT_CEILING; }
    *x = (uint16_t)x0;
    return AT_LEFT;
}
