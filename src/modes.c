/* The modes between the title and a stage: the weapon select (modes $1C, $20), the stage select
 * ($24, $28; $30 before the fifth stage: who goes first), and the stage's start ($34). The
 * selects are small stages: the players stand on them (their state 4 a choice, 8 the next), the
 * camera scrolls from one to the next, objects ($49C, $4A0 the text box's pointer, $4F0 / $4F4
 * the players' arrows, $4FC the stage icons, $660 the blocks) show the choices. */
#include "game.h"

#define P_CHOOSING 0xC8          /* a player's choice: 0 done, 4 its character, 8 its weapon */
#define P_PRESS    0x6A          /* its pad's presses as the status bar copies them */
#define TEXT       0xCA88        /* the text box's text (a long: 0 when it has finished) */
#define TEXT_BUSY  0xCA8C        /* the text box writing (no new text) */

/* $01F4BE. Three colours of palette line 3 (from $1F4D8), into the fade's target too. */
void SelectColours(void)
{
    for (uint16_t k = 0; k < 6; k += 2) {
        wr16((uint16_t)(v_paletteTarget + 0x72 + k), rom16(0x01F4D8 + k));
        wr16((uint16_t)(v_palette + 0x72 + k), rom16(0x01F4D8 + k));
    }
}

/* $01F456. The text box (type $4F8) in slot `box`, waiting for its text. */
void SelectTextBox(uint16_t box)
{
    wr16(box + O_TYPE, 0x4F8);
    wr16(box + O_STATE, 0);
    wr16(box + 0x0E, 0x8200);
    wr16(box + 0x50, 4);
    wr16(box + O_X, 0x120);
    wr16(box + O_Y, 0x144);
    wr8(box + 0x58, 0);
    wr8(box + 0x59, 0x80);
}

/* $01F482. The same, with its typing sound ($AD). */
void SelectTextBoxB(uint16_t box)
{
    SelectTextBox(box);
    wr8(box + 0x58, 0xAD);
}

/* $01F4B0. The text's line for the scroll ($FFA90C). */
void SelectScrollLine(void)
{
    wr16(0xA90C, (uint16_t)(rd16(v_textLine) + 4));
}

/* $00EE0E. Player 2's choices kept for the next start, when it plays. */
void Player2KeepChoices(void)
{
    if (!bit8(v_players, 1)) return;
    wr16(0xA222, rd16(v_player2 + 0xCA));
    wr16(0xA226, rd16(v_player2 + 0x6E));
}

/* $00EDEE. Both players' choices kept. */
void PlayersKeepChoices(void)
{
    Player2KeepChoices();
    if (!bit8(v_players, 0)) return;
    wr16(0xA220, rd16(v_player1 + 0xCA));
    wr16(0xA224, rd16(v_player1 + 0x6E));
}

/* a player starting the game from the title: set up, its choices kept, playing (+$D4 $C) */
static void player_starts(uint16_t p, int second)
{
    if (second) Player2JoinReset();
    else Player1JoinReset();
    wr16(second ? 0xA222 : 0xA220, rd16(p + 0xCA));
    wr16(second ? 0xA226 : 0xA224, rd16(p + 0x6E));
    wr16(p + 0xD4, 0xC);
    wr16(p + 0xC0, (uint16_t)(rd16(p + 0xC0) + 1));
}

/* $00EB58. Who plays, by the title's choice ($A290: 0 player 1, 4 player 2, 8 both). */
void PlayersStart(void)
{
    int16_t c = (int16_t)(rd16(0xA290) - 4);
    if (c == 0) {
        wr8(v_players, 2);
        player_starts(v_player2, 1);
    } else if (c < 0) {
        wr8(v_players, 1);
        player_starts(v_player1, 0);
    } else {
        wr8(v_players, 3);
        player_starts(v_player1, 0);
        player_starts(v_player2, 1);
    }
}

/* $01E2AC. The players' arrows (types $4F0, $4F4). */
void SelectArrows(void)
{
    if (bit8(v_players, 0)) {
        wr16(0xCAA0, 0x4F0);
        wr16(0xCAA4, 0);
    }
    if (bit8(v_players, 1)) {
        wr16(0xCB00, 0x4F4);
        wr16(0xCB04, 0);
    }
}

/* the selects' small stage */
static void select_stage(uint16_t camx)
{
    wr16(v_playerMode, 0);
    PlayersInit();
    PlayersVitality();
    PlayersPlace(0, 0, 0x70);
    CameraPlace(camx, 0x100, 0, 0);
    PlaneARedraw();
    wr16(v_textLine, 0);
    wr16(v_deathLine, 0x180);
    wr16(v_limitLeft, 0x8F);
    wr16(v_limitRight, 0x1B0);
    wr16(v_limitTop, 0x40);
    wr16(v_limitBottom, 0x1C0);
    wr32(v_speedLimitX, 0x70000);
    wr32(v_speedLimitY, 0x70000);
    wr32(v_layout, 0x01B45A);
}

/* the selects' objects: the cue ($49C) and the text box's pointer ($4A0) */
static void select_cues(void)
{
    wr16(v_boss, 0x49C);
    wr16(0xC984, 0);
    wr16(0xC9C8, 4);
    wr16(0xC9E0, 0x4A0);
    wr16(0xC9E4, 0);
    wr16(0xCA28, 4);
}

/* the display on, fading in from black, the music */
static void select_show(uint8_t music)
{
    set8(VDP_REG(1) + 1, 6);
    wr8(v_displayOn, 0x80);
    wr16(v_fadeSpeed, 0xFE00);
    wr16(v_fadeLevel, 0x1000);
    wr8(v_soundNext, music);
    wr16(v_soundDelay, 0x12);
}

/* $01E18C. Mode $1C: the weapon select set up (the players' last choices: Red, and the second
 * weapon). */
void WeaponSelectStart(void)
{
    ScreenReset();
    RunCommands(0x01EF3E);
    LoadPalettes(0x01EFE0);
    SelectColours();
    wr16(0xA220, 0);
    wr16(0xA222, 4);
    wr16(0xA224, 4);
    wr16(0xA226, 4);
    PlayersStart();
    select_stage(0x200);
    select_cues();
    SelectTextBox(0xCA40);
    SelectArrows();
    HudStart();
    select_show(0x95);
    wr16(v_stage4, 0);
    wr16(v_mode, 0x20);
    wr16(v_modeState, 0);
}

/* $01E4AC. A player still choosing (its +$C8 or'd). */
uint16_t WeaponPending(void)
{
    uint16_t c = 0;
    if (bit8(v_players, 0)) c = (uint16_t)(c | rd16(v_player1 + P_CHOOSING));
    if (bit8(v_players, 1)) c = (uint16_t)(c | rd16(v_player2 + P_CHOOSING));
    return c;
}

/* $01E368. Both players to their character's choice. */
void WeaponPlayersReady(void)
{
    if (bit8(v_players, 0)) wr16(v_player1 + P_CHOOSING, 4);
    if (bit8(v_players, 1)) wr16(v_player2 + P_CHOOSING, 4);
}

/* $01E5DA. "Choose your character" when the text box is free (with two players: the other text). */
void WeaponTextCharacter(void)
{
    if (rd16(TEXT_BUSY)) return;
    TextClear8();
    wr16(0xC9C8, 8);
    wr16(TEXT, rd8(v_players) == 3 ? 0x10 : 8);
}

/* $01E614. "Choose your weapon" the same. */
void WeaponTextWeapon(void)
{
    if (rd16(TEXT_BUSY)) return;
    if (rd8(v_players) == 3) {
        TextClear8();
        wr16(0xC9C8, 8);
        wr16(TEXT, 0x10);
        return;
    }
    TextClear8();
    wr16(TEXT, 0xC);
    wr16(0xC9C8, 8);
    wr16(0xCA28, 4);
}

/* $01E63E. The stage select's text. */
void StageText(void)
{
    if (rd16(TEXT_BUSY)) return;
    TextClear8();
    wr16(TEXT, 0x14);
    wr16(0xC9C8, 8);
    wr16(0xCA28, 4);
}

/* $01E5D8 */
void WeaponNothing(uint16_t p, uint16_t arrow)
{
    (void)p;
    (void)arrow;
}

