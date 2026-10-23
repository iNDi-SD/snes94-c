#!/usr/bin/env python3
"""Make build/spc_program.inc (make does): the sound CPU's two programs as C, a case per instruction.

usage: gen_spc.py ROM game/symbols_gen.h > build/spc_program.inc

The programs: the boot ROM ($FFC0-$FFFF, which receives the driver) and NHL '94's driver
(SoundDriver in the disassembly: uploaded to $0400; its data, the voice bits and the command
table, $04E9-$0512, is skipped). Each instruction becomes the steps snes9x's SPC700 takes for it:
the fetch with the first bus cycle, then a cycle a step, its reads and writes on the same cycles
(the templates below give each opcode's cycles in that order). Operand bytes are constants of the
listing; the program counter is the instruction's address. ROM: the player's own dump of the
cartridge (a copier header is skipped), as the game reads it: the driver is not in this repository.
"""
import sys

IPL = bytes.fromhex(
    'cdefbde800c61dd0fc8faaf48fbbf578ccf4d0fb2f19ebf4d0fc7ef4d00be4f5cbf4d700fcd0f3ab0110ef7ef410eb'
    'baf6da00baf4c4f4dd5dd0db1f0000c0ff')
DRIVER_AT, DRIVER_DATA = 0x0400, (0x04E9, 0x0513)

# the driver's labels in the disassembly (SoundDriver_...), named in the listing's comments
LABELS = {
    0x0400: 'start', 0x0420: 'clearfirst', 0x042E: 'clearpages', 0x043C: 'clearlast', 0x04D0: 'reply',
    0x04D2: 'waitclear', 0x04D6: 'mainloop', 0x04DA: 'waitcmd', 0x0513: 'cmdReset', 0x052C: 'cmdStatus',
    0x0534: 'statusack', 0x0545: 'cmdUpload', 0x056E: 'uploadbyte', 0x057E: 'uploadidle', 0x0596: 'refuse',
    0x059B: 'cmdUnload', 0x05B4: 'cmdKeyOff', 0x05C4: 'cmdKeyOn', 0x05F2: 'cmdSetVoice', 0x060B: 'voiceack',
    0x0631: 'cmdSetMaster', 0x064D: 'masterack', 0x0670: 'cmdSetEcho', 0x068C: 'echoack', 0x06AF: 'cmdSetFIR',
    0x06CB: 'firack', 0x06EB: 'firack2', 0x0704: 'cmdSetPitch', 0x071F: 'cmdSetGain', 0x073D: 'cmdKeyOffAll',
    0x074A: 'cmdSetVolume', 0x0763: 'cmdStreamStart', 0x0796: 'streambyte', 0x07A5: 'streamidle',
    0x07CE: 'streamfull', 0x07D3: 'cmdStreamNext', 0x07DF: 'nextbyte', 0x080B: 'cmdDownload',
    0x0817: 'downloadack', 0x082B: 'downloadgo', 0x0835: 'sendbyte',
}

