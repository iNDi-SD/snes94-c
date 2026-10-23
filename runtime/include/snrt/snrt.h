/* The SNES runtime: the console around a game reimplemented in C. There is no 65816 emulation;
 * the client writes the registers where its game did and moves the beam (a client-owned clock)
 * as its game's time passes. Timing follows snes9x (against which the C was checked): a line is 1364 master cycles,
 * line y (1-224) is drawn at cycle 32 of line y (snes9x's setting for NHL '94), HDMA starts at
 * line 0, cycle 20 and transfers at cycle 1106 of lines 0-224, and the vertical blank begins at
 * line 225, cycle 0. */
#pragma once
#include <stdint.h>
#include "snrt/ppu.h"
#include "snrt/dma.h"
#include "snrt/io.h"

#define SNRT_WIDTH 256
#define SNRT_HEIGHT 224
#define SNRT_LINES 262
#define SNRT_LINE_CYCLES 1364
#define SNRT_DRAW_CYCLE 32
#define SNRT_HDMA_INIT_CYCLE 20
#define SNRT_HDMA_CYCLE 1106

typedef struct snrt snrt;
struct snrt {
  snrt_ppu ppu;
  snrt_dma dma;
  snrt_io io;
  int line, cycle;                                   /* the beam */
  uint16_t picture[SNRT_HEIGHT * SNRT_WIDTH];        /* RGB565, as snes9x shows it */
  void (*frame_done)(snrt *m, void *user);           /* line 225: the picture is complete */
  void *user;
  /* the A bus (the game's memory: WRAM, ROM, save RAM) as DMA reaches it */
  uint8_t (*read_a)(void *user, uint32_t addr);
  void (*write_a)(void *user, uint32_t addr, uint8_t v);
};

void snrt_power(snrt *m);
/* move the beam forward to (line, cycle), doing what happens on the way; the beam wraps from
 * line 261 to 0 */
void snrt_beam(snrt *m, int line, int cycle);
/* a CPU write to $2100-$21FF, $4016 or $4200-$437F; a CPU read of one (cycle: the read's own, for
 * the H blank flag; the beam's is used when -1) */
void snrt_write(snrt *m, uint16_t addr, uint8_t v);
uint8_t snrt_read(snrt *m, uint16_t addr, int cycle);
/* the B bus ($2100-$21FF), as the CPU or DMA reaches it */
void snrt_bwrite(snrt *m, uint16_t addr, uint8_t v);
uint8_t snrt_bread(snrt *m, uint16_t addr);