/* $01E4E2. The character: left or right the other one (its text), a button chooses it. */
void WeaponPickCharacter(uint16_t p, uint16_t arrow)
{
    (void)arrow;
    WeaponTextCharacter();
    uint8_t in = rd8(p + P_PRESS);
    if (in & 0xC) {
        wr16(p + 0xCA, rd16(p + 0xCA) ^ 4);
        set8(p + 0x81, 0);
        wr16(p + 0xBC, 0);
        QueueSound(0xAD);
        TextClear8();
        wr16(0xC9C8, 4);
        wr16(0xCA28, 8);
        wr16(TEXT, (uint16_t)(rd16(p + 0xCA) + 0x38));
    }
    if (!(in & 0xE0)) return;
    wr16(p + P_CHOOSING, 8);
    if (rd8(v_players) != 3) wr16(TEXT_BUSY, 0);
    QueueSound(0xC4);
}

/* $01E552. The weapon: left or right the next (4 to $10 round), a button chooses it. */
void WeaponPickWeapon(uint16_t p, uint16_t arrow)
{
    (void)arrow;
    WeaponTextWeapon();
    uint8_t in = rd8(p + P_PRESS);
    uint16_t w = rd16(p + 0x6E);
    if (in & 0xC) {
        if (in & 8) {
            w = (uint16_t)(w + 4);
            if ((int16_t)w > 0x10) w = 4;
        } else {
            w = (uint16_t)(w - 4);
            if (!w) w = 0x10;
        }
        wr16(p + 0x6E, w);
        QueueSound(0xAD);
        TextClear8();
        wr16(0xC9C8, 4);
        wr16(0xCA28, 8);
        wr16(TEXT, (uint16_t)(rd16(p + 0x6E) + 0x3C));
    }
    if (!(in & 0xE0)) return;
    wr16(p + P_CHOOSING, 0);
    if (!WeaponPending()) return;
    QueueSound(0xC4);
}

/* $01E4CC. A player's choice, by its +$C8. */
void WeaponPlayer(uint16_t p, uint16_t arrow)
{
    uint32_t fn = rom32(0x01E4D6 + (uint32_t)(int16_t)rd16(p + P_CHOOSING));
    switch (fn) {
    case 0x01E5D8: WeaponNothing(p, arrow); break;
    case 0x01E4E2: WeaponPickCharacter(p, arrow); break;
    case 0x01E552: WeaponPickWeapon(p, arrow); break;
    default: call68k_missing(fn); break;
    }
}

/* $01E6B0. Player 1's arrow at the "done" mark. */
void WeaponArrowDone(uint16_t p, uint16_t arrow)
{
    (void)p;
    wr16(arrow + O_X, 0xD0);
    wr16(arrow + O_Y, 0xFE);
}

/* $01E676. At its character. */
void WeaponArrowCharacter(uint16_t p, uint16_t arrow)
{
    wr16(arrow + O_X, rom16(0x01E6A4 + (uint32_t)(int16_t)((int16_t)rd16(p + 0xCA) >> 1)));
    wr16(arrow + O_Y, 0xAE);
}

/* $01E68A. At its weapon. */
void WeaponArrowWeapon(uint16_t p, uint16_t arrow)
{
    uint16_t w = rd16(p + 0x6E);
    if (!w) return;
    wr16(arrow + O_X, rom16(0x01E6A8 + (uint32_t)(int16_t)((int16_t)(w - 4) >> 1)));
    wr16(arrow + O_Y, 0xD6);
}

/* $01E660. Player 1's arrow (`arrow`) by its choice. */
void WeaponArrow(uint16_t p, uint16_t arrow)
{
    uint32_t fn = rom32(0x01E66A + (uint32_t)(int16_t)rd16(p + P_CHOOSING));
    switch (fn) {
    case 0x01E6B0: WeaponArrowDone(p, arrow); break;
    case 0x01E676: WeaponArrowCharacter(p, arrow); break;
    case 0x01E68A: WeaponArrowWeapon(p, arrow); break;
    default: call68k_missing(fn); break;
    }
}

/* $01E70E. Player 2's arrow: done. */
void WeaponArrow2Done(uint16_t p, uint16_t arrow)
{
    (void)p;
    wr16(arrow + O_X, 0x170);
    wr16(arrow + O_Y, 0xFE);
}

/* $01E6D4 */
void WeaponArrow2Character(uint16_t p, uint16_t arrow)
{
    wr16(arrow + O_X, rom16(0x01E702 + (uint32_t)(int16_t)((int16_t)rd16(p + 0xCA) >> 1)));
    wr16(arrow + O_Y, 0xAE);
}

/* $01E6E8 */
void WeaponArrow2Weapon(uint16_t p, uint16_t arrow)
{
    uint16_t w = rd16(p + 0x6E);
    if (!w) return;
    wr16(arrow + O_X, rom16(0x01E706 + (uint32_t)(int16_t)((int16_t)(w - 4) >> 1)));
    wr16(arrow + O_Y, 0xD6);
}

/* loc_01E6BE: player 2's arrow */
static void weapon_arrow2(uint16_t p, uint16_t arrow)
{
    uint32_t fn = rom32(0x01E6C8 + (uint32_t)(int16_t)rd16(p + P_CHOOSING));
    switch (fn) {
    case 0x01E70E: WeaponArrow2Done(p, arrow); break;
    case 0x01E6D4: WeaponArrow2Character(p, arrow); break;
    case 0x01E6E8: WeaponArrow2Weapon(p, arrow); break;
    default: call68k_missing(fn); break;
    }
}

/* $01E354. Faded in: the players choose. */
void WeaponFadeIn(void)
{
    if (rd16(v_fadeSpeed)) return;
    wr16(v_modeState, 4);
    wr16(TEXT_BUSY, 0);
    WeaponPlayersReady();
}

/* $01E42E. The select's rows scrolled with the camera ($12 of them; the bottom $A still). */
void WeaponScrollRows(void)
{
    uint16_t x = (uint16_t)-rd16(v_camX), still = (uint16_t)-0x200;
    for (uint16_t r = 0xE400; r <= 0xE620; r = (uint16_t)(r + 0x20)) wr16(r, x);
    for (uint16_t r = 0xE640; r <= 0xE760; r = (uint16_t)(r + 0x20)) wr16(r, still);
}

/* $01E412. Scrolling on to the stage select: there, mode $28. */
void WeaponScroll(void)
{
    if ((int32_t)rd32(v_camX) >= 0x3400000) {
        wr16(v_mode, 0x28);
        wr16(v_modeState, 0);
        wr16(v_modeState, 4);
    }
    WeaponScrollRows();
}

/* $01E38A. The players' choices; all made: their choices kept, the scroll to the stage select. */
void WeaponChoose(void)
{
    if (WeaponPending()) {
        if (bit8(v_players, 0)) {
            WeaponPlayer(v_player1, 0xCAA0);
            WeaponArrow(v_player1, 0xCAA0);
        }
        if (bit8(v_players, 1)) {
            WeaponPlayer(v_player2, 0xCB00);
            weapon_arrow2(v_player2, 0xCB00);
        }
        return;
    }
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
    PlayersKeepChoices();
    TextClear8();
    wr16(TEXT, 0xFFFF);
    wr16(0xC9C8, 4);
    wr16(0xCA28, 4);
    wr32(v_camSpeedX, 0x100000);
    wr32(v_planeSpeeds, 0x100000);
    wr8(v_planeFlagsA, 0x80);
    wr8(VDP_REG(11) + 1, 2);
    QueueSound(0xD4);
    WeaponScroll();
}

/* $01E33E */
void WeaponStep(void)
{
    call68k(rom32(0x01E348 + (uint32_t)(int16_t)rd16(v_modeState)));
}

/* $01E728. Palette line 1 from $BBB8C (and the fade's target). */
void SelectPalette(void)
{
    for (uint16_t k = 0; k < 0x20; k += 2) {
        uint16_t c = rom16(0x0BBB8C + k);
        wr16((uint16_t)(v_palette + 0x20 + k), c);
        wr16((uint16_t)(v_paletteTarget + 0x20 + k), c);
    }
}

