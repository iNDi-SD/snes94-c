/* The SNES runtime: the console and its beam. */
#include <string.h>
#include "snrt/snrt.h"

void snrt_power(snrt *m) {
  void (*frame_done)(snrt *, void *) = m->frame_done;
  void *user = m->user;
  uint8_t (*read_a)(void *, uint32_t) = m->read_a;
  void (*write_a)(void *, uint32_t, uint8_t) = m->write_a;
  memset(m, 0, sizeof *m);
  m->frame_done = frame_done; m->user = user; m->read_a = read_a; m->write_a = write_a;
  snrt_ppu_power(&m->ppu);
  snrt_dma_power(&m->dma);
  snrt_io_power(&m->io);
  m->line = 225;   /* power on lands in the vertical blank, where snes9x's first frame begins */
}

static void draw(snrt *m, int y) {
  uint16_t row[SNRT_WIDTH];
  m->ppu.vline = m->line;
  snrt_ppu_line(&m->ppu, y, row);
  int b = m->ppu.forced_blank ? 0 : m->ppu.brightness;
  snrt_ppu_rgb565(row, SNRT_WIDTH, b, m->picture + (y - 1) * SNRT_WIDTH);
}

/* what happens at (line, cycle) */
static void event(snrt *m, int line, int cycle) {
  if (cycle == SNRT_DRAW_CYCLE && line >= 1 && line <= SNRT_HEIGHT) draw(m, line);
  else if (cycle == SNRT_HDMA_INIT_CYCLE && line == 0) snrt_hdma_init(m);
  else if (cycle == SNRT_HDMA_CYCLE && line <= SNRT_HEIGHT) snrt_hdma_line(m);
  else if (cycle == 0) {
    snrt_ppu_newline(&m->ppu, line);
    if (line == 225) {
      snrt_ppu_vblank(&m->ppu);
      if (m->frame_done) m->frame_done(m, m->user);
    }
    snrt_io_line(m, line);
  }
}

/* an event at cycle E happens as the beam moves through (from, to]: a write at cycle E comes after it */
void snrt_beam(snrt *m, int line, int cycle) {
  static const int at[] = { SNRT_HDMA_INIT_CYCLE, SNRT_DRAW_CYCLE, SNRT_HDMA_CYCLE };
  for (;;) {
    int here = m->line == line && cycle >= m->cycle;
    int to = here ? cycle : SNRT_LINE_CYCLES - 1;
    int irq = snrt_io_irq_cycle(&m->io, m->line);
    for (unsigned k = 0; k < sizeof at / sizeof *at; k++) {
      if (irq >= 0 && irq < at[k] && irq > m->cycle && irq <= to) { snrt_io_irq(m); irq = -1; }
      if (at[k] > m->cycle && at[k] <= to) event(m, m->line, at[k]);
    }
    if (irq > m->cycle && irq <= to) snrt_io_irq(m);
    if (here) { m->cycle = cycle; break; }
    m->line = m->line == SNRT_LINES - 1 ? 0 : m->line + 1;
    m->cycle = 0;
    event(m, m->line, 0);
  }
  m->ppu.vline = m->line;
}

void snrt_bwrite(snrt *m, uint16_t addr, uint8_t v) {
  m->ppu.vline = m->line;
  if (addr >= 0x2100 && addr <= 0x2133) snrt_ppu_write(&m->ppu, addr, v);
}

uint8_t snrt_read(snrt *m, uint16_t addr, int cycle) {
  if (addr == 0x2180 || addr == 0x4016 || addr == 0x4017 || (addr >= 0x4210 && addr <= 0x421F)) return snrt_io_read(m, addr, cycle < 0 ? m->cycle : cycle);
  if (addr >= 0x4300 && addr <= 0x437F) return snrt_dma_read(&m->dma, addr);
  return snrt_bread(m, addr);
}

uint8_t snrt_bread(snrt *m, uint16_t addr) {
  if ((addr >= 0x2134 && addr <= 0x2136) || addr == 0x213E || addr == 0x213F) return snrt_ppu_read(&m->ppu, addr);
  return 0;
}

void snrt_write(snrt *m, uint16_t addr, uint8_t v) {
  if ((addr >= 0x2180 && addr <= 0x2183) || addr == 0x4016 || (addr >= 0x4200 && addr <= 0x420A)) snrt_io_write(m, addr, v);
  else if (addr >= 0x2100 && addr <= 0x21FF) snrt_bwrite(m, addr, v);
  else if (addr == 0x420B || addr == 0x420C || (addr >= 0x4300 && addr <= 0x437F)) snrt_dma_write(m, addr, v);
}
