/* The S-DSP, after ares's (ISC licence): the same clock-by-clock schedule, with ares's n-bit
 * fields kept to their widths by hand. */
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <string.h>
#include "snrt/dsp.h"

enum { RELEASE, ATTACK, DECAY, SUSTAIN };

static int16_t gauss[512];

static void gauss_table(void) {
  double t[512];
  for (int n = 0; n < 512; n++) {
    double k = 0.5 + n;
    double s = sin(M_PI * k * 1.280 / 1024);
    double a = (cos(M_PI * k * 2.000 / 1023) - 1) * 0.50;
    double b = (cos(M_PI * k * 4.000 / 1023) - 1) * 0.08;
    t[511 - n] = s * (a + b + 1.0) / k;
  }
  for (int ph = 0; ph < 128; ph++) {
    double scale = 2048.0 / (t[ph] + t[ph + 256] + t[511 - ph] + t[255 - ph]);
    int idx[4] = { ph, ph + 256, 511 - ph, 255 - ph };
    for (int k = 0; k < 4; k++) gauss[idx[k]] = (int16_t)(t[idx[k]] * scale + 0.5);
  }
}

static int32_t clamp16(int32_t x) { return x < -32768 ? -32768 : x > 32767 ? 32767 : x; }

void snrt_dsp_power(snrt_dsp *d) {
  void (*sample)(void *, int16_t, int16_t) = d->sample;
  void *user = d->user;
  memset(d, 0, sizeof *d);
  d->sample = sample; d->user = user;
  if (!gauss[256]) gauss_table();
  d->sample_clock = 1;
  d->main_reset = d->main_mute = d->echo_readonly = 1;   /* FLG $E0 */
  d->regs[0x6C] = 0xE0;
  d->lfsr = 0x4000;
  for (int i = 0; i < 8; i++) d->voice[i].brr_offset = 1;
  d->voice[4].pitch = d->voice[5].pitch = 1;   /* snes9x powers on with these */
}

uint8_t snrt_dsp_read(const snrt_dsp *d, uint8_t addr) { return d->regs[addr & 0x7F]; }

void snrt_dsp_write(snrt_dsp *d, uint8_t addr, uint8_t v) {
  addr &= 0x7F;
  d->regs[addr] = v;
  switch (addr) {
    case 0x0C: d->main_volume[0] = (int8_t)v; break;
    case 0x1C: d->main_volume[1] = (int8_t)v; break;
    case 0x2C: d->echo_volume[0] = (int8_t)v; break;
    case 0x3C: d->echo_volume[1] = (int8_t)v; break;
    case 0x4C: for (int n = 0; n < 8; n++) d->voice[n].keyon = d->voice[n].keylatch = v >> n & 1; break;
    case 0x5C: for (int n = 0; n < 8; n++) d->voice[n].keyoff = v >> n & 1; break;
    case 0x6C:
      d->noise_frequency = v & 31; d->echo_readonly = v >> 5 & 1;
      d->main_mute = v >> 6 & 1; d->main_reset = v >> 7;
      break;
    case 0x7C: for (int n = 0; n < 8; n++) d->voice[n].end_l = 0; d->regs[0x7C] = 0; break;
    case 0x0D: d->echo_feedback = (int8_t)v; break;
    case 0x2D: for (int n = 0; n < 8; n++) d->voice[n].modulate = v >> n & 1; d->voice[0].modulate = 0; break;
    case 0x3D: for (int n = 0; n < 8; n++) d->voice[n].noise = v >> n & 1; break;
    case 0x4D: for (int n = 0; n < 8; n++) d->voice[n].echo = v >> n & 1; break;
    case 0x5D: d->brr_bank = v; break;
    case 0x6D: d->echo_page = v; break;
    case 0x7D: d->echo_delay = v & 15; break;
  }
  snrt_dsp_voice *vc = &d->voice[addr >> 4];
  switch (addr & 15) {
    case 0x0: vc->volume[0] = (int8_t)v; break;
    case 0x1: vc->volume[1] = (int8_t)v; break;
    case 0x2: vc->pitch = (vc->pitch & 0x3F00) | v; break;
    case 0x3: vc->pitch = (uint16_t)((vc->pitch & 0x00FF) | (v & 0x3F) << 8); break;
    case 0x4: vc->source = v; break;
    case 0x5: vc->adsr0 = v; break;
    case 0x6: vc->adsr1 = v; break;
    case 0x7: vc->gain = v; break;
    case 0x8: d->latch_envx = v; break;
    case 0x9: d->latch_outx = v; break;
    case 0xF: d->fir[addr >> 4] = (int8_t)v; break;
  }
}