/* loc_01E782: a player's character and weapon lit (colour $8E) in its colours from `at` */
static void weapon_mark(uint16_t p, uint16_t at)
{
    uint16_t i = (uint16_t)((int16_t)rd16(p + 0xCA) >> 1);
    wr16((uint16_t)(at + i), 0x8E);
    wr16((uint16_t)(at + 0x80 + i), 0x8E);
    uint16_t w = rd16(p + 0x6E);
    if (!w) return;
    i = (uint16_t)((int16_t)(w - 4) >> 1);
    wr16((uint16_t)(at + 4 + i), 0x8E);
    wr16((uint16_t)(at + 0x84 + i), 0x8E);
}

/* $01E746. Player 1's choices lit. */
void WeaponMark1(void)
{
    if (!bit8(v_players, 0)) return;
    weapon_mark(v_player1, (uint16_t)(v_palette + 0x22));
}

/* $01E7CE. The cards' sprites (24, from $1E804) at x `x` (hidden left of $60). */
void SelectCards(uint16_t x)
{
    uint16_t a = 0xA100;
    for (uint16_t k = 0; k < 24; k++) {
        uint32_t e = 0x01E804 + 8u * k;
        wr16(a, rom16(e));
        wr16((uint16_t)(a + 2), rom16(e + 2));
        wr16((uint16_t)(a + 4), rom16(e + 4));
        uint16_t sx = (uint16_t)(rom16(e + 6) + x);
        wr16((uint16_t)(a + 6), (int16_t)(sx - 0x60) >= 0 ? sx : 0);
        a = (uint16_t)(a + 8);
    }
    wr16(a, 0xFFFF);
    DrawSpriteList(0xFFFFA100);
}

/* $01E7A6. Each player's card of choices, moving with the camera. */
void WeaponCards(void)
{
    if (bit8(v_players, 0)) SelectCards((uint16_t)(0x200 - rd16(v_camX)));
    if (!bit8(v_players, 1)) return;
    SelectCards((uint16_t)(0x2A0 - rd16(v_camX)));
}

/* $01E332. The step, the cards, the lit choices. */
void WeaponSelectStep(void)
{
    WeaponStep();
    WeaponCards();
    SelectPalette();
    WeaponMark1();
    if (!bit8(v_players, 1)) return;
    weapon_mark(v_player2, (uint16_t)(v_palette + 0x2E));
}

/* the selects' frame around their step */
static void select_frame(void (*step)(void))
{
    ObjectsMove();
    CameraMove();
    Collisions();
    SpritesStart();
    DrawListStart();
    PlayersUpdate();
    EffectsUpdate();
    step();
    ObjectsUpdate();
    DrawListEnd();
    SelectScrollLine();
    HudUpdate();
    ScreenShake();
    BuildSprites();
    ScrollFrameSelect();
    wr16(v_gameFrames, (uint16_t)(rd16(v_gameFrames) + 1));
}

/* $01E2D6. Mode $20: the weapon select. */
void WeaponSelect(void)
{
    select_frame(WeaponSelectStep);
}

/* ---- the stage select ---- */

/* $01EAA6. The four blocks (type $660) under the stages' icons. */
void StageBlocks(void)
{
    for (uint16_t s = 0xCCE0; s <= 0xCF20; s = (uint16_t)(s + 0xC0)) {
        wr16(s + O_TYPE, 0x660);
        wr16(s + O_STATE, 0);
        wr16(s + 0x48, 0);
    }
}

/* $01EB1C. A cleared stage's mark (stage `stage` times 4) into plane A. */
void StageChunk(uint16_t stage)
{
    uint16_t v = rom16(0x01EB4A + (uint32_t)(int16_t)((int16_t)stage >> 1));
    ChunkDma(0xFFFF50E0, v);
    ChunkDma(0xFFFF5100, (uint16_t)(v + 8));
}

/* $01EAE0. The cleared stages marked; the cursor on the first one not cleared. */
void StageClearedChunks(void)
{
    uint16_t s = 0, k = 0;
    for (; k < 4; k++, s = (uint16_t)(s + 4)) {
        if (!bit8(v_stagesCleared, k)) break;
        StageChunk(s);
    }
    if (k == 4) return;
    wr16(v_stage4, s);
    for (k++, s = (uint16_t)(s + 4); k < 4; k++, s = (uint16_t)(s + 4))
        if (bit8(v_stagesCleared, k)) StageChunk(s);
}

/* the stages' icons (type $4FC) at x from $1EE80 plus `dx` */
static void stage_icons(uint16_t dx, int clear48)
{
    for (uint16_t k = 0; k < 8; k += 2) {
        uint16_t s = rom16(0x01EE88 + k);
        wr16(s + O_TYPE, 0x4FC);
        wr16(s + O_STATE, 0);
        if (clear48) wr16(s + 0x48, 8);
        wr16(s + O_X, (uint16_t)(rom16(0x01EE80 + k) + dx));
        wr16(s + O_Y, 0xE8);
        if (clear48) wr16(s + 0x48, 0);
    }
}

/* $01EE1E */
void StageIcons(void)
{
    stage_icons(0, 1);
}

/* $01EE46. Placed by the camera (arriving from the weapon select). */
void StageIconsMoved(void)
{
    stage_icons((uint16_t)(0x480 - rd16(v_camX)), 1);
}

/* $01F000. Placed, their looks left. */
void StageIconsPlace(void)
{
    stage_icons(0, 0);
}

/* $01E8C4. Mode $24: the stage select set up (stage 5, $14: mode $30, who goes first; $10, the
 * final stages: mode $2C). */
void StageSelectStart(void)
{
    ScreenResetAlt();
    RunCommands(0x01EF3E);
    LoadPalettes(0x01EFF0);
    SelectColours();
    select_stage(0x480);
    uint16_t st = rd16(v_stage4);
    if (st == 0x10) {
        wr16(v_boss, 0x49C);
        wr16(0xC984, 0);
        wr16(0xC9C8, 0x10);
        StageBlocks();
        SelectTextBoxB(0xCA40);
        StageClearedChunks();
        StageIconsPlace();
        HudStart();
        select_show(0x8F);
        wr16(v_mode, 0x2C);
        wr16(v_modeState, 0);
        return;
    }
    select_cues();
    StageBlocks();
    if (st == 0x14) {
        SelectTextBoxB(0xCA40);
        StageClearedChunks();
        StageIconsPlace();
        HudStart();
        select_show(0x95);
        wr16(v_mode, 0x30);
        wr16(v_modeState, 0);
        return;
    }
    SelectTextBox(0xCA40);
    StageClearedChunks();
    WeaponPlayersReady();
    StageIcons();
    HudStart();
    select_show(0x95);
    wr16(v_mode, 0x28);
    wr16(v_modeState, 0);
}

/* $01EEC8. Both players' presses. */
uint8_t SelectPressed(void)
{
    uint8_t in = 0;
    if (bit8(v_players, 0)) in |= rd8(0xA46A);
    if (bit8(v_players, 1)) in |= rd8(0xA66A);
    return in;
}

/* loc_01EC3A: choosing */
static void stage_choosing(void)
{
    wr16(v_modeState, 0xC);
    wr16(TEXT_BUSY, 0);
}

/* loc_01ECEE: the stage chosen: the fade out */
static void stage_go(void)
{
    wr16(v_modeState, 0x14);
    wr8(v_soundNext, 0xF6);
    wr16(v_soundDelay, 1);
    wr16(v_fadeSpeed, 0x200);
    wr16(v_fadeLevel, 0);
}

/* $01EBD8 */
void StageFadeIn(void)
{
    if (rd16(v_fadeSpeed)) return;
    stage_choosing();
}

/* $01EC04. The camera arriving at the stage select ($480): there, still. */
void StageScrollIn(void)
{
    if ((int32_t)rd32(v_camX) < 0x4800000) {
        WeaponScrollRows();
        return;
    }
    wr32(v_camX, 0x4800000);
    wr32(v_planes, 0x4800000);
    wr32(v_camSpeedX, 0);
    wr32(v_planeSpeeds, 0);
    WeaponScrollRows();
    QueueSound(0xBA);
    stage_choosing();
}

/* $01EBE2. From the weapon select: its colours, the stages' marks and icons. */
void StageArrive(void)
{
    LoadPalettes(0x01EFF0);
    SelectColours();
    WeaponPlayersReady();
    StageClearedChunks();
    StageIconsMoved();
    wr16(v_modeState, 8);
    StageScrollIn();
}

/* $01EDD2 */
void StageNothing(uint16_t p)
{
    (void)p;
}

