/* pulseman's own 5 x 8 font (cells of 6 x 10 pixels): text drawn over the picture at the
 * console's resolution (the controller setup screen, the settings menu, the frame counter). Nothing
 * here knows SDL, files or the game. */
#ifndef PM_MENU_H
#define PM_MENU_H
#include <stdint.h>

#define PM_MENU_CELL_W 6
#define PM_MENU_CELL_H 10

/* text in the font with its top left at pixel x, y (clipped to the picture), with a shadow; the
 * x after it */
int pm_menu_text(uint32_t *px, int w, int h, int pitch, int x, int y, const char *s, uint32_t rgb);

/* what the settings menu shows: its items (names, and values: "" for an item that opens a screen
 * rather than being set), n of them, and the chosen one (at) */
typedef struct pm_menu_settings_view {
    const char *const *names;
    const char *const *values;
    int n, at;
} pm_menu_settings_view;

/* the menu into px (w x h, pitch pixels a row; ARGB, its alpha for laying over the picture): the
 * picture darkened, a title, the items with the chosen one marked, and the keys that work it */
void pm_menu_settings_draw(uint32_t *px, int w, int h, int pitch, const pm_menu_settings_view *v);

#endif
