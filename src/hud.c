/* The status bar: each player's two rows of tiles (at a4: $FFB250 player 1's, $FFB278 player 2's;
 * a row $50 bytes on), sent to plane A's top each frame; its face, score, vitality and the
 * weapon icons (sprites 0 to 5). A player not playing shows PUSH START and can join. Its state is
 * the player's +$D4 (+$D6 its first frame), what to redraw +$81. */
#include "game.h"

#define P_INPUT_HELD  0x69       /* the pad as the status bar reads it (a player not playing) */
#define P_INPUT_PRESS 0x6A
#define P_INPUT_UP    0x6B
#define P_WEAPON      0x6E
#define P_WEAPON2     0x70
#define P_WEAPONS     0x72       /* bit 0, 1: the weapons held (else they blink in the select) */
#define P_REDRAW      0x81       /* bit 0 the frame, 1 the score, 2 the vitality; 5 PUSH START */
#define P_HEALTH_LOOK 0xBC       /* the face's tiles queued: 4 healthy, 8 hurt */
#define P_JOINS       0xC0
#define P_CHARACTER   0xCA
#define P_HUD         0xD4
#define P_HUD_START   0xD6
#define BLANK         0xE7EB     /* a blank tile; the digits from $E7ED */
#define DIGIT0        0xE7ED

/* $00A7B8. Both players' rows blank. */
void HudClear(void)
{
    for (uint16_t a = 0xB200; a < 0xB2F0; a += 2) wr16(a, BLANK);
}

/* $00A7CC. The six sprites the status bar keeps (the weapon icons), at the sprite table's start. */
void HudSprites(void)
{
    static const uint32_t spr[12] = { 0x880501, 0xF0, 0x880502, 0x100, 0x880503, 0x190, 0x880504, 0x1A0,
                                      0x800605, 1, 0x800600, 0 };
    for (int k = 0; k < 12; k++) wr32((uint16_t)(v_sprites + 4 * k), spr[k]);
    wr16(0xBE02, (uint16_t)(v_sprites + 0x30));
    wr8(0xBE00, 6);
    wr16(v_reservedSprites, 6);
}

/* $00AF54. A player's rows blank (and the row above). */
void HudBlank(uint16_t buf)
{
    for (uint16_t k = 0; k < 0x28; k += 2) {
        wr16((uint16_t)(buf - 0x50 + k), BLANK);
        wr16((uint16_t)(buf + k), BLANK);
        wr16((uint16_t)(buf + 0x50 + k), BLANK);
    }
}

/* $00AF72. A player's two icons hidden. */
void HudIconsOff(uint16_t p)
{
    uint16_t at = (uint16_t)(v_sprites + (p == v_player1 ? 0 : 0x10));
    wr16((uint16_t)(at + 4), 0);
    wr16((uint16_t)(at + 0xC), 0);
}

/* a DMA of tiles queued */
static void hud_dma(uint32_t src, uint16_t vram)
{
    uint16_t q = rd16(v_dmaQueue);
    QueueDmaBlock(&q, src, &vram);
    wr16(v_dmaQueue, q);
}

/* $00AF94. The weapon icons' tiles to the player's place in VRAM. */
void HudIcons(uint16_t p)
{
    hud_dma(0x0D5C1C, p == v_player1 ? 0xB800 : 0xBC00);
}

/* $00AB60. The frame round the face, the rows blank. */
void HudFrame(uint16_t p, uint16_t buf)
{
    set8(p + P_REDRAW, 0);
    HudBlank(buf);
    static const uint16_t top[4] = { 0xE7EC, 0xEFEC, 0xE7EC, 0xEFEC }, bottom[4] = { 0xF7EC, 0xFFEC, 0xF7EC, 0xFFEC };
    for (int k = 0; k < 4; k++) {
        wr16((uint16_t)(buf + 0x1C + 2 * k), top[k]);
        wr16((uint16_t)(buf + 0x6C + 2 * k), bottom[k]);
    }
}

/* $00A964. Can this player join now: not playing, not out of continues, not in mode $20, and in
 * the boss rush (mode $38) only while the other has vitality to share. */
