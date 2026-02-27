/**
 * LoRa App for M5StickC Plus 2
 * LoRa communication functionality
 */

#ifndef __IDK_LORA_H__
#define __IDK_LORA_H__

#include <Arduino.h>

// Use i18n for translations
extern const char* overlay_translate(const char* key);

namespace idk_firmware {

class LoraApp {
public:
    LoraApp() : isTransmitting(false), isReceiving(false), frequency(433.0), spreadingFactor(7) {}

    void begin() {
        isTransmitting = false;
        isReceiving = false;
        log_i("LoRa app initialized");
    }

    bool startTransmitting(float freq, int sf) {
        frequency = freq;
        spreadingFactor = sf;
        isTransmitting = true;
        log_i("LoRa transmitting at %.1f MHz, SF%d", frequency, spreadingFactor);
        return true;
    }

    void stopTransmitting() {
        isTransmitting = false;
    }

    bool startReceiving(float freq) {
        frequency = freq;
        isReceiving = true;
        log_i("LoRa receiving at %.1f MHz", frequency);
        return true;
    }

    void stopReceiving() {
        isReceiving = false;
    }

    bool sendMessage(const String& message) {
        if (!isTransmitting) return false;
        // In real implementation, send via LoRa
        log_i("LoRa TX: %s", message.c_str());
        return true;
    }

    String receiveMessage() {
        if (!isReceiving) return "";
        // In real implementation, receive from LoRa
        return "";
    }

    bool isTxActive() const { return isTransmitting; }
    bool isRxActive() const { return isReceiving; }
    float getFrequency() const { return frequency; }
    int getSpreadingFactor() const { return spreadingFactor; }

    const char* getTitle() {
        return "LoRa";
    }

private:
    bool isTransmitting;
    bool isReceiving;
    float frequency;
    int spreadingFactor;
};

// Global instance
static LoraApp loraApp;

void loraBegin() {
    loraApp.begin();
}

void loraLoop() {
    // Handle LoRa receiving
    if (loraApp.isRxActive()) {
        String msg = loraApp.receiveMessage();
        if (msg.length() > 0) {
            log_i("LoRa RX: %s", msg.c_str());
        }
    }
}

} // namespace idk_firmware

#endif // __IDK_LORA_H__
