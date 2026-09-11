<p align="right">
  <a href="employee-badge-provisioning.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Wilderness Employee Badge Provisioning

The employee badge uses one fixed product template. Its three normal pages, Chinese-language Wilderness service page, brand page, colors, typography, battery placement, QR size, and button behavior are shared by every device. The fixed service labels are the Chinese translations of brand social, livestream, AIGC video, brand TVC, and SaaS product tutorial video; the paired Chinese document records the exact on-screen copy. The backlight dims to 10% after 3 seconds idle on normal badge pages and after 60 seconds on the voice-input page; the first complete button gesture restores full brightness without changing the UI. This is backlight dimming, not deep sleep. Long-pressing `UP` enters voice input, holding and releasing `UP` starts and stops push-to-talk, `DOWN` sends, and a short `OK` click returns to badge mode. Per-employee provisioning may change only the portrait, display name, role, and WeChat QR code.

## Private profile contract

Each employee has a local `private-profiles/<slug>/` directory. The entire tree is ignored by Git because portraits and QR payloads are personal data. A complete profile contains:

- `badge_profile_private.h`: display name, role, and QR-page label.
- `badge_avatar_private.h` and `badge_avatar_private.c`: 224 × 224 RGB565 portrait inside the fixed WM logo frame.
- `wechat_qr_private.h`: private QR payload bytes; the provisioning tools never print the payload.
- `badge-avatar-private-224.png`: visual-review preview.
- local source portrait, transparent cutout, QR source, and `profile.json` metadata.

Do not copy these files into `main/`, documentation, a release package, or version control.

## Create or update one employee

On macOS, activate the project Python environment if necessary and run:

```bash
python3 tools/provision_employee_badge.py \
  --slug employee-name \
  --name "Employee Name" \
  --role "Employee Role" \
  --portrait /absolute/path/to/portrait.jpg \
  --wechat-qr /absolute/path/to/wechat-qr.jpg \
  --build
```

Use `--replace` when updating an existing employee. The old private directory is moved to `private-profiles/.backups/` before replacement. The command normalizes the two display fields to uppercase, rejects text that cannot fit the fixed-font contract, creates a real-alpha foreground with macOS Vision, composes the portrait inside the WM logo, requires exactly one decodable QR code, builds a profile-specific firmware, and verifies the protected Flash layout.

For a manual build, explicitly pass `-D BADGE_PROFILE_DIR=/absolute/path/to/private-profiles/<slug>` and verify the target profile in the CMake output, `CMakeCache.txt`, and `compile_commands.json`. Never provision an employee device from a build that lacks this evidence.

The firmware now fails configuration when neither a private profile nor an explicit
`-D BADGE_ALLOW_GENERIC=ON` is supplied. The generic override exists only for
CI/community validation and must never be flashed to an employee device. The old
implicit person-specific fallback was removed because it could combine one employee's
portrait with generic identity text.

## Visual and device acceptance

Before flashing, inspect `badge-avatar-private-224.png`: the head, neck, collar, and shoulders must be present; the person must remain inside the logo opening and must not touch `WM` or `media`. Check that the name stays on one line and the role is readable.

For every provisioned device:

1. Identify the USB device and confirm its expected MAC address.
2. Read and back up the full 8 MB Flash and `cardid` at `0x356000` for `0x4000` bytes.
3. Never use `erase-flash`. Write only bootloader `0x0`, partition table `0x8000`, and application `0x10000` from the employee build's `flash_args`.
4. Read `cardid` again and require a byte-for-byte match with the pre-flash backup.
5. Confirm a clean boot without panic or reboot loops, then visually check all three pages and repeatedly open the 232 × 232 QR page by long-pressing `OK`. Wait 3 seconds for the backlight to dim to 10%, confirm that the first button gesture only restores full brightness, and wait through another idle cycle to confirm repeatable dimming. Enter voice input with a long `UP`, verify that the page stays bright for 60 seconds, and return with a short `OK` click.
6. Scan the QR with WeChat and confirm it belongs to the intended employee.

Build verification is not device acceptance. Record the firmware build, target MAC, pre/post `cardid` hash, write-region verification, boot log, screen review, and QR scan separately.
