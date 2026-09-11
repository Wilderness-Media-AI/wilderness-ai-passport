<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, and sound effects, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

## Images

Store reusable source images and generated display assets in `images/`.

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

### WILDERNESS WM logo

- Source: the company-owned official website asset `public/assets/home-wm-logo.svg` from the WILDERNESS website project.
- Stored source: `images/wilderness-wm-logo.svg`.
- Generated preview: `images/wilderness-wm-logo-192.png`, 192 × 192, black background.
- Firmware output: `main/wilderness_logo.c` and `main/wilderness_logo.h`, RGB565 byte-swapped for LVGL.
- Regenerate with `python3 tools/generate_wilderness_logo.py <official-home-wm-logo.svg>`; this requires ImageMagick and Pillow on the development Mac.

### Local private badge avatar

- Personal portrait sources and generated avatar files are local-only and Git-ignored; never commit them.
- Generate the 152 × 152 RGB565 asset from a true RGBA portrait cutout using `python3 tools/generate_private_avatar.py <transparent-portrait.png>`. The person is clipped to the official WM logo's paper-colored interior while the black WM/media artwork remains above it.
- Legacy local firmware outputs are `main/badge_avatar_private.c` and `.h`; the employee workflow writes private outputs under the ignored `private-profiles/` directory.

### Fixed Chinese services page

- Product copy: company-owned Chinese translations of the five live website categories: brand social, livestream, AIGC video, brand TVC, and SaaS product tutorial video. The paired Chinese document records the exact on-screen labels.
- Generated preview: `images/wilderness-services-cn-240x320.png`, 240 × 320.
- Firmware output: `main/wilderness_services_cn.c` and `main/wilderness_services_cn.h`, RGB565 byte-swapped for LVGL.
- Regenerate with `python3 tools/generate_wilderness_services_cn.py`. The development Mac uses its system STHeiti font to rasterize the fixed copy; no redistributable font file is stored in the repository.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.
