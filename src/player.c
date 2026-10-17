/* The players: their objects at v_player1 and v_player2 ($200 bytes each), updated before the
 * other objects each frame. A player's input comes from its pad, from a demo's recording, or
 * from a script (the story's and the stage's scenes walk them about); its routine (type 8,
 * PlayerFrame) then runs its moves, its weapon and its animation. */
#include "game.h"

/* a player's fields (beside the objects' common ones) */
#define P_HELD     0x69          /* bytes: the buttons held, pressed this frame, released */
#define P_PRESSED  0x6A
#define P_RELEASED 0x6B
#define P_WEAPON_A 0x6E          /* the two weapons held (words; 0: none) */
#define P_WEAPON_B 0x70
#define P_WEAPON   0x72          /* byte: which is used: 1, 2, or 3 (the two combined) */
#define P_INPUT    0x74          /* the input's kind, times 4 (PlayerInput's table) */
#define P_FLOOR_KIND 0x76        /* the floor stood on: 0 none, 4 flat, 8 and $C two slopes */
#define P_INPUT_WAIT 0x7A        /* frames with no input before it is read again */
#define P_BLINK    0x7C          /* frames of blinking left (hurt) */
#define P_FLOOR    0x82          /* the floor's attribute (MapSnap), 0 in the air */
#define P_WALL     0x84          /* the last wall's */
#define P_ANIM     0xB8          /* the animation running */
#define P_DEMO     0xC2          /* long: the demo's recording (button state, frames) */
#define P_DEMO_LEFT 0xC6         /* byte: frames left of the recording's entry */
#define P_MOTION   0xCE          /* the motion input's step (0, 4, 8: a direction pair seen) */
#define P_MOTION_DIR 0xD0        /* byte: the first direction of it */
#define P_MOTION_TIME 0xD1       /* byte: frames left for the second */

#define P_SPRITE   0x0E          /* byte: bit 3 flipped (facing left), bit 4 upside down */
#define P_X_FRAC   0x12
#define P_Y_FRAC   0x16
#define P_VX       0x18          /* long: the speeds, 16.16 */
#define P_VY       0x1C
#define P_TOP      0x28          /* signed bytes: the box's top and bottom from y */
#define P_BOTTOM   0x29
#define P_HITS     0x47          /* bits: 0 a bar above held, 1 a floor, 2 a wall left, 3 right */

#define PAD_UP    0x01
#define PAD_DOWN  0x02
#define PAD_LEFT  0x04
#define PAD_RIGHT 0x08
#define PAD_A     0x40

/* $00F0E2. Each player there is: its input, its routine, its vitality kept to 999; one marked
 * for removal is cleared. */
void PlayersUpdate(void)
{
    for (uint16_t obj = v_player1;; obj = v_player2) {
        if (rd16(obj + O_TYPE)) {
            PlayerInput(obj);
            call_object(rom32(OBJECT_ROUTINES + (uint32_t)(int16_t)rd16(obj + O_TYPE)), obj);
            PlayerClampVitality(obj);
            if (bit8(obj + O_FLAGS, 4)) PlayerRemove(obj);
        }
        if (obj == v_player2) return;
    }
}

/* $00F12E. The object cleared. It means to keep the position, but ObjectClear leaves d0 0 and
 * d1's low word $FFFF: x is 0 and y's fraction $FFFF. */
void PlayerRemove(uint16_t obj)
{
    uint32_t y = rd32(obj + O_Y);
    ObjectClear(obj);
    wr32(obj + O_X, 0);
    wr32(obj + O_Y, (y & 0xFFFF0000u) | 0xFFFF);
}

/* $00F146 */
void PlayerClampVitality(uint16_t obj)
{
    if ((int16_t)rd16(obj + O_VITALITY) > 999) wr16(obj + O_VITALITY, 999);
}

