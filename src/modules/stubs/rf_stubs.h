#ifndef __RF_STUBS_H__
#define __RF_STUBS_H__

#include <Arduino.h>
#include <FS.h>
#include <SPI.h>

bool initRfModule(String mode = "", float frequency = 0);
void deinitRfModule();
void initCC1101once(SPIClass *SSPI);

void sendCustomRF();
bool txSubFile(FS *fs, String filepath, bool hideDefaultUI = false);
void RCSwitch_send(uint64_t data, unsigned int bits, int pulse = 0, int protocol = 1, int repeat = 10);
String rf_scan(float start_freq, float stop_freq, int max_loops = -1);
String RCSwitch_Read(float frequency = 0, int max_loops = -1, bool raw = false);

#endif