/* $01ED44. Right (left) the next stage not cleared, its text. */
void StagePick(uint16_t p)
{
    StageText();
    uint8_t in = (uint8_t)(rd8(p + P_PRESS) & 0xC);
    uint16_t s = rd16(v_stage4);
    if (in) {
        if (in & 8) {
            do {
                s = (uint16_t)(s + 4);
                if ((int16_t)s > 0xC) s = 0;
            } while (bit8(v_stagesCleared, (int16_t)s >> 2));
        } else {
            do {
                s = (uint16_t)(s - 4);
                if ((int16_t)s < 0) s = 0xC;
            } while (bit8(v_stagesCleared, (int16_t)s >> 2));
        }
        wr16(v_stage4, s);
        QueueSound(0xAD);
        TextClear8();
        wr16(0xC9C8, 4);
        wr16(0xCA28, 8);
        wr16(TEXT, (uint16_t)(rd16(v_stage4) + 0x50));
    }
    if (!(in & 0xE0)) return;                     /* (in kept only bits 2, 3: never) */
    wr16(p + P_CHOOSING, 0);
    QueueSound(0xC4);
}

/* $01ED32 */
void StageMove(uint16_t p)
{
    uint32_t fn = rom32(0x01ED3C + (uint32_t)(int16_t)rd16(p + P_CHOOSING));
    switch (fn) {
    case 0x01EDD2: StageNothing(p); break;
    case 0x01ED44: StagePick(p); break;
    default: call68k_missing(fn); break;
    }
}

/* $01EC46. Start: that stage; A, B or C: its description; else the cursor. */
void StageChoose(void)
{
    uint8_t in = SelectPressed();
    if (in & 0x80) {
        QueueSound(0xC4);
        stage_go();
        return;
    }
    if (in & 0x60) {
        wr16(v_modeState, 0x10);
        QueueSound(0xC4);
        TextClear8();
        wr16(TEXT, (uint16_t)((rd16(v_stage4) & 0xC) + 0x18));
        wr16(0xC9C8, 8);
        wr16(0xCA28, 0xC);
        return;
    }
    if (bit8(v_players, 0)) StageMove(v_player1);
    if (bit8(v_players, 1)) StageMove(v_player2);
}

/* $01ECC2. The description; it ends (or Start): that stage. */
void StageDescribe(void)
{
    if (!rd32(TEXT)) {
        stage_go();
        return;
    }
    uint8_t in = SelectPressed();
    if (in & 0x80) {
        stage_go();
        return;
    }
    if (!(rd16(0xCA92) & 0x8000)) return;
    if (in & 0x70) wr16(0xC9C8, 0xC);
}

/* $01ED0C. Faded out: the stage (mode $34). */
void StageLeave(void)
{
    if (rd16(v_fadeSpeed) || !rd16(v_fadeLevel)) return;
    clr8(VDP_REG(1) + 1, 6);
    wr8(v_displayOn, 0);
    wr16(v_mode, 0x34);
    wr16(v_modeState, 0);
}

/* $01EDDC. The blocks of the stages whose gems are taken ($A228's bits). */
void StageMarks(void)
{
    uint8_t g = rd8(0xA228);
    if (!g) return;
    if (g & 1) wr16(0xCD28, 4);
    if (g & 2) wr16(0xCDE8, 8);
    if (g & 4) wr16(0xCEA8, 0xC);
    if (g & 8) wr16(0xCF68, 0x10);
}

/* $01EBB6 */
void StageStep(void)
{
    call68k(rom32(0x01EBC0 + (uint32_t)(int16_t)rd16(v_modeState)));
}

/* $01EBAE. The step, the blocks, the icons' looks (lit: cleared or the cursor's). */
void StageSelectStep(void)
{
    StageStep();
    StageMarks();
    for (uint16_t s = 0; s < 0x10; s = (uint16_t)(s + 4)) {
        uint16_t icon = rom16(0x01EE88 + (uint32_t)((int16_t)s >> 1));
        int lit = bit8(v_stagesCleared, (int16_t)s >> 2) || s == rd16(v_stage4);
        wr16(icon + 0x48, lit ? 4 : 8);
    }
}

/* $01EB52. Mode $28: the stage select. */
void StageSelect(void)
{
    select_frame(StageSelectStep);
}

/* ---- mode $30: before the fifth stage, with two players: who goes first ---- */

#define FIRST 0xA298             /* 0 player 1 first, 4 player 2 */

/* loc_01F284: the fade out to the stage */
static void first_go(void)
{
    wr16(v_modeState, 8);
    QueueSound(0xF6);
    wr16(v_fadeSpeed, 0x200);
    wr16(v_fadeLevel, 0);
}

/* $01F3D0. The last text read: the stage. */
void FirstWait(void)
{
    if (rd32(TEXT)) return;
    first_go();
}

/* $01F390. The players' places swapped: the text. */
void FirstSwapped(void)
{
    if (rd32(0xCC08)) return;
    wr16(v_sceneStep, 0x18);
    wr16(0xCC08, 4);
    wr16(0xCC68, 4);
    TextClear8();
    wr16(0xC9C8, 8);
    wr16(TEXT, rd16(FIRST) ? 0x30 : 0x34);
    FirstWait();
}

/* $01F3DE. Left or right on either pad: the other player first. */
void FirstToggle(void)
{
    if ((rd8(v_padPressed) | rd8(v_padPressed + 1)) & 0xC) wr16(FIRST, rd16(FIRST) ^ 4);
}

/* loc_01F3F6: the choice's sprites, the two players' marks */
static void first_sprites(void)
{
    uint16_t a = 0xA100;
    for (uint16_t k = 0; k < 8; k += 2) {
        wr32(a, 0x00F205E0);
        wr16((uint16_t)(a + 4), rom16(0x01F446 + k));
        wr16((uint16_t)(a + 6), rom16(0x01F44E + k));
        a = (uint16_t)(a + 8);
    }
    wr16(a, 0xFFFF);
    DrawSpriteList(0xFFFFA100);
    int swap = rd16(FIRST) != 0;
    wr16(0xCC08, swap ? 8 : 4);
    wr16(0xCC68, swap ? 4 : 8);
}

/* $01F36A. Choosing; a button: chosen. */
void FirstChoose(void)
{
    if (!(SelectPressed() & 0xF0)) {
        FirstToggle();
        first_sprites();
        return;
    }
    wr16(v_sceneStep, 0x14);
    wr16(0xCC08, 0x10);
    wr16(0xCC68, 0x10);
    FirstSwapped();
}

/* $01F34A */
void FirstReady(void)
{
    if (rd32(0xCC08)) return;
    wr16(v_sceneStep, 0x10);
    wr16(0xCC08, 8);
    wr16(0xCC68, 8);
    wr16(FIRST, 0);
    FirstChoose();
}

/* $01F32E */
void FirstShow(void)
{
    if (rd32(TEXT)) return;
    wr16(v_sceneStep, 0xC);
    wr16(0xCC08, 0xC);
    wr16(0xCC68, 0xC);
    FirstReady();
}

/* $01F304. The first text read: with two players the question, else the stage. */
void FirstAsk(void)
{
    if (rd32(TEXT)) return;
    if (rd8(v_players) != 3) {
        first_go();
        return;
    }
    wr16(v_sceneStep, 8);
    TextClear8();
    wr16(TEXT, 0x2C);
    wr16(0xC9C8, 8);
    FirstShow();
}

/* $01F2EC */
void FirstText(void)
{
    wr16(v_sceneStep, 4);
    TextClear8();
    wr16(TEXT, 0x28);
    wr16(0xC9C8, 8);
    FirstAsk();
}

/* $01F280 */
void FirstScene(void)
{
    call68k(rom32(0x01F2D0 + (uint32_t)(int16_t)rd16(v_sceneStep)));
}

/* $01F26C */
void FirstFadeIn(void)
{
    if (rd16(v_fadeSpeed)) return;
    wr16(v_modeState, 4);
    wr16(v_sceneStep, 0);
    FirstScene();
}

/* $01F2A0 */
void FirstLeave(void)
{
    StageLeave();
}

/* $01F256 */
void FirstStep(void)
{
    call68k(rom32(0x01F260 + (uint32_t)(int16_t)rd16(v_modeState)));
}

/* $01F1FA. Mode $30. */
void FirstSelect(void)
{
    select_frame(FirstStep);
}

