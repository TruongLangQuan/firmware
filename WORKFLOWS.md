- WORKFLOW: Build firmware
- Step: pick an env from `platformio.ini` (default: `m5stack-cplus2`)
- Step: `pio run -e <env>`
- Step: use `pio run -e <env> -t build-firmware` to generate `Bruce-<env>.bin`

- WORKFLOW: Flash device
- Step: `pio run -e <env> -t upload`
- Step: `pio device monitor -b 115200`
- Step: optional: `esptool.py --port <PORT> write_flash 0x00000 Bruce-<env>.bin`

- WORKFLOW: Upload filesystem assets
- Step: `pio run -e <env> -t uploadfs` (LittleFS)
- Step: copy `sd_files/*` to SD card when features expect SD content

- WORKFLOW: Debug hardware
- Step: monitor logs via `pio device monitor -b 115200`
- Step: validate pins in Settings menu (IR/RF/SD/I2C/UART)
- Step: confirm `/brucePins.conf` persists changes

- WORKFLOW: Add a new module
- Step: add code under `src/modules/<name>/`
- Step: add menu item in `src/core/menu_items/` and/or `src/core/main_menu.cpp`
- Step: add config hooks in `src/core/settings.cpp` if new pins/options
- Step: wire dispatch in `src/main.cpp`
