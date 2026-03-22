- Task: Add new sensor (ASSUMPTION: external sensor hardware exists)
- Files: `src/modules/<name>/`, `src/core/configPins.*`, `src/core/settings.cpp`, `src/core/menu_items/*`, `src/main.cpp`
- Risk: HIGH

- Task: Change display logic or theme
- Files: `src/core/display.*`, `src/core/theme.*`, `src/core/main_menu.cpp`, `embedded_resources/web_interface/*`
- Risk: MEDIUM

- Task: Modify WiFi behavior or WebUI
- Files: `src/modules/wifi/*`, `src/core/config.*`, `src/core/menu_items/*`, `embedded_resources/web_interface/*`
- Risk: MEDIUM

- Task: Debug boot issue / crash on startup
- Files: `src/main.cpp`, `src/core/config.*`, `src/core/display.*`, `boards/*/*.ini`
- Risk: HIGH
