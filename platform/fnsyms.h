/* The C routines' cartridge addresses (build/fnsyms_gen.c, made by tools/gen_fnsyms.awk): game/ is
 * compiled with -finstrument-functions, and each routine's entry is told to the clock by its
 * address (platform/hal.c). */
#pragma once
#include <stdint.h>

typedef struct { void (*fn)(void); uint32_t addr; } FnSym;
extern FnSym fn_syms[];
extern const int n_fn_syms;
