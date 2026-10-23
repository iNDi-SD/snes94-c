/* DMA and HDMA, after ares's (ISC licence). */
#include <string.h>
#include "snrt/snrt.h"

void snrt_dma_power(snrt_dma *d) {
  memset(d, 0, sizeof *d);
  for (int i = 0; i < 8; i++) { d->ch[i].dmap = 0xFF; d->ch[i].bbad = 0xFF; d->ch[i].unknown = 0xFF; }
  d->active = -1;
}

/* the A bus cannot reach the B bus or the CPU's own registers */
static int valid_a(uint32_t a) {
  return (a & 0x40FF00) != 0x2100 && (a & 0x40FE00) != 0x4000 && (a & 0x40FFE0) != 0x4200 && (a & 0x40FF80) != 0x4300;
}

static uint8_t read_a(snrt *m, uint32_t a) { return valid_a(a) && m->read_a ? m->read_a(m->user, a) : 0; }
static void write_a(snrt *m, uint32_t a, uint8_t v) { if (valid_a(a) && m->write_a) m->write_a(m->user, a, v); }

static void transfer(snrt *m, snrt_dma_channel *c, uint32_t a, int index) {
  uint8_t b = c->bbad;
  switch (c->dmap & 7) {
    case 1: case 5: b += index & 1; break;
    case 3: case 7: b += index >> 1 & 1; break;
    case 4: b += index & 3; break;
  }
  /* WRAM to WRAM through $2180 does nothing */
  int valid = b != 0x80 || ((a & 0xFE0000) != 0x7E0000 && (a & 0x40E000) != 0);
  if (!(c->dmap & 0x80)) {
    uint8_t v = read_a(m, a);
    if (valid) snrt_bwrite(m, 0x2100 | b, v);
  } else {
    uint8_t v = valid ? snrt_bread(m, 0x2100 | b) : 0;
    write_a(m, a, v);
  }
}

static void run_dma(snrt *m, uint8_t enable) {
  for (int i = 0; i < 8; i++) {
    if (!(enable >> i & 1)) continue;
    snrt_dma_channel *c = &m->dma.ch[i];
    m->dma.active = i;
    int index = 0;
    do {
      transfer(m, c, (uint32_t)c->a1b << 16 | c->a1t, index++ & 3);
      if (!(c->dmap & 0x08)) c->a1t += (c->dmap & 0x10) ? -1 : 1;
    } while (--c->das);
  }
  m->dma.active = -1;
}

void snrt_dma_write(snrt *m, uint16_t addr, uint8_t v) {
  if (addr == 0x420B) { run_dma(m, v); return; }
  if (addr == 0x420C) { m->dma.hdmaen = v; return; }
  snrt_dma_channel *c = &m->dma.ch[addr >> 4 & 7];
  switch (addr & 0xF) {
    case 0x0: c->dmap = v; return;
    case 0x1: c->bbad = v; return;
    case 0x2: c->a1t = (c->a1t & 0xFF00) | v; return;
    case 0x3: c->a1t = (c->a1t & 0x00FF) | v << 8; return;
    case 0x4: c->a1b = v; return;
    case 0x5: c->das = (c->das & 0xFF00) | v; return;
    case 0x6: c->das = (c->das & 0x00FF) | v << 8; return;
    case 0x7: c->dasb = v; return;
    case 0x8: c->a2a = (c->a2a & 0xFF00) | v; return;
    case 0x9: c->a2a = (c->a2a & 0x00FF) | v << 8; return;
    case 0xA: c->ntrl = v; return;
    case 0xB: case 0xF: c->unknown = v; return;
  }
}

uint8_t snrt_dma_read(const snrt_dma *d, uint16_t addr) {
  const snrt_dma_channel *c = &d->ch[addr >> 4 & 7];
  switch (addr & 0xF) {
    case 0x0: return c->dmap;
    case 0x1: return c->bbad;
    case 0x2: return (uint8_t)c->a1t;
    case 0x3: return (uint8_t)(c->a1t >> 8);
    case 0x4: return c->a1b;
    case 0x5: return (uint8_t)c->das;
    case 0x6: return (uint8_t)(c->das >> 8);
    case 0x7: return c->dasb;
    case 0x8: return (uint8_t)c->a2a;
    case 0x9: return (uint8_t)(c->a2a >> 8);
    case 0xA: return c->ntrl;
    case 0xB: case 0xF: return c->unknown;
  }
  return 0;
}

static int hdma_active(const snrt_dma *d, int i) { return (d->hdmaen >> i & 1) && !d->ch[i].completed; }

/* no channel after i still runs: a finished indirect channel then reads one byte of its pointer
 * (what is left in its registers matters when HDMA is turned on again without a fresh start) */
static int hdma_finished(const snrt_dma *d, int i) {
  for (int k = i + 1; k < 8; k++) if (hdma_active(d, k)) return 0;
  return 1;
}

static void hdma_reload(snrt *m, int i) {
  snrt_dma_channel *c = &m->dma.ch[i];
  if (c->ntrl & 0x7F) return;   /* (the hardware reads the next byte anyway; nothing comes of it) */
  m->dma.table_read = 1;
  uint8_t data = read_a(m, (uint32_t)c->a1b << 16 | c->a2a);
  {
    c->ntrl = data;
    c->a2a++;
    c->completed = c->ntrl == 0;
    c->do_transfer = !c->completed;
    if (c->dmap & 0x40) {
      data = read_a(m, (uint32_t)c->a1b << 16 | c->a2a++);
      c->das = data << 8;
      if (c->completed && hdma_finished(&m->dma, i)) { m->dma.table_read = 0; return; }
      data = read_a(m, (uint32_t)c->a1b << 16 | c->a2a++);
      c->das = (uint16_t)(data << 8 | c->das >> 8);
    }
  }
  m->dma.table_read = 0;
}

void snrt_hdma_init(snrt *m) {
  m->dma.in_hdma = 1;
  for (int i = 0; i < 8; i++) {
    snrt_dma_channel *c = &m->dma.ch[i];
    c->completed = 0;
    c->do_transfer = m->dma.hdmaen >> i & 1;   /* snes9x clears it for a channel not on (ares sets it) */
    if (!c->do_transfer) continue;
    c->a2a = c->a1t;
    c->ntrl = 0;
    m->dma.active = i;
    hdma_reload(m, i);
  }
  m->dma.active = -1;
  m->dma.in_hdma = 0;
}

void snrt_hdma_line(snrt *m) {
  static const int lengths[8] = { 1, 2, 2, 4, 4, 4, 2, 4 };
  m->dma.in_hdma = 1;
  for (int i = 0; i < 8; i++) {
    snrt_dma_channel *c = &m->dma.ch[i];
    if (!hdma_active(&m->dma, i) || !c->do_transfer) continue;
    m->dma.active = i;
    for (int k = 0; k < lengths[c->dmap & 7]; k++) {
      uint32_t a = !(c->dmap & 0x40) ? (uint32_t)c->a1b << 16 | c->a2a++ : (uint32_t)c->dasb << 16 | c->das++;
      transfer(m, c, a, k);
    }
  }
  for (int i = 0; i < 8; i++) {
    snrt_dma_channel *c = &m->dma.ch[i];
    if (!hdma_active(&m->dma, i)) continue;
    m->dma.active = i;
    c->ntrl--;
    c->do_transfer = c->ntrl >> 7;
    hdma_reload(m, i);
  }
  m->dma.active = -1;
  m->dma.in_hdma = 0;
}
