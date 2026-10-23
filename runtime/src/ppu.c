/* The SNES picture processor, after ares's PPU (ISC licence), drawn a line at a time. */
#include <string.h>
#include "snrt/ppu.h"

enum { BG1, BG2, BG3, BG4, OBJ, COL };
enum { BPP2, BPP4, BPP8, MODE7, NONE };

void snrt_ppu_power(snrt_ppu *p) {
  memset(p, 0, sizeof *p);
  p->forced_blank = 1;
  p->vram_inc = 1;
  p->mosaic_size = 1;
  p->field = 1;   /* snes9x's phase */
  /* CGRAM is random at power on; this is snes9x's */
  for (int c = 0; c < 256; c++)
    p->cgram[c] = (uint16_t)((c & 7) << 2 | ((c >> 3) & 7) << 7 | ((c >> 6) & 2) << 13);
}

static uint16_t vram_address(const snrt_ppu *p) {
  uint16_t a = p->vram_addr;
  switch (p->vram_mapping) {
    case 1: return (a & 0xFF00) | (a & 0x1F) << 3 | (a >> 5 & 7);
    case 2: return (a & 0xFE00) | (a & 0x3F) << 3 | (a >> 6 & 7);
    case 3: return (a & 0xFC00) | (a & 0x7F) << 3 | (a >> 7 & 7);
  }
  return a;
}

static int drawing(const snrt_ppu *p) { return !p->forced_blank && p->vline < 225; }

static void write_vram(snrt_ppu *p, int high, uint8_t v) {
  if (drawing(p)) return;
  uint16_t *w = &p->vram[vram_address(p) & 0x7FFF];
  *w = high ? (*w & 0x00FF) | v << 8 : (*w & 0xFF00) | v;
}

static void oam_reset(snrt_ppu *p) {
  p->oam_addr = p->oam_base;
  p->first_sprite = p->oam_priority ? (p->oam_addr >> 2) & 0x7F : 0;
}

void snrt_ppu_newline(snrt_ppu *p, int line) {
  memset(p->m7_latched, 0, sizeof p->m7_latched);
  if (line == 0) p->field ^= 1;
}

static int16_t m7_reg(const snrt_ppu *p, int k) {
  return k == 0 ? p->m7x : k == 1 ? p->m7y : k == 2 ? (int16_t)p->m7hofs : (int16_t)p->m7vofs;
}

static void m7_latch(snrt_ppu *p, int k) {
  if (p->m7_latched[k]) return;
  p->m7_line[k] = m7_reg(p, k);
  p->m7_latched[k] = 1;
}

void snrt_ppu_vblank(snrt_ppu *p) {
  if (!p->forced_blank) oam_reset(p);
}

