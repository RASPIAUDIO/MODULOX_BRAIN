# 32MB full firmware images

This directory contains complete Modulox firmware images for ESP32-S3-WROOM-2-N32R16V modules.

Flash each `.bin` at address `0x0`. These images include:

- bootloader
- partition table
- boot app selector
- application firmware
- FFat data partition at `0x910000`

Use these images when you want to replace the application and its flash data in one pass.

The expected checksums and build options are listed in `manifest.json`.
