/* The level's collision. The stage keeps the plane's map in work RAM at $FF0000 (64 x 64 cells,
 * a word each: the tile's number in bits 0-10, its flips above) and an attribute per tile at
 * $FF7800: 0 nothing, $10-$1F walls, $20-$3F floors (flat, thin, the slopes), $60 a floor that
 * shakes. A position on the screen is turned into a cell by the camera (v_mapX, v_mapY);
 * a flipped tile's attribute is the flipped one (the table at $0B9A2). MapSnap moves a position
 * onto what it hit: a wall's side, a floor's top, a slope's height at x. */
#include "game.h"

#define MAP        0x0000         /* $FF0000 */
#define ATTRIBUTES 0x7800         /* $FF7800 */
#define FLIPPED    0x00B9A2u      /* an attribute as its tile's flips change it */
#define SLOPES     0x00BCA0u      /* MapSnap's routines for the floors, by attribute - $20 */

/* $00B95A. The attribute at a position on the screen. */
uint16_t MapAttribute(uint16_t x, uint16_t y)
{
    x = (uint16_t)(x + rd16(v_mapX));
    y = (uint16_t)(y + rd16(v_mapY));
    uint16_t cell = (uint16_t)(((x >> 2) & 0x7E) + ((y << 4) & 0x1F80));
    uint16_t tile = rd16((uint16_t)(MAP + cell));
    uint16_t a = rd8((uint16_t)(ATTRIBUTES + (tile & 0x7FF)));
    if (!a) return 0;
    a ^= ((tile >> 6) & a & 0x60) >> 2;
    return rom8(FLIPPED + a);
}

/* $00B94A. The same, keeping the caller's registers. */
uint16_t MapAttr(uint16_t x, uint16_t y) { return MapAttribute(x, y); }

/* $00BB74. Whether the tile at a position is one of the four (attributes $20, $21, $28, $29,
 * unflipped) the table at $0BBAE marks: 1, else 0 (Z). */
uint16_t MapMarked(uint16_t x, uint16_t y)
{
    x = (uint16_t)(x + rd16(v_mapX));
    y = (uint16_t)(y + rd16(v_mapY));
    uint16_t cell = (uint16_t)(((x >> 2) & 0x7E) + ((y << 4) & 0x1F80));
    uint16_t a = rd8((uint16_t)(ATTRIBUTES + (rd16((uint16_t)(MAP + cell)) & 0x7FF)));
    if (!a) return 0;
    return rom8(0x00BBAE + a);
}

/* $00BA4C. An attribute from $20 up, else 0. */
uint16_t MapAttrHigh(uint16_t x, uint16_t y)
{
    uint16_t a = MapAttr(x, y);
    return a > 0x1F ? a : 0;
}

/* $00BA74. A floor ($20-$37), else 0. */
uint16_t MapFloor(uint16_t x, uint16_t y)
{
    uint16_t a = MapAttr(x, y);
    return a >= 0x20 && a < 0x38 ? a : 0;
}

/* $00BAA4. A thin floor ($28-$2F), else 0. */
uint16_t MapThinFloor(uint16_t x, uint16_t y)
{
    uint16_t a = MapAttr(x, y);
    return a >= 0x28 && a < 0x30 ? a : 0;
}

/* $00BAFC. A wall that stops a move right (below $20, bit 2 clear), else 0. */
uint16_t MapWallRight(uint16_t x, uint16_t y)
{
    uint16_t a = MapAttr(x, y);
    return a < 0x20 && !(a & 4) ? a : 0;
}

/* $00BB34. A wall that stops a move left (below $20, bit 2 set), else 0. */
uint16_t MapWallLeft(uint16_t x, uint16_t y)
{
    uint16_t a = MapAttr(x, y);
    return a < 0x20 && (a & 4) ? a : 0;
}

/* a coordinate's cell's start (on the screen) */
static uint16_t cell_x(uint16_t x) { return (uint16_t)(((x + rd16(v_mapX)) & 0xFFF8) - rd16(v_mapX)); }
static uint16_t cell_y(uint16_t y) { return (uint16_t)(((y + rd16(v_mapY)) & 0xFFF8) - rd16(v_mapY)); }

/* $00BC2E. The position moved onto what its attribute is: a wall's side (x: the cell's left,
 * +3, +4 or +7), a floor's top (y, by MapSnap's slope routines), the shaking floor's (y down
 * a line on odd frames). attr is left as the 68k leaves d2. */