/* ---- counter ---- */

static const uint16_t rates[32] = { 0, 2048, 1536, 1280, 1024, 768, 640, 512, 384, 320, 256, 192, 160, 128, 96, 80,
  64, 48, 40, 32, 24, 20, 16, 12, 10, 8, 6, 5, 4, 3, 2, 1 };
static const uint16_t offsets[32] = { 0, 0, 1040, 536, 0, 1040, 536, 0, 1040, 536, 0, 1040, 536, 0, 1040, 536,
  0, 1040, 536, 0, 1040, 536, 0, 1040, 536, 0, 1040, 536, 0, 1040, 0, 0 };

static void counter_tick(snrt_dsp *d) { if (!d->counter) d->counter = 30720; d->counter--; }
static int counter_poll(const snrt_dsp *d, int rate) { return rate && (d->counter + offsets[rate]) % rates[rate] == 0; }

/* ---- envelope ---- */

static void envelope_run(snrt_dsp *d, snrt_dsp_voice *v) {
  int32_t env = v->envelope, rate, data = v->adsr1;
  if (v->envelope_mode == RELEASE) {
    env -= 8;
    if (env < 0) env = 0;
    v->envelope = (uint16_t)env;
    return;
  }
  if (d->latch_adsr0 & 0x80) {
    if (v->envelope_mode >= DECAY) {
      env--;
      env -= env >> 8;
      rate = data & 0x1F;
      if (v->envelope_mode == DECAY) rate = (d->latch_adsr0 >> 4 & 7) * 2 + 16;
    } else {
      rate = (d->latch_adsr0 & 15) * 2 + 1;
      env += rate < 31 ? 0x20 : 0x400;
    }
  } else {
    data = v->gain;
    int mode = data >> 5;
    if (mode < 4) { env = data << 4; rate = 31; }
    else {
      rate = data & 0x1F;
      if (mode == 4) env -= 0x20;
      else if (mode < 6) { env--; env -= env >> 8; }
      else {
        env += 0x20;
        if (mode > 6 && (uint32_t)v->envelope_raw >= 0x600) env += 0x8 - 0x20;
      }
    }
  }
  if ((env >> 8) == (data >> 5) && v->envelope_mode == DECAY) v->envelope_mode = SUSTAIN;
  v->envelope_raw = env;
  if ((uint32_t)env > 0x7FF) {
    env = env < 0 ? 0 : 0x7FF;
    if (v->envelope_mode == ATTACK) v->envelope_mode = DECAY;
  }
  if (counter_poll(d, rate)) v->envelope = (uint16_t)env;
}

/* ---- BRR ---- */

static void brr_decode(snrt_dsp *d, snrt_dsp_voice *v) {
  int32_t nybbles = d->brr_byte << 8 | d->ram[(uint16_t)(v->brr_address + v->brr_offset + 1)];
  int filter = d->brr_header >> 2 & 3, scale = d->brr_header >> 4;
  for (int n = 0; n < 4; n++) {
    int32_t s = (int16_t)nybbles >> 12;
    nybbles <<= 4;
    if (scale <= 12) { s <<= scale; s >>= 1; }
    else s &= ~0x7FF;
    int off = v->buffer_offset;
    if (--off < 0) off = 11;
    int32_t p1 = v->buffer[off];
    if (--off < 0) off = 11;
    int32_t p2 = v->buffer[off] >> 1;
    switch (filter) {
      case 1: s += p1 >> 1; s += (-p1) >> 5; break;
      case 2: s += p1; s -= p2; s += p2 >> 4; s += (p1 * -3) >> 6; break;
      case 3: s += p1; s -= p2; s += (p1 * -13) >> 7; s += (p2 * 3) >> 4; break;
    }
    s = clamp16(s);
    s = (int16_t)(s << 1);
    v->buffer[v->buffer_offset] = (int16_t)s;
    if (++v->buffer_offset >= 12) v->buffer_offset = 0;
  }
}

static int32_t gaussian(const snrt_dsp_voice *v) {
  uint8_t off = (uint8_t)(v->gaussian_offset >> 4);
  const int16_t *fwd = gauss + 255 - off, *rev = gauss + off;
  int i = (v->buffer_offset + (v->gaussian_offset >> 12)) % 12;
  int32_t out;
  out  = fwd[0]   * v->buffer[i] >> 11; if (++i >= 12) i = 0;
  out += fwd[256] * v->buffer[i] >> 11; if (++i >= 12) i = 0;
  out += rev[256] * v->buffer[i] >> 11; if (++i >= 12) i = 0;
  out = (int16_t)out;
  out += rev[0] * v->buffer[i] >> 11;
  return clamp16(out) & ~1;
}

