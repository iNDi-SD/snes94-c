/* The cartridge (retail NHL '94, LoROM, 2 MB): loaded from the player's own ROM file, read by
 * the 65816 address of its data ($8000-$FFFF of banks $80-$BF; the other mirrors map the same). */
#pragma once
#include <stddef.h>
#include <stdint.h>

extern const uint8_t *rom_cart;
extern uint32_t rom_size;

int rom_load(const char *path);   /* a .smc's 512-byte copier header is skipped; 0 when unreadable */
int rom_set(const uint8_t *bytes, size_t len);   /* the same from bytes (copied) */

static inline uint32_t rom_offset(uint32_t a) { return ((a >> 16 & 0x7F) << 15 | (a & 0x7FFF)) % rom_size; }
static inline uint8_t rom8(uint32_t a) { return rom_cart[rom_offset(a)]; }
static inline uint16_t rom16(uint32_t a) { return (uint16_t)(rom8(a) | rom8(a + 1) << 8); }
static inline uint32_t rom24(uint32_t a) { return rom16(a) | (uint32_t)rom8(a + 2) << 16; }
