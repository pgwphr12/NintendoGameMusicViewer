// YM2413 interface for NGMV V1.3, using bundled MIT emu2413.
// Original Game_Music_Emu interface license is retained (LGPL-2.1-or-later).
#include "Ym2413_Emu.h"
#include "ext/emu2413.h"
#include <algorithm>
Ym2413_Emu::Ym2413_Emu() : opll(0) {}
Ym2413_Emu::~Ym2413_Emu() { if (opll) OPLL_delete(opll); }
int Ym2413_Emu::set_rate(double rate, double clock) {
    if (opll) OPLL_delete(opll);
    opll = OPLL_new((uint32_t)clock, (uint32_t)rate);
    if (!opll) return 1;
    OPLL_setChipType(opll, 0);
    OPLL_resetPatch(opll, OPLL_2413_TONE);
    return 0;
}
void Ym2413_Emu::reset() { if (opll) OPLL_reset(opll); }
void Ym2413_Emu::write(int reg, int value) { if (opll) OPLL_writeReg(opll, reg, value); }
void Ym2413_Emu::mute_voices(int mask) {
    if (opll) {
        OPLL_setMask(opll, mask);
        // emu2413 skips updating masked outputs; discard the previous samples.
        std::fill(opll->ch_out, opll->ch_out + 14, 0);
    }
}
void Ym2413_Emu::run(int count, sample_t* out) {
    while (count--) { int32_t stereo[2]; OPLL_calcStereo(opll, stereo);
        *out++ += (sample_t)stereo[0]; *out++ += (sample_t)stereo[1]; }
}
