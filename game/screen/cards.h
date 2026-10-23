/* cards.c: player cards (src/routines/cards). */
#pragma once
#include <stdint.h>

uint16_t CalcAttrib(uint16_t d0, uint16_t a2, uint16_t mask, uint16_t r2, uint16_t *count);
uint32_t PrintCentered(uint32_t a1);
void DrawPlayerPicture(uint16_t d0, uint16_t a2, uint16_t x, uint16_t y);

void DrawPlayerCard(uint16_t a2);
