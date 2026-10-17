/* Mode $C: the options screen. The menu (LEVEL, the CONFIG, the SOUND TEST of the music $81-$99,
 * the S.E. TEST of the effects $A0-$F1, EXIT), and the pads' button configs, the screen scrolled
 * up ($E400, the plane's vertical scroll, to -$200) to show them. A line is lit ($E700, else
 * $8700) when its cursor is on it; the colours are the continue's (ContinueBlink). */
#include "game.h"

#define CURSOR      0xA28A       /* the menu's line, 0-$10 by 4 (the config: pad 1's line, 0-8) */
#define SOUND       0xA28C       /* the sound test's music, $81-$99 */
#define EFFECT      0xA28E       /* the S.E. test's effect, $A0-$F1 */
#define CURSOR2     0xA296       /* the config: pad 2's line, 0-8 */
#define LEVEL       0xFF0E       /* the difficulty, 0-6 by 2 */
#define SCROLL      0xE400

/* $001A10. value as `digits` hex digits (the tiles from $B5: 0-9, A-F) by the VRAM command cmd,
 * through the column buffer and the DMA queue, the tile words' high byte attr's; digits 0: five,
 * the leading zeros blank ($B4) and the divisors $48E7, $1000, $100, $10, 1 (the first one the
 * table's next word). */
void DrawHex(uint16_t value, uint32_t cmd, uint16_t digits, uint16_t attr)
{
    uint16_t q = rd16(v_dmaQueue), buf = rd16(v_columnBuffer);
    q = (uint16_t)(q - 4);
    wr32(q, cmd);
    q = QueueSource(q, RAM_BASE | buf);
    int blank = digits == 0;
    int16_t at;
    if (blank) {
        digits = 5;
        at = 8;
    } else {
        at = (int16_t)((digits - 1) << 1);
    }
    uint32_t d1 = value;
    for (; at >= 0; at -= 2) {
        uint16_t div = rom16(0x001AA8 + (uint32_t)at);
        uint32_t quot = d1 / div;
        d1 %= div;
        if (blank && !quot) {
            wr16(buf, (uint16_t)((attr & 0xFF00) | 0xB4));
        } else {
            blank = 0;
            wr16(buf, (uint16_t)((attr & 0xFF00) | (uint8_t)(quot + 0xB5)));
        }
        buf = (uint16_t)(buf + 2);
    }
    q = (uint16_t)(q - 2);
    wr16(q, 0x8F02);
    q = (uint16_t)(q - 4);
    wr32(q, 0x94009300 | (uint8_t)digits);
    wr16(v_dmaQueue, q);
    wr16(v_columnBuffer, buf);
}

/* the attribute of a line: lit when the cursor is on it */
static uint16_t lit(uint16_t cursor, uint16_t line)
{
    return rd16(cursor) == line ? 0xE700 : 0x8700;
}

static uint8_t pressed(void)
{
    return (uint8_t)(rd8(v_padPressed) | rd8(v_padPressed + 1));
}

/* $01C18A. Mode $C. */
void Options(void)
{
    call68k(rom32(0x01C194 + (uint32_t)(int16_t)rd16(v_modeState)));
}

/* $01C1A8. The screen, the menu, the fade in, the options' music. */
void OptionsSetup(void)
{
    ScreenReset();
    RunCommands(0x01FDD2);
    StageTilesJapan();
    RunCommands(0x01C962);
    LoadPalettes(0x01FDF4);
    GameOverWaves();
    wr16(CURSOR, 0);
    wr16(SOUND, 0x81);
    wr16(EFFECT, 0xA0);
    DrawText(0x01C8AC);
    OptionsDraw();
    set8(0xF7D3, 6);
    wr8(v_displayOn, 0x80);
    wr16(v_fadeSpeed, 0xFE00);
    wr16(v_fadeLevel, 0x1000);
    wr8(v_soundNext, 0x98);
    wr16(v_soundDelay, 0x12);
    wr16(v_modeState, 4);
}

/* $01C23C. The menu: on the CONFIG, a button or left/right scrolls to the configs; Start (or on
 * EXIT a button) fades out, to the title once faded. */