/* ---- voices ---- */

static void voice_output(snrt_dsp *d, snrt_dsp_voice *v, int ch) {
  int32_t amp = d->latch_output * v->volume[ch] >> 7;
  d->main_output[ch] = clamp16(d->main_output[ch] + amp);
  if (v->echo_l) d->echo_output[ch] = clamp16(d->echo_output[ch] + amp);
}

static void voice1(snrt_dsp *d, snrt_dsp_voice *v) {
  d->brr_address = (uint16_t)((d->brr_bank_l << 8) + (d->brr_source << 2));
  d->brr_source = v->source;
}

static void voice2(snrt_dsp *d, snrt_dsp_voice *v) {
  uint16_t a = d->brr_address;
  if (!v->keyon_delay) a += 2;
  d->brr_next_address = (uint16_t)(d->ram[a] | d->ram[(uint16_t)(a + 1)] << 8);
  d->latch_adsr0 = v->adsr0;
  d->latch_pitch = v->pitch & 0xFF;
}

static void voice3a(snrt_dsp *d, snrt_dsp_voice *v) { d->latch_pitch |= v->pitch & ~0xFF; }

static void voice3b(snrt_dsp *d, snrt_dsp_voice *v) {
  d->brr_byte = d->ram[(uint16_t)(v->brr_address + v->brr_offset)];
  d->brr_header = d->ram[v->brr_address];
}

static void voice3c(snrt_dsp *d, snrt_dsp_voice *v) {
  if (v->modulate_l) d->latch_pitch = (uint16_t)((d->latch_pitch + ((d->latch_output >> 5) * d->latch_pitch >> 10)) & 0x7FFF);
  if (v->keyon_delay) {
    if (v->keyon_delay == 5) {
      v->brr_address = d->brr_next_address;
      v->brr_offset = 1;
      v->buffer_offset = 0;
      d->brr_header = 0;
    }
    v->envelope = 0;
    v->envelope_raw = 0;
    v->gaussian_offset = 0;
    v->keyon_delay--;
    if (v->keyon_delay & 3) v->gaussian_offset = 0x4000;
    d->latch_pitch = 0;
  }
  int32_t out = gaussian(v);
  if (v->noise_l) out = (int16_t)(d->lfsr << 1);
  d->latch_output = (int16_t)((out * v->envelope >> 11) & ~1);
  v->envx = (uint8_t)(v->envelope >> 4);
  if (d->main_reset || (d->brr_header & 3) == 1) { v->envelope_mode = RELEASE; v->envelope = 0; }
  if (d->sample_clock) {
    if (v->keyoff_l) v->envelope_mode = RELEASE;
    if (v->keyon_l) { v->keyon_delay = 5; v->envelope_mode = ATTACK; }
  }
  if (!v->keyon_delay) envelope_run(d, v);
}

static void voice3(snrt_dsp *d, snrt_dsp_voice *v) { voice3a(d, v); voice3b(d, v); voice3c(d, v); }

static void voice4(snrt_dsp *d, snrt_dsp_voice *v) {
  v->looped = 0;
  if (v->gaussian_offset >= 0x4000) {
    brr_decode(d, v);
    v->brr_offset += 2;
    if (v->brr_offset >= 9) {
      v->brr_address = (uint16_t)(v->brr_address + 9);
      if (d->brr_header & 1) { v->brr_address = d->brr_next_address; v->looped = 1; }
      v->brr_offset = 1;
    }
  }
  uint32_t g = (v->gaussian_offset & 0x3FFF) + d->latch_pitch;
  v->gaussian_offset = (uint16_t)(g > 0x7FFF ? 0x7FFF : g);
  voice_output(d, v, 0);
}

static void voice5(snrt_dsp *d, snrt_dsp_voice *v) {
  voice_output(d, v, 1);
  v->end_l |= v->looped;
  if (v->keyon_delay == 5) v->end_l = 0;
}

static void voice6(snrt_dsp *d, snrt_dsp_voice *v) { (void)v; d->latch_outx = (uint8_t)(d->latch_output >> 8); }

