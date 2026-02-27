#include "rf_stubs.h"

bool initRfModule(String mode, float frequency) {
    (void)mode;
    (void)frequency;
    return false;
}

void deinitRfModule() {}

void initCC1101once(SPIClass *SSPI) { (void)SSPI; }

void sendCustomRF() {}

bool txSubFile(FS *fs, String filepath, bool hideDefaultUI) {
    (void)fs;
    (void)filepath;
    (void)hideDefaultUI;
    return false;
}

void RCSwitch_send(uint64_t data, unsigned int bits, int pulse, int protocol, int repeat) {
    (void)data;
    (void)bits;
    (void)pulse;
    (void)protocol;
    (void)repeat;
}

String rf_scan(float start_freq, float stop_freq, int max_loops) {
    (void)start_freq;
    (void)stop_freq;
    (void)max_loops;
    return "";
}

String RCSwitch_Read(float frequency, int max_loops, bool raw) {
    (void)frequency;
    (void)max_loops;
    (void)raw;
    return "";
}