# opcode: (length, text, [step bodies]). In a body: {b1} {b2} the operand bytes, {w} their word,
# {t} a branch's target, END ends the instruction here (the pc moves on), JUMP(x) ends it at x.
OPS = {
    0x00: (1, 'nop', ['IO(s);']),
    0x04: (2, 'or a,${b1:02X}', ['s->dp = RPC(s, {b1}); s->rd = RD(s, DP(s->dp)); s->a = NZ(s, (uint8_t)(s->a | s->rd));']),
    0x10: (2, 'bpl ${t:04X}', ['RPC(s, {b1}); if (s->n) END; IO(s); IO(s); JUMP({t});']),
    0x1A: (2, 'decw ${b1:02X}', ['s->dp = RPC(s, {b1}); s->rd = RD(s, DP(s->dp)); s->rd--; WR(s, DP(s->dp), (uint8_t)s->rd); s->dp++; '
                                's->rd += RD(s, DP(s->dp)) << 8; WR(s, DP(s->dp), (uint8_t)(s->rd >> 8)); s->n = s->rd >> 15; s->z = s->rd == 0;']),
    0x1C: (1, 'asl a', ['IO(s); s->a = ASL(s, s->a);']),
    0x1D: (1, 'dec x', ['IO(s); s->x = NZ(s, (uint8_t)(s->x - 1));']),
    0x1F: (3, 'jmp (${w:04X}+x)', ['s->dp = RPC(s, {b1}); s->dp |= RPC(s, {b2}) << 8; IO(s); s->dp += s->x; s->rd = RD(s, s->dp); '
                                   's->rd |= RD(s, (uint16_t)(s->dp + 1)) << 8; JUMP(s->rd);']),
    0x20: (1, 'clrp', ['IO(s); s->p = 0;']),
    0x2F: (2, 'bra ${t:04X}', ['RPC(s, {b1}); IO(s); IO(s); JUMP({t});']),
    0x3A: (2, 'incw ${b1:02X}', ['s->dp = RPC(s, {b1}); s->rd = RD(s, DP(s->dp)); s->rd++; WR(s, DP(s->dp), (uint8_t)s->rd); s->dp++; '
                                's->rd += RD(s, DP(s->dp)) << 8; WR(s, DP(s->dp), (uint8_t)(s->rd >> 8)); s->n = s->rd >> 15; s->z = s->rd == 0;']),
    0x3D: (1, 'inc x', ['IO(s); s->x = NZ(s, (uint8_t)(s->x + 1));']),
    0x3E: (2, 'cmp x,${b1:02X}', ['s->dp = RPC(s, {b1}); s->rd = RD(s, DP(s->dp)); CMP(s, s->x, (uint8_t)s->rd);']),
    0x5D: (1, 'mov x,a', ['IO(s); s->x = NZ(s, s->a);']),
    0x5F: (3, 'jmp ${w:04X}', ['RPC(s, {b1}); RPC(s, {b2}); JUMP({w});']),
    0x60: (1, 'clrc', ['IO(s); s->c = 0;']),
    0x64: (2, 'cmp a,${b1:02X}', ['s->dp = RPC(s, {b1}); s->rd = RD(s, DP(s->dp)); CMP(s, s->a, (uint8_t)s->rd);']),
    0x68: (2, 'cmp a,#${b1:02X}', ['s->rd = RPC(s, {b1}); CMP(s, s->a, (uint8_t)s->rd);']),
    0x78: (3, 'cmp ${b2:02X},#${b1:02X}', ['s->rd = RPC(s, {b1}); s->dp = RPC(s, {b2}); s->wr = RD(s, DP(s->dp)); CMP(s, (uint8_t)s->wr, (uint8_t)s->rd); IO(s);']),
    0x7E: (2, 'cmp y,${b1:02X}', ['s->dp = RPC(s, {b1});', 's->rd = RD(s, DP(s->dp)); CMP(s, s->y, (uint8_t)s->rd);']),
    0x80: (1, 'setc', ['IO(s); s->c = 1;']),
    0x84: (2, 'adc a,${b1:02X}', ['s->dp = RPC(s, {b1}); s->rd = RD(s, DP(s->dp)); s->a = ADC(s, s->a, (uint8_t)s->rd);']),
    0x88: (2, 'adc a,#${b1:02X}', ['s->rd = RPC(s, {b1}); s->a = ADC(s, s->a, (uint8_t)s->rd);']),
    0x8D: (2, 'mov y,#${b1:02X}', ['s->y = NZ(s, RPC(s, {b1}));']),
    0x8F: (3, 'mov ${b2:02X},#${b1:02X}', ['s->rd = RPC(s, {b1}); s->dp = RPC(s, {b2});', 'RD(s, DP(s->dp));', 'WR(s, DP(s->dp), (uint8_t)s->rd);']),
    0x9C: (1, 'dec a', ['IO(s); s->a = NZ(s, (uint8_t)(s->a - 1));']),
    0xA4: (2, 'sbc a,${b1:02X}', ['s->dp = RPC(s, {b1}); s->rd = RD(s, DP(s->dp)); s->a = SBC(s, s->a, (uint8_t)s->rd);']),
    0xAB: (2, 'inc ${b1:02X}', ['s->dp = RPC(s, {b1}); s->rd = RD(s, DP(s->dp)); s->rd = NZ(s, (uint8_t)(s->rd + 1)); WR(s, DP(s->dp), (uint8_t)s->rd);']),
    0xBA: (2, 'movw ya,${b1:02X}', ['s->tp = RPC(s, {b1});', 's->a = RD(s, DP(s->tp)); IO(s);',
                                    's->y = RD(s, DP(s->tp + 1)); s->n = s->y >> 7; s->z = (s->a | s->y) == 0;']),
    0xBD: (1, 'mov sp,x', ['IO(s); s->sp = s->x;']),
    0xC4: (2, 'mov ${b1:02X},a', ['s->dp = RPC(s, {b1});', 'RD(s, DP(s->dp));', 'WR(s, DP(s->dp), s->a);']),
    0xC6: (1, 'mov (x),a', ['IO(s);', 'RD(s, DP(s->x));', 'WR(s, DP(s->x), s->a);']),
    0xCB: (2, 'mov ${b1:02X},y', ['s->dp = RPC(s, {b1});', 'RD(s, DP(s->dp));', 'WR(s, DP(s->dp), s->y);']),
    0xCD: (2, 'mov x,#${b1:02X}', ['s->x = NZ(s, RPC(s, {b1}));']),
    0xD0: (2, 'bne ${t:04X}', ['RPC(s, {b1}); if (s->z) END; IO(s); IO(s); JUMP({t});']),
    0xD5: (3, 'mov ${w:04X}+x,a', ['s->dp = RPC(s, {b1}); s->dp |= RPC(s, {b2}) << 8; IO(s); s->dp += s->x;', 'RD(s, s->dp);', 'WR(s, s->dp, s->a);']),
    0xD7: (2, 'mov (${b1:02X})+y,a', ['s->tp = RPC(s, {b1});', 's->dp = RD(s, DP(s->tp));', 's->dp |= RD(s, DP(s->tp + 1)) << 8; IO(s); s->dp += s->y;',
                                      'RD(s, s->dp);', 'WR(s, s->dp, s->a);']),
    0xD8: (2, 'mov ${b1:02X},x', ['s->dp = RPC(s, {b1});', 'RD(s, DP(s->dp));', 'WR(s, DP(s->dp), s->x);']),
    0xDA: (2, 'movw ${b1:02X},ya', ['s->dp = RPC(s, {b1});', 'RD(s, DP(s->dp));', 'WR(s, DP(s->dp), s->a);', 'WR(s, DP(s->dp + 1), s->y);']),
    0xDC: (1, 'dec y', ['IO(s); s->y = NZ(s, (uint8_t)(s->y - 1));']),
    0xDD: (1, 'mov a,y', ['IO(s); s->a = NZ(s, s->y);']),
    0xE4: (2, 'mov a,${b1:02X}', ['s->tp = RPC(s, {b1});', 's->a = NZ(s, RD(s, DP(s->tp)));']),
    0xE8: (2, 'mov a,#${b1:02X}', ['s->a = NZ(s, RPC(s, {b1}));']),
    0xEB: (2, 'mov y,${b1:02X}', ['s->tp = RPC(s, {b1});', 's->y = NZ(s, RD(s, DP(s->tp)));']),
    0xF0: (2, 'beq ${t:04X}', ['RPC(s, {b1}); if (!s->z) END; IO(s); IO(s); JUMP({t});']),
    0xF5: (3, 'mov a,${w:04X}+x', ['s->tp = RPC(s, {b1}); s->tp |= RPC(s, {b2}) << 8; IO(s);', 's->a = NZ(s, RD(s, (uint16_t)(s->tp + s->x)));']),
    0xF7: (2, 'mov a,(${b1:02X})+y', ['s->dp = RPC(s, {b1}); IO(s);', 's->tp = RD(s, DP(s->dp));', 's->tp |= RD(s, DP(s->dp + 1)) << 8;',
                                      's->a = NZ(s, RD(s, (uint16_t)(s->tp + s->y)));']),
    0xF8: (2, 'mov x,${b1:02X}', ['s->tp = RPC(s, {b1});', 's->x = NZ(s, RD(s, DP(s->tp)));']),
    0xFC: (1, 'inc y', ['IO(s); s->y = NZ(s, (uint8_t)(s->y + 1));']),
    0xFD: (1, 'mov y,a', ['IO(s); s->y = NZ(s, s->a);']),
}


