/* crowd: the hot / cold table (src/routines/crowd). */
#include "game.h"

/* Fill the hot / cold table (both teams, 624 bytes, from the end down) with random values -12 to 12: a
 * random 0-13 limited to 12, negative when bit 4 of the seed is clear after it */
void Create_HotCold_Table(void)
{
    for (int x = 0x26F; x >= 0; x--) {
        uint16_t r3 = randoma8(0x0E);
        if (r3 >= 0x0C) r3 = 0x0C;
        g.HotColdTable[x] = (uint8_t)(g.seed[0] & 0x10 ? r3 : -r3);
    }
}