void OptionsMenu(void)
{
    if (!rd16(v_fadeSpeed)) {
        if (rd16(v_fadeLevel)) {
            wr16(v_mode, 8);
            wr16(v_modeState, 0);
            return;
        }
        int leave = 0;
        if (rd16(CURSOR) == 0x10) {
            leave = (pressed() & 0x70) != 0;
        } else if (rd16(CURSOR) == 4 && (pressed() & 0x7C)) {
            QueueSound(0xD4);
            wr16(v_modeState, 8);
            OptionsToConfig();
            return;
        }
        if (leave || (pressed() & 0x80)) {
            wr16(v_fadeSpeed, 0x200);
            wr16(v_fadeLevel, 0);
            QueueSound(0xC4);
            QueueSound(0xF6);
        }
    }
    OptionsInput();
    OptionsDraw();
    ContinueBlink();
}

/* $01C2D0. Scrolling up to the configs, drawn halfway. */
void OptionsToConfig(void)
{
    uint16_t s = (uint16_t)(rd16(SCROLL) - 0x10);
    wr16(SCROLL, s);
    if (s == 0xFE00) {
        QueueSound(0xBA);
        wr16(v_modeState, 0xC);
        OptionsConfig();
        return;
    }
    if ((int16_t)s > -0x100) {
        OptionsDraw();
        ContinueBlink();
        return;
    }
    if (s == 0xFF00) {
        ClearVramC000();
        RunCommands(0x01CBBA);
        wr16(CURSOR, 0);
        wr16(CURSOR2, 0);
        DrawText(0x01CB18);
    }
    OptionsDrawPad1();
    OptionsDrawPad2();
    ContinueBlink();
}

/* $01C33E. The configs, until Start. */
void OptionsConfig(void)
{
    if (pressed() & 0x80) {
        QueueSound(0xD4);
        wr16(v_modeState, 0x10);
        OptionsFromConfig();
        return;
    }
    OptionsConfigInput();
    OptionsDrawPad1();
    OptionsDrawPad2();
    ContinueBlink();
}

/* $01C370. Scrolling back down to the menu (the cursor on the CONFIG, the tests reset). */
void OptionsFromConfig(void)
{
    uint16_t s = (uint16_t)(rd16(SCROLL) + 0x10);
    wr16(SCROLL, s);
    if (!s) {
        QueueSound(0xBA);
        wr16(v_modeState, 4);
        return;
    }
    if ((int16_t)s < -0x100) {
        OptionsDrawPad1();
        OptionsDrawPad2();
        ContinueBlink();
        return;
    }
    if (s == 0xFF00) {
        ClearVramC000();
        RunCommands(0x01C962);
        wr16(CURSOR, 4);
        wr16(SOUND, 0x81);
        wr16(EFFECT, 0xA0);
        DrawText(0x01C8AC);
    }
    OptionsDraw();
    ContinueBlink();
}

/* $01C3E0. Up and down, then the line's own input. */
void OptionsInput(void)
{
    uint8_t press = pressed();
    OptionsCursor(press);
    switch (rom32(0x01C3F6 + (uint32_t)(int16_t)rd16(CURSOR))) {
    case 0x01C458: OptionsLevel(press); break;
    case 0x01C4A8: OptionsSoundTest(press); break;
    case 0x01C528: OptionsEffectTest(press); break;
    default: OptionsIdle(); break;
    }
}

/* $01C40A. The CONFIG's and EXIT's input: none. */
void OptionsIdle(void)
{
}

/* $01C40C */
void OptionsCursor(uint8_t press)
{
    wr16(CURSOR, OptionsUpDown(press, 0x10, rd16(CURSOR)));
}

/* $01C41E. Down and up step a cursor by 4 through 0-last, round. */
uint16_t OptionsUpDown(uint8_t press, uint16_t last, uint16_t at)
{
    if (press & 2) {
        QueueSound(0xAD);
        at = (uint16_t)(at + 4);
        return (int16_t)at <= (int16_t)last ? at : 0;
    }
    if (press & 1) {
        QueueSound(0xAD);
        at = (uint16_t)(at - 4);
        return (int16_t)at >= 0 ? at : last;
    }
    return at;
}

