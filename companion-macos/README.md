<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Wilderness Mic for macOS

This native companion connects to the AI Passport over Bluetooth LE, decodes its 16 kHz mono IMA-ADPCM stream, writes diagnostic WAV files, and sends the live PCM stream to BlackHole 2ch when that audio device is installed.

Badge input events control the current Mac application:

- Badge `UP` press and release post left-Control down and up events for Doubao IME.
- Badge `DOWN` posts Return.
- Physical left Control can still start and stop a diagnostic recording from the Mac.

Build and open the app:

```bash
./build-app.sh
open build/WildernessMic.app
```

On first launch, allow Bluetooth and Accessibility access. Install BlackHole 2ch separately and select it as the microphone inside the dictation application. The companion never installs a driver or changes the system default input.

Terminal diagnostics remain available when the binary is run directly: `start`, `stop`, and `record 3`. WAV files are written under `~/Library/Application Support/WildernessMic/recordings/`.