uint16_t HudCanJoin(uint16_t p)
{
    int me = p == v_player1 ? 0 : 1;
    if (bit8(v_players, me)) return 0;
    if (rd16(p + P_JOINS) & 0x8000) return 0;
    if (rd16(v_mode) == 0x20) return 0;
    if (rd16(v_mode) != 0x38) return 1;
    int16_t half = (int16_t)rd16((uint16_t)((me ? v_player1 : v_player2) + O_VITALITY)) >> 1;
    return half > 0;
}

/* loc_00AFCA: PUSH START */
static void hud_push_start(uint16_t p, uint16_t buf)
{
    uint32_t top = p == v_player1 ? 0x00B00C : 0x00B05C, bottom = p == v_player1 ? 0x00B034 : 0x00B084;
    for (uint16_t k = 0; k < 0x28; k += 2) {
        wr16((uint16_t)(buf + k), rom16(top + k));
        wr16((uint16_t)(buf + 0x50 + k), rom16(bottom + k));
    }
}

/* loc_00A8AA: back to PUSH START */
static void hud_absent(uint16_t p)
{
    wr16(p + P_HUD, 0);
    wr16(p + P_HUD_START, 0);
}

/* loc_00AAF6: joined: playing */
static void hud_joined(uint16_t p)
{
    PlayerJoin(p);
    wr16(p + P_HUD, 0xC);
    wr16(p + P_HUD_START, 0);
}

/* the pause's skip: paused with A held on either pad */
static int pause_skip(void)
{
    uint8_t f = rd8(v_pauseFlags);
    return (f & 0x80) && (f & 0x40) && ((rd8(v_padHeld) | rd8(v_padHeld + 1)) & 0x40);
}

/* $00A8B6. Not playing: PUSH START blinking every $20 frames; Start joins (in a continue, the
 * select first). */
void HudAbsent(uint16_t p, uint16_t buf)
{
    if (!rd16(p + P_HUD_START)) {
        wr16(p + P_HUD_START, 4);
        wr8(p + P_REDRAW, 0x80);
        HudBlank(buf);
        HudIconsOff(p);
        HudIcons(p);
    }
    if (rd16(v_mode) == 0x14) return;
    if (!HudCanJoin(p)) {
        set8(p + P_REDRAW, 5);
        HudBlank(buf);
        return;
    }
    if (bit8(p + P_INPUT_PRESS, 7)) {
        PlayerJoinReset(p);
        if (rd8(p + 0xC1)) {
            hud_joined(p);
            return;
        }
        wr16(p + P_HUD, 4);
        wr16(p + P_HUD_START, 0);
        return;
    }
    if (pause_skip()) {
        set8(p + P_REDRAW, 5);
        if (rd16(p + 0xC0) & 0x8000) HudBlank(buf);
        else hud_push_start(p, buf);
        return;
    }
    if (rd16(v_frameCount) & 0x1F) return;
    set8(p + P_REDRAW, 5);
    if (bit8(0xA281, 5)) hud_push_start(p, buf);
    else HudBlank(buf);
}

/* $00AA32. The select's left and right: the other character. */
void HudSwapCharacter(uint16_t p)
{
    if (!(rd8(p + P_INPUT_PRESS) & 0xF)) return;
    wr16(p + P_CHARACTER, rd16(p + P_CHARACTER) ^ 4);
    set8(p + P_REDRAW, 0);
    wr16(p + P_HEALTH_LOOK, 0);
}

/* $00AAA6. The weapon select's moves: up or right the next weapon, down or left the one before
 * (4 to $10, round). */
void HudPickWeapon(uint16_t p)
{
    uint8_t in = rd8(p + P_INPUT_PRESS);
    int16_t w;
    if ((in & 1) || (in & 8)) {
        w = (int16_t)(rd16(p + P_WEAPON) + 4);
        if (w > 0x10) w = 4;
    } else if ((in & 2) || (in & 4)) {
        w = (int16_t)(rd16(p + P_WEAPON) - 4);
        if (w <= 0) w = 0x10;
    } else {
        return;
    }
    wr16(p + P_WEAPON, (uint16_t)w);
}

