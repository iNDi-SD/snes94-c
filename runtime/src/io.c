/* The CPU's own registers and the WRAM port, as snes9x behaves. */
#include <string.h>
#include "snrt/snrt.h"

#define VERSION_5A22 2

void snrt_io_power(snrt_io *io) {
  memset(io, 0, sizeof *io);
  io->htime = io->vtime = 0x1FF;
}

int snrt_io_irq_cycle(const snrt_io *io, int line) {
  int h = io->nmitimen >> 4 & 1, v = io->nmitimen >> 5 & 1;
  if (!h && !v) return -1;
  if (v && (io->vtime >= SNRT_LINES || line != io->vtime)) return -1;
  if (!h) return 14 - 4;
  int c = io->htime * 4 + 14 - (io->htime ? 0 : 4) + (io->htime > 322 ? 2 : 0) + (io->htime > 326 ? 2 : 0);
  return c < SNRT_LINE_CYCLES ? c : -1;
}

void snrt_io_irq(snrt *m) {
  m->io.irq_flag = 1;
}

void snrt_io_line(snrt *m, int line) {
  snrt_io *io = &m->io;
  if (line == 0) io->nmi_flag = 0;
  if (line == 225) {
    io->nmi_flag = 1;
  }
  if (line == 227 && (io->nmitimen & 1)) {   /* auto-read: port 1's pad, port 2's pad or the multitap's pair $4201 picks */
    int hi = !io->multitap || (io->wrio & 0x80);
    io->joy[0] = io->pad[0]; io->joy[2] = 0;
    io->joy[1] = io->pad[hi ? 1 : 3]; io->joy[3] = io->multitap ? io->pad[hi ? 2 : 4] : 0;
    io->read_idx[0] = 16;
    io->read_idx[1] = hi ? 16 : 0; io->read_idx[2] = hi ? 0 : 16;
  }
}

void snrt_io_write(snrt *m, uint16_t addr, uint8_t v) {
  snrt_io *io = &m->io;
  switch (addr) {
    case 0x2180:
      if (m->write_a) m->write_a(m->user, 0x7E0000 + io->wmadd, v);
      io->wmadd = (io->wmadd + 1) & 0x1FFFF;
      return;
    case 0x2181: io->wmadd = (io->wmadd & 0x1FF00) | v; return;
    case 0x2182: io->wmadd = (io->wmadd & 0x100FF) | v << 8; return;
    case 0x2183: io->wmadd = (io->wmadd & 0x0FFFF) | (v & 1) << 16; return;
    case 0x4016:
      if ((v & 1) && !io->latch) io->read_idx[0] = io->read_idx[1] = io->read_idx[2] = 0;
      io->latch = v & 1;
      return;
    case 0x4200: {
      io->nmitimen = v;
      if (!(v & 0x30)) io->irq_flag = 0;
      return;
    }
    case 0x4201: io->wrio = v; return;
    case 0x4202: io->wrmpya = v; return;
    case 0x4203: io->rdmpy = (uint16_t)(io->wrmpya * v); return;
    case 0x4204: io->wrdiv = (io->wrdiv & 0xFF00) | v; return;
    case 0x4205: io->wrdiv = (io->wrdiv & 0x00FF) | v << 8; return;
    case 0x4206:
      io->rddiv = v ? io->wrdiv / v : 0xFFFF;
      io->rdmpy = v ? io->wrdiv % v : io->wrdiv;
      return;
    case 0x4207: io->htime = (io->htime & 0x100) | v; return;
    case 0x4208: io->htime = (io->htime & 0x0FF) | (v & 1) << 8; return;
    case 0x4209: io->vtime = (io->vtime & 0x100) | v; return;
    case 0x420A: io->vtime = (io->vtime & 0x0FF) | (v & 1) << 8; return;
  }
}

/* the multitap (as snes9x's): latched, its signature (data 2 high); else a bit of each of the pair
 * $4201 bit 7 picks, 1s after the 16th */
static uint8_t tap(snrt_io *io) {
  if (io->latch) return 0x1C | 2;
  int hi = (io->wrio & 0x80) != 0;
  uint8_t *idx = &io->read_idx[hi ? 1 : 2];
  int r = *idx;
  if (*idx < 255) (*idx)++;
  const uint16_t *p = &io->pad[hi ? 1 : 3];
  return 0x1C | (r >= 16 ? 3 : (p[0] >> (15 - r) & 1) | (p[1] >> (15 - r) & 1) << 1);
}

static uint8_t serial(snrt_io *io, int n) {
  if (n && io->multitap) return tap(io);
  uint8_t bits = n ? 0x1C : 0;
  if (io->latch) return bits | (io->pad[n] >> 15);
  int r = io->read_idx[n] < 16 ? io->read_idx[n]++ : 16;
  return bits | (r >= 16 ? 1 : (io->pad[n] >> (15 - r) & 1));
}

uint8_t snrt_io_read(snrt *m, uint16_t addr, int cycle) {
  snrt_io *io = &m->io;
  uint8_t v;
  switch (addr) {
    case 0x2180:
      v = m->read_a ? m->read_a(m->user, 0x7E0000 + io->wmadd) : 0;
      io->wmadd = (io->wmadd + 1) & 0x1FFFF;
      return v;
    case 0x4016: return serial(io, 0);
    case 0x4017: return serial(io, 1);
    case 0x4210: v = (uint8_t)(io->nmi_flag << 7 | VERSION_5A22); io->nmi_flag = 0; return v;
    case 0x4211: v = (uint8_t)(io->irq_flag << 7); io->irq_flag = 0; return v;
    case 0x4212:
      return (uint8_t)((m->line >= 225 && m->line < 228 ? 1 : 0) | (cycle < 4 || cycle >= 1096 ? 0x40 : 0) | (m->line >= 225 ? 0x80 : 0));
    case 0x4213: return io->wrio;
    case 0x4214: return (uint8_t)io->rddiv;
    case 0x4215: return (uint8_t)(io->rddiv >> 8);
    case 0x4216: return (uint8_t)io->rdmpy;
    case 0x4217: return (uint8_t)(io->rdmpy >> 8);
  }
  if (addr >= 0x4218 && addr <= 0x421F) return (uint8_t)(io->joy[(addr - 0x4218) >> 1] >> ((addr & 1) * 8));
  return 0;
}
