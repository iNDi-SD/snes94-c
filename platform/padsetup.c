/* The controller setup screen's own parts (padsetup.h). */
#include <stdio.h>
#include <stdlib.h>
#include "menu.h"
#include "padsetup.h"

void pm_padsetup_assign(int *map, int n, int i, int value)
{
    for (int j = 0; j < n; j++)
        if (j != i && map[j] == value) map[j] = map[i];
    map[i] = value;
}

enum { CW = PM_MENU_CELL_W, CH = PM_MENU_CELL_H };
#define SHADE 0xC0000000u                                /* (the picture darkened under the screen) */
#define WHITE 0xFFFFFF
#define DIM 0x9090A0
#define GOLD 0xFFD84A
#define RED 0xFF5A4A

static void text(uint32_t *px, int w, int h, int pitch, int col, int row, const char *s, uint32_t rgb)
{
    pm_menu_text(px, w, h, pitch, 4 + col * CW, 4 + row * CH, s, rgb);
}

static void dot(uint32_t *px, int w, int h, int pitch, int x, int y, uint32_t rgb)
{
    if (x >= 0 && x < w && y >= 0 && y < h) px[y * pitch + x] = rgb;
}

/* a square's outline, r from its middle to each side */
static void square(uint32_t *px, int w, int h, int pitch, int cx, int cy, int r, uint32_t rgb)
{
    for (int k = -r; k <= r; k++) {
        dot(px, w, h, pitch, cx + k, cy - r, rgb); dot(px, w, h, pitch, cx + k, cy + r, rgb);
        dot(px, w, h, pitch, cx - r, cy + k, rgb); dot(px, w, h, pitch, cx + r, cy + k, rgb);
    }
}

void pm_padsetup_draw(uint32_t *px, int w, int h, int pitch, const pm_padsetup_view *v)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) px[y * pitch + x] = SHADE;
    char line[64];
    snprintf(line, sizeof line, "CONTROLLER SETUP");
    text(px, w, h, pitch, 0, 0, line, GOLD);
    if (v->controller[0]) { snprintf(line, sizeof line, "%.40s", v->controller); text(px, w, h, pitch, 0, 1, line, DIM); }
    if (v->at == -2) {
        text(px, w, h, pitch, 0, 4, v->none ? "No controller is plugged in." : "Press any button on the controller", WHITE);
        if (!v->none) text(px, w, h, pitch, 0, 5, "to set up.", WHITE);
        text(px, w, h, pitch, 0, 8, "Esc: close", DIM);
    } else if (v->at >= 0) {
        snprintf(line, sizeof line, "Press the button for %s", v->names[v->at]);
        text(px, w, h, pitch, 0, 3, line, WHITE);
        if (v->roles[v->at][0]) snprintf(line, sizeof line, "(%s; %d s: kept as it is)", v->roles[v->at], v->seconds);
        else snprintf(line, sizeof line, "(nothing for %d s: kept as it is)", v->seconds);
        text(px, w, h, pitch, 0, 4, line, DIM);
        for (int i = 0; i < v->n; i++) {
            snprintf(line, sizeof line, "%c %-6s %-13s %s", i == v->at ? 0x7F : ' ', v->names[i], v->values[i][0] ? v->values[i] : "-", v->roles[i]);
            text(px, w, h, pitch, 0, 6 + i, line, i == v->at ? GOLD : i < v->at ? WHITE : DIM);
        }
        text(px, w, h, pitch, 0, 7 + v->n, "Esc: cancel", DIM);
    } else {
        snprintf(line, sizeof line, "Dead zone: %d%%", v->dead_zone);
        text(px, w, h, pitch, 0, 3, line, WHITE);
        text(px, w, h, pitch, 0, 4, "Left / Right: smaller / larger", DIM);
        int r = 50, cx = w / 2, cy = 4 + 7 * CH + r;      /* the stick's reach, the dead zone inside it (each */
        square(px, w, h, pitch, cx, cy, r, DIM);          /* axis apart: a square) */
        square(px, w, h, pitch, cx, cy, r * v->dead_zone / 100, GOLD);
        int sx = cx + v->stick_x * r / 32768, sy = cy + v->stick_y * r / 32768, dead = 32767 * v->dead_zone / 100;
        int out = abs(v->stick_x) > dead || abs(v->stick_y) > dead;
        for (int y = -2; y <= 2; y++)
            for (int x = -2; x <= 2; x++) if (x * x + y * y <= 5) dot(px, w, h, pitch, sx + x, sy + y, out ? RED : WHITE);
        text(px, w, h, pitch, 0, 6, "Red: the stick counts as a direction.", DIM);
        text(px, w, h, pitch, 0, 20, "Start or Enter: save   Esc: cancel", WHITE);
    }
    for (int y = 0; y < h; y++)                           /* (the text and drawing opaque: theirs had no alpha) */
        for (int x = 0; x < w; x++)
            if (!(px[y * pitch + x] >> 24)) px[y * pitch + x] |= 0xFF000000u;
}
