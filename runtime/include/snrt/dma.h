/* DMA and HDMA: the eight channels ($4300-$437F), MDMAEN ($420B) and HDMAEN ($420C). Behaviour
 * after ares's (ISC licence); timing as snes9x: a DMA runs whole when MDMAEN is written, HDMA
 * starts at line 0, cycle 20 and transfers at cycle 1106 of lines 0-224. The A bus (the game's
 * memory) is reached through the runtime's callbacks. */
#pragma once
#include <stdint.h>

typedef struct snrt_dma_channel {
  uint8_t dmap, bbad, a1b, dasb, ntrl, unknown;
  uint16_t a1t, das, a2a;
  uint8_t do_transfer, completed;
} snrt_dma_channel;

typedef struct snrt_dma {
  snrt_dma_channel ch[8];
  uint8_t hdmaen;
  int active;                 /* the channel transferring now (-1 none) and whether by HDMA */
  uint8_t in_hdma, table_read;   /* table_read: an HDMA read of its table (line count, address) */
} snrt_dma;

struct snrt;
void snrt_dma_power(snrt_dma *d);
void snrt_dma_write(struct snrt *m, uint16_t addr, uint8_t v);   /* $420B, $420C, $4300-$437F */
uint8_t snrt_dma_read(const snrt_dma *d, uint16_t addr);        /* $4300-$437F */
void snrt_hdma_init(struct snrt *m);                             /* line 0, cycle 20 */
void snrt_hdma_line(struct snrt *m);                             /* cycle 1106 of lines 0-224 */