void MapSnap(uint16_t *x, uint16_t *y, uint16_t *attr)
{
    uint16_t a = (uint16_t)(*attr - 0x20);
    *attr = a;
    if (!(a & 0x8000)) {
        if (a == 0x40) {
            if (rd8(v_gameFrames + 1) & 1) *y = (uint16_t)(*y + 1);
            return;
        }
        a = (uint16_t)(a << 2);
        *attr = a;
        uint32_t fn = rom32(SLOPES + (uint32_t)(int16_t)a);
        switch (fn) {
        case 0x00BD2E: *y = MapFloorTop(*y); return;
        case 0x00BD3C: *y = MapFloorBottom(*y); return;
        case 0x00BD4C: *y = MapSlopeUp(*x, *y, attr); return;
        case 0x00BD6A: *y = MapSlopeUpLow(*x, *y, attr); return;
        case 0x00BD8C: *y = MapSlopeUpHigh(*x, *y, attr); return;
        case 0x00BDAC: *y = MapFloorMiddle(*y); return;
        case 0x00BDBC: *y = MapFloorMiddleHigh(*y); return;
        case 0x00BDCC: *y = MapSlopeDown(*x, *y, attr); return;
        case 0x00BDE6: *y = MapSlopeDownLow(*x, *y, attr); return;
        case 0x00BE04: *y = MapSlopeDownHigh(*x, *y, attr); return;
        default: call68k_missing(fn); return;
        }
    }
    a = (uint16_t)(a + 0x10);
    *attr = a;
    if (a & 0x8000) return;
    uint16_t odd = a & 1;
    *attr = a >> 1;
    if (!(a & 4)) *x = (uint16_t)(cell_x(*x) + (odd ? 4 : 0));
    else *x = (uint16_t)(cell_x(*x) + (odd ? 3 : 7));
}

/* $00BD2E, $00BD3C, $00BDAC, $00BDBC: a flat floor's top at the cell's line 0, 7, 4, 3 */
uint16_t MapFloorTop(uint16_t y) { return cell_y(y); }
uint16_t MapFloorBottom(uint16_t y) { return (uint16_t)(cell_y(y) + 7); }
uint16_t MapFloorMiddle(uint16_t y) { return (uint16_t)(cell_y(y) + 4); }
uint16_t MapFloorMiddleHigh(uint16_t y) { return (uint16_t)(cell_y(y) + 3); }

/* the slopes: the top's line in the cell from x's column in it (rising to the right: 7 - column) */
static uint16_t rising(uint16_t x) { return (uint16_t)(-(x + rd16(v_mapX) + 1)) & 7; }
static uint16_t falling(uint16_t x) { return (uint16_t)(x + rd16(v_mapX)) & 7; }

/* $00BD4C */
uint16_t MapSlopeUp(uint16_t x, uint16_t y, uint16_t *d2)
{
    *d2 = rising(x);
    return (uint16_t)(cell_y(y) + *d2);
}

/* $00BD6A. Half as steep, the cell's lower half. */
uint16_t MapSlopeUpLow(uint16_t x, uint16_t y, uint16_t *d2)
{
    *d2 = (uint16_t)((rising(x) >> 1) + 4);
    return (uint16_t)(cell_y(y) + *d2);
}

/* $00BD8C. Half as steep, the upper half. */
uint16_t MapSlopeUpHigh(uint16_t x, uint16_t y, uint16_t *d2)
{
    *d2 = rising(x) >> 1;
    return (uint16_t)(cell_y(y) + *d2);
}

/* $00BDCC */
uint16_t MapSlopeDown(uint16_t x, uint16_t y, uint16_t *d2)
{
    *d2 = falling(x);
    return (uint16_t)(cell_y(y) + *d2);
}

/* $00BDE6 */
uint16_t MapSlopeDownLow(uint16_t x, uint16_t y, uint16_t *d2)
{
    *d2 = (uint16_t)((falling(x) >> 1) + 4);
    return (uint16_t)(cell_y(y) + *d2);
}

/* $00BE04 */
uint16_t MapSlopeDownHigh(uint16_t x, uint16_t y, uint16_t *d2)
{
    *d2 = falling(x) >> 1;
    return (uint16_t)(cell_y(y) + *d2);
}

/* $00C31C. A box's floor and ceiling (corners v_boundsX0/Y0 and v_boundsX1/Y1): y held to the
 * edge it is past ($20 the floor, $28 the ceiling, 0 neither). The second corner's side of the
 * first decides which edges count and where x must be for the first corner's. */
uint16_t MapBoundsY(uint16_t x, uint16_t *y)
{
    int16_t x0 = (int16_t)rd16(v_boundsX0), y0 = (int16_t)rd16(v_boundsY0);
    int16_t x1 = (int16_t)rd16(v_boundsX1), y1 = (int16_t)rd16(v_boundsY1);
    int16_t px = (int16_t)x, py = (int16_t)*y;
    uint16_t a = 0;
    if (y1 == y0) return 0;
    if (x1 == x0) {
        if (y1 < y0) {
            if (y1 >= py) { *y = (uint16_t)y1; return 0x28; }
            if (y0 > py) return 0;
            *y = (uint16_t)y0;
            return 0x20;
        }
        if (y1 <= py) { py = y1; *y = (uint16_t)y1; a = 0x20; }
        if (y0 < py) return a;
        *y = (uint16_t)y0;
        return 0x28;
    }
    int beyond = x1 > x0 ? x0 < px : x0 > px;           /* x on the wrong side of the first corner */
    if (y1 < y0) {
        if (y1 >= py) { py = y1; *y = (uint16_t)y1; a = 0x28; }
        if (beyond || y0 > py) return a;
        *y = (uint16_t)y0;
        return 0x20;
    }
    if (y1 <= py) { py = y1; *y = (uint16_t)y1; a = 0x20; }
    if (beyond || y0 < py) return a;
    *y = (uint16_t)y0;
    return 0x28;
}