void snrt_ppu_write(snrt_ppu *p, uint16_t addr, uint8_t v) {
  int n;
  switch (addr) {
    case 0x2100:
      p->brightness = v & 15; p->forced_blank = v >> 7;
      return;
    case 0x2101: p->obj_base = (v & 7) << 13; p->obj_name_select = v >> 3 & 3; p->obj_size = v >> 5; return;
    case 0x2102: p->oam_base = (p->oam_base & 0x200) | v << 1; oam_reset(p); return;
    case 0x2103: p->oam_base = (p->oam_base & 0x1FE) | (v & 1) << 9; p->oam_priority = v >> 7; oam_reset(p); return;
    case 0x2104: {
      uint16_t a = p->oam_addr;
      p->oam_addr = (p->oam_addr + 1) & 0x3FF;
      if (!(a & 1)) p->oam_latch = v;
      if (a & 0x200) p->oam[0x200 | (a & 0x1F)] = v;
      else if (a & 1) { p->oam[a & ~1] = p->oam_latch; p->oam[a] = v; }
      if (p->oam_priority) p->first_sprite = (p->oam_addr >> 2) & 0x7F;
      return;
    }
    case 0x2105:
      p->bg_mode = v & 7; p->bg3_priority = v >> 3 & 1;
      for (n = 0; n < 4; n++) p->tile_size[n] = v >> (4 + n) & 1;
      return;
    case 0x2106: {
      int was = p->mosaic_enable;
      p->mosaic_enable = v & 15; p->mosaic_size = (v >> 4) + 1;
      if (!was && p->mosaic_enable) p->mosaic_vcounter = p->mosaic_size + 1;
      return;
    }
    case 0x2107: case 0x2108: case 0x2109: case 0x210A:
      n = addr - 0x2107; p->sc_size[n] = v & 3; p->sc_addr[n] = (v & 0xFC) << 8; return;
    case 0x210B: p->chr_addr[0] = (v & 15) << 12; p->chr_addr[1] = (v >> 4) << 12; return;
    case 0x210C: p->chr_addr[2] = (v & 15) << 12; p->chr_addr[3] = (v >> 4) << 12; return;
    case 0x210D: case 0x210F: case 0x2111: case 0x2113:
      if (addr == 0x210D) { p->m7hofs = v << 8 | p->m7_latch; p->m7_latch = v; }
      n = (addr - 0x210D) >> 1;
      p->hofs[n] = (v << 8 | (p->bgofs_latch1 & ~7) | (p->bgofs_latch2 & 7)) & 0x3FF;
      p->bgofs_latch1 = p->bgofs_latch2 = v;
      return;
    case 0x210E: case 0x2110: case 0x2112: case 0x2114:
      if (addr == 0x210E) { p->m7vofs = v << 8 | p->m7_latch; p->m7_latch = v; }
      n = (addr - 0x210E) >> 1;
      p->vofs[n] = (v << 8 | p->bgofs_latch1) & 0x3FF;
      p->bgofs_latch1 = v;
      return;
    case 0x2115: {
      static const uint16_t inc[4] = { 1, 32, 128, 128 };
      p->vram_inc = inc[v & 3]; p->vram_mapping = v >> 2 & 3; p->vram_inc_high = v >> 7;
      return;
    }
    case 0x2116: p->vram_addr = (p->vram_addr & 0xFF00) | v; p->vram_latch = p->vram[vram_address(p) & 0x7FFF]; return;
    case 0x2117: p->vram_addr = (p->vram_addr & 0x00FF) | v << 8; p->vram_latch = p->vram[vram_address(p) & 0x7FFF]; return;
    case 0x2118: write_vram(p, 0, v); if (!p->vram_inc_high) p->vram_addr += p->vram_inc; return;
    case 0x2119: write_vram(p, 1, v); if (p->vram_inc_high) p->vram_addr += p->vram_inc; return;
    case 0x211A: p->m7sel = v; return;
    case 0x211B: p->m7a = (int16_t)(v << 8 | p->m7_latch); p->m7_latch = v; return;
    case 0x211C: p->m7b = (int16_t)(v << 8 | p->m7_latch); p->m7_latch = v; return;
    case 0x211D: p->m7c = (int16_t)(v << 8 | p->m7_latch); p->m7_latch = v; return;
    case 0x211E: p->m7d = (int16_t)(v << 8 | p->m7_latch); p->m7_latch = v; return;
    case 0x211F: p->m7x = (int16_t)(v << 8 | p->m7_latch); p->m7_latch = v; return;
    case 0x2120: p->m7y = (int16_t)(v << 8 | p->m7_latch); p->m7_latch = v; return;
    case 0x2121: p->cg_addr = v; p->cg_flip = 0; return;
    case 0x2122:
      if (!p->cg_flip) p->cg_latch = v;
      else p->cgram[p->cg_addr++] = (v & 0x7F) << 8 | p->cg_latch;
      p->cg_flip ^= 1;
      return;
    case 0x2123: p->wsel[0] = v & 15; p->wsel[1] = v >> 4; return;
    case 0x2124: p->wsel[2] = v & 15; p->wsel[3] = v >> 4; return;
    case 0x2125: p->wsel[4] = v & 15; p->wsel[5] = v >> 4; return;
    case 0x2126: case 0x2127: case 0x2128: case 0x2129: p->wh[addr - 0x2126] = v; return;
    case 0x212A: for (n = 0; n < 4; n++) p->wlog[n] = v >> (2 * n) & 3; return;
    case 0x212B: p->wlog[4] = v & 3; p->wlog[5] = v >> 2 & 3; return;
    case 0x212C: p->tm = v; return;
    case 0x212D: p->ts = v; return;
    case 0x212E: p->tmw = v; return;
    case 0x212F: p->tsw = v; return;
    case 0x2130: p->cgwsel = v; return;
    case 0x2131: p->cgadsub = v; return;
    case 0x2132:
      if (v & 0x20) p->fixed_color = (p->fixed_color & ~0x001F) | (v & 31);
      if (v & 0x40) p->fixed_color = (p->fixed_color & ~0x03E0) | (v & 31) << 5;
      if (v & 0x80) p->fixed_color = (p->fixed_color & ~0x7C00) | (v & 31) << 10;
      return;
    case 0x2133:
      p->setini = v;
      return;
  }
}

