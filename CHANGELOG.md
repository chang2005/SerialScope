# Changelog

All notable changes to SerialScope are documented in this file.

## [1.0.0] — 2026-11-17

First public release.

### Added

- Serial port selection with automatic enumeration, manual entry and hot-plug
  refresh that preserves the current selection.
- Common baud-rate presets from 300 to 4,000,000 plus custom baud rates;
  data bits 5–8, parity none/odd/even/mark/space, stop bits 1/1.5/2 and
  hardware/software flow control.
- ASCII and HEX receive/send modes, UTF-8/GBK/ASCII decoding, timestamps,
  pause, auto-scroll and display export.
- Timed sending, send history, file sending with progress/cancel and optional
  CRC16-Modbus, CRC16-CCITT or Sum8 checksums.
- Asynchronous, bounded-queue logging with RX/TX direction and HEX output.
- Multi-channel waveform view fed either by numeric values in ASCII lines or
  by fields extracted from binary HEX frames.
- CRC toolbox, light/dark themes, connection statistics and persistent
  settings.
- About dialog with version, GitHub project link and GPLv3 license viewer.
- Dedicated Windows executable icon and version metadata.
- Automated core and serial-configuration UI tests.

### Notes

- This release is built and tested on Windows 11 with Qt 6.8.3 and MinGW 13.1.
- The source is platform-neutral and can be built on Linux or macOS with Qt 6.
