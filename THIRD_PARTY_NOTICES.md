# Third-party components and attribution

The root MIT license applies to the original MOZHANG application code and documentation. It does not replace the licenses of framework code, drivers, tools, manufacturer information, or other third-party material. Hardware component symbols and footprints retain their original provenance.

## Firmware

- Arduino-ESP32 2.0.17: LGPL-2.1-or-later core and separately licensed bundled components. License copy: `licenses/Arduino-ESP32-LICENSE.md`. Corresponding source: https://github.com/espressif/arduino-esp32/tree/2.0.17 . BLE library license: `licenses/Arduino-BLE-LICENSE`; libb64 license: `licenses/libb64-LICENSE`.
- ESP-IDF 4.4 series SDK supplied by that Arduino release: Apache-2.0 framework with separately licensed components and radio libraries. Framework license copy: `licenses/ESP-IDF-LICENSE`. Source: https://github.com/espressif/esp-idf/tree/v4.4.7 . Preserve component-specific notices when redistributing rebuilt SDK libraries; refer to the exact Arduino framework package selected by PlatformIO for its bundled SDK components.
- Build platform: https://github.com/platformio/platform-espressif32/tree/v7.0.1 . The pinned PlatformIO environment downloads its toolchain and framework. Do not assume downloaded dependencies are MIT.

The release application is supplied together with its complete original application sources, pinned build configuration, and `release/relink/main.cpp.o`. The application may be modified and rebuilt/relinked against compatible modified framework libraries. The reproducible workflow is documented in `docs/DEVELOPMENT.md`. Debug information in the historical object file may contain original build paths; those are not runtime dependencies.

## Desktop tools

- pySerial 3.5, BSD-style: https://github.com/pyserial/pyserial/tree/v3.5 ; license copy `licenses/pyserial-LICENSE`.
- esptool 5.4.0, GPL-2.0-or-later: https://github.com/espressif/esptool/tree/v5.4.0 ; license copy `licenses/esptool-LICENSE`. It is invoked as a separate program. The clean release folder installs it on the recipient's machine rather than vendoring its Python distribution.
- PlatformIO Core, Apache-2.0: https://github.com/platformio/platformio-core . Installed only for development.
- Bleak, MIT: https://github.com/hbldh/bleak . Optional BLE receiving dependency.

Transitive dependencies retain their own installed distribution licenses. No Python runtime, virtual environment, third-party package cache or operating-system driver installer is bundled in this source delivery.

## Design reference

Electromagic_Wand_ESP32 by dimo333: https://github.com/dimo333/Electromagic_Wand_ESP32 , reviewed commit `e5cdec63b436fc7755ccf2bf9391a25fc41e4142`, GPL-3.0. The MOZHANG application was independently written using its high-level approach as inspiration; no source, model weights or training dataset from that repository is included.
