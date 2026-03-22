- Folder: `src/`
- Purpose: firmware source (Arduino entry + core + modules)
- Key files: `src/main.cpp`, `src/core/configPins.*`, `src/core/display.*`
- Edit level: EDIT WITH CAUTION

- Folder: `src/core/`
- Purpose: core services (config, display, input, storage, settings)
- Key files: `src/core/config.*`, `src/core/configPins.*`, `src/core/sd_functions.*`, `src/core/settings.cpp`
- Edit level: EDIT WITH CAUTION

- Folder: `src/modules/`
- Purpose: feature modules (wifi, ble, ir, NRF24, others)
- Key files: `src/modules/wifi/*`, `src/modules/ble/*`, `src/modules/ir/*`, `src/modules/NRF24/*`
- Edit level: EDIT WITH CAUTION

- Folder: `boards/`
- Purpose: board definitions + pin maps + build flags
- Key files: `boards/*/*.ini`, `boards/*/pins_arduino.h`
- Edit level: DO NOT EDIT

- Folder: `lib/`
- Purpose: third-party libraries + HAL
- Key files: `lib/HAL/*`, `lib/TFT_eSPI/*`, `lib/PN532_SRIX/*`
- Edit level: DO NOT EDIT

- Folder: `include/`
- Purpose: shared headers + build-time globals
- Key files: `include/precompiler_flags.h`, `include/globals.h`, `include/webFiles.h`
- Edit level: EDIT WITH CAUTION

- Folder: `embedded_resources/`
- Purpose: web UI assets bundled into firmware
- Key files: `embedded_resources/web_interface/*`
- Edit level: SAFE TO EDIT

- Folder: `sd_files/`
- Purpose: runtime SD assets (IR files, portals, scripts, themes)
- Key files: `sd_files/infrared/*`, `sd_files/portals/*`, `sd_files/themes/*`
- Edit level: SAFE TO EDIT

- Folder: `media/`
- Purpose: documentation images and assets
- Key files: `media/pictures/*`, `media/pcbs/*`
- Edit level: SAFE TO EDIT

- Folder: `pcbs/`
- Purpose: hardware design sources
- Key files: `pcbs/*`
- Edit level: DO NOT EDIT

- Folder: `other/`
- Purpose: side projects, stubs, legacy code
- Key files: `other/stubs/*`, `other/idk-*/*`
- Edit level: EDIT WITH CAUTION

- Folder: `docker/`
- Purpose: containerized build support
- Key files: `docker/*`, `docker-compose.yml`
- Edit level: SAFE TO EDIT

- Folder: `.github/`
- Purpose: CI workflows and repo metadata
- Key files: `.github/*`
- Edit level: SAFE TO EDIT

- Folder: `.pio/`
- Purpose: PlatformIO build output (generated)
- Key files: generated
- Edit level: DO NOT EDIT

- Folder: `release/`
- Purpose: build artifacts / release output (generated)
- Key files: generated
- Edit level: DO NOT EDIT

- Folder: `venv/`
- Purpose: local Python env (generated)
- Key files: generated
- Edit level: DO NOT EDIT

- Files: `Bruce-*.bin`, `Bruce-m5stack-cplus2.bin`
- Purpose: generated firmware images
- Edit level: DO NOT EDIT
