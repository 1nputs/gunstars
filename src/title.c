/* The title screen (modes 8 and $50): the logo on a turning cube, drawn by line scrolling (the
 * H-interrupt effect 4 shows the table at $9400, the scroll table at $E400 its rows), the story
 * scene's first actor behind it, and the menu once Start is pressed. */
#include "game.h"

#define CHOICE      0xA28A       /* the menu's choice: 0 one player, 2 two players, 4 options */
#define TURN        0x802C       /* the cube's turn: 0 to $FE, $40 the logo facing */
#define FACES       0x8002       /* the faces drawn this frame (bits 14, 15) */
#define LINES       0x9400       /* a source line per screen line */
#define LINES_SHOWN 0x9600       /* the copy the effect shows */
#define ROWS        0xE400       /* a horizontal scroll per screen line */
#define SPRITE_LIST 0xA100       /* a list of sprites built for DrawSpriteList */
#define CUBE_WIDTHS 0x01C08A     /* the face's width (of $100) by its angle */

/* $01B908. A column of count+1 sprites (their patterns from `list`), 8 apart downward from y,
 * built at `buf` and drawn. */
void SpriteColumn(uint32_t buf, uint16_t x, uint16_t attr, uint32_t list, uint16_t y, uint16_t count)
{
    uint16_t a = (uint16_t)buf;
    for (uint16_t k = count;; k--) {
        wr16(a, x);
        wr16((uint16_t)(a + 2), attr);
        wr16((uint16_t)(a + 4), bus_rd16(list));
        list += 2;
        wr16((uint16_t)(a + 6), y);
        a = (uint16_t)(a + 8);
        y = (uint16_t)(y + 8);
        if (k == 0) break;
    }
    wr16(a, 0xFFFF);
    DrawSpriteList(buf);
}

/* loc_01B8F0: "PRESS START" (or the copyright line under the menu) */
static void title_bottom(void)
{
    SpriteColumn(0xFFFFA100, 0x140, 0, 0x01B9BA, 0x140, 0xA);
}

/* $01B922. The menu's three lines, and the bottom line. */
void TitleMenu(void)
{
    SpriteColumn(0xFFFFA100, 0x128, 0, 0x01B974, 0xC0, 7);
    SpriteColumn(0xFFFFA100, 0x138, 0, 0x01B984, 0xC0, 8);
    SpriteColumn(0xFFFFA100, 0x148, 0, 0x01B996, 0xC0, 6);
    title_bottom();
}

/* $01B830. Up and down move the choice (two players only with two pads, $D0D). */
void TitleCursor(void)
{
    uint8_t press = (uint8_t)(rd8(v_padPressed) | rd8(v_padPressed + 1));
    int16_t at = (int16_t)(rd16(CHOICE) - 2);
    uint16_t to;
    if (at == 0) {
        if (press & 2) to = 4;
        else if (press & 1) to = 0;
        else return;
    } else if (at > 0) {
        if (press & 2) to = 0;
        else if (!(press & 1)) return;
        else to = rd16(v_padId) == 0xD0D ? 2 : 0;
    } else {
        if (press & 1) to = 4;
        else if (!(press & 2)) return;
        else to = rd16(v_padId) == 0xD0D ? 2 : 4;
    }
    wr16(CHOICE, to);
    QueueSound(0xAD);
}

/* $01B828. The cursor, and the arrow object (the scene's actor at $C980) at its line. */
void TitleSelect(void)
{
    TitleCursor();
    wr16(0xC990, 0xB4);
    wr16(0xC994, rom16(0x01B8CA + (uint32_t)(int16_t)rd16(CHOICE)));
}

/* $01B800. What the stage select's players are: two players for the two-player choice, else
 * by who pressed Start. */
void TitleChoose(void)
{
    if (rd16(CHOICE)) {
        wr16(0xA290, 8);
        return;
    }
    wr16(0xA290, (rd8(v_padPressed + 1) & 0x80) ? 4 : 0);
}