/* $00F158. The player's routine (type 8): nothing while the game is paused. */
void PlayerFrame(uint16_t obj)
{
    PlayerWeaponButton(obj);
    if (rd8(v_pauseFlags) & 0x80) return;
    PlayerMotionInput(obj);
    PlayerCollide(obj);
    sub_16BF6(obj);
    sub_12BA0(obj);
    PlayerFloorKind(obj);
    PlayerStates(obj);
    sub_16BF6(obj);
    sub_11560(obj);
    sub_16BF6(obj);
    /* the blinking after a hit: shown on odd frames, then for good */
    if (!rd16(obj + P_BLINK)) return;
    wr16(obj + P_BLINK, (uint16_t)(rd16(obj + P_BLINK) - 1));
    if (rd16(obj + P_BLINK)) {
        set8(obj + 0x23, 2);
        wr16(obj + O_FLAGS, (uint16_t)((rd16(obj + O_FLAGS) & 0x7FFF) | (rd16(v_frameCount) & 1) << 15));
        return;
    }
    clr8(obj + 0x23, 2);
    set8(obj + O_FLAGS, 7);
}

/* $00F194. A motion input: down and a side, then the other side within 10 frames. */
void PlayerMotionInput(uint16_t obj)
{
    uint32_t at = 0x00F1A2 + (uint32_t)((int16_t)rd16(obj + P_MOTION) >> 1);
    switch (at + (uint32_t)(int16_t)rom16(at)) {
    case 0x00F1A8: MotionStart(obj); return;
    case 0x00F1D0: MotionSecond(obj); return;
    case 0x00F1FC: MotionWait(obj); return;
    default: call_object_missing(at + (uint32_t)(int16_t)rom16(at), obj); return;
    }
}

static void motion_next(uint16_t obj, uint8_t dir)
{
    wr8(obj + P_MOTION_DIR, dir);
    wr8(obj + P_MOTION_TIME, 10);
    wr16(obj + P_MOTION, (uint16_t)(rd16(obj + P_MOTION) + 4));
}

/* $00F1A8 */
void MotionStart(uint16_t obj)
{
    uint8_t dir = rd8(obj + P_HELD) & 0xF;
    if (dir == (PAD_DOWN | PAD_RIGHT) || dir == (PAD_DOWN | PAD_LEFT)) { motion_next(obj, dir); return; }
    MotionWait(obj);
}

/* $00F1D0 */
void MotionSecond(uint16_t obj)
{
    uint8_t dir = rd8(obj + P_HELD) & 0xF;
    if (dir == PAD_RIGHT && bit8(obj + P_MOTION_DIR, 2)) { motion_next(obj, dir); return; }
    if (dir == PAD_LEFT && bit8(obj + P_MOTION_DIR, 3)) { motion_next(obj, dir); return; }
    MotionWait(obj);
}

/* $00F1FC */
void MotionWait(uint16_t obj)
{
    uint8_t t = rd8(obj + P_MOTION_TIME);
    if (t) { wr8(obj + P_MOTION_TIME, (uint8_t)(t - 1)); return; }
    wr16(obj + P_MOTION, 0);
}