/* ---- mode $34: the stage set up ---- */

/* $01F548. The stage clear's wait (by the stage). */
void StageClearTime(void)
{
    wr16(0xA026, rom16(0x01F556 + (uint32_t)(rd16(v_stage4) >> 1)));
}

/* $01F4DE. Mode $34: the screen cleared (its difficulty from the options), then the stage. */
void StageEnter(void)
{
    if (!rd16(v_modeState)) {
        ScreenResetAlt();
        wr8(VDP_REG(7) + 1, 0x10);
        wr16(v_difficulty, rd16(0xFF0E));
        clr8(VDP_REG(1) + 1, 6);
        wr8(v_displayOn, 0);
        wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
        return;
    }
    StageStart();
    PlayersUpdate();
    HudStart();
    set8(v_pauseFlags, 6);
    StageClearTime();
    set8(VDP_REG(1) + 1, 6);
    wr8(v_displayOn, 0x80);
    wr16(v_fadeSpeed, 0xFE00);
    wr16(v_fadeLevel, 0x1000);
    wr16(v_mode, 0x38);
    wr16(v_modeState, 0);
}

/* ---- mode $38: a stage being played ---- */

/* loc_0013A0 (after $1360's routine): the frame's end: with $FFF746 set, the backdrop through
 * colours 15 to 0 with the display off (a flash) */
void FrameFlash(void)
{
    if (!(rd8(0xF746) & 0x80)) return;
    vdp_ctrl16((uint16_t)(rd16(VDP_REG(1)) & ~0x40));
    for (int16_t c = 0xF; c >= 0; c--) vdp_ctrl16((uint16_t)(c | 0x8700));
    vdp_ctrl16(rd16(VDP_REG(1)));
    vdp_ctrl16(rd16(VDP_REG(7)));
}

/* $01F8EA */
void StagePlayNothing(void)
{
}

/* $01F686. PAUSE (blinking) while paused, unless A is held. */
void PauseText(void)
{
    uint8_t f = rd8(v_pauseFlags);
    if (!(f & 0x80) || !(f & 0x40)) return;
    if ((rd8(v_padHeld) | rd8(v_padHeld + 1)) & 0x40) return;
    if (!bit8(0xA281, 4)) return;
    uint16_t a = 0xA100;
    for (uint16_t k = 0; k < 12; k += 2) {
        wr32(a, 0x01480000);
        wr16((uint16_t)(a + 4), rom16(0x01F6DE + k));
        wr16((uint16_t)(a + 6), rom16(0x01F6EA + k));
        a = (uint16_t)(a + 8);
    }
    wr16(a, 0xFFFF);
    DrawSpriteList(0xFFFFA100);
}

/* $01F566. Mode $38: the stage. Cleared: its wait ($A026), the fade, then mode $40; nobody left:
 * the fade, then the game over (mode $3C). */
void StagePlay(void)
{
    if (!rd16(v_fadeSpeed)) {
        if (rd8(v_stageClear) & 0x80) {
            if (!(rd8(v_pauseFlags) & 0x80)) {
                uint16_t wait = rd16(0xA026);
                if (wait) {
                    wr16(0xA026, (uint16_t)(wait - 1));
                } else if (rd16(v_fadeLevel)) {
                    wr16(v_mode, 0x40);
                    wr16(v_modeState, 0);
                    return;
                } else {
                    wr16(v_fadeSpeed, 0x200);
                    wr16(v_fadeLevel, 0);
                    set8(v_pauseFlags, 6);
                }
            }
        } else if (!rd8(v_players)) {
            if (rd16(v_fadeLevel)) {
                wr16(v_mode, 0x3C);
                wr16(v_modeState, 0);
                QueueSound(0xF6);
                return;
            }
            wr16(v_fadeSpeed, 0x200);
            wr16(v_fadeLevel, 0);
            set8(v_pauseFlags, 6);
        }
    }
    StagePlayNothing();
    ObjectsMove();
    CameraMove();
    Collisions();
    SpritesStart();
    PauseText();
    DrawListStart();
    PlayersUpdate();
    EffectsUpdate();
    StageFrame();
    Messages();
    DifficultyUpdate();
    ObjectsUpdate();
    DrawListEnd();
    HudUpdate();
    ScreenShake();
    BuildSprites();
    ScrollFrame();
    if (!(rd8(v_pauseFlags) & 0x80)) wr16(v_gameFrames, (uint16_t)(rd16(v_gameFrames) + 1));
    FrameFlash();
}

/* ---- mode $40: a stage cleared ---- */

/* $020156. The stage marked cleared ($A208's bit by stage, the first four). */
void StageClearedMark(void)
{
    uint8_t b = rom8(0x02016A + (uint32_t)((int16_t)rd16(v_stage4) >> 2));
    if (b) wr8(v_stagesCleared, rd8(v_stagesCleared) | b);
}

/* $02013A. Its gem taken ($A228). */
void StageGemMark(void)
{
    uint8_t b = rom8(0x02014E + (uint32_t)((int16_t)rd16(v_stage4) >> 2));
    if (b) wr8(0xA228, rd8(0xA228) | b);
}

/* $0200AC. Mode $40: the stage cleared: the stage select again (all four first stages
 * cleared: the fifth); after the fifth the sixth's select; after the sixth the seventh ($34);
 * after the seventh the ending (mode $44). */
void StageCleared(void)
{
    QueueSound(0xF9);
    wr16(v_checkpoint, 0);
    StageClearedMark();
    StageGemMark();
    int16_t s = (int16_t)rd16(v_stage4);
    uint16_t mode = 0x24;
    if (s > 0xC) {
        if (s == 0x10) wr16(v_stage4, 0x14);
        else if (s == 0x14) {
            wr16(v_stage4, 0x18);
            mode = 0x34;
        } else {
            mode = 0x44;
        }
    } else {
        wr16(v_stage4, rd8(v_stagesCleared) == 0xF ? 0x10 : 0);
    }
    wr16(v_mode, mode);
    wr16(v_modeState, 0);
}

/* ---- mode $3C: the continue, then the game over ---- */

#define CONTINUE_CHOICE 0xA28A       /* 0 yes, 2 no */

/* $020172. Plane B's lines wavering (the table at $20198 into every other word of the vertical
 * scroll source, from its second). */
void GameOverWaves(void)
{
    set8(VDP_REG(11) + 1, 2);
    wr8(v_planeFlagsB, 0x40);
    uint16_t a = (uint16_t)(rd32(v_vscrollSrc) + 2);
    for (uint16_t k = 0; k < 0x40; k += 2, a = (uint16_t)(a + 4)) wr16(a, rom16(0x020198 + k));
}

/* $01FE04. Up or down: the other choice. */
void ContinueCursor(void)
{
    if (!((rd8(v_padPressed) | rd8(v_padPressed + 1)) & 0xF)) return;
    wr16(CONTINUE_CHOICE, rd16(CONTINUE_CHOICE) ^ 2);
    QueueSound(0xAD);
}

/* $01FEB6. Five colours (two sets, flashing with $A281 bit 1). */
void ContinueColours(void)
{
    static const uint16_t at[5] = { 0x22, 0x24, 0x26, 0x38, 0x3C };
    static const uint16_t dim[5] = { 0xCEE, 0x8EE, 0xAAC, 0xAEE, 0x488 }, lit[5] = { 0xEEE, 0xCEE, 0xACE, 0x6CC, 0x244 };
    const uint16_t *c = bit8(0xA281, 1) ? lit : dim;
    for (int k = 0; k < 5; k++) {
        wr16((uint16_t)(v_palette + at[k]), c[k]);
        wr16((uint16_t)(v_paletteTarget + at[k]), c[k]);
    }
}

/* $01FE72. Colour $1F blinking every 3rd frame. */
void ContinueFlash(void)
{
    uint8_t t = rd8(0xA293);
    if (t) {
        wr8(0xA293, (uint8_t)(t - 1));
        return;
    }
    wr8(0xA293, 2);
    wr8(0xA292, (uint8_t)(rd8(0xA292) + 1));
    uint16_t c = (rd8(0xA292) & 1) ? 0x22 : 0x244;
    wr16((uint16_t)(v_palette + 0x3E), c);
    wr16((uint16_t)(v_paletteTarget + 0x3E), c);
}