/* the selects' blinking text (eight words each row from `top`, `bottom`), or the frame */
static void hud_blink(uint16_t p, uint16_t buf, uint32_t top1, uint32_t bottom1, uint32_t top2, uint32_t bottom2)
{
    if (!pause_skip()) {
        if (rd16(v_frameCount) & 0xF) {
            if (!rd8(p + P_REDRAW)) return;
        }
        if (!bit8(0xA281, 4)) {
            HudFrame(p, buf);
            return;
        }
    }
    set8(p + P_REDRAW, 0);
    uint32_t top = p == v_player1 ? top1 : top2, bottom = p == v_player1 ? bottom1 : bottom2;
    for (uint16_t k = 0; k < 0x10; k += 2) {
        wr16((uint16_t)(buf + 0xA + k), rom16(top + k));
        wr16((uint16_t)(buf + 0x5A + k), rom16(bottom + k));
    }
}

/* $00AB94. The character select's text. */
void HudBlinkCharacter(uint16_t p, uint16_t buf)
{
    hud_blink(p, buf, 0x00AC1A, 0x00AC2A, 0x00AC3A, 0x00AC4A);
}

/* $00AC5A. The weapon select's text. */
void HudBlinkWeapon(uint16_t p, uint16_t buf)
{
    hud_blink(p, buf, 0x00ACE2, 0x00ACF2, 0x00AD02, 0x00AD12);
}

/* $00AD22. The face (its palette line by the player). */
void HudFace(uint16_t p, uint16_t buf)
{
    if (!bit8(p + P_REDRAW, 0)) return;
    uint16_t pal = p == v_player1 ? 0x4000 : 0x6000;
    uint32_t face = p == v_player1 ? 0x00AD72 : 0x00AD7E;
    for (uint16_t k = 0; k < 6; k += 2) wr16((uint16_t)(buf + 2 + k), rom16(face + k) ^ pal);
    for (uint16_t k = 0; k < 6; k += 2) wr16((uint16_t)(buf + 0x52 + k), rom16(face + 6 + k) ^ pal);
}

/* $00ADD4. A digit (blank while no digit before it was shown, unless not 0). */
void HudDigit(uint16_t digit, uint16_t *at, uint16_t *shown)
{
    digit &= 0xF;
    if (digit) *shown = 1;
    digit = (uint16_t)(digit + DIGIT0);
    if (!*shown) digit = BLANK;
    wr16(*at, digit);
    *at = (uint16_t)(*at + 2);
}

/* $00AD8A. The score: nine digits of BCD (+$64 on), the leading zeros blank (all 0: one 0). */
void HudScore(uint16_t p, uint16_t buf)
{
    if (!bit8(p + 0x81, 1)) return;
    uint16_t at = (uint16_t)(buf + 0xA), shown = 0, from = (uint16_t)(p + 0x64);
    HudDigit(rd8(from++), &at, &shown);
    for (int k = 0; k < 4; k++) {
        uint8_t b = rd8(from++);
        HudDigit((uint16_t)(b >> 4), &at, &shown);
        HudDigit(b, &at, &shown);
    }
    if (shown) return;
    at = (uint16_t)(at - 2);
    shown = 1;
    HudDigit(0, &at, &shown);
}

/* $00ADF0. The vitality: its label and up to four digits. */
void HudVitality(uint16_t p, uint16_t buf)
{
    if (!bit8(p + P_REDRAW, 2)) return;
    uint16_t at = (uint16_t)(buf + 0x5A);
    for (uint16_t k = 0; k < 0x12; k += 2) wr16((uint16_t)(at + k), rom16(0x00AE3E + k));
    at = (uint16_t)(at + 0x12);
    uint16_t v = rd16(p + O_VITALITY);
    if (v & 0x8000) return;
    for (int k = 0; k < 4; k++) {
        uint16_t q = (uint16_t)(v / 10), r = (uint16_t)(v % 10);
        at = (uint16_t)(at - 2);
        wr16(at, (uint16_t)(r + DIGIT0));
        if (!q) return;
        v = q;
    }
}

/* $00AE50. The face's tiles by the vitality ($32 and up healthy) and the character, queued when
 * they change. */
void HudHealth(uint16_t p)
{
    if (!bit8(p + P_REDRAW, 2) && !bit8(p + P_REDRAW, 0)) return;
    uint16_t vram = p == v_player1 ? 0xFB40 : 0xFCA0;
    uint32_t src;
    if ((int16_t)rd16(p + O_VITALITY) >= 0x32) {
        if (rd16(p + P_HEALTH_LOOK) == 4) return;
        wr16(p + P_HEALTH_LOOK, 4);
        src = rd16(p + P_CHARACTER) ? 0x08050E : 0x08038A;
    } else {
        if (rd16(p + P_HEALTH_LOOK) == 8) return;
        wr16(p + P_HEALTH_LOOK, 8);
        src = rd16(p + P_CHARACTER) ? 0x0805D0 : 0x08044C;
    }
    hud_dma(src, vram);
}

