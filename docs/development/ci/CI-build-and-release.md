<p align="right">
  <a href="CI-build-and-release.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Automated build and release

`.github/workflows/firmware-checks.yml` builds every change to `main` and every pull request with ESP-IDF 5.5.3. `.github/workflows/build-firmware.yml` builds tags and can be started manually.

The firmware gate creates a temporary merged image and verifies bootloader, partition table, application offset, 8 MB Flash arguments, the 3 MB application limit, and the protected `cardid` region. It then exports only `build/Wilderness-AI-Passport.bin`.

GitHub Releases contain the generic application-only image. They never contain an employee profile, portrait, WeChat QR code, device backup, recording, or `cardid`. Flash the artifact at `0x10000` with `tools/safe-flash-app.sh`; do not write it at `0x0`.

Real employee firmware must be built locally from an ignored private profile. It is intentionally not created by public CI.

All GitHub Actions use full commit SHA pins. Build jobs have read-only repository permission; only the tag release job receives `contents: write`.