/* $01BD40. The line table to the copy shown. */
void CubeShow(void)
{
    for (uint16_t k = 0; k < 0x1F4; k += 4) wr32((uint16_t)(LINES_SHOWN + k), rd32((uint16_t)(LINES + k)));
}

/* $01BD08. Every line its own (from $EF down): no face. */
void CubeClear(void)
{
    uint16_t v = 0xEF;
    for (uint16_t k = 0; k < 0x200; k += 2, v--) wr16((uint16_t)(LINES + k), v);
}

/* $01BD1E. A face drawn (`bit`): its shade by its angle, 0 to 3 facing, -4 to -1 turned away. */
uint16_t CubeShade(uint16_t bit, uint16_t angle)
{
    wr16(FACES, rd16(FACES) | bit);
    uint16_t a = angle & 0x7F;
    if ((int16_t)(a - 0x40) >= 0) return (uint16_t)(((int16_t)a >> 4) | 0xFFFC);
    return (uint16_t)((int16_t)(a & 0x3F) >> 4);
}

/* $01BC46. A face's near edge: where it is on screen at this angle (its centre `centre`, its
 * size `size`), as the line tables' addresses going up (*up, *uprow) and down (*down, *downrow)
 * from there, and the source line it starts at (`base` less the edge's place, in d1, d2, d3). */
void CubeEdge(uint16_t angle, uint16_t size, uint16_t centre, uint16_t base, uint16_t *up, uint16_t *down,
              uint16_t *uprow, uint16_t *downrow, uint16_t *d1, uint16_t *d2, uint16_t *d3)
{
    uint16_t i = (uint16_t)((angle & 0x7F) << 1);
    uint32_t w = (uint32_t)rom16(CUBE_WIDTHS + i) * size;
    uint16_t half = (uint16_t)(((int32_t)w >> 7) & 0x1FE);
    uint16_t at = (uint16_t)(LINES + ((uint16_t)(centre << 1) & 0x1FE));
    if ((int16_t)(i - 0x80) >= 0) at = (uint16_t)(at + half);
    else at = (uint16_t)(at - half);
    *up = at;
    *down = (uint16_t)(at - 4);
    uint16_t row = (uint16_t)(ROWS + ((at & 0x1FE) << 1));
    *uprow = row;
    *downrow = row;
    *d3 = (uint16_t)(base - (uint16_t)(at << 7));
    *d1 = *d3;
    *d2 = *d3;
}

/* $01BC9C. A face's rows: how many (*count) and the source step per line (returned, $100 one to
 * one), from its width at this angle and its height `height`. The 68k's remainders are kept in
 * the top halves. */
uint32_t CubeSlope(uint16_t angle, uint16_t height, uint32_t *count)
{
    uint32_t h = (uint16_t)(height << 8) & 0xFF00;
    uint16_t w = rom16(CUBE_WIDTHS + (uint32_t)((angle & 0x7F) << 1));
    if (!w) {
        *count = 0;
        return 0;
    }
    uint32_t step = ((0x10000 % w) << 16) | (0x10000 / w);
    uint16_t q = (uint16_t)step;
    *count = ((h % q) << 16) | (h / q);
    return (step & 0xFFFF0000u) | (uint16_t)(q - 0x100);
}

/* $01BCC8. count+1 lines each way from the edge: going up from `up` (the source line d1, by
 * `step` each), going down from `down` (d2), each with the scroll `scroll`; within the table. */
void CubeRows(uint16_t up, uint16_t down, uint16_t uprow, uint16_t downrow, uint16_t step, uint16_t d1, uint16_t d2,
              uint16_t scroll, uint16_t count)
{
    if (!count) return;
    for (uint16_t k = count;; k--) {
        up = (uint16_t)(up - 2);
        uprow = (uint16_t)(uprow - 4);
        if ((int16_t)(up - 0x9402) >= 0) {
            wr16(up, d1 >> 8);
            wr16(uprow, scroll);
            d1 = (uint16_t)(d1 - step);
        }
        down = (uint16_t)(down + 2);
        downrow = (uint16_t)(downrow + 4);
        if ((int16_t)(down - 0x9600) < 0) {
            wr16(down, d2 >> 8);
            wr16(downrow, scroll);
            d2 = (uint16_t)(d2 + step);
        }
        if (k == 0) break;
    }
}