/* loc_01FE26: the colours, and colour $1D blinking every 5th frame */
static void continue_blink(void)
{
    ContinueColours();
    ContinueFlash();
    uint8_t t = rd8(0xA295);
    if (t) {
        wr8(0xA295, (uint8_t)(t - 1));
        return;
    }
    wr8(0xA295, 4);
    wr8(0xA294, (uint8_t)(rd8(0xA294) + 1));
    uint16_t c = (rd8(0xA294) & 1) ? 0x4AA : 0x6CC;
    wr16((uint16_t)(v_palette + 0x3A), c);
    wr16((uint16_t)(v_paletteTarget + 0x3A), c);
}

/* $01FF3C. The pointer (the object at $C980) at the choice. */
void ContinueArrow(void)
{
    uint16_t c = rd16(CONTINUE_CHOICE);
    wr16(0xC990, rom16(0x01FF4E + c));
    wr16(0xC994, rom16(0x01FF52 + c));
}

/* sprites from tables: `n` of them, y / size / tile / x */
static void sprite_table(int n, uint32_t ys, uint16_t size, uint32_t tiles, uint32_t xs, int fixed_y, uint16_t y)
{
    uint16_t a = 0xA100;
    for (int k = 0; k < n; k++) {
        wr16(a, fixed_y ? y : rom16(ys + 2u * k));
        wr16((uint16_t)(a + 2), size);
        wr16((uint16_t)(a + 4), rom16(tiles + 2u * k));
        wr16((uint16_t)(a + 6), rom16(xs + 2u * k));
        a = (uint16_t)(a + 8);
    }
    wr16(a, 0xFFFF);
    DrawSpriteList(0xFFFFA100);
}

/* tiles to VRAM $B800 from a list of `n` sources */
static void tiles_b800(uint32_t list, int n)
{
    uint16_t q = rd16(v_dmaQueue), vram = 0xB800;
    for (int k = 0; k < n; k++) QueueDmaBlock(&q, rom32(list + 4u * k), &vram);
    wr16(v_dmaQueue, q);
}

/* $01FF56. CONTINUE? YES NO: its tiles and sprites. */
void ContinueText(void)
{
    tiles_b800(0x01FFA6, 10);
    sprite_table(14, 0x020006, 0x500, 0x01FFCE, 0x01FFEA, 0, 0);
}

/* loc_020022: GAME OVER */
static void game_over_text(void)
{
    tiles_b800(0x020070, 7);
    sprite_table(8, 0, 0x500, 0x02008C, 0x02009C, 1, 0xE0);
}

/* $01FCAA. Choosing: "no" (or the choice the original leaves unreached, "yes": the fade back to
 * the stage) ends in the game over. */
void ContinueInput(void)
{
    if (!rd16(v_fadeSpeed)) {
        if (rd16(v_fadeLevel)) {
            clr8(VDP_REG(1) + 1, 6);
            wr8(v_displayOn, 0);
            wr16(v_mode, 0x34);
            wr16(v_modeState, 0);
            call68k(0x00ED90);                       /* loc_00ED90: the players again (not ported) */
            return;
        }
        ContinueCursor();
        if ((rd8(v_padPressed) | rd8(v_padPressed + 1)) & 0xF0) {
            if (rd16(CONTINUE_CHOICE)) {
                wr8(v_soundNext, 0x97);
                wr16(v_soundDelay, 0x12);
                set8(0xC982, 4);
                wr16(v_modeTimer, 0x200);
                wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
                game_over_text();
                return;
            }
            QueueSound(0xC4);
            wr8(v_soundNext, 0xF6);
            wr16(v_soundDelay, 1);
            wr16(v_fadeSpeed, 0x200);
            wr16(v_fadeLevel, 0);
        }
    }
    ContinueArrow();
    ContinueText();
    continue_blink();
}

/* $01FD62. GAME OVER until Start or its time: the SEGA logo (mode 4). */
void GameOverWait(void)
{
    if (!rd16(v_fadeSpeed)) {
        if (rd16(v_fadeLevel) || ((rd8(v_padPressed) | rd8(v_padPressed + 1)) & 0x80)) {
            QueueSound(0xF6);
            clr8(VDP_REG(1) + 1, 6);
            wr8(v_displayOn, 0);
            wr16(v_mode, 4);
            wr16(v_modeState, 0);
            return;
        }
        if (!rd16(v_modeTimer)) {
            wr16(v_fadeSpeed, 0x200);
            wr16(v_fadeLevel, 0);
        }
    }
    game_over_text();
}

/* the continue's frame */
static void continue_frame(void (*step)(void))
{
    ObjectsMove();
    CameraMove();
    SpritesStart();
    DrawListStart();
    PlayersUpdate();
    step();
    ObjectsUpdate();
    DrawListEnd();
    BuildSprites();
    ScrollFrame();
    wr16(v_gameFrames, (uint16_t)(rd16(v_gameFrames) + 1));
}

/* $01FBEE. Mode $3C. */
void GameOver(void)
{
    int16_t st = (int16_t)(rd16(v_modeState) - 4);
    if (st > 0) {
        continue_frame(GameOverWait);
        return;
    }
    if (st < 0) {
        ScreenResetAlt();
        wr8(v_soundNext, 0x99);
        wr16(v_soundDelay, 0x12);
        RunCommands(0x01FDD2);
        StageTilesJapan();
        LoadPalettes(0x01FDF4);
        GameOverWaves();
        wr16(v_boss, 0x4F0);
        wr16(0xC984, 0);
        wr32(v_layout, 0x01B45A);
        wr16(0xA292, 0);
        wr16(0xA294, 0);
        wr16(CONTINUE_CHOICE, 0);
        wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
        set8(VDP_REG(1) + 1, 6);
        wr8(v_displayOn, 0x80);
        wr16(v_fadeSpeed, 0xFE00);
        wr16(v_fadeLevel, 0x1000);
    }
    continue_frame(ContinueInput);
}

/* ---- mode $2C: a scene of talk (pages $60, $64, $68, $70 at $CA88; the four objects from
 * $CB60 given their step at +$48), Start skipping it; then the stage (mode $34) ---- */

/* $01F02C. The four objects' step. */
void InterludeParts(uint16_t step)
{
    wr16(0xCBA8, step);
    wr16(0xCC08, step);
    wr16(0xCC68, step);
    wr16(0xCCC8, step);
}

/* loc_01F0E4: the scene ended: a fade out */
static void interlude_end(void)
{
    wr16(v_modeState, 8);
    QueueSound(0xF6);
    wr16(v_fadeSpeed, 0x200);
    wr16(v_fadeLevel, 0);
}

/* $01F1F0 */
void InterludePage6(void)
{
    if (!rd32(0xCA88)) interlude_end();
}

/* $01F1C6 */
void InterludePage5(void)
{
    if (rd32(0xCBA8)) return;
    wr16(v_sceneStep, 0x14);
    InterludeParts(4);
    TextClear8();
    wr16(0xCA88, 0x70);
    wr16(0xC9C8, 8);
    InterludePage6();
}

/* $01F1AE */
void InterludePage4(void)
{
    if (rd32(0xCA88)) return;
    wr16(v_sceneStep, 0x10);
    InterludeParts(0x10);
    InterludePage5();
}

/* $01F18A */
void InterludePage3(void)
{
    if (rd32(0xCBA8)) return;
    wr16(v_sceneStep, 0x0C);
    TextClear8();
    wr16(0xCA88, 0x68);
    InterludeParts(0x14);
    InterludePage4();
}

/* $01F16C */
void InterludePage2(void)
{
    if (rd32(0xCA88)) return;
    wr16(v_sceneStep, 8);
    wr16(0xC9C8, 8);
    InterludeParts(0x0C);
    InterludePage3();
}

/* $01F148 */
void InterludePage1(void)
{
    if (rd32(0xCA88)) return;
    wr16(v_sceneStep, 4);
    TextClear8();
    wr16(0xCA88, 0x64);
    wr16(0xC9C8, 8);
}

/* $01F0D8. Start: ended; else its page. */
void InterludeTalk(void)
{
    if (SelectPressed() & 0x80) {
        interlude_end();
        return;
    }
    call68k(rom32(0x01F130 + (uint32_t)(int16_t)rd16(v_sceneStep)));
}