/* $00F212. The input, by its kind (none while P_INPUT_WAIT counts down). */
void PlayerInput(uint16_t obj)
{
    uint16_t wait = rd16(obj + P_INPUT_WAIT);
    if (wait) {
        wr16(obj + P_INPUT_WAIT, (uint16_t)(wait - 1));
        PlayerInputNone(obj);
        return;
    }
    uint32_t fn = rom32(0x00F224 + (uint32_t)(int16_t)rd16(obj + P_INPUT));
    switch (fn) {
    case 0x00F24C: PlayerInputPad(obj); return;
    case 0x00F264: PlayerInputNone(obj); return;
    case 0x00F32C: PlayerInputWalkTo(obj); return;
    case 0x00F39E: PlayerInputC(obj); return;
    case 0x00F29A: PlayerInputDemo(obj); return;
    case 0x00F434: PlayerInput14(obj); return;
    case 0x00F478: PlayerInput18(obj); return;
    case 0x00F498: PlayerInput1C(obj); return;
    case 0x00F4D2: PlayerInput20(obj); return;
    case 0x00F404: PlayerInputDirections(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $00F24C. Its pad. */
void PlayerInputPad(uint16_t obj)
{
    if (obj == v_player1) ReadPlayerPad1(obj);
    else if (obj == v_player2) ReadPlayerPad2(obj);
}

/* $00F264 */
void PlayerInputNone(uint16_t obj)
{
    wr8(obj + P_HELD, 0);
    wr8(obj + P_PRESSED, 0);
    wr8(obj + P_RELEASED, 0);
}

/* $00F272 */
void ReadPlayerPad1(uint16_t obj)
{
    wr8(obj + P_HELD, rd8(v_padHeld));
    wr8(obj + P_PRESSED, rd8(v_padPressed));
    wr8(obj + P_RELEASED, rd8(v_padReleased));
}

/* $00F286 */
void ReadPlayerPad2(uint16_t obj)
{
    wr8(obj + P_HELD, rd8(v_padHeld + 1));
    wr8(obj + P_PRESSED, rd8(v_padPressed + 1));
    wr8(obj + P_RELEASED, rd8(v_padReleased + 1));
}

/* $00F29A. A demo's recording played back: (buttons held, frames) pairs. With v_demoRecord the
 * pad is recorded instead. */
void PlayerInputDemo(uint16_t obj)
{
    if (rd8(v_demoRecord)) {
        uint8_t n = rd8(obj + P_DEMO_LEFT);
        if (!n) {
            PlayerInputPad(obj);
            wr8(obj + P_DEMO_LEFT, 1);
            return;
        }
        uint8_t was = rd8(obj + P_HELD);
        PlayerInputPad(obj);
        if (was == rd8(obj + P_HELD) && rd8(obj + P_DEMO_LEFT) != 0xFF) {
            wr8(obj + P_DEMO_LEFT, (uint8_t)(rd8(obj + P_DEMO_LEFT) + 1));
            return;
        }
        uint32_t at = rd32(obj + P_DEMO);
        bus_wr8(at, was);
        bus_wr8(at + 1, rd8(obj + P_DEMO_LEFT));
        wr32(obj + P_DEMO, at + 2);
        wr8(obj + P_DEMO_LEFT, 1);
        return;
    }
    uint8_t n = rd8(obj + P_DEMO_LEFT);
    if (n && --n) {
        wr8(obj + P_DEMO_LEFT, n);
        wr8(obj + P_PRESSED, 0);
        wr8(obj + P_RELEASED, 0);
        return;
    }
    uint32_t at = rd32(obj + P_DEMO);
    uint8_t held = bus_rd8(at), was = rd8(obj + P_HELD), changed = held ^ was;
    wr8(obj + P_HELD, held);
    wr8(obj + P_PRESSED, held & changed);
    wr8(obj + P_RELEASED, was & changed);
    wr8(obj + P_DEMO_LEFT, bus_rd8(at + 1));
    wr32(obj + P_DEMO, at + 2);
}

/* $00F32C. Walk to x $E0 (C pressed on the way unless in state $50) and y $118 (player 1) or
 * $108. */
void PlayerInputWalkTo(uint16_t obj)
{
    PlayerInputNone(obj);
    int16_t x = (int16_t)(rd16(obj + O_X) & 0xFFF8);
    if (x < 0xE0) set8(obj + P_HELD, 3);
    else if (x > 0xE0) {
        set8(obj + P_HELD, 2);
        if (rd16(obj + O_STATE) != 0x50) set8(obj + P_PRESSED, 5);
    }
    int16_t to = obj == v_player1 ? 0x118 : 0x108;
    int16_t y = (int16_t)(rd16(obj + O_Y) & 0xFFF8);
    if (y < to) set8(obj + P_HELD, 1);
    else if (y > to) set8(obj + P_HELD, 0);
}

/* $00F4EC. The player's object becomes the scene's (type $5BC, $5C0 for player 2). */
void PlayerSceneType(uint16_t obj)
{
    wr16(obj + O_TYPE, obj == v_player1 ? 0x5BC : 0x5C0);
}

/* the scene's object takes over, in its state s */
static void to_scene(uint16_t obj, uint16_t s)
{
    PlayerSceneType(obj);
    wr16(obj + 0x48, s);
    wr16(obj + O_STATE, 0);
    wr16(obj + P_INPUT, 4);
}

/* $00F39E */
void PlayerInputC(uint16_t obj)
{
    if (rd16(0xA950) == 0x38) { PlayerInputNone(obj); return; }
    if ((int16_t)rd16(0xA950) < 0x48) { PlayerInputDirections(obj); return; }
    if (rd16(obj + P_ANIM) != 1 || !bit8(0xA028, 1)) { PlayerInputNone(obj); return; }
    to_scene(obj, rd8(0xA200) == 3 && obj != v_player1 ? 0x18 : 0x14);
}

/* $00F404. The pad's directions only. */
void PlayerInputDirections(uint16_t obj)
{
    if (obj == v_player1) ReadPlayerPad1(obj);
    else if (obj == v_player2) ReadPlayerPad2(obj);
    else return;
    wr8(obj + P_PRESSED, rd8(obj + P_PRESSED) & 0xC);
    wr8(obj + P_HELD, rd8(obj + P_HELD) & 0xC);
    wr8(obj + P_RELEASED, rd8(obj + P_RELEASED) & 0xC);
}

static void hold(uint16_t obj, uint8_t held, uint8_t pressed)
{
    wr8(obj + P_PRESSED, pressed);
    wr8(obj + P_HELD, held);
    wr8(obj + P_RELEASED, 0);
}

/* $00F434 */
void PlayerInput14(uint16_t obj)
{
    if (rd16(obj + P_ANIM) == 0x16) { hold(obj, 0x22, 0x22); return; }
    if (rd16(obj + P_ANIM) != 1) { PlayerInputNone(obj); return; }
    if ((int16_t)rd16(obj + O_Y) <= 0x100) { hold(obj, 0x22, 0x22); return; }
    to_scene(obj, 0x14);
}

/* $00F478 */
void PlayerInput18(uint16_t obj)
{
    if (rd16(obj + P_ANIM) != 1) { PlayerInputNone(obj); return; }
    to_scene(obj, 0x18);
}

/* $00F498. Walk right to x $DC, then the scene. */
void PlayerInput1C(uint16_t obj)
{
    if ((int16_t)rd16(obj + O_X) < 0xDC) { hold(obj, PAD_RIGHT, 0); return; }
    if ((int16_t)rd16(0xA950) < 0x3C) { PlayerInputNone(obj); return; }
    to_scene(obj, 0x1C);
}

/* $00F4D2. Walk right to x $BC. */
void PlayerInput20(uint16_t obj)
{
    if ((int16_t)rd16(obj + O_X) >= 0xBC) { PlayerInputNone(obj); return; }
    hold(obj, PAD_RIGHT, 0);
}

/* $00F500. A changes the weapon used, 1, 2, 3, when both are held (3 when one is missing). */
void PlayerWeaponButton(uint16_t obj)
{
    if (!bit8(obj + P_PRESSED, 6)) return;
    wr8(obj + 0x96, 0);
    wr16(obj + 0x94, 0);
    if (!rd8(obj + P_WEAPON)) return;
    if (!rd16(obj + P_WEAPON_A) || !rd16(obj + P_WEAPON_B)) { wr8(obj + P_WEAPON, 3); return; }
    wr8(obj + P_WEAPON, (uint8_t)(rd8(obj + P_WEAPON) + 1));
    if (rd8(obj + P_WEAPON) > 3) wr8(obj + P_WEAPON, 1);
}

/* $00F5D0. The kind of floor stood on: 4, or 8 / $C for two of the slopes ($30 up). */
void PlayerFloorKind(uint16_t obj)
{
    uint16_t floor = rd16(obj + P_FLOOR);
    if (!floor) { wr16(obj + P_FLOOR_KIND, 0); return; }
    wr16(obj + 0x78, 0);
    wr8(obj + 0xAA, 0);
    uint16_t kind = 4;
    if (floor >= 0x30) {
        if ((floor & 7) == 1) kind = 8;
        else if ((floor & 7) == 5) kind = 0xC;
    }
    wr16(obj + P_FLOOR_KIND, kind);
}

/* $01ADB0. An object's $60 bytes cleared. */
void ObjectClear(uint16_t obj)
{
    for (uint16_t i = 0; i < 0x60; i += 4) wr32((uint16_t)(obj + i), 0);
}

/* ---- the level: walls, floors, bars ---- */

/* $01212A. The level's walls and floors, by the player's state. */
void PlayerCollide(uint16_t obj)
{
    uint32_t at = 0x012138 + (uint32_t)((int16_t)rd16(obj + O_STATE) >> 1);
    uint32_t fn = at + (uint32_t)(int16_t)rom16(at);
    switch (fn) {
    case 0x01218E: CollideNone(obj); return;
    case 0x012190: CollideGround(obj); return;
    case 0x01219C: CollideAir(obj); return;
    case 0x0121C6: CollideAirNoGrab(obj); return;
    case 0x0121D6: CollideHanging(obj); return;
    default: call_object_missing(fn, obj); return;
    }
}

/* $01218E */
void CollideNone(uint16_t obj) { (void)obj; }

static int16_t top(uint16_t obj) { return (int8_t)rd8(obj + P_TOP); }
static int16_t bottom(uint16_t obj) { return (int8_t)rd8(obj + P_BOTTOM); }

/* a bar above (a thin floor at the box's top): held, the player under it, still */
static void grab_bar(uint16_t obj)
{
    uint16_t x = rd16(obj + O_X), y = (uint16_t)(rd16(obj + O_Y) + top(obj));
    uint16_t a = MapThinFloor(x, y);
    if (!a) return;
    set8(obj + P_HITS, 0);
    MapSnap(&x, &y, &a);
    wr16(obj + O_Y, (uint16_t)(y - top(obj)));
    wr16(obj + P_Y_FRAC, 0);
    wr32(obj + P_VY, 0);
}

/* a floor's underside at the box's top (less $A), stood under */
static void hang_on(uint16_t obj, uint16_t x, uint16_t y, uint16_t a, int16_t off)
{
    wr16(obj + P_FLOOR, a);
    MapSnap(&x, &y, &a);
    wr16(obj + O_Y, (uint16_t)(y - off));
    wr16(obj + P_Y_FRAC, 0);
}

static void grab_floor(uint16_t obj)
{
    int16_t off = (int16_t)(top(obj) - 0xA);
    uint16_t x = rd16(obj + O_X), y = (uint16_t)(rd16(obj + O_Y) + off);
    uint16_t a = MapFloor(x, y);
    if (a) hang_on(obj, x, y, a, off);
}

/* the same, three lines of cells down from 8 below */
static void grab_floor3(uint16_t obj)
{
    int16_t off = (int16_t)(top(obj) - 0xA);
    uint16_t x = rd16(obj + O_X), y = (uint16_t)(rd16(obj + O_Y) + off + 8);
    uint16_t a;
    if (!(a = MapFloor(x, y))) {
        y = (uint16_t)(y - 8);
        if (!(a = MapFloor(x, y))) {
            y = (uint16_t)(y - 8);
            if (!(a = MapFloor(x, y))) return;
        }
    }
    hang_on(obj, x, y, a, off);
}

/* $012190. On the ground. */
void CollideGround(uint16_t obj)
{
    PlayerWallsFeet(obj);
    PlayerFloor3(obj);
    grab_bar(obj);
}

/* $01219C. In the air: a bar going up; a floor coming down, or with Up a floor's underside. */
void CollideAir(uint16_t obj)
{
    PlayerWalls(obj);
    if (rd16(obj + P_VY) & 0x8000) { grab_bar(obj); return; }
    PlayerFloor2(obj);
    if (bit8(obj + P_HITS, 1) || !bit8(obj + P_HELD, 0)) return;
    grab_floor(obj);
}

/* $0121C6 */
void CollideAirNoGrab(uint16_t obj)
{
    PlayerWalls(obj);
    if (rd16(obj + P_VY) & 0x8000) { grab_bar(obj); return; }
    PlayerFloor2(obj);
}

/* $0121D6. Hanging under a floor. */
void CollideHanging(uint16_t obj)
{
    PlayerWallsHead(obj);
    PlayerFloor1(obj);
    if (!bit8(obj + P_HITS, 1)) grab_floor3(obj);
}

/* which way walls are looked for: the way it moves; still, the side last hit, else the way it
 * faces */
static int walls_left(uint16_t obj)
{
    int32_t vx = (int32_t)rd32(obj + P_VX);
    if (vx) return vx < 0;
    if (bit8(obj + P_HITS, 3)) return 1;
    if (bit8(obj + P_HITS, 2)) return 0;
    return bit8(obj + P_SPRITE, 3);
}

/* against the wall: x moved onto its side, a cell back */
static void wall_snap(uint16_t obj, uint16_t x, uint16_t y, uint16_t a, int16_t step)
{
    wr16(obj + P_WALL, a);
    MapSnap(&x, &y, &a);
    wr16(obj + O_X, (uint16_t)(x - step));
    wr16(obj + P_X_FRAC, 0);
}

/* $01220C. The walls at the feet (4 above the box's bottom), at x and a cell on. */
void PlayerWallsFeet(uint16_t obj)
{
    uint16_t y = (uint16_t)(bottom(obj) + rd16(obj + O_Y) - 4);
    uint16_t x = rd16(obj + O_X), a;
    if (walls_left(obj)) {
        if (!(a = MapWallLeft(x, y))) {
            x = (uint16_t)(x - 8);
            if (!(a = MapWallLeft(x, y))) return;
        }
        set8(obj + P_HITS, 2);
        wall_snap(obj, x, y, a, -8);
    } else {
        if (!(a = MapWallRight(x, y))) {
            x = (uint16_t)(x + 8);
            if (!(a = MapWallRight(x, y))) return;
        }
        set8(obj + P_HITS, 3);
        wall_snap(obj, x, y, a, 8);
    }
}

/* $0122A0 */
void PlayerWalls(uint16_t obj) { PlayerWallsFeet(obj); }

/* $012324. The walls at the head ($A below the box's top) and a cell down, a cell on. A wall
 * on the right at the head is not marked in P_HITS. */
void PlayerWallsHead(uint16_t obj)
{
    uint16_t y = (uint16_t)(top(obj) + rd16(obj + O_Y) - 0xA);
    uint16_t x = rd16(obj + O_X), a;
    if (walls_left(obj)) {
        x = (uint16_t)(x - 8);
        if (!(a = MapWallLeft(x, y))) {
            y = (uint16_t)(y + 8);
            if (!(a = MapWallLeft(x, y))) return;
        }
        set8(obj + P_HITS, 2);
        wall_snap(obj, x, y, a, -8);
    } else {
        x = (uint16_t)(x + 8);
        if (!(a = MapWallRight(x, y))) {
            y = (uint16_t)(y + 8);
            if (!(a = MapWallRight(x, y))) return;
            set8(obj + P_HITS, 3);
        }
        wall_snap(obj, x, y, a, 8);
    }
}

/* on the floor found (the shaking one: a line down) */
static void land(uint16_t obj, uint16_t x, uint16_t y, uint16_t a)
{
    set8(obj + P_HITS, 1);
    wr16(obj + P_FLOOR, a);
    if (a == 0x60) { wr16(obj + O_Y, (uint16_t)(rd16(obj + O_Y) + 1)); return; }
    MapSnap(&x, &y, &a);
    wr16(obj + O_Y, (uint16_t)(y - bottom(obj)));
    wr16(obj + P_Y_FRAC, 0);
}

/* the floor under the box's bottom, from `from` above it, `n` cells down */
static void floor_under(uint16_t obj, int16_t from, int n)
{
    uint16_t x = rd16(obj + O_X), y = (uint16_t)(rd16(obj + O_Y) + bottom(obj) - from);
    for (int i = 0;; i++, y = (uint16_t)(y + 8)) {
        uint16_t a = MapAttrHigh(x, y);
        if (a) { land(obj, x, y, a); return; }
        if (i == n - 1) return;
    }
}

/* $0123BC, $01241A, $012464: the floor at the feet, from 8 above in three cells, from 4 in two,
 * at them in one */
void PlayerFloor3(uint16_t obj) { floor_under(obj, 8, 3); }
void PlayerFloor2(uint16_t obj) { floor_under(obj, 4, 2); }
void PlayerFloor1(uint16_t obj) { floor_under(obj, 0, 1); }

