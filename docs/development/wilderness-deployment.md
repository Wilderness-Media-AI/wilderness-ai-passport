<p align="right">
  <a href="wilderness-deployment.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# WILDERNESS deployment and acceptance

## 1. Create a private profile

Run `tools/provision_employee_badge.py` on macOS. Portraits, QR payloads, source images, generated C arrays, and local metadata stay under the ignored `private-profiles/` directory. Review the 224 x 224 preview before building.

## 2. Build and verify firmware

Use ESP-IDF 5.5.3. A real employee build must pass an explicit profile path:

```bash
idf.py -B build-employee-name \
  -D BADGE_PROFILE_DIR="$PWD/private-profiles/employee-name" build
idf.py -B build-employee-name merge-bin \
  -o build-employee-name/Wilderness-AI-Passport-full.bin
python3 tools/verify_firmware.py build-employee-name
```

The merged image exists for factory diagnostics. Do not use it for routine employee updates. Use the application-only image with `tools/safe-flash-app.sh`.

## 3. Flash without touching device identity

Connect one device at a time and resolve its current `/dev/cu.usbmodem*` port. The safe flashing helper:

1. Reads the device MAC.
2. Backs up the full 8 MB Flash.
3. Reads `cardid` at `0x356000` for `0x4000` bytes.
4. Writes only the application image at `0x10000`.
5. Verifies the application bytes.
6. Reads `cardid` again and requires an exact match.

Never run `idf.py erase-flash` on a provisioned device.

## 4. Install the macOS companion

Install BlackHole 2ch separately, build `WildernessMic.app`, open it, and grant Bluetooth and Accessibility permission. Select BlackHole 2ch as microphone input inside the dictation application. The companion does not change the system default input and does not install drivers.

## 5. Per-device acceptance

- Boot log has no panic or restart loop.
- Identity, services, brand, battery, and large WeChat QR render correctly.
- Normal pages dim to 10% after 3 seconds and wake without an accidental action.
- Voice mode enters on a long `UP`, stays bright for 60 seconds, records while `UP` is held, sends on `DOWN`, and exits on a short `OK`.
- The companion reconnects after Bluetooth interruption.
- A diagnostic WAV has intelligible speech and reports zero dropped blocks.
- The dictation app receives real text from the badge microphone.
- The pre-flash and post-flash `cardid` files are identical.
- The independently programmed NFC URL opens the intended website on an iPhone and an Android phone.
