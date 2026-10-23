/* The controller setup screen's own parts (no SDL; platform/main.c runs the screen, F9 or a
 * controller's Select and Start held, and saves what it sets): a controller button given to one of
 * the pad's buttons, and the screen drawn over the game's picture. */
#ifndef PM_PADSETUP_H
#define PM_PADSETUP_H
#include <stdint.h>

/* map[i] (a controller button for the pad's button i, of n) made value; the pad's button that had
 * value takes map[i]'s old one (no controller button left doing two) */
void pm_padsetup_assign(int *map, int n, int i, int value);

/* what the screen shows: the pages are choosing a controller (at -2), the pad's buttons one at a
 * time (at: the one asked for), the stick's dead zone (at -1) */
typedef struct pm_padsetup_view {
    const char *controller;             /* the controller's name ("": none chosen yet) */
    const char *const *names;           /* the pad's buttons ("B", "A" ...), n of them */
    const char *const *values;          /* the controller button each has (SDL's names; "" none) */
    const char *const *roles;           /* what each does in the game ("" nothing said) */
    int n, at;
    int seconds;                        /* (asking for a button) the seconds left before it is kept as it is */
    int dead_zone;                      /* % */
    int stick_x, stick_y;               /* the left stick now (-32768 to 32767) */
    int none;                           /* (choosing) no controller plugged in */
} pm_padsetup_view;

/* the screen into px (w x h, pitch pixels a row; ARGB, its alpha for laying over the picture) */
void pm_padsetup_draw(uint32_t *px, int w, int h, int pitch, const pm_padsetup_view *v);

#endif
