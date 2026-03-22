- TARGET BOARD: `m5stack-cplus2` (ASSUMPTION; `default_envs` in `platformio.ini`)
- Pin sources: `boards/*/*.ini`, `boards/*/pins_arduino.h`, runtime `/brucePins.conf` via `src/core/configPins.*`

- SECTION: Display + backlight
- Component: TFT ST7789
- Files: `src/core/display.*`, `lib/HAL/display/*`, `lib/TFT_eSPI/*`
- Pins: TFT_CS=5, TFT_DC=14, TFT_RST=12, TFT_MOSI=15, TFT_SCLK=13, TFT_BL=27 (ASSUMPTION: `boards/m5stack-cplus2/m5stack-cplus2.ini`)
- Protocol: SPI
- Risk: HIGH

- Component: Buttons
- Files: `src/core/main_menu.*`, `src/core/settings.cpp`, `src/main.cpp`
- Pins: SEL_BTN=37, UP_BTN=35, DW_BTN=39, BTN_ACT=LOW (ASSUMPTION: `boards/m5stack-cplus2/m5stack-cplus2.ini`)
- Protocol: GPIO
- Risk: HIGH

- Component: SD card
- Files: `src/core/sd_functions.*`, `lib/HAL/sd_card/*`
- Pins: SDCARD_CS=14, SDCARD_SCK=0, SDCARD_MISO=25, SDCARD_MOSI=26 (ASSUMPTION: `boards/m5stack-cplus2/m5stack-cplus2.ini`)
- Protocol: SPI
- Risk: HIGH

- Component: IR TX/RX
- Files: `src/modules/ir/*`, `src/core/configPins.*`, `src/core/settings.cpp`
- Pins: TXLED=19, IR_RX via GROVE_SCL (ASSUMPTION: board config + `precompiler_flags.h`)
- Protocol: IR
- Risk: HIGH

- Component: I2C/Grove bus
- Files: `src/core/i2c_finder.*`, `src/core/configPins.*`, `lib/PN532_SRIX/*`
- Pins: GROVE_SDA=32, GROVE_SCL=33 (ASSUMPTION: `boards/m5stack-cplus2/m5stack-cplus2.ini`)
- Protocol: I2C
- Risk: HIGH

- Component: RF (CC1101) + NRF24
- Files: `src/modules/NRF24/*`, `src/core/configPins.*`, `src/core/settings.cpp`
- Pins: defined by `CC1101_*` and `NRF24_*` macros in `boards/*/*.ini`
- Protocol: SPI
- Risk: HIGH

- Component: RFID (PN532/RC522)
- Files: `lib/PN532_SRIX/*`, `src/modules/bjs_interpreter/rfid_js.*`, `src/core/settings.cpp`
- Pins: module-specific, configured via `bruceConfigPins` (ASSUMPTION)
- Protocol: I2C/SPI
- Risk: HIGH

- Component: WiFi + BLE
- Files: `src/modules/wifi/*`, `src/modules/ble/*`, `src/modules/ble_api/*`
- Pins: N/A (radio)
- Protocol: 802.11 + BLE
- Risk: HIGH

- Component: Audio (buzzer / speaker)
- Files: `src/modules/others/audio*`, `src/main.cpp`
- Pins: BUZZ_PIN=2 or HAS_NS4168_SPKR (ASSUMPTION: board config)
- Protocol: GPIO/I2S
- Risk: MEDIUM

- Component: GPS/UART
- Files: `src/core/configPins.*`, `src/core/serial_commands/*`
- Pins: SERIAL_RX/SERIAL_TX or GPS_SERIAL_RX/TX from board config
- Protocol: UART
- Risk: HIGH

- Component: W5500 Ethernet + LoRa (optional)
- Files: `src/core/configPins.*`, `src/core/settings.cpp`
- Pins: W5500_* / LORA_* macros from `boards/*/*.ini`
- Protocol: SPI
- Risk: HIGH

- SECTION: PCB sources
- Component: hardware designs under `pcbs/*`
- Files: `pcbs/*`
- Pins: see PCB docs (ASSUMPTION)
- Risk: HIGH
