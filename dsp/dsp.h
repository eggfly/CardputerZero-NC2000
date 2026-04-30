#pragma once
#include <cstdint>
#include <functional>

class Dsp {
public:
    int dspMode = 0;
    std::function<void(unsigned char*, int)> callback;
    void reset() { dspMode = 0; }
    void write(uint8_t high, uint8_t low) {}
};

inline void set_dsp_log_level(int level) {}