/* $00AF42. An icon's pattern by the weapon (0 none). */
void HudIcon(uint16_t at, uint16_t weapon)
{
    wr16(at, rom16(0x00AF4A + (uint32_t)(int16_t)((int16_t)weapon >> 1)));
}

/* $00AEEA. The two weapon icons; one not held blinks (with $A281 bit 0). */
void HudWeapons(uint16_t p)
{
    int p1 = p == v_player1;
    uint16_t w = rd16(p + P_WEAPON);
    if (!bit8(p + P_WEAPONS, 0) && bit8(0xA281, 0)) w = 0;
    HudIcon((uint16_t)(v_sprites + (p1 ? 4 : 0x14)), w);
    w = rd16(p + P_WEAPON2);
    if (!bit8(p + P_WEAPONS, 1) && bit8(0xA281, 0)) w = 0;
    HudIcon((uint16_t)(v_sprites + (p1 ? 0xC : 0x1C)), w);
}

/* $00A9E8. The character select (a continue). */
void HudSelectCharacter(uint16_t p, uint16_t buf)
{
    if (!rd16(p + P_HUD_START)) {
        wr16(p + P_HUD_START, 4);
        wr8(p + P_REDRAW, 0x81);
        wr16(p + P_HEALTH_LOOK, 0);
        HudIcons(p);
        HudFrame(p, buf);
    }
    HudBlinkCharacter(p, buf);
    if (!HudCanJoin(p)) {
        hud_absent(p);
        return;
    }
    if (rd8(p + P_INPUT_PRESS) & 0xF0) {
        wr16(p + P_HUD, 8);
        wr16(p + P_HUD_START, 0);
        return;
    }
    HudSwapCharacter(p);
    HudFace(p, buf);
    HudHealth(p);
    HudWeapons(p);
}

/* $00AA5C. The weapon select (a continue); a button joins. */
void HudSelectWeapon(uint16_t p, uint16_t buf)
{
    if (!rd16(p + P_HUD_START)) {
        wr16(p + P_HUD_START, 4);
        wr8(p + P_REDRAW, 0x81);
        wr16(p + P_HEALTH_LOOK, 0);
        HudIcons(p);
        HudFrame(p, buf);
    }
    HudBlinkWeapon(p, buf);
    if (!HudCanJoin(p)) {
        hud_absent(p);
        return;
    }
    if (rd8(p + P_INPUT_PRESS) & 0xF0) {
        hud_joined(p);
        return;
    }
    HudPickWeapon(p);
    HudFace(p, buf);
    HudHealth(p);
    HudWeapons(p);
}

/* $00AB48. Is this player out (Z set: not playing). */
int HudOut(uint16_t p)
{
    return bit8(v_players, p == v_player1 ? 0 : 1) ? 0 : 1;
}

/* $00B0FE. The bar's palette: the vitality blinking it, faster the lower (never above $31);
 * 0: always; the pause's skip shows it. Bit 2 of +$81 set on the frames it shows. */
uint16_t HudFlashLine(uint16_t p)
{
    if (pause_skip()) {
        set8(p + P_REDRAW, 2);
        return 0x6000;
    }
    int16_t v = (int16_t)rd16(p + O_VITALITY);
    uint16_t f = rd16(v_frameCount) & 3;
    if (v <= 0) {
        if (!f) set8(p + P_REDRAW, 2);
        return 0x4000;
    }
    uint16_t level = 1;
    if (v >= 0xA) {
        level = 2;
        if (v >= 0x19) {
            level = 3;
            if (v >= 0x32) return 0x6000;
        }
    }
    if (!f) {
        set8(p + P_REDRAW, 2);
        return 0x6000;
    }
    if (f > level) return 0x4000;
    if (f < level) return 0x6000;
    set8(p + P_REDRAW, 2);
    return 0x4000;
}

/* $00B0EE. count+1 tiles from `at` to palette line `pal` (bits 13, 14). */
void HudTint(uint16_t at, uint16_t count, uint16_t pal)
{
    for (uint16_t k = count;; k--) {
        wr16(at, (uint16_t)((rd16(at) & 0x9FFF) | pal));
        at = (uint16_t)(at + 2);
        if (k == 0) break;
    }
}

