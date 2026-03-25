# External Modules Wiring (M5StickC Plus2)

This guide lists default pin mappings for the `m5stack-cplus2` environment.
All signals are 3.3V logic. Power modules from 3.3V unless the module explicitly requires 5V.

## CH9329 (BadUSB)
- TX (ESP32 -> CH9329 RX): `GPIO32` (BAD_TX)
- RX (ESP32 <- CH9329 TX): `GPIO33` (BAD_RX)
- GND: GND
- VCC: follow CH9329 module spec (often 5V input, 3.3V logic)

## IR Transmitter
- Default IR TX: `GPIO19` (TXLED)
- Alternate pins selectable in firmware: `GPIO32`, `GPIO33`, `GPIO26`, `GPIO25`, `GPIO0`
- GND: GND
- VCC: 3.3V (module dependent)

## NRF24L01
- CE: `GPIO25`
- CSN (SS): `GPIO26`
- SCK: `GPIO0`
- MOSI: `GPIO32`
- MISO: `GPIO33`
- GND: GND
- VCC: 3.3V (do not use 5V)

## Micro SD (SPI)
- CS: `GPIO26`
- SCK: `GPIO0`
- MOSI: `GPIO32`
- MISO: `GPIO33`
- GND: GND
- VCC: 3.3V

## Notes
- These pins are defined in `boards/m5stack-cplus2/m5stack-cplus2.ini`.
- This uses the Grove SPI bus (GPIO32/33). Avoid using Grove I2C or BadUSB on the same pins at the same time.
