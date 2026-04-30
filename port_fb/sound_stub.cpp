#include "sound.h"
#include "dsp/dsp.h"
#include "state.h"

extern nc2k_states_t nc2k_states;

Dsp dsp;

void init_audio() {}
void shutdown_audio() {}
void reset_dsp() { dsp.reset(); }
void write_data_to_dsp(uint8_t high, uint8_t low) { dsp.write(high, low); }

void dsp_move(int len) {
    // stub: just discard DSP samples
}

void beeper_on_io_write(int a) {
    // stub: no audio output
}

void post_cpu_run_sound_handling() {
    // stub
}

bool sound_busy() {
    return false;
}
