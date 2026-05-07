# 32MB no-data firmware images

This directory contains smaller Modulox firmware images for ESP32-S3-WROOM-2-N32R16V modules.

Flash each `.bin` at address `0x0`. These images include:

- bootloader
- partition table
- boot app selector
- application firmware

They do not include the FFat data partition. Use these images when the data partition already exists on the module and must be preserved.

The expected checksums and build options are listed in `manifest.json`.
