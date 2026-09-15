<p align="right">
  <a href="CHANGELOG.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Changelog

## Unreleased

- Changed the BLE microphone lifecycle so `WildernessMic` advertises and accepts a Mac connection only on the voice-input page. Returning to the badge now stops capture, disconnects the Mac, closes the audio codec, and stops BLE advertising.
- Required an expected device MAC before protected application-only flashing and made the backup/readback baud configurable.

## 0.1.0 - 2026-09-11

- Added the fixed three-page WILDERNESS employee badge with local-only portrait, name, role, and WeChat QR provisioning.
- Added the shared Chinese services page and WILDERNESS brand page.
- Added 3-second 10% backlight dimming in badge mode and a 60-second delay in voice mode, with wake-only handling for the first complete button gesture.
- Added badge-controlled voice input: long `UP` enters voice mode, held `UP` streams microphone audio, `DOWN` sends Return, and short `OK` returns to the badge.
- Added a native macOS Bluetooth LE companion, IMA-ADPCM decoding, diagnostic WAV capture, virtual microphone output through BlackHole 2ch, and Doubao left-Control control.
- Added private employee profile generation, application-only protected flashing, host tests, firmware layout verification, CI, and bilingual deployment documentation.
- Based the firmware on FoloToy AI Passport commit `f75873f1aab24ac4c0ba9394c131669f66cce650` and retained its MIT attribution.