static void voice7(snrt_dsp *d, snrt_dsp_voice *v) {
  uint8_t endx = 0;
  for (int n = 0; n < 8; n++) endx |= d->voice[n].end_l << n;
  d->regs[0x7C] = endx;
  d->latch_envx = v->envx;
}

static void voice8(snrt_dsp *d, snrt_dsp_voice *v) { d->regs[(v - d->voice) << 4 | 9] = d->latch_outx; }
static void voice9(snrt_dsp *d, snrt_dsp_voice *v) { d->regs[(v - d->voice) << 4 | 8] = d->latch_envx; }

/* ---- echo ---- */

static int32_t fir(const snrt_dsp *d, int ch, int i) {
  return d->history[ch][(d->history_offset + i + 1) & 7] * d->fir[i] >> 6;
}

static int16_t echo_mix(const snrt_dsp *d, int ch) {
  int16_t main = (int16_t)(d->main_output[ch] * d->main_volume[ch] >> 7);
  int16_t echo = (int16_t)(d->echo_input[ch] * d->echo_volume[ch] >> 7);
  return (int16_t)clamp16(main + echo);
}

static void echo_read(snrt_dsp *d, int ch) {
  uint16_t a = (uint16_t)(d->echo_address + ch * 2);
  int32_t s = (int16_t)(d->ram[(uint16_t)(a + 1)] << 8 | d->ram[a]);
  d->history[ch][d->history_offset] = (int16_t)(s >> 1);
}

static void echo_write(snrt_dsp *d, int ch) {
  if (!d->echo_readonly_l) {
    uint16_t a = (uint16_t)(d->echo_address + ch * 2);
    d->ram[a] = (uint8_t)d->echo_output[ch];
    d->ram[(uint16_t)(a + 1)] = (uint8_t)(d->echo_output[ch] >> 8);
  }
  d->echo_output[ch] = 0;
}

static void echo22(snrt_dsp *d) {
  d->history_offset = (d->history_offset + 1) & 7;
  d->echo_address = (uint16_t)((d->echo_page_l << 8) + d->echo_offset);
  echo_read(d, 0);
  d->echo_input[0] = fir(d, 0, 0);
  d->echo_input[1] = fir(d, 1, 0);
}

static void echo23(snrt_dsp *d) {
  d->echo_input[0] += fir(d, 0, 1) + fir(d, 0, 2);
  d->echo_input[1] += fir(d, 1, 1) + fir(d, 1, 2);
  echo_read(d, 1);
}

static void echo24(snrt_dsp *d) {
  d->echo_input[0] += fir(d, 0, 3) + fir(d, 0, 4) + fir(d, 0, 5);
  d->echo_input[1] += fir(d, 1, 3) + fir(d, 1, 4) + fir(d, 1, 5);
}

static void echo25(snrt_dsp *d) {
  for (int ch = 0; ch < 2; ch++) {
    int32_t s = (int16_t)(d->echo_input[ch] + fir(d, ch, 6));
    s += (int16_t)fir(d, ch, 7);
    d->echo_input[ch] = clamp16(s) & ~1;
  }
}

static void echo26(snrt_dsp *d) {
  d->main_output[0] = echo_mix(d, 0);
  for (int ch = 0; ch < 2; ch++) {
    int32_t s = d->echo_output[ch] + (int16_t)(d->echo_input[ch] * d->echo_feedback >> 7);
    d->echo_output[ch] = clamp16(s) & ~1;
  }
}

static void echo27(snrt_dsp *d) {
  int32_t l = d->main_output[0], r = echo_mix(d, 1);
  d->main_output[0] = d->main_output[1] = 0;
  if (d->main_mute) l = r = 0;
  if (d->sample) d->sample(d->user, (int16_t)l, (int16_t)r);
}

static void echo28(snrt_dsp *d) { d->echo_readonly_l = d->echo_readonly; }

static void echo29(snrt_dsp *d) {
  d->echo_page_l = d->echo_page;
  if (!d->echo_offset) d->echo_length = (uint16_t)(d->echo_delay << 11);
  d->echo_offset += 4;
  if (d->echo_offset >= d->echo_length) d->echo_offset = 0;
  echo_write(d, 0);
  d->echo_readonly_l = d->echo_readonly;
}

static void echo30(snrt_dsp *d) { echo_write(d, 1); }

/* ---- the other per-sample steps ---- */

static void misc27(snrt_dsp *d) { for (int n = 0; n < 8; n++) d->voice[n].modulate_l = d->voice[n].modulate; }