/* $01C458. Left and right: the difficulty. */
void OptionsLevel(uint8_t press)
{
    uint16_t at = OptionsLeftRight(press, 0xC, (uint16_t)(rd16(LEVEL) << 1));
    wr16(LEVEL, (uint16_t)((int16_t)at >> 1));
}

/* $01C46E. Right and left step by 4 through 0-last, round. */
uint16_t OptionsLeftRight(uint8_t press, uint16_t last, uint16_t at)
{
    if (press & 8) {
        QueueSound(0xAD);
        at = (uint16_t)(at + 4);
        return (int16_t)at <= (int16_t)last ? at : 0;
    }
    if (press & 4) {
        QueueSound(0xAD);
        at = (uint16_t)(at - 4);
        return (int16_t)at >= 0 ? at : last;
    }
    return at;
}

/* $01C4A8. The sound test: C or A plays the music, B stops it. */
void OptionsSoundTest(uint8_t press)
{
    OptionsSoundPick(press);
    if (press & 0x20) QueueSound(rd8(SOUND + 1));
    else if (press & 0x10) QueueSound(0xF9);
    else if (press & 0x40) QueueSound(rd8(SOUND + 1));
}

/* $01C4DA */
void OptionsSoundPick(uint8_t press)
{
    uint16_t id = rd16(SOUND);
    if (press & 8) {
        QueueSound(0xAD);
        id = (uint16_t)(id + 1);
        if ((int16_t)id > 0x99) id = 0x81;
    } else if (press & 4) {
        QueueSound(0xAD);
        id = (uint16_t)(id - 1);
        if ((int16_t)id < 0x81) id = 0x99;
    } else {
        return;
    }
    wr16(SOUND, id);
}

/* $01C528. The S.E. test: C or A stops the sound and has the effect next (2 frames on), B only
 * stops; the effect $9B when the effect is next with the delay's high byte 1. */
void OptionsEffectTest(uint8_t press)
{
    OptionsEffectPick(press);
    if (rd8(v_soundNext) == rd8(EFFECT + 1) && rd8(v_soundDelay) == 1) {
        QueueSound(0x9B);
        return;
    }
    if (!(press & 0x20)) {
        if (press & 0x10) {
            QueueSound(0xF9);
            return;
        }
        if (!(press & 0x40)) return;
    }
    wr8(v_soundNext, rd8(EFFECT + 1));
    wr16(v_soundDelay, 2);
    QueueSound(0xF9);
}

/* $01C57C */
void OptionsEffectPick(uint8_t press)
{
    uint16_t id = rd16(EFFECT);
    if (press & 8) {
        QueueSound(0xAD);
        id = (uint16_t)(id + 1);
        if ((int16_t)id > 0xF1) id = 0xA0;
    } else if (press & 4) {
        QueueSound(0xAD);
        id = (uint16_t)(id - 1);
        if ((int16_t)id < 0xA0) id = 0xF1;
    } else {
        return;
    }
    wr16(EFFECT, id);
}

/* $01C5CA. Each pad's line and its button (pad 1's map at $FF20, pad 2's at $FF21, a byte every
 * other one: SELECT, SHOT, JUMP). */
void OptionsConfigInput(void)
{
    uint8_t press = rd8(v_padPressed);
    uint16_t at = OptionsConfigUpDown(press, rd16(CURSOR));
    wr16(CURSOR, at);
    OptionsConfigPick(press, at, v_buttonMap);
    press = rd8(v_padPressed + 1);
    at = OptionsConfigUpDown(press, rd16(CURSOR2));
    wr16(CURSOR2, at);
    OptionsConfigPick(press, at, (uint16_t)(v_buttonMap + 1));
}

/* $01C5FC. Down and up through the three lines, round. */
uint16_t OptionsConfigUpDown(uint8_t press, uint16_t at)
{
    if (press & 2) {
        QueueSound(0xAD);
        at = (uint16_t)(at + 4);
        return (int16_t)at <= 8 ? at : 0;
    }
    if (press & 1) {
        QueueSound(0xAD);
        at = (uint16_t)(at - 4);
        return (int16_t)at >= 0 ? at : 8;
    }
    return at;
}

/* $01C63A. The line's button (the pad bit: A 6, B 4, C 5): pressed A, B or C, or right and left
 * through them; the line that had it takes this one's. */
