- Issue: Flash upload fails
- Fix: verify USB cable/data, retry `pio run -e <env> -t upload`
- Fix: check serial port selection in PlatformIO

- Issue: Device not detected
- Fix: reconnect USB, try another port, check OS driver
- Fix: run `pio device list` and target the listed port

- Issue: Boot loop or crash on startup
- Fix: monitor logs via `pio device monitor -b 115200`
- Fix: revert recent changes in `src/main.cpp` or `src/core/config*`
- Fix: verify LittleFS/SD mounts and pin config

- Issue: Hardware mismatch
- Fix: confirm correct env in `platformio.ini` and matching `boards/<env>/pins_arduino.h`
- Fix: align pins in `src/core/configPins.*` with board definition
