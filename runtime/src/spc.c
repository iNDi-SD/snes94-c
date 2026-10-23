/* The sound CPU's bus and I/O around its programs (build/spc_program.inc, made by make). */
#include <string.h>
#include "snrt/spc.h"

void snrt_spc_power(snrt_spc *s, snrt_dsp *dsp) {
  memset(s, 0, sizeof *s);
  s->dsp = dsp;
  memset(dsp->ram, 0, sizeof dsp->ram);
  s->pc = 0xFFC0;
  s->sp = 0xEF;
  s->ipl_enable = 1;
}

void snrt_spc_sync_dsp(snrt_spc *s) {
  if (s->dsp_pending) { snrt_dsp_run(s->dsp, (int)s->dsp_pending); s->dsp_pending = 0; }
}

void snrt_spc_port_write(snrt_spc *s, int port, uint8_t v) { s->port_in[port & 3] = v; }
uint8_t snrt_spc_port_read(const snrt_spc *s, int port) { return s->dsp->ram[0xF4 + (port & 3)]; }

static void tick(snrt_spc *s) { s->clock++; s->dsp_pending++; }

static uint8_t io_read(snrt_spc *s, uint16_t a) {
  switch (a) {
    case 0xF2: return s->dsp_addr;
    case 0xF3: snrt_spc_sync_dsp(s); return snrt_dsp_read(s->dsp, s->dsp_addr & 0x7F);
    case 0xF4: case 0xF5: case 0xF6: case 0xF7: return s->port_in[a & 3];
    case 0xF8: case 0xF9: return s->dsp->ram[a];
  }
  return 0;
}

static void io_write(snrt_spc *s, uint16_t a, uint8_t v) {
  switch (a) {
    case 0xF1:
      s->control = v;
      s->ipl_enable = v >> 7;
      if (v & 0x20) s->port_in[2] = s->port_in[3] = 0;
      if (v & 0x10) s->port_in[0] = s->port_in[1] = 0;
      break;
    case 0xF2: s->dsp_addr = v; break;
    case 0xF3: if (!(s->dsp_addr & 0x80)) { snrt_spc_sync_dsp(s); snrt_dsp_write(s->dsp, s->dsp_addr, v); } break;
  }
}

/* a bus cycle: the read or write happens after the cycle's tick, as snes9x has it */
static uint8_t RD(snrt_spc *s, uint16_t a) {
  tick(s);
  if ((a & 0xFFF0) == 0x00F0) return io_read(s, a);
  return s->dsp->ram[a];   /* (the boot ROM's own bytes are only fetched, never read as data) */
}
static void WR(snrt_spc *s, uint16_t a, uint8_t v) {
  tick(s);
  if ((a & 0xFFF0) == 0x00F0) io_write(s, a, v);
  s->dsp->ram[a] = v;      /* every write reaches RAM, the I/O registers' too */
}
static void IO(snrt_spc *s) { tick(s); }
static uint8_t RPC(snrt_spc *s, uint8_t k) { tick(s); return k; }   /* an operand byte of the listing */

#define DP(a) ((uint16_t)(s->p << 8 | ((a) & 0xFF)))

static uint8_t NZ(snrt_spc *s, uint8_t r) { s->n = r >> 7; s->z = r == 0; return r; }
static void CMP(snrt_spc *s, uint8_t x, uint8_t y) { int r = x - y; s->n = (r >> 7) & 1; s->z = (uint8_t)r == 0; s->c = r >= 0; }
static uint8_t ADC(snrt_spc *s, uint8_t x, uint8_t y) {
  int r = x + y + s->c;
  s->n = (r >> 7) & 1; s->v = (~(x ^ y) & (x ^ r) & 0x80) != 0; s->h = ((x ^ y ^ r) & 0x10) != 0;
  s->z = (uint8_t)r == 0; s->c = r > 0xFF;
  return (uint8_t)r;
}
static uint8_t SBC(snrt_spc *s, uint8_t x, uint8_t y) { return ADC(s, x, (uint8_t)~y); }
static uint8_t ASL(snrt_spc *s, uint8_t x) { s->c = x >> 7; return NZ(s, (uint8_t)(x << 1)); }

static void step(snrt_spc *s) {
#include "spc_program.inc"
}

void snrt_spc_run(snrt_spc *s, uint64_t until) {
  while (s->clock < until && !s->error) step(s);
}