def instructions(code, base, skip=None):
    at = 0
    while at < len(code):
        a = base + at
        if skip and skip[0] <= a < skip[1]:
            at = skip[1] - base
            continue
        op = code[at]
        if op not in OPS:
            sys.exit(f'${a:04X}: opcode ${op:02X} has no template')
        n = OPS[op][0]
        yield a, code[at:at + n]
        at += n


def case(a, b, labels):
    n, text, steps = OPS[b[0]]
    f = dict(b1=b[1] if n > 1 else 0, b2=b[2] if n > 2 else 0)
    f['w'] = f['b1'] | f['b2'] << 8
    f['t'] = (a + 2 + (f['b1'] - 256 if f['b1'] > 127 else f['b1'])) & 0xFFFF
    nxt = (a + n) & 0xFFFF
    end = f'{{ s->cyc = 0; s->pc = 0x{nxt:04X}; return; }}'
    out = [f'  case 0x{a:04X}:' + (f'  /* {labels[a]} */' if a in labels else '') + f'  /* {text.format(**f)} */']
    out.append('    switch (s->cyc++) {')
    for k, body in enumerate(steps):
        body = jump(body.format(**f).replace('END', end))
        last = k == len(steps) - 1
        out.append(f'    case {k}: ' + ('' if k else '/* (fetch) */ IO(s); ') + body +
                   (f' s->cyc = 0; s->pc = 0x{nxt:04X}; return;' if last else ' return;'))
    out.append('    }')
    out.append('    return;')
    return '\n'.join(out)


