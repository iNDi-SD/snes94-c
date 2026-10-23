/* Work RAM by its machine address: the machine's view of struct g (src/ram.h). The 65816 is
 * little-endian like the host and g is laid out at WRAM's own offsets, so this view is g's bytes
 * themselves: r16(RAM_OFF_Xpos + 2 * o) and g.Xpos[o] are the same two bytes.
 *
 * The game reads its state by name (g.x); what goes by address: what the game computes an address
 * for beyond its arrays (a byte index into a word array, as assgoalie's pfteam read with an object
 * number), pointers the game keeps and follows (into RAM or the cartridge: m8 and its kin), and
 * clears or copies of a span of fields. */
#pragma once
#include <stdint.h>
#include <string.h>
#include "ram.h"
#include "rom.h"
#include "hal.h"

#define WRAM_BYTES ((uint8_t *)&g)

/* bytes and words at a WRAM offset ($00000-$1FFFF), as the 65816 reads them */
static inline uint8_t r8(uint32_t off) { return WRAM_BYTES[off & 0x1FFFF]; }
static inline uint16_t r16(uint32_t off) { return (uint16_t)(r8(off) | r8(off + 1) << 8); }
static inline void w8(uint32_t off, uint8_t v) { WRAM_BYTES[off & 0x1FFFF] = v; }
static inline void w16(uint32_t off, uint16_t v) { w8(off, (uint8_t)v); w8(off + 1, (uint8_t)(v >> 8)); }

/* a 24-bit address as the main CPU sees it (LoROM): WRAM in banks $7E-$7F and its low 8 KB in
 * banks $00-$3F / $80-$BF, the cartridge at $8000-$FFFF of every bank but $7E-$7F */
static inline int is_wram(uint32_t a)
{
    uint32_t b = a >> 16 & 0xFF;
    return b == 0x7E || b == 0x7F || ((b < 0x40 || (b >= 0x80 && b < 0xC0)) && (a & 0xFFFF) < 0x2000);
}
static inline uint32_t wram_off(uint32_t a)
{
    uint32_t b = a >> 16 & 0xFF;
    return b == 0x7E || b == 0x7F ? (b - 0x7E) << 16 | (a & 0xFFFF) : a & 0x1FFF;
}
static inline uint8_t m8(uint32_t a) { return is_wram(a) ? r8(wram_off(a)) : rom8(a); }
static inline uint16_t m16(uint32_t a) { return (uint16_t)(m8(a) | m8(a + 1) << 8); }

/* the direct page's scratch registers and the stack: the C keeps them in locals (the checks skip them) */
static inline int scratch_off(uint32_t off) { return off < 0x30 || (off >= 0x87 && off < 0xC1) || (off >= 0xC3 && off < 0xD1) || (off >= RAM_STACK_LO && off < 0x2000); }
/* m8 / m16 where the original may read through a pointer into the scratch registers (an instrument
 * pointer of 0): the C keeps those in locals, so a scratch byte reads as 0 */
static inline uint8_t lm8(uint32_t a) { return is_wram(a) && scratch_off(wram_off(a)) ? 0 : m8(a); }
static inline uint16_t lm16(uint32_t a) { return (uint16_t)(lm8(a) | lm8(a + 1) << 8); }

/* an absolute address ($0000-$FFFF) as `lda.w addr,y` reads it through the data bank: the game
 * keeps a ROM bank there ($80, $9D-$9F: phk / plb), so below $2000 it is WRAM and above it the
 * cartridge (or the I/O registers, which no such read reaches) */
static inline uint8_t db8(uint8_t db, uint16_t a) { return m8((uint32_t)db << 16 | a); }
static inline uint16_t db16(uint8_t db, uint16_t a) { return (uint16_t)(db8(db, a) | db8(db, (uint16_t)(a + 1)) << 8); }
