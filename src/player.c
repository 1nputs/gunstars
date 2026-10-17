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
#define P_HIT      0x76          /* how the last hit is taken (PlayerHitKind) */
#define P_INPUT_WAIT 0x7A        /* frames with no input before it is read again */
#define P_BLINK    0x7C          /* frames of blinking left (hurt) */
#define P_HIT_BY   0x82          /* what hit it */
#define P_ANIM     0xB8          /* the animation running */
#define P_DEMO     0xC2          /* long: the demo's recording (button state, frames) */
#define P_DEMO_LEFT 0xC6         /* byte: frames left of the recording's entry */
#define P_MOTION   0xCE          /* the motion input's step (0, 4, 8: a direction pair seen) */
#define P_MOTION_DIR 0xD0        /* byte: the first direction of it */
#define P_MOTION_TIME 0xD1       /* byte: frames left for the second */

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
    sub_1212A(obj);
    sub_16BF6(obj);
    sub_12BA0(obj);
    PlayerHitKind(obj);
    sub_0F64E(obj);
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

/* $00F5D0. How a hit is taken, by what hit: 4, or 8 / $C for some of the kinds from $30 up. */
void PlayerHitKind(uint16_t obj)
{
    uint16_t by = rd16(obj + P_HIT_BY);
    if (!by) { wr16(obj + P_HIT, 0); return; }
    wr16(obj + 0x78, 0);
    wr8(obj + 0xAA, 0);
    uint16_t kind = 4;
    if (by >= 0x30) {
        if ((by & 7) == 1) kind = 8;
        else if ((by & 7) == 5) kind = 0xC;
    }
    wr16(obj + P_HIT, kind);
}

/* $01ADB0. An object's $60 bytes cleared. */
void ObjectClear(uint16_t obj)
{
    for (uint16_t i = 0; i < 0x60; i += 4) wr32((uint16_t)(obj + i), 0);
}