uint8_t snrt_ppu_read(snrt_ppu *p, uint16_t addr) {
  int32_t r = (int32_t)p->m7a * (int8_t)(p->m7b >> 8);
  switch (addr) {
    case 0x2134: return (uint8_t)r;
    case 0x2135: return (uint8_t)(r >> 8);
    case 0x2136: return (uint8_t)(r >> 16);
    case 0x213E: return 1;
    case 0x213F: return (uint8_t)(p->field << 7 | 3);
  }
  return 0;
}

/* ---- drawing ---------------------------------------------------------------------------------- */

typedef struct { uint8_t pri, pal; } px;   /* priority 0 = nothing here */

static void layer_modes(const snrt_ppu *p, int mode[4], uint8_t pri[5][4]) {
  static const uint8_t bp[8][4] = { {BPP2, BPP2, BPP2, BPP2}, {BPP4, BPP4, BPP2, NONE}, {BPP4, BPP4, NONE, NONE},
    {BPP8, BPP4, NONE, NONE}, {BPP8, BPP2, NONE, NONE}, {BPP4, BPP2, NONE, NONE}, {BPP4, NONE, NONE, NONE},
    {MODE7, NONE, NONE, NONE} };
  /* priorities (tile priority 0, 1; objects 0-3), higher wins: ares's updateVideoMode */
  static const uint8_t prio[9][5][4] = {
    { {8, 11}, {7, 10}, {2, 5}, {1, 4}, {3, 6, 9, 12} },
    { {6, 9}, {5, 8}, {1, 3}, {0, 0}, {2, 4, 7, 10} },
    { {3, 7}, {1, 5}, {0, 0}, {0, 0}, {2, 4, 6, 8} },
    { {3, 7}, {1, 5}, {0, 0}, {0, 0}, {2, 4, 6, 8} },
    { {3, 7}, {1, 5}, {0, 0}, {0, 0}, {2, 4, 6, 8} },
    { {3, 7}, {1, 5}, {0, 0}, {0, 0}, {2, 4, 6, 8} },
    { {2, 5}, {0, 0}, {0, 0}, {0, 0}, {1, 3, 4, 6} },
    { {2, 2}, {0, 0}, {0, 0}, {0, 0}, {1, 3, 4, 5} },
    { {5, 8}, {4, 7}, {1, 10}, {0, 0}, {2, 3, 6, 9} },   /* mode 1 with BG3 on top */
  };
  for (int i = 0; i < 4; i++) mode[i] = bp[p->bg_mode][i];
  memcpy(pri, prio[p->bg_mode == 1 && p->bg3_priority ? 8 : p->bg_mode], sizeof prio[0]);
}

static void draw_bg(const snrt_ppu *p, int id, int mode, const uint8_t pri[2], int y, px *line) {
  int mos = (p->mosaic_enable >> id & 1) ? p->mosaic_size : 1;
  int vpixel = y;
  if (p->mosaic_enable >> id & 1) vpixel -= p->mosaic_size - p->mosaic_vcounter;
  int ts = p->tile_size[id];
  uint32_t hsize = 256u << ts << (p->sc_size[id] & 1), vsize = 256u << ts << (p->sc_size[id] >> 1);
  int vtiles = 3 + ts;
  uint32_t hscreen = (p->sc_size[id] & 1) ? 32 << 5 : 0;
  uint32_t vscreen = (p->sc_size[id] & 2) ? 32 << (5 + (p->sc_size[id] & 1)) : 0;
  uint32_t char_mask = 0x7FFF >> (3 + mode), char_index = p->chr_addr[id] >> (3 + mode);
  uint32_t pal_off = p->bg_mode == 0 ? id << 5 : 0;
  uint32_t voffset = (uint32_t)(vpixel + p->vofs[id]) & (vsize - 1);
  px sample = { 0, 0 };
  for (int x = 0; x < 256; x++) {
    if (x % mos) { line[x] = sample; continue; }
    uint32_t hoffset = (uint32_t)(x + p->hofs[id]) & (hsize - 1);
    uint32_t htile = hoffset >> vtiles, vtile = voffset >> vtiles;
    uint16_t off = (uint16_t)((htile & 31) | (vtile & 31) << 5);
    if (htile & 0x20) off += hscreen;
    if (vtile & 0x20) off += vscreen;
    uint16_t attr = p->vram[(uint16_t)(p->sc_addr[id] + off) & 0x7FFF];
    uint32_t character = attr & 0x3FF;
    int hmirror = attr >> 14 & 1, vmirror = attr >> 15;
    if (ts && ((hoffset & 8) != 0) != hmirror) character += 1;
    if (ts && ((voffset & 8) != 0) != vmirror) character += 16;
    uint32_t origin = (character + char_index) & char_mask;
    uint32_t fy = vmirror ? (voffset & 7) ^ 7 : voffset & 7;
    uint32_t address = (origin << (3 + mode)) + fy;
    int bit = hmirror ? hoffset & 7 : 7 - (hoffset & 7);
    uint32_t color = 0;
    for (int k = 0; k < (1 << mode); k++) {
      uint16_t d = p->vram[(address + (k << 3)) & 0x7FFF];
      color |= ((d >> bit) & 1) << (2 * k) | ((d >> (8 + bit)) & 1) << (2 * k + 1);
    }
    uint32_t group = attr >> 10 & 7;
    sample.pri = color ? pri[attr >> 13 & 1] : 0;
    sample.pal = color ? (uint8_t)(pal_off + (group << (2 << mode)) + color) : 0;
    line[x] = sample;
  }
}

