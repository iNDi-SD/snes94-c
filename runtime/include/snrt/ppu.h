/* The SNES picture processor: its registers, VRAM, CGRAM and OAM, and a renderer that draws one
 * line at a time from the state at the moment it is called (the runtime calls it where snes9x
 * draws: cycle 32 of each visible line for NHL '94). Behaviour after ares's PPU (ISC licence), drawn
 * a line at a time. Not drawn, as NHL '94 does not use them: the hires modes 5 and 6,
 * offset-per-tile (modes 2, 4, 6), interlace, overscan, EXTBG and direct colour. */
#pragma once
#include <stdint.h>

typedef struct snrt_ppu {
  uint16_t vram[0x8000];      /* words */
  uint16_t cgram[256];        /* BGR555 */
  uint8_t oam[544];

  uint8_t brightness, forced_blank;
  uint8_t obj_size, obj_name_select;
  uint16_t obj_base;          /* word address */
  uint16_t oam_base, oam_addr;   /* byte addresses (10 bits) */
  uint8_t oam_priority, oam_latch, first_sprite;
  uint8_t bg_mode, bg3_priority, tile_size[4];
  uint8_t mosaic_size, mosaic_enable;
  uint16_t sc_addr[4], chr_addr[4];   /* word addresses */
  uint8_t sc_size[4];
  uint16_t hofs[4], vofs[4], m7hofs, m7vofs;
  int16_t m7_line[4];         /* M7X, M7Y, M7HOFS, M7VOFS as latched (snes9x's rule: see snrt_ppu_newline) */
  uint8_t m7_latched[4];
  uint8_t bgofs_latch1, bgofs_latch2, m7_latch;
  int16_t m7a, m7b, m7c, m7d, m7x, m7y;
  uint8_t m7sel;
  uint8_t vram_inc_high, vram_mapping;
  uint16_t vram_inc, vram_addr, vram_latch;
  uint8_t cg_addr, cg_flip, cg_latch;
  uint8_t wsel[6];            /* BG1-4, OBJ, colour: bit 0 one invert, 1 one enable, 2 two invert, 3 two enable */
  uint8_t wh[4];              /* WH0-WH3 */
  uint8_t wlog[6];
  uint8_t tm, ts, tmw, tsw, cgwsel, cgadsub;
  uint16_t fixed_color;       /* BGR555 */
  uint8_t setini;
  uint8_t field;              /* toggles at line 0 each frame ($213F bit 7) */

  int vline;                  /* the beam's line, set by the runtime: VRAM is closed while drawing */
  int mosaic_vcounter;
} snrt_ppu;

void snrt_ppu_power(snrt_ppu *p);
void snrt_ppu_write(snrt_ppu *p, uint16_t addr, uint8_t v);   /* $2100-$2133 */
/* $2134-$2136 (the multiplier), $213E-$213F (versions as snes9x's first-model console, NTSC, the
 * field; the sprite overflow flags, which NHL '94 never reads, are 0) */
uint8_t snrt_ppu_read(snrt_ppu *p, uint16_t addr);
void snrt_ppu_vblank(snrt_ppu *p);                            /* line 225: the OAM address reloads */
/* the start of each line (mode 7's centre and scroll are taken as they are when the line is drawn) */
void snrt_ppu_newline(snrt_ppu *p, int line);
/* draw screen line y (1-224, every one in turn each frame) into out[256] as BGR555 */
void snrt_ppu_line(snrt_ppu *p, int y, uint16_t *out);
/* BGR555 at a brightness (0-15) into RGB565, as snes9x shows it */
void snrt_ppu_rgb565(const uint16_t *in, int n, int brightness, uint16_t *out);
