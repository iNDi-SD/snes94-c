/* The sound CPU's programs, NHL '94's driver (SoundDriver, uploaded to $0400) and the boot ROM
 * that receives it, ported to C a bus cycle at a time: each instruction is C made from the
 * listing (build/spc_program.inc, made from the player's ROM by tools/gen_spc.py), its cycles in the order
 * snes9x's SPC700 makes them. The runtime steps it as snes9x does (an instruction's fetch with its
 * first cycle, then a cycle a step), so port and DSP accesses fall on snes9x's cycles. Its RAM is the
 * DSP's; the DSP is caught up lazily, as in snes9x: at each DSP register access and when the
 * client asks (snes9x: the end of every line). Timers are not kept: the driver never reads them. */
#pragma once
#include <stdint.h>
#include "snrt/dsp.h"

typedef struct snrt_spc {
  uint8_t a, x, y, sp, n, z, c, v, h, p;   /* registers and flags (p: the direct page) */
  uint16_t pc;                /* the address of the instruction in the listing */
  int cyc;                    /* the step within it */
  uint16_t dp, tp, rd, wr;    /* values an instruction keeps between steps */
  uint64_t clock;             /* cycles since power on */
  uint8_t port_in[4];         /* what the CPU wrote to $2140-$2143 */
  uint8_t control, dsp_addr, ipl_enable;
  uint32_t dsp_pending;       /* cycles the DSP is behind */
  int error;                  /* the program went where the listing has no instruction */
  snrt_dsp *dsp;
} snrt_spc;

void snrt_spc_power(snrt_spc *s, snrt_dsp *dsp);
void snrt_spc_run(snrt_spc *s, uint64_t until);   /* steps while the clock is short of until */
void snrt_spc_sync_dsp(snrt_spc *s);              /* catch the DSP up */
void snrt_spc_port_write(snrt_spc *s, int port, uint8_t v);   /* the CPU writes $2140-$2143 */
uint8_t snrt_spc_port_read(const snrt_spc *s, int port);      /* the CPU reads them */