def jump(body):
    while 'JUMP(' in body:
        i = body.index('JUMP(')
        depth, j = 0, i + 5
        while body[j] != ')' or depth:
            depth += body[j] == '('
            depth -= body[j] == ')'
            j += 1
        body = body[:i] + f'{{ s->cyc = 0; s->pc = (uint16_t)({body[i + 5:j]}); return; }}' + body[j + 2:]
    return body


def main():
    sys.stdout.reconfigure(newline='\n')     # (LF on Windows too)
    rom = open(sys.argv[1], 'rb').read()
    if len(rom) % 0x8000 == 512:                  # (a copier header, as game/system/rom.c skips it)
        rom = rom[512:]
    if rom[0x7FC0:0x7FC7] != b"NHL '94" or rom[0x7FD5] != 0x30:
        sys.exit(f'{sys.argv[1]}: not the original NHL \'94 SNES ROM')
    driver = None
    for line in open(sys.argv[2], encoding='utf-8'):
        p = line.split()
        if len(p) == 3 and p[0] == '#define' and p[1] == 'SYM_SoundDriver':
            a = int(p[2].rstrip('u'), 16)         # (its length, its address, then the code)
            off = ((a >> 16) & 0x7F) * 0x8000 + (a & 0x7FFF)
            size = rom[off] | rom[off + 1] << 8
            driver = rom[off + 4:off + 4 + size]
    if driver is None:
        sys.exit(f'{sys.argv[2]}: no SYM_SoundDriver')
    print('/* Made by tools/gen_spc.py from the cartridge (the player\'s ROM): do not edit.')
    print(' * The boot ROM, then NHL \'94\'s sound driver, a case per instruction (see snrt/spc.h). */')
    print('switch (s->pc) {')
    for a, b in instructions(IPL[:-2], 0xFFC0):   # (the last two bytes are the reset vector)
        print(case(a, b, {0xFFC0: 'boot ROM'}))
    for a, b in instructions(driver, DRIVER_AT, DRIVER_DATA):
        print(case(a, b, LABELS))
    print('  default: s->error = 1; return;')
    print('}')


if __name__ == '__main__':
    main()