/* a face: its bit and shade, its edge (from `size`), its rows (from `height`), its scroll */
static void cube_face(uint16_t angle, uint16_t bit, uint16_t shade_at, uint16_t size, uint16_t height, uint16_t base,
                      uint16_t scroll)
{
    wr16(shade_at, CubeShade(bit, angle));
    uint16_t up, down, uprow, downrow, d1, d2, d3;
    CubeEdge(angle, size, rd16(0x802E), base, &up, &down, &uprow, &downrow, &d1, &d2, &d3);
    uint32_t count;
    uint32_t step = CubeSlope((uint16_t)(angle + 0x40), height, &count);
    CubeRows(up, down, uprow, downrow, (uint16_t)step, d1, d2, scroll, (uint16_t)count);
}

/* $01BB6E. The first face (the logo), by its near edge. */
void CubeFaceA(uint16_t angle)
{
    cube_face(angle, 0x4000, 0x8014, rd16(0x8032), rd16(0x8034), 0x4800, 0x40);
}

/* $01BBA4. The second face. */
void CubeFaceB(uint16_t angle)
{
    cube_face(angle, 0x8000, 0x8016, rd16(0x8034), rd16(0x8032), 0xA800, 0x40);
}

/* $01BBDA. The first face, the other way round (its far side). */
void CubeFaceC(uint16_t angle)
{
    cube_face(angle, 0x4000, 0x8014, rd16(0x8032), rd16(0x8034), 0x4800, 0x140);
}

/* $01BC10. The second face, the other way round. */
void CubeFaceD(uint16_t angle)
{
    cube_face(angle, 0x8000, 0x8016, rd16(0x8034), rd16(0x8032), 0xA800, 0x140);
}

/* $01BA74. The cube: turning to the logo (down held turns it the other way), its two faces in
 * view drawn into the line tables, the palettes shaded. */
void CubeTurn(void)
{
    wr8(VDP_REG(0) + 1, 0x14);
    wr8(VDP_REG(10) + 1, 0);
    wr8(VDP_REG(11) + 1, 3);
    wr16(0xEC00, 0xFE);
    wr16(0x800A, 0xF000);
    wr16(0x802E, 0xFF56);
    wr16(0x8034, 0x48);
    wr16(0x8032, 0x18);
    if (rd16(TURN) != 0x40 || (int16_t)(rd16(v_modeTimer) - 0x1600) >= 0) {
        wr16(TURN, (uint16_t)(rd16(TURN) - 2));
        wr16(TURN, rd16(TURN) & 0xFE);
        if (bit8(v_padHeld, 1)) wr16(TURN, (uint16_t)(rd16(TURN) + 4));
    }
    CubeShow();
    CubeClear();
    wr16(FACES, 0);
    uint16_t t = rd16(TURN);
    if ((int16_t)(t - 0x40) < 0) {
        CubeFaceA(t);
        CubeFaceB((uint16_t)(rd16(TURN) + 0x40));
    } else if ((int16_t)(t - 0x80) < 0) {
        CubeFaceD((uint16_t)(t + 0x40));
        CubeFaceA(rd16(TURN));
    } else if ((int16_t)(t - 0xC0) < 0) {
        CubeFaceC(t);
        CubeFaceD((uint16_t)(rd16(TURN) + 0x40));
    } else {
        CubeFaceB((uint16_t)(t + 0x40));
        CubeFaceC(rd16(TURN));
    }
    AdjustPalettes();
}

/* $01BA6E */
void TitleCube(void)
{
    CubeTurn();
}

