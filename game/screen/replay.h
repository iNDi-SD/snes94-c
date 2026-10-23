/* replay.c: replays (src/routines/replay). */
#pragma once
#include <stdint.h>

void freezewindow(void);
uint32_t adda43(uint32_t a4);
void checkwindow(void);
void updatereplay(void);
void RestoreReplayFrame(uint32_t a4);
uint32_t adda4(uint32_t a4);
uint32_t suba4(uint32_t a4);
void updateanim(int o);
void updateplayers(void);
void ReplayMode(void);