/* $01F0B8. Faded in: the first page. */
void InterludeStart(void)
{
    if (rd16(v_fadeSpeed)) return;
    wr16(v_modeState, 4);
    wr16(v_sceneStep, 0);
    TextClear8();
    wr16(0xCA88, 0x60);
    InterludeTalk();
}

/* $01F100. Faded out: the stage. */
void InterludeOut(void)
{
    if (rd16(v_fadeSpeed) || !rd16(v_fadeLevel)) return;
    clr8(0xF7D3, 6);
    wr8(v_displayOn, 0);
    wr16(v_mode, 0x34);
    wr16(v_modeState, 0);
}

/* $01F0A2 */
void InterludeStepRun(void)
{
    call68k(rom32(0x01F0AC + (uint32_t)(int16_t)rd16(v_modeState)));
}

/* $01F09A */
void InterludeStep(void)
{
    InterludeStepRun();
    StageMarks();
}

/* $01F03E. Mode $2C. */
void Interlude(void)
{
    select_frame(InterludeStep);
}

/* ---- mode $44: the ending: two scenes (the planes' lines drawn from $AA60 by $AE60; then
 * the climb with the board's warp), their texts by the timer $A29A, a fade to the title ---- */

/* $01BF6C. The lines' skew ($E5C2 on, both ways) by the frame count shifted `shift`. */
void EndingSkew(uint16_t shift)
{
    uint32_t d0 = (uint32_t)rd16(v_gameFrames) << (shift & 63), d1 = 0;
    uint16_t a = 0xE5C2;
    for (int k = 0; k <= 0x70; k++, a = (uint16_t)(a + 4)) {
        d1 += d0;
        wr16(a, (uint16_t)(d1 >> 16));
    }
    d1 = 0;
    a = 0xE5C2;
    for (int k = 0; k <= 0x70; k++, a = (uint16_t)(a - 4)) {
        d1 -= d0;
        wr16(a, (uint16_t)(d1 >> 16));
    }
}

/* $01BFB2. The columns' skew ($EC2A, both ways), eight times as steep. */
void EndingColumns(uint16_t shift)
{
    uint32_t d0 = ((uint32_t)rd16(v_gameFrames) << (shift & 63)) << 3, d1 = 0;
    uint16_t a = 0xEC2A;
    do {
        d1 += d0;
        wr16(a, (uint16_t)(d1 >> 16));
        a = (uint16_t)(a + 4);
    } while ((int16_t)(a - 0xEC5A) < 0);
    d1 = 0;
    a = 0xEC2A;
    do {
        d1 -= d0;
        wr16(a, (uint16_t)(d1 >> 16));
        a = (uint16_t)(a - 4);
    } while ((int16_t)(a - 0xEC02) >= 0);
}

/* $01BE5E. The warp: the board's map ($1BEFC) at $9800, the planes skewed, four colours cycling. */
void EndingWarp(void)
{
    wr16(0xE30A, 0);
    wr16(0xE38A, 0);
    for (uint16_t a = 0x9800, k = 0; k < 56; k++, a = (uint16_t)(a + 2)) wr16(a, rom16(0x01BEFC + 2u * k));
    wr8(0xF7E7, 6);
    wr8(v_planeFlagsA, 0);
    wr8(v_planeFlagsB, 0xC0);
    EndingSkew(10);
    EndingColumns(10);
    uint32_t t = 0x01BEBC + (((uint16_t)((int16_t)rd16(v_gameFrames) >> 2)) & 0x38);
    wr16(0xE302, rom16(t));
    wr16(0xE304, rom16(t + 2));
    wr16(0xE306, rom16(t + 4));
    wr16(0xE308, rom16(t + 6));
}

/* $023A86. A colour pulsing. */
void EndingCycle(void)
{
    uint16_t c = rom16(0x023A9E + ((rd16(v_gameFrames) >> 3) & 0x1E));
    wr16(0xE33C, c);
    wr16(0xE3BC, c);
}

/* $023614. The 200 lines' scroll moved by their speeds. */
void EndingLines(void)
{
    uint16_t a1 = 0xAA60, a2 = 0xAE60, a3 = 0xE462;
    for (int k = 0; k < 200; k++, a1 = (uint16_t)(a1 + 4), a2 = (uint16_t)(a2 + 4), a3 = (uint16_t)(a3 + 4)) {
        uint32_t v = rd32(a1) - rd32(a2);
        wr32(a1, v);
        wr16(a3, (uint16_t)(v >> 16));
    }
}

/* $0237FA. The text's object ($4F8 in $D580) for text `which`. */
void EndingText(uint16_t which)
{
    uint16_t t = 0xD580;
    wr16(t + 0x48, which);
    wr16(t + O_TYPE, 0x4F8);
    wr16(t + O_STATE, 0);
    wr16(t + 0x0E, 0x8050);
    wr16(t + 0x50, 4);
    wr16(t + O_X, 0x120);
    wr16(t + O_Y, 0x144);
    wr8(t + 0x58, 0xAD);
    wr8(t + 0x59, 0);
    SceneTextClear();
}

/* $023636. The four of the cast ($638, $D760 on). */
void EndingCast(void)
{
    for (uint16_t k = 0; k < 4; k++) {
        uint16_t o = rom16(0x023656 + 2u * k);
        wr16(o + O_TYPE, 0x638);
        wr16(o + 0x48, rom16(0x02365E + 2u * k));
        wr16(o + O_STATE, 0);
    }
}

static int ending_count(void)
{
    wr16(0xA29A, (uint16_t)(rd16(0xA29A) - 1));
    return (int16_t)rd16(0xA29A) < 0;
}

static void ending_next(uint16_t count)
{
    wr16(v_sceneStep, (uint16_t)(rd16(v_sceneStep) + 4));
    wr16(0xA29A, count);
}

static void ending_say(uint16_t text, uint16_t pose)
{
    wr16(0xD5C8, text);
    wr16(0xD628, pose);
}

/* $023602. The last wait: the scene over. */
void EndingSceneOver(void)
{
    if (ending_count()) wr16(0xA29C, 1);
}

/* $0235B4. A beep each 4 frames, another sound for the last 8: then a fade. */
void EndingBeeps2(void)
{
    if (ending_count()) {
        wr16(0xA29A, 0x100);
        wr16(v_sceneStep, (uint16_t)(rd16(v_sceneStep) + 4));
        wr16(v_fadeSpeed, 0xC0);
        wr16(v_fadeLevel, 0);
        EndingSceneOver();
        return;
    }
    uint16_t c = rd16(0xA29A);
    if ((int16_t)(c - 8) > 0) {
        if (!(c & 3)) QueueSound(0xA9);
    } else if (!(c & 3)) {
        QueueSound(0xBD);
    }
}

/* $02357A */
void EndingBeeps(void)
{
    if (ending_count()) {
        wr16(v_sceneStep, (uint16_t)(rd16(v_sceneStep) + 4));
        SceneTextClear();
        ending_say(0xAC, 0x10);
        wr16(0xA29A, 0x80);
        EndingBeeps2();
        return;
    }
    if (!(rd16(0xA29A) & 3)) QueueSound(0xA9);
}

/* $023520 */
void EndingText3(void)
{
    if (ending_count()) {
        wr16(v_sceneStep, (uint16_t)(rd16(v_sceneStep) + 4));
        ending_say(0xA8, 8);
        QueueSound(0xF6);
        wr16(0xA29A, 0x101);
        EndingBeeps();
        return;
    }
    if (rd16(0xA29A) == 0x380) {
        SceneTextClear();
        ending_say(0xA0, 4);
        return;
    }
    if (rd16(0xA29A) == 0x200) {
        SceneTextClear();
        wr16(0xD5C8, 0xA4);
    }
}

/* $0234EC */
void EndingText2(void)
{
    if (ending_count()) {
        ending_next(0x400);
        QueueSound(0xB5);
        EndingText3();
        return;
    }
    if (rd16(0xA29A) == 0x180) ending_say(0x9C, 0x14);
}

/* $0234D8 */
void EndingFadeIn(void)
{
    if (!ending_count()) return;
    ending_next(0x280);
    EndingText2();
}