/* $01B7AA. The cube's tiles and palettes, the planes from the top. */
void TitleSetup(void)
{
    RunCommands(0x01B9E4);
    wr16(0x7000, 0x102);
    wr16(0x7400, 0x102);
    LoadPalettes(0x01BA26);
    PlaneAReset();
    PlaneBReset();
    wr8(VDP_REG(7) + 1, 0);
}

/* $01B67E. The cube; Start shows the menu (once the logo is up), Start on it begins: the fade,
 * then the choice's mode (the story, $1C; options, $C). Nothing pressed while the timer runs
 * out: the demo (mode $10 from the title, $4C after the story). */
void TitleInput(void)
{
    if (!rd16(v_fadeSpeed)) {
        if (rd16(v_fadeLevel)) {
            wr16(v_hintEffect, 0);
            wr16(v_hintState, 0);
            wr8(VDP_REG(11) + 1, 0);
            wr8(VDP_REG(7) + 1, 0x10);
            if (rd16(v_modeTimer)) {
                QueueSound(0xF6);
                wr16(v_mode, rd16(CHOICE) == 4 ? 0xC : 0x1C);
                wr16(v_modeState, 0);
                return;
            }
            if (rd16(v_mode) == 8) {
                QueueSound(0xF6);
                wr16(v_mode, 0x10);
            } else {
                wr16(v_mode, 0x4C);
            }
            wr16(v_modeState, 0);
            return;
        }
        wr16(v_fadeColor, 0);
        int fade = 0;
        if (!rd16(v_modeTimer)) {
            fade = 1;
        } else if ((rd8(v_padPressed) | rd8(v_padPressed + 1)) & 0x80) {
            if (rd16(v_sceneStep)) {
                QueueSound(0xC4);
                TitleChoose();
                fade = 1;
            } else if ((int16_t)rd16(v_modeTimer) <= 0x160) {
                QueueSound(0xC4);
                wr16(v_sceneStep, 4);
                wr16(v_mode, 8);
                wr16(v_modeTimer, 0x1C0);
            }
        }
        if (fade) {
            wr16(v_fadeSpeed, 0x100);
            wr16(v_fadeLevel, 0);
        }
    }
    TitleCube();
    if (rd16(v_sceneStep)) {
        TitleMenu();
        TitleSelect();
        if (rd8(v_padPressed) | rd8(v_padPressed + 1)) wr16(v_modeTimer, 0x1C0);
        return;
    }
    if ((int16_t)rd16(v_modeTimer) <= 0x160) SpriteColumn(0xFFFFA100, 0x130, 0, 0x01B9A4, 0x140, 0xA);
    title_bottom();
}

/* $01B5B0. Modes 8 and $50. */
void Title(void)
{
    if (!rd16(v_modeState)) {
        ScreenReset();
        RunCommands(0x01B9D0);
        StageTilesJapan();
        TitleSetup();
        TitleCube();
        wr16(v_boss, 0x4F0);
        wr16(0xC984, 0);
        wr32(v_layout, 0x01B45A);
        wr16(v_hintEffect, 4);
        wr16(v_hintState, 0);
        wr16(CHOICE, 0);
        set8(VDP_REG(1) + 1, 6);
        wr8(v_displayOn, 0x80);
        wr16(v_fadeSpeed, rd16(v_fadeColor) ? 0xFF80 : 0xFF00);
        wr16(v_fadeLevel, 0x1000);
        wr16(v_modeTimer, 0x1C0);
        wr16(v_modeState, (uint16_t)(rd16(v_modeState) + 4));
        wr16(v_sceneStep, 0);
        wr8(v_soundNext, 0x8B);
        wr16(v_soundDelay, 0x12);
        return;
    }
    SpritesStart();
    DrawListStart();
    PlayersUpdate();
    TitleInput();
    ObjectsUpdate();
    DrawListEnd();
    BuildSprites();
    wr16(v_gameFrames, (uint16_t)(rd16(v_gameFrames) + 1));
}
