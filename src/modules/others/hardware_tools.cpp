#include "hardware_tools.h"

#include <IRsend.h>
#include <RF24.h>

#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include "core/scrollableTextArea.h"
#include "modules/NRF24/nrf_common.h"
#include <globals.h>

namespace {
const char kWiringGuide[] = R"TXT(
External Modules Wiring (M5StickC Plus2)

CH9329 (BadUSB)
- TX (ESP32 -> CH9329 RX): GPIO32 (BAD_TX)
- RX (ESP32 <- CH9329 TX): GPIO33 (BAD_RX)
- GND: GND
- VCC: follow module spec (often 5V input, 3.3V logic)

IR Transmitter
- Default IR TX: GPIO19 (TXLED)
- Alternate selectable: GPIO32, GPIO33, GPIO26, GPIO25, GPIO0
- GND: GND
- VCC: 3.3V (module dependent)

NRF24L01
- CE: GPIO25
- CSN: GPIO26
- SCK: GPIO0
- MOSI: GPIO32
- MISO: GPIO33
- GND: GND
- VCC: 3.3V only

Micro SD (SPI)
- CS: GPIO26
- SCK: GPIO0
- MOSI: GPIO32
- MISO: GPIO33
- GND: GND
- VCC: 3.3V

Notes:
- Pins based on boards/m5stack-cplus2/m5stack-cplus2.ini
- Uses Grove SPI bus (GPIO32/33). Avoid using Grove I2C or BadUSB on same pins.
)TXT";

void showWiringGuide() {
    ScrollableTextArea area(FP, 0, 0, tftWidth, tftHeight, false, false);
    area.rebuildLayout();
    area.fromString(String(kWiringGuide));
    area.show(true);
}

void sdCardTest() {
    if (!setupSdCard()) {
        displayError("SD init failed", true);
        return;
    }

    const char *path = "/sd_test.txt";
    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        displayError("SD write failed", true);
        return;
    }
    f.println("Bruce SD OK");
    f.close();

    f = SD.open(path, FILE_READ);
    if (!f) {
        displayError("SD read failed", true);
        return;
    }
    String content = f.readStringUntil('\n');
    f.close();

    displaySuccess("SD OK: " + content, true);
}

void nrfSend(RF24 &radio) {
    String msg = keyboard("Hello", 32, "NRF Send");
    if (msg == "\x1B") return;
    radio.stopListening();
    bool ok = radio.write(msg.c_str(), msg.length() + 1);
    displayTextLine(ok ? "Sent" : "Send failed", true);
}

void nrfListen(RF24 &radio) {
    radio.startListening();

    ScrollableTextArea area(FP, 0, 0, tftWidth, tftHeight, false, false);
    area.rebuildLayout();
    area.addLine("Listening...");
    area.draw(true);

    char buf[33] = {0};
    while (true) {
        InputHandler();
        if (check(EscPress) || check(PrevPress) || check(SelPress)) break;

        if (radio.available()) {
            radio.read(buf, sizeof(buf));
            area.addLine(String(buf));
            area.draw(true);
        }
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }

    radio.stopListening();
}

void nrf24Chat() {
    if (!nrf_start(NRF_MODE_SPI)) {
        displayError("NRF24 init failed", true);
        return;
    }

    NRFradio.setChannel(108);
    NRFradio.setPALevel(RF24_PA_LOW);
    NRFradio.setDataRate(RF24_1MBPS);
    NRFradio.setRetries(3, 5);
    const uint8_t addr[6] = "BRC24";
    NRFradio.openWritingPipe(addr);
    NRFradio.openReadingPipe(1, addr);

    options = {
        {"Send", []() { nrfSend(NRFradio); }},
        {"Listen", []() { nrfListen(NRFradio); }},
        {"Back", []() { returnToMenu = true; }}
    };
    loopOptions(options, MENU_TYPE_SUBMENU, "NRF24 Chat");
}

void irSendMenu() {
    String codeStr = keyboard("0x00FF00FF", 16, "NEC Code");
    if (codeStr == "\x1B") return;
    codeStr.trim();
    uint32_t code = (uint32_t)strtoul(codeStr.c_str(), nullptr, 0);
    sendIR(code);
    displayTextLine("IR Sent", true);
}
} // namespace

void sendIR(uint32_t code) {
    IRsend irsend(bruceConfigPins.irTx);
    irsend.begin();
    irsend.sendNEC(code, 32);
    if (bruceConfigPins.irTxRepeats > 0) {
        for (uint8_t i = 1; i <= bruceConfigPins.irTxRepeats; i++) { irsend.sendNEC(code, 32); }
    }
    digitalWrite(bruceConfigPins.irTx, LED_OFF);
}

void hardware_tools_menu() {
    options = {
        {"Wiring Guide", []() { showWiringGuide(); }},
        {"SD Card Test", []() { sdCardTest(); }},
        {"NRF24 Chat", []() { nrf24Chat(); }},
        {"IR Send", []() { irSendMenu(); }},
        {"Back", []() { returnToMenu = true; }}
    };
    loopOptions(options, MENU_TYPE_SUBMENU, "Hardware Tools");
}
