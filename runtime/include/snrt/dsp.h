/* The S-DSP: eight BRR voices, ADSR/GAIN envelopes, Gaussian interpolation, noise, pitch
 * modulation and echo with its FIR filter, one stereo sample every 32 clocks (32 kHz), run a
 * clock at a time. After ares's DSP (ISC licence), which follows the hardware's clock-by-clock
 * schedule; power-on state as snes9x's (all internal registers 0 but FLG $E0 and the pitch of
 * voices 4 and 5). The sound CPU's 64 KB RAM lives here: samples, the sample directory and the
 * echo buffer. */
#pragma once
#include <stdint.h>

typedef struct snrt_dsp_voice {
  int8_t volume[2];
  uint16_t pitch;             /* 14 bits */
  uint8_t source, adsr0, adsr1, gain, envx;
  uint8_t keyon, keyoff, modulate, noise, echo, end;
  int16_t buffer[12];
  uint8_t buffer_offset;      /* 0-11 */
  uint16_t gaussian_offset;
  uint16_t brr_address;
  uint8_t brr_offset;         /* 1-8 */
  uint8_t keyon_delay;        /* 0-5 */
  uint8_t envelope_mode;
  uint16_t envelope;          /* 0-2047 */
  int32_t envelope_raw;       /* GAIN mode 7's quirk */
  uint8_t keylatch, keyon_l, keyoff_l, modulate_l, noise_l, echo_l, end_l, looped;
} snrt_dsp_voice;

typedef struct snrt_dsp {
  uint8_t ram[0x10000];
  uint8_t regs[128];          /* as the sound CPU reads them back */
  int phase;                  /* 0-31: the clock within the sample */
  uint16_t counter;           /* 15 bits */
  uint8_t sample_clock;
  uint8_t main_reset, main_mute;
  int8_t main_volume[2];
  int32_t main_output[2];
  int8_t echo_feedback, echo_volume[2], fir[8];
  int16_t history[2][8];
  uint8_t echo_page, echo_delay, echo_readonly;
  int32_t echo_input[2], echo_output[2];
  uint8_t echo_page_l, echo_readonly_l, history_offset;
  uint16_t echo_address, echo_offset, echo_length;
  uint8_t noise_frequency;
  uint16_t lfsr;
  uint8_t brr_bank, brr_bank_l, brr_source, brr_header, brr_byte;
  uint16_t brr_address, brr_next_address;
  uint8_t latch_adsr0, latch_envx, latch_outx;
  uint16_t latch_pitch;       /* 15 bits */
  int16_t latch_output;
  snrt_dsp_voice voice[8];
  void (*sample)(void *user, int16_t left, int16_t right);
  void *user;
} snrt_dsp;

void snrt_dsp_power(snrt_dsp *d);
void snrt_dsp_write(snrt_dsp *d, uint8_t addr, uint8_t v);   /* $00-$7F */
uint8_t snrt_dsp_read(const snrt_dsp *d, uint8_t addr);
void snrt_dsp_run(snrt_dsp *d, int clocks);                   /* 32 clocks: one sample */