/* $023474. A white flash, a boom. */
void EndingFlash(void)
{
    if (ending_count()) {
        ending_next(0x80);
        set8(0xF7D3, 6);
        wr8(v_displayOn, 0x80);
        wr16(v_fadeSpeed, 0xFF80);
        wr16(v_fadeLevel, 0x1000);
        SceneTextClear();
        wr16(0xD628, 0);
        EndingFadeIn();
        return;
    }
    if (rd16(0xA29A) == 0x120) {
        wr16(v_fadeColor, 0xEEE);
        wr16(v_fadeSpeed, 0x80);
        wr16(v_fadeLevel, 0);
        return;
    }
    if (rd16(0xA29A) == 0xE0) QueueSound(0xB2);
}

/* $023442. Rumbling the last $20 frames. */
void EndingRumble(void)
{
    if (ending_count()) {
        ending_next(0x140);
        EndingFlash();
        return;
    }
    uint16_t c = rd16(0xA29A);
    if ((int16_t)(c - 0x20) > 0 || (c & 3)) return;
    QueueSound(0xB2);
}

/* $02341E */
void EndingWait2(void)
{
    if (!ending_count()) return;
    wr16(v_sceneStep, (uint16_t)(rd16(v_sceneStep) + 4));
    SceneTextClear();
    ending_say(0x98, 0x18);
    wr16(0xA29A, 0x40);
    EndingRumble();
}

/* $0233F0 */
void EndingText1(void)
{
    if (ending_count()) {
        ending_next(0x200);
        EndingWait2();
        return;
    }
    if (rd16(0xA29A) != 0x200) return;
    SceneTextClear();
    ending_say(0x94, 0x0C);
}

/* $0233DC */
void EndingWait1(void)
{
    if (!ending_count()) return;
    ending_next(0x300);
    EndingText1();
}

/* $0233D2 */
void EndingBegin(void)
{
    ending_next(0x200);
    EndingWait1();
}

/* $023378. Faded out and over: on; else its lines and its text's step. */
void EndingScene1Play(void)
{
    if (!rd16(v_fadeSpeed) && rd16(0xA29C)) {
        wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
        return;
    }
    EndingCycle();
    EndingLines();
    call68k(rom32(0x0233A2 + (uint32_t)(int16_t)rd16(v_sceneStep)));
}

/* $02327A. The first scene's set-up: its graphics, the stars, the cast, faded in. */
void EndingScene1(void)
{
    ClearObjectRam();
    RunCommands(0x023832);
    LoadPalettes(0x023A46);
    CameraPlace(0, 0, 0x200, 0x100);
    PlaneARedraw();
    PlaneBRedraw();
    SceneStill();
    StarsStart();
    wr8(0xF7E7, 3);
    wr8(v_planeFlagsA, 0);
    wr8(v_planeFlagsB, 0x80);
    wr32(v_speedLimitX, 0x70000);
    wr32(v_speedLimitY, 0x70000);
    wr32(v_layout, 0x01B45A);
    wr8(0xF765, 0);
    EndingText(0);
    wr16(0xD5E0, 0x630);
    wr16(0xD628, 0);
    wr16(0xD5E4, 0);
    wr16(v_player1 + O_TYPE, 0x62C);
    wr16(0xA448, 0);
    wr16(0xA404, 0);
    wr16(0xD6A0, 0x634);
    wr16(0xD6E8, 0);
    wr16(0xD6A4, 0);
    wr16(0xD640, 0x5D0);
    wr16(0xD688, 0x0C);
    wr16(0xD644, 0);
    EndingCast();
    wr16(0xD940, 0x640);
    wr16(0xD988, 0);
    wr16(0xD944, 0);
    wr16(v_fadeColor, 0);
    wr16(0xA29C, 0);
    wr16(v_sceneStep, 0);
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
    set8(0xF7D3, 6);
    wr8(v_displayOn, 0x80);
    wr16(v_fadeSpeed, 0xFF00);
    wr16(v_fadeLevel, 0x1000);
    EndingScene1Play();
}

/* $0237B0 */
void EndingPause(void)
{
    wr16(0xA29A, 0x80);
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
}

/* $0237BC */
void EndingPauseWait(void)
{
    wr16(0xA29A, (uint16_t)(rd16(0xA29A) - 1));
    if ((int16_t)rd16(0xA29A) < 0) wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
}

/* $02378E. At the top: a while, then over. */
void EndingTop(void)
{
    if (rd16(0xA29C)) return;
    wr16(0xA29A, (uint16_t)(rd16(0xA29A) - 1));
    if (rd16(0xA29A)) return;
    wr16(0xA29C, 1);
    QueueSound(0xF6);
}

/* $023770. Climbing to $B10. */
void EndingClimbing(void)
{
    if ((int16_t)(rd16(0xA924) - 0xB10) < 0) return;
    wr16(v_sceneStep, (uint16_t)(rd16(v_sceneStep) + 4));
    wr32(v_camSpeedY, 0);
    wr32(0xA934, 0);
    wr16(0xA29A, 0x180);
    EndingTop();
}

/* $02375E */
void EndingClimb(void)
{
    wr32(v_camSpeedY, 0x8000);
    wr32(0xA934, 0x8000);
    wr16(v_sceneStep, (uint16_t)(rd16(v_sceneStep) + 4));
    EndingClimbing();
}

/* $0236F4. Faded out: on; over (or Start, or the timer out): a fade; the warp, its step. */
void EndingScene2Play(void)
{
    if (!rd16(v_fadeSpeed)) {
        if (rd16(v_fadeLevel)) {
            wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
            return;
        }
        wr16(v_fadeColor, 0);
        if (rd16(0xA29C)) {
            uint8_t pads = (uint8_t)(rd8(v_padPressed) | rd8(0xF709));
            if ((pads & 0x80) || !rd16(v_modeTimer)) {
                QueueSound(0xF6);
                wr16(v_fadeSpeed, 0x80);
                wr16(v_fadeLevel, 0);
            }
        }
    }
    EndingWarp();
    call68k(rom32(0x023752 + (uint32_t)(int16_t)rd16(v_sceneStep)));
}

/* $023666. The second scene's set-up: the climb, its music, faded in. */
void EndingScene2(void)
{
    wr16(v_hintEffect, 0x18);
    wr16(v_hintState, 0);
    ClearObjectRam();
    RunCommands(0x0238A4);
    LoadPalettes(0x023A56);
    CameraPlace(0x200, 0, 0x200, 0x100);
    PlaneARedraw();
    PlaneBRedraw();
    SceneStill();
    wr32(v_layout, 0x01B45A);
    wr8(v_soundNext, 0x96);
    wr16(v_soundDelay, 0x20);
    wr16(v_modeTimer, 0x2000);
    wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
    wr16(v_sceneStep, 0);
    wr16(0xA29C, 0);
    set8(0xF7D3, 6);
    wr8(v_displayOn, 0x80);
    wr16(v_fadeSpeed, 0xFF40);
    wr16(v_fadeLevel, 0x1000);
    EndingScene2Play();
}

/* $0237E4. Back to mode 4 (the title's). */
void EndingDone(void)
{
    clr8(0xF7D3, 6);
    wr8(v_displayOn, 0);
    wr16(v_mode, 4);
    wr16(v_modeState, 0);
}

/* $023252 */
void EndingStep(void)
{
    call68k(rom32(0x02325E + (uint32_t)(int16_t)(uint16_t)(rd16(v_modeState) - 4)));
}

/* $0231B4. Mode $44. */
void Ending(void)
{
    if (!rd16(v_modeState)) {
        wr16(v_fadeColor, 0);
        ScreenReset();
        clr8(v_pauseFlags, 6);
        wr8(0xF7DF, 0x10);
        wr32(v_speedLimitX, 0x1000000);
        wr32(v_speedLimitY, 0x1000000);
        wr16(v_hintEffect, 0x14);
        wr16(v_hintState, 0);
        wr8(0xF765, 0);
        wr8(v_soundNext, 0x8C);
        wr16(v_soundDelay, 0x20);
        wr16(v_modeState, 4);
        wr16(v_sceneStep, 0);
    }
    ObjectsMove();
    CameraMove();
    SpritesStart();
    DrawListStart();
    PlayersUpdate();
    EffectsUpdate();
    EndingStep();
    ObjectsUpdate();
    DrawListEnd();
    ScreenShake();
    BuildSprites();
    ScrollFrame();
    wr16(v_gameFrames, (uint16_t)(rd16(v_gameFrames) + 1));
}
