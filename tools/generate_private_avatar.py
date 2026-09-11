#!/usr/bin/env python3
"""Composite a transparent local-only portrait inside the official WM logo."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageChops, ImageFilter, ImageOps


SIZE = 224
PORTRAIT_HEIGHT = 132
PORTRAIT_Y = 70
PORTRAIT_X_OFFSET = 22
LOGO_CLEARANCE = 4
PORTRAIT_CROP = (0.20, 0.03, 0.80, 0.92)
PAPER = (0xE8, 0xE8, 0xE3)


def rgb565_bytes(image: Image.Image) -> bytes:
    output = bytearray()
    for red, green, blue in image.convert("RGB").getdata():
        pixel = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
        output.extend(pixel.to_bytes(2, "big"))
    return bytes(output)


def c_array(data: bytes) -> str:
    rows = []
    for offset in range(0, len(data), 16):
        values = ", ".join(f"0x{value:02x}" for value in data[offset : offset + 16])
        rows.append(f"    {values},")
    return "\n".join(rows)


def scale_cutout(
    image: Image.Image,
    portrait_width: int | None = None,
) -> Image.Image:
    """Tightly frame the face and neck, then resize with soft edges."""
    image = image.convert("RGBA")
    if image.getextrema()[3] == (255, 255):
        raise ValueError("source portrait must have a real alpha channel")
    left, top, right, bottom = PORTRAIT_CROP
    image = image.crop(
        (
            round(image.width * left),
            round(image.height * top),
            round(image.width * right),
            round(image.height * bottom),
        )
    )
    width = portrait_width or round(image.width * PORTRAIT_HEIGHT / image.height)
    portrait = image.resize((width, PORTRAIT_HEIGHT), Image.Resampling.LANCZOS)

    # Vision's foreground mask retains a faint halo from the source backdrop.
    # Erode one output pixel while keeping the remaining edge antialiased.
    alpha = portrait.getchannel("A").filter(ImageFilter.MinFilter(3))
    portrait.putalpha(alpha)
    return portrait


def framed_avatar(
    source: Path,
    logo_source: Path,
    portrait_width: int | None = None,
    portrait_y: int = PORTRAIT_Y,
) -> Image.Image:
    portrait = scale_cutout(Image.open(source), portrait_width)
    canvas = Image.open(logo_source).convert("RGB").resize(
        (SIZE, SIZE), Image.Resampling.LANCZOS
    )

    # Keep a paper-colored safety gap around every black logo element. Besides
    # preserving the outer hexagon, this prevents the portrait from visually
    # touching or sitting behind either the WM or media lettering.
    paper_reference = Image.new("RGB", canvas.size, PAPER)
    difference = ImageChops.difference(canvas, paper_reference).convert("L")
    logo_ink = difference.point(lambda value: 255 if value > 12 else 0)
    protected_logo_ink = logo_ink.filter(ImageFilter.MaxFilter(LOGO_CLEARANCE * 2 + 1))
    open_logo_area = ImageOps.invert(protected_logo_ink)

    x = (SIZE - portrait.width) // 2 + PORTRAIT_X_OFFSET
    portrait_layer = Image.new("RGBA", canvas.size, (*PAPER, 0))
    portrait_layer.alpha_composite(portrait, (x, portrait_y))
    portrait_alpha = portrait_layer.getchannel("A")
    mask = ImageChops.multiply(portrait_alpha, open_logo_area)
    canvas.paste(portrait_layer.convert("RGB"), (0, 0), mask)
    return canvas


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source_image", type=Path)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument(
        "--profile-dir",
        type=Path,
        help="write a generic private employee profile asset into this directory",
    )
    parser.add_argument(
        "--portrait-width",
        type=int,
        help="override the normalized portrait width for exact template matching",
    )
    parser.add_argument(
        "--portrait-y",
        type=int,
        default=PORTRAIT_Y,
        help="override the normalized portrait top position for exact template matching",
    )
    args = parser.parse_args()

    root = args.root.resolve()
    logo_candidates = sorted((root / "assets" / "images").glob("wilderness-wm-logo-*.png"))
    if not logo_candidates:
        raise FileNotFoundError("generate the official Wilderness logo PNG first")
    image = framed_avatar(
        args.source_image.resolve(),
        logo_candidates[-1],
        portrait_width=args.portrait_width,
        portrait_y=args.portrait_y,
    )

    if args.profile_dir:
        output_dir = args.profile_dir.resolve()
        output_dir.mkdir(parents=True, exist_ok=True)
        preview = output_dir / f"badge-avatar-private-{SIZE}.png"
        header = output_dir / "badge_avatar_private.h"
        source = output_dir / "badge_avatar_private.c"
        symbol = "badge_avatar_private"
    else:
        preview = root / "assets" / "images" / f"badge-avatar-private-{SIZE}.png"
        header = root / "main" / "badge_avatar_private.h"
        source = root / "main" / "badge_avatar_private.c"
        symbol = "badge_avatar_private"

    image.save(preview)

    data = rgb565_bytes(image)
    header.write_text(
        f"#pragma once\n\n#include \"lvgl.h\"\n\nextern const lv_image_dsc_t {symbol};\n",
        encoding="utf-8",
    )
    source.write_text(
        f'''#include "{header.name}"

static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t {symbol}_map[] = {{
{c_array(data)}
}};

const lv_image_dsc_t {symbol} = {{
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.cf = LV_COLOR_FORMAT_RGB565_SWAPPED,
    .header.flags = 0,
    .header.w = {SIZE},
    .header.h = {SIZE},
    .header.stride = {SIZE * 2},
    .header.reserved_2 = 0,
    .data_size = sizeof({symbol}_map),
    .data = {symbol}_map,
    .reserved = NULL,
}};
''',
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