static void misc28(snrt_dsp *d) {
  for (int n = 0; n < 8; n++) { d->voice[n].noise_l = d->voice[n].noise; d->voice[n].echo_l = d->voice[n].echo; }
  d->brr_bank_l = d->brr_bank;
}

static void misc29(snrt_dsp *d) {
  d->sample_clock ^= 1;
  if (d->sample_clock) for (int n = 0; n < 8; n++) d->voice[n].keylatch &= !d->voice[n].keyon_l;
}

static void misc30(snrt_dsp *d) {
  if (d->sample_clock)
    for (int n = 0; n < 8; n++) { d->voice[n].keyon_l = d->voice[n].keylatch; d->voice[n].keyoff_l = d->voice[n].keyoff; }
  counter_tick(d);
  if (counter_poll(d, d->noise_frequency)) {
    int32_t feedback = d->lfsr << 13 ^ d->lfsr << 14;
    d->lfsr = (uint16_t)((feedback & 0x4000) | d->lfsr >> 1);
  }
}

/* one clock: ares's DSP::main between two ticks */
static void step(snrt_dsp *d) {
  snrt_dsp_voice *v = d->voice;
  switch (d->phase) {
    case 0:  voice5(d, &v[0]); voice2(d, &v[1]); break;
    case 1:  voice6(d, &v[0]); voice3(d, &v[1]); break;
    case 2:  voice7(d, &v[0]); voice4(d, &v[1]); voice1(d, &v[3]); break;
    case 3:  voice8(d, &v[0]); voice5(d, &v[1]); voice2(d, &v[2]); break;
    case 4:  voice9(d, &v[0]); voice6(d, &v[1]); voice3(d, &v[2]); break;
    case 5:  voice7(d, &v[1]); voice4(d, &v[2]); voice1(d, &v[4]); break;
    case 6:  voice8(d, &v[1]); voice5(d, &v[2]); voice2(d, &v[3]); break;
    case 7:  voice9(d, &v[1]); voice6(d, &v[2]); voice3(d, &v[3]); break;
    case 8:  voice7(d, &v[2]); voice4(d, &v[3]); voice1(d, &v[5]); break;
    case 9:  voice8(d, &v[2]); voice5(d, &v[3]); voice2(d, &v[4]); break;
    case 10: voice9(d, &v[2]); voice6(d, &v[3]); voice3(d, &v[4]); break;
    case 11: voice7(d, &v[3]); voice4(d, &v[4]); voice1(d, &v[6]); break;
    case 12: voice8(d, &v[3]); voice5(d, &v[4]); voice2(d, &v[5]); break;
    case 13: voice9(d, &v[3]); voice6(d, &v[4]); voice3(d, &v[5]); break;
    case 14: voice7(d, &v[4]); voice4(d, &v[5]); voice1(d, &v[7]); break;
    case 15: voice8(d, &v[4]); voice5(d, &v[5]); voice2(d, &v[6]); break;
    case 16: voice9(d, &v[4]); voice6(d, &v[5]); voice3(d, &v[6]); break;
    case 17: voice1(d, &v[0]); voice7(d, &v[5]); voice4(d, &v[6]); break;
    case 18: voice8(d, &v[5]); voice5(d, &v[6]); voice2(d, &v[7]); break;
    case 19: voice9(d, &v[5]); voice6(d, &v[6]); voice3(d, &v[7]); break;
    case 20: voice1(d, &v[1]); voice7(d, &v[6]); voice4(d, &v[7]); break;
    case 21: voice8(d, &v[6]); voice5(d, &v[7]); voice2(d, &v[0]); break;
    case 22: voice3a(d, &v[0]); voice9(d, &v[6]); voice6(d, &v[7]); echo22(d); break;
    case 23: voice7(d, &v[7]); echo23(d); break;
    case 24: voice8(d, &v[7]); echo24(d); break;
    case 25: voice3b(d, &v[0]); voice9(d, &v[7]); echo25(d); break;
    case 26: echo26(d); break;
    case 27: misc27(d); echo27(d); break;
    case 28: misc28(d); echo28(d); break;
    case 29: misc29(d); echo29(d); break;
    case 30: misc30(d); voice3c(d, &v[0]); echo30(d); break;
    case 31: voice4(d, &v[0]); voice1(d, &v[2]); break;
  }
  d->phase = (d->phase + 1) & 31;
}

void snrt_dsp_run(snrt_dsp *d, int clocks) {
  while (clocks-- > 0) step(d);
}