static void draw_mode7(const snrt_ppu *p, const uint8_t pri[2], int y, px *line) {
  int32_t a = p->m7a, b = p->m7b, c = p->m7c, d = p->m7d;
  int32_t hcenter = (int16_t)(p->m7_line[0] << 3) >> 3, vcenter = (int16_t)(p->m7_line[1] << 3) >> 3;
  int32_t hoffset = (int16_t)(p->m7_line[2] << 3) >> 3, voffset = (int16_t)(p->m7_line[3] << 3) >> 3;
  int mos = (p->mosaic_enable & 1) ? p->mosaic_size : 1;
  if (p->mosaic_enable & 1) y -= p->mosaic_size - p->mosaic_vcounter;
  if (p->m7sel & 2) y = 255 - y;
  #define CLIP(n) (((n) & 0x2000) ? ((n) | ~1023) : ((n) & 1023))
  int32_t ox = (a * CLIP(hoffset - hcenter) & ~63) + (b * CLIP(voffset - vcenter) & ~63) + (b * y & ~63) + (hcenter << 8);
  int32_t oy = (c * CLIP(hoffset - hcenter) & ~63) + (d * CLIP(voffset - vcenter) & ~63) + (d * y & ~63) + (vcenter << 8);
  #undef CLIP
  int repeat = p->m7sel >> 6;
  for (int sx = 0; sx < 256; sx++) {
    int x = sx - sx % mos;
    if (p->m7sel & 1) x = 255 - x;
    int32_t pxx = (ox + a * x) >> 8, pyy = (oy + c * x) >> 8;
    int out = ((pxx | pyy) & ~1023) != 0;
    uint16_t tile_addr = (uint16_t)((pyy >> 3 & 127) << 7 | (pxx >> 3 & 127));
    uint8_t tile = repeat == 3 && out ? 0 : (uint8_t)p->vram[tile_addr & 0x7FFF];
    uint8_t pal = repeat == 2 && out ? 0 : (uint8_t)(p->vram[(tile << 6 | (pyy & 7) << 3 | (pxx & 7)) & 0x7FFF] >> 8);
    line[sx].pri = pal ? pri[0] : 0;
    line[sx].pal = pal;
  }
}

static void sprite_dims(const snrt_ppu *p, int large, int *w, int *h) {
  static const uint8_t sw[2][8] = { {8, 8, 8, 16, 16, 32, 16, 16}, {16, 32, 64, 32, 64, 64, 32, 32} };
  static const uint8_t sh[2][8] = { {8, 8, 8, 16, 16, 32, 32, 32}, {16, 32, 64, 32, 64, 64, 64, 32} };
  *w = sw[large][p->obj_size]; *h = sh[large][p->obj_size];
}