void OptionsConfigPick(uint8_t press, uint16_t at, uint16_t map)
{
    int16_t k = (int16_t)(at - 4);
    uint16_t line = k == 0 ? 2 : k > 0 ? 4 : 0;
    uint8_t was = rd8((uint16_t)(map + line)), key;
    if (press & 0x40) key = 6;
    else if (press & 0x10) key = 4;
    else if (press & 0x20) key = 5;
    else if (press & 8) key = ConfigNext(was);
    else if (press & 4) key = ConfigPrev(was);
    else return;
    QueueSound(0xAD);
    wr8((uint16_t)(map + line), key);
    for (uint16_t o = 0; o <= 4; o += 2) {
        if (o == line) continue;
        if (rd8((uint16_t)(map + o)) == key) {
            wr8((uint16_t)(map + o), was);
            return;
        }
    }
}

/* $01C776. A, B, C, A ... */
uint8_t ConfigNext(uint8_t key)
{
    return key == 6 ? 4 : key == 4 ? 5 : 6;
}

/* $01C798 */
uint8_t ConfigPrev(uint8_t key)
{
    return key == 6 ? 5 : key == 4 ? 6 : 4;
}

/* $01C7BA. The menu's lines. */
void OptionsDraw(void)
{
    OptionsDrawLevel();
    OptionsDrawConfig();
    OptionsDrawSound();
    OptionsDrawEffect();
    DrawTextPlain(0x01C928, lit(CURSOR, 0x10));
}

/* $01C7CE. LEVEL and the difficulty's name. */
void OptionsDrawLevel(void)
{
    uint16_t attr = lit(CURSOR, 0);
    DrawTextPlain(0x01C8F2, attr);
    DrawTextPlain(rom32(0x01C7FC + (uint32_t)(int16_t)(rd16(LEVEL) << 1)), attr);
}

/* $01C80C */
void OptionsDrawConfig(void)
{
    DrawTextPlain(0x01C8FC, lit(CURSOR, 4));
}

/* $01C82A */
void OptionsDrawSound(void)
{
    uint16_t attr = lit(CURSOR, 8);
    DrawTextPlain(0x01C908, attr);
    DrawHex(rd16(SOUND), 0x46300083, 2, attr);
}

/* $01C85C */
void OptionsDrawEffect(void)
{
    uint16_t attr = lit(CURSOR, 0xC);
    DrawTextPlain(0x01C918, attr);
    DrawHex(rd16(EFFECT), 0x47300083, 2, attr);
}

/* a config line: its name, and its button's letter */
static void config_line(uint16_t cursor, uint16_t line, uint32_t text, uint16_t key, uint32_t cmd)
{
    uint16_t attr = lit(cursor, line);
    DrawTextPlain(text, attr);
    DrawHex(ConfigLetter(rd8(key)), cmd, 1, attr);
}

/* $01C99C. Pad 1's config. */
void OptionsDrawPad1(void)
{
    OptionsSelect1();
    OptionsShot1();
    config_line(CURSOR, 8, 0x01CBA6, 0xFF24, 0x479E0083);
}

/* $01C9A8 */
void OptionsSelect1(void)
{
    config_line(CURSOR, 0, 0x01CB7A, 0xFF20, 0x459E0083);
}

/* $01C9DE */
void OptionsShot1(void)
{
    config_line(CURSOR, 4, 0x01CB92, 0xFF22, 0x469E0083);
}

/* $01CA4A. A pad bit's letter as a hex digit: A $A, B $B, C $C. */
uint8_t ConfigLetter(uint8_t key)
{
    return key == 6 ? 0xA : key == 4 ? 0xB : 0xC;
}

/* $01CA6C. Pad 2's config. */
void OptionsDrawPad2(void)
{
    OptionsSelect2();
    OptionsShot2();
    config_line(CURSOR2, 8, 0x01CBB0, 0xFF25, 0x47BE0083);
}

/* $01CA78 */
void OptionsSelect2(void)
{
    config_line(CURSOR2, 0, 0x01CB86, 0xFF21, 0x45BE0083);
}

/* $01CAAC */
void OptionsShot2(void)
{
    config_line(CURSOR2, 4, 0x01CB9C, 0xFF23, 0x46BE0083);
}
