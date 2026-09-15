<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# WILDERNESS AI Passport

Reusable firmware and a native macOS companion that turn a FoloToy AI Passport into a WILDERNESS employee badge, a push-to-talk input device for desktop dictation, and a short-range direct walkie-talkie.

## What it does

### Badge mode

- Three fixed WILDERNESS pages: employee identity, Chinese service list, and brand page.
- Each employee changes only the portrait, name, role, and WeChat QR code.
- Hold `OK` to show the employee's QR code.
- Normal pages dim to 10% after 3 seconds. The first complete button gesture wakes the display without triggering an action.
- The passive NTAG213 NFC tag is independent from the firmware. Program it separately with `https://wmedia.cc/` or a per-card redirect URL.

### Voice input mode

- Hold `UP` in badge mode to enter voice mode.
- Hold `UP` to speak. The badge streams its ES8311 microphone over Bluetooth LE while the Mac companion holds left Control for the dictation app.
- Release `UP` to stop, press `DOWN` to send Return, and click `OK` to return to badge mode.
- Voice mode waits 60 seconds before dimming.
- Audio is 16 kHz, 16-bit, mono, compressed as independent 100 ms IMA-ADPCM blocks.

The current companion is for macOS and has been tested with Doubao IME through BlackHole 2ch. It can also save each session as a WAV file for diagnostics.

### Walkie-talkie mode (experimental)

- In badge mode, the radio stays in low-power call standby. Hold `DOWN` to send a 1.5-second call burst; a compatible nearby badge wakes and opens WILDERNESS RADIO automatically.
- Hold `UP` to talk and release it to listen. Click `OK` to return to badge mode.
- Badges communicate directly over ESP-NOW on Wi-Fi channel 6; no phone, Mac, router, or cloud service is required.
- Either badge can end the session with `OK`; both return to badge standby. A session with no voice activity also ends after 30 seconds.
- Badge standby listens for calls in a 40 ms window every 500 ms. The BLE microphone transport is suspended during radio standby and sessions, and radio standby is suspended while the voice-input page is active.
- Audio is 16 kHz, 16-bit, mono in independent 25 ms IMA-ADPCM frames. The radio page waits 60 seconds before dimming.
- Protocol V2 uses a fixed, unencrypted broadcast room for two-device lab validation. Do not treat it as private or deploy it at a public event until encrypted pairing and unicast peers are implemented.

## Requirements

- FoloToy AI Passport: ESP32-C3, 8 MB Flash, no PSRAM.
- ESP-IDF 5.5.3 for firmware builds.
- macOS 13 or newer with Bluetooth.
- BlackHole 2ch and a dictation app configured to use it as microphone input.
- Python 3 with Pillow, plus macOS Vision, for employee portrait provisioning.

## Quick start

1. Activate ESP-IDF 5.5.3.
2. Create a local employee profile. The entire `private-profiles/` directory is ignored by Git:

   ```bash
   python3 tools/provision_employee_badge.py \
     --slug employee-name \
     --name "Employee Name" \
     --role "Employee Role" \
     --portrait /path/to/portrait.jpg \
     --wechat-qr /path/to/wechat-qr.jpg \
     --build
   ```

3. Review the generated 224 x 224 portrait preview and QR code on a test device.
4. Flash only the application partition with the protected flashing helper:

   ```bash
   ./tools/safe-flash-app.sh \
     --port /dev/cu.usbmodemXXXX \
     --expected-mac aa:bb:cc:dd:ee:ff \
     --app build-employee-employee-name/Wilderness-AI-Passport.bin
   ```

5. Build and open the Mac companion:

   ```bash
   ./companion-macos/build-app.sh
   open companion-macos/build/WildernessMic.app
   ```

6. Grant Accessibility and Bluetooth permission when macOS asks. Install BlackHole 2ch separately, then select `BlackHole 2ch` as the input in Doubao IME.

See [deployment and safety](docs/development/wilderness-deployment.md) and [employee profile provisioning](docs/development/employee-badge-provisioning.md) before flashing real devices.

## Safe defaults

- `idf.py erase-flash` is forbidden for provisioned devices.
- `cardid` at `0x356000`, length `0x4000`, must remain byte-for-byte unchanged.
- The flashing helper backs up the full 8 MB Flash and `cardid`, writes only the application at `0x10000`, verifies the write, then reads and compares `cardid` again.
- CI builds a generic demonstration profile only. Release artifacts never contain employee portraits, names, WeChat QR codes, device identity, recordings, or backups.
- NFC is a public website touchpoint, not access control or trusted identity.

## Validation

```bash
./tools/validate.sh --static
./tools/validate.sh --firmware
swiftc -warnings-as-errors companion-macos/WildernessMic.swift \
  -framework AppKit -framework AudioToolbox -framework AVFoundation \
  -framework CoreBluetooth -o /tmp/WildernessMic
```

Firmware build success is not device acceptance. Check boot logs, display, buttons, BLE reconnect, microphone audio, dictation output, power behavior, radio discovery and two-way audio, and the protected `cardid` on each physical unit.

## License and attribution

The software is MIT licensed and derived from [FoloToy/ai-passport](https://github.com/FoloToy/ai-passport). See [LICENSE](LICENSE) and [third-party notices](THIRD_PARTY_NOTICES.md). WILDERNESS MEDIA brand assets are not licensed as trademarks.