/* the sprites on screen line y: evaluated (as on the line before) and drawn, ares's way */
static void draw_obj(const snrt_ppu *p, const uint8_t pri[4], int y, px *line) {
  int ty = y - 1, items[32], n_items = 0, n_tiles = 0;
  struct { uint16_t x; uint8_t pri, pal, hflip; uint32_t data; } tiles[34];
  for (int i = 0; i < 128 && n_items <= 32; i++) {
    int s = (p->first_sprite + i) & 127;
    const uint8_t *o = &p->oam[s * 4];
    uint8_t hi = p->oam[0x200 + (s >> 2)] >> ((s & 3) * 2);
    int sxp = o[0] | (hi & 1) << 8, w, h;
    sprite_dims(p, hi >> 1 & 1, &w, &h);
    if (sxp > 256 && sxp + w - 1 < 512) continue;
    if (((ty - o[1]) & 255) >= h) continue;
    if (n_items++ < 32) items[n_items - 1] = s;
  }
  if (n_items > 32) n_items = 32;
  for (int i = n_items - 1; i >= 0; i--) {
    int s = items[i];
    const uint8_t *o = &p->oam[s * 4];
    uint8_t hi = p->oam[0x200 + (s >> 2)] >> ((s & 3) * 2);
    int x = o[0] | (hi & 1) << 8, w, h;
    sprite_dims(p, hi >> 1 & 1, &w, &h);
    int vflip = o[3] >> 7, hflip = o[3] >> 6 & 1;
    int yy = (ty - o[1]) & 255;
    if (vflip) {
      if (w == h) yy = h - 1 - yy;
      else if (yy < w) yy = w - 1 - yy;
      else yy = w + (w - 1) - (yy - w);
    }
    yy &= 255;
    uint16_t base = p->obj_base;
    if (o[3] & 1) base += (1 + p->obj_name_select) << 12;
    int chrx = o[2] & 15, chry = (((o[2] >> 4) + (yy >> 3)) & 15) << 4;
    int tw = w >> 3;
    for (int tx = 0; tx < tw; tx++) {
      int sx = (x + (tx << 3)) & 511;
      if (x != 256 && sx >= 256 && sx + 7 < 512) continue;
      if (n_tiles >= 34) break;
      int mx = !hflip ? tx : tw - 1 - tx;
      uint16_t pos = (uint16_t)(base + ((chry + ((chrx + mx) & 15)) << 4));
      uint16_t address = (pos & 0xFFF0) + (yy & 7);
      tiles[n_tiles].x = sx;
      tiles[n_tiles].pri = o[3] >> 4 & 3;
      tiles[n_tiles].pal = 128 + ((o[3] >> 1 & 7) << 4);
      tiles[n_tiles].hflip = hflip;
      tiles[n_tiles].data = p->vram[address & 0x7FFF] | (uint32_t)p->vram[(address + 8) & 0x7FFF] << 16;
      n_tiles++;
    }
  }
  memset(line, 0, 256 * sizeof *line);
  for (int x = 0; x < 256; x++) {
    for (int n = 0; n < n_tiles; n++) {
      int pxl = x - (tiles[n].x >= 256 ? tiles[n].x - 512 : tiles[n].x);   /* the 9-bit x, signed */
      if (pxl & ~7) continue;
      int shift = tiles[n].hflip ? pxl : 7 - pxl;
      uint32_t d = tiles[n].data, color = (d >> shift & 1) | (d >> (shift + 7) & 2) | (d >> (shift + 14) & 4) | (d >> (shift + 21) & 8);
      if (color) { line[x].pal = tiles[n].pal + color; line[x].pri = pri[tiles[n].pri]; }
    }
  }
}

static int window_test(int one_en, int one, int two_en, int two, int mask) {
  if (!one_en) return two && two_en;
  if (!two_en) return one;
  if (mask == 0) return one | two;
  if (mask == 1) return one & two;
  return (one ^ two) == 3 - mask;
}

static int window_at(const snrt_ppu *p, int layer, int x) {
  uint8_t s = p->wsel[layer];
  int one = x >= p->wh[0] && x <= p->wh[1], two = x >= p->wh[2] && x <= p->wh[3];
  return window_test(s >> 1 & 1, one ^ (s & 1), s >> 3 & 1, two ^ (s >> 2 & 1), p->wlog[layer]);
}

