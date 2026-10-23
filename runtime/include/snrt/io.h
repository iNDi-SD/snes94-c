/* The CPU's own registers ($4016-$4017, $4200-$421F) and the WRAM port ($2180-$2183): interrupt
 * enables and flags, the H/V timer, the multiplier and divider, the pads (a standard pad in port 1;
 * in port 2 a standard pad or the MultiPlayer 5 multitap with pads 2-5, $4201 bit 7 choosing pads
 * 2 and 3 or 4 and 5; auto-read or read a bit at a time). Behaviour as snes9x's: results are ready
 * at once, the NMI flag is set at line 225 and cleared at line 0 or when read, the IRQ flag when
 * the timer fires (14 cycles after its dot) and cleared when read, auto-read happens at line 227.
 * Bits the hardware leaves to the open bus read as 0. */
#pragma once
#include <stdint.h>

typedef struct snrt_io {
  uint8_t nmitimen, wrio, wrmpya;
  uint16_t wrdiv, rddiv, rdmpy;
  uint16_t htime, vtime;
  uint8_t nmi_flag, irq_flag;
  uint16_t pad[5];            /* the buttons held now (bit 15 B ... bit 4 R), from the client (3-5: the multitap's) */
  uint8_t multitap;           /* the multitap in port 2 (the client's, at power on) */
  uint16_t joy[4];            /* $4218-$421F: auto-read */
  uint8_t latch, read_idx[3]; /* the bits read: port 1, port 2 (the multitap: pads 2 and 3), the multitap's pads 4 and 5 */
  uint32_t wmadd;             /* $2181-$2183 */
} snrt_io;

struct snrt;
void snrt_io_power(snrt_io *io);
void snrt_io_write(struct snrt *m, uint16_t addr, uint8_t v);
uint8_t snrt_io_read(struct snrt *m, uint16_t addr, int cycle);   /* cycle: the read's own (HVBJOY) */
/* the beam's events: line starts (NMI flag, auto-read) and the timer */
void snrt_io_line(struct snrt *m, int line);
int snrt_io_irq_cycle(const snrt_io *io, int line);   /* the cycle the timer fires on this line, or -1 */
void snrt_io_irq(struct snrt *m);