/* $00AB08. Playing: the face, score, vitality and icons; the bar's palette. With nobody playing
 * (the demo) it stays. */
void HudPlayer(uint16_t p, uint16_t buf)
{
    if (!rd16(p + P_HUD_START)) {
        wr16(p + P_HUD_START, 4);
        wr8(p + P_REDRAW, 0x87);
        HudFrame(p, buf);
    }
    if (HudOut(p) && rd8(v_players)) {
        hud_absent(p);
        return;
    }
    HudFace(p, buf);
    HudHealth(p);
    HudScore(p, buf);
    HudVitality(p, buf);
    HudWeapons(p);
    uint16_t pal = HudFlashLine(p);
    if (!rd8(p + P_REDRAW)) return;
    HudTint((uint16_t)(buf - 0x50), 0x13, pal);
    HudTint(buf, 0, pal);
    HudTint((uint16_t)(buf + 8), 0xF, pal);
    HudTint((uint16_t)(buf + 0x50), 0, pal);
    HudTint((uint16_t)(buf + 0x58), 0xF, pal);
}

/* a player's status bar state */
static void hud_state(uint16_t p, uint16_t buf)
{
    uint32_t fn = rom32(0x00A85C + (uint32_t)(int16_t)rd16(p + P_HUD));
    switch (fn) {
    case 0x00A8B6: HudAbsent(p, buf); break;
    case 0x00A9E8: HudSelectCharacter(p, buf); break;
    case 0x00AA5C: HudSelectWeapon(p, buf); break;
    case 0x00AB08: HudPlayer(p, buf); break;
    default: call68k_missing(fn); break;
    }
}

/* $00A86C. Player 1's status bar (fed pad 1 when not playing). */
void HudPlayer1(void)
{
    if (!bit8(v_players, 0)) {
        wr8(v_player1 + P_INPUT_HELD, rd8(v_padHeld));
        wr8(v_player1 + P_INPUT_PRESS, rd8(v_padPressed));
        wr8(v_player1 + P_INPUT_UP, rd8(v_padReleased));
    }
    hud_state(v_player1, 0xB250);
}

/* $00A82A. Both status bars. */
void HudUpdate(void)
{
    HudPlayer1();
    if (!bit8(v_players, 1)) {
        wr8(v_player2 + P_INPUT_HELD, rd8(v_padHeld + 1));
        wr8(v_player2 + P_INPUT_PRESS, rd8(v_padPressed + 1));
        wr8(v_player2 + P_INPUT_UP, rd8(v_padReleased + 1));
    }
    hud_state(v_player2, 0xB278);
}

/* one DMA of $30 words from $FFB200 on to VRAM (command `cmd`) */
static void hud_send(uint32_t cmd)
{
    vdp_ctrl16(0x8F02);
    vdp_ctrl(0x93309400);
    vdp_ctrl16(0x9500);
    vdp_ctrl16(0x96D9);
    vdp_ctrl16(0x977F);
    wr32(v_dmaCommand, cmd);
    vdp_ctrl16(rd16(v_dmaCommand));
    vdp_ctrl16(rd16(v_dmaCommand + 2));
}

/* $00A6DE. The status bar set up: its rows blank and sent (three DMAs, the Z80 off the bus),
 * its sprites, both players' bars drawn once. */
void HudStart(void)
{
    HudClear();
    uint16_t sr = cpu_get_sr();
    cpu_set_sr(0x2700);
    z80_busreq(0x100);
    while (z80_busack() & 1) {}
    vdp_ctrl16(rd16(VDP_REG(1)) | 0x10);
    hud_send(0x50000083);
    hud_send(0x50800083);
    hud_send(0x51000083);
    vdp_ctrl16(rd16(VDP_REG(1)) & ~0x10);
    z80_busreq(0);
    while (!(z80_busack() & 1)) {}
    cpu_set_sr(sr);
    HudSprites();
    wr16(v_player1 + P_HUD_START, 0);
    wr16(v_player2 + P_HUD_START, 0);
    HudUpdate();
    wr8(VDP_REG(17) + 1, 0);
    wr8(VDP_REG(18) + 1, 3);
}