static uint16_t blend(uint32_t x, uint32_t y, int sub, int half) {
  if (!sub) {
    if (!half) {
      uint32_t sum = x + y, carry = (sum - ((x ^ y) & 0x0421)) & 0x8420;
      return (uint16_t)((sum - carry) | (carry - (carry >> 5)));
    }
    return (uint16_t)((x + y - ((x ^ y) & 0x0421)) >> 1);
  }
  uint32_t diff = x - y + 0x8420, borrow = (diff - ((x ^ y) & 0x8420)) & 0x8420;
  if (!half) return (uint16_t)((diff - borrow) & (borrow - (borrow >> 5)));
  return (uint16_t)((((diff - borrow) & (borrow - (borrow >> 5))) & 0x7BDE) >> 1);
}


void snrt_ppu_line(snrt_ppu *p, int y, uint16_t *out) {
  /* the vertical mosaic counter steps at the start of every line */
  if (y == 1) p->mosaic_vcounter = p->mosaic_enable ? p->mosaic_size + 1 : 0;
  if (p->mosaic_vcounter && !--p->mosaic_vcounter) p->mosaic_vcounter = p->mosaic_enable ? p->mosaic_size : 0;
  if (p->bg_mode == 7) for (int k = 0; k < 4; k++) m7_latch(p, k);
  if (p->forced_blank) { memset(out, 0, 256 * sizeof *out); return; }

  static px layers[5][256];
  int mode[4];
  uint8_t pri[5][4];
  layer_modes(p, mode, pri);
  for (int i = 0; i < 4; i++) {
    if (mode[i] == NONE || !((p->tm | p->ts) >> i & 1)) { memset(layers[i], 0, sizeof layers[i]); continue; }
    if (mode[i] == MODE7) draw_mode7(p, pri[i], y, layers[i]);
    else draw_bg(p, i, mode[i], pri[i], y, layers[i]);
  }
  if ((p->tm | p->ts) & 0x10) draw_obj(p, pri[4], y, layers[OBJ]);
  else memset(layers[OBJ], 0, sizeof layers[OBJ]);

  int sub_mode = p->cgadsub >> 7, halve = p->cgadsub >> 6 & 1, blend_sub = p->cgwsel >> 1 & 1;
  for (int x = 0; x < 256; x++) {
    /* each layer on each screen, unless its window hides it there */
    uint8_t mpri = 0, spri = 0;
    uint16_t mcol = p->cgram[0], scol = p->cgram[0];
    int mcolor_en = (p->cgadsub >> 5) & 1;   /* the backdrop's colour math enable */
    for (int l = 0; l < 5; l++) {
      px v = layers[l][x];
      if (!v.pri) continue;
      int win = ((p->tmw | p->tsw) >> l & 1) && window_at(p, l, x);
      if ((p->tm >> l & 1) && !(win && (p->tmw >> l & 1)) && v.pri > mpri) {
        mpri = v.pri; mcol = p->cgram[v.pal];
        mcolor_en = l == OBJ ? ((p->cgadsub >> 4 & 1) && v.pal >= 192) : (p->cgadsub >> l & 1);
      }
      if ((p->ts >> l & 1) && !(win && (p->tsw >> l & 1)) && v.pri > spri) {
        spri = v.pri; scol = p->cgram[v.pal];
      }
    }
    int transparent = spri == 0;
    int cw = window_at(p, COL, x);
    int sel_main = p->cgwsel >> 6, sel_sub = p->cgwsel >> 4 & 3;
    int main_on = sel_main == 0 ? 1 : sel_main == 1 ? cw : sel_main == 2 ? !cw : 0;   /* above.colorEnable */
    int math_on = sel_sub == 0 ? 1 : sel_sub == 1 ? cw : sel_sub == 2 ? !cw : 0;      /* below.colorEnable */
    if (!math_on) mcolor_en = 0;
    if (!mcolor_en) { out[x] = main_on ? mcol : 0; continue; }
    int use_sub = blend_sub, half = halve && main_on;
    if (blend_sub && transparent) { use_sub = 0; half = 0; }
    out[x] = blend(main_on ? mcol : 0, use_sub ? scol : p->fixed_color, sub_mode, half);
  }
}

void snrt_ppu_rgb565(const uint16_t *in, int n, int b, uint16_t *out) {
  for (int i = 0; i < n; i++) {
    int r = in[i] & 31, g = in[i] >> 5 & 31, bl = in[i] >> 10 & 31;
    r = (r * b * 2 + 15) / 30; g = (g * b * 2 + 15) / 30; bl = (bl * b * 2 + 15) / 30;
    out[i] = (uint16_t)(r << 11 | g << 6 | (g & 0x10) << 1 | bl);
  }
}
