#!/usr/bin/env python3
"""Generate the embedded RGB565 WM logo from the official website SVG."""

from __future__ import annotations

import argparse
import shutil
import subprocess
from pathlib import Path

from PIL import Image


SIZE = 192
BACKGROUND = "#090909"


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


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source_svg", type=Path)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()

    root = args.root.resolve()
    source_svg = args.source_svg.resolve()
    asset_dir = root / "assets" / "images"
    asset_dir.mkdir(parents=True, exist_ok=True)
    saved_svg = asset_dir / "wilderness-wm-logo.svg"
    saved_png = asset_dir / f"wilderness-wm-logo-{SIZE}.png"
    if source_svg != saved_svg.resolve():
        shutil.copy2(source_svg, saved_svg)

    subprocess.run(
        [
            "magick",
            "-background",
            BACKGROUND,
            "-density",
            "384",
            str(saved_svg),
            "-resize",
            f"{SIZE}x{SIZE}",
            "-alpha",
            "remove",
            "-alpha",
            "off",
            str(saved_png),
        ],
        check=True,
    )

    data = rgb565_bytes(Image.open(saved_png))
    header = root / "main" / "wilderness_logo.h"
    source = root / "main" / "wilderness_logo.c"
    header.write_text(
        "#pragma once\n\n#include \"lvgl.h\"\n\nextern const lv_image_dsc_t wilderness_logo;\n",
        encoding="utf-8",
    )
    source.write_text(
        f'''#include "wilderness_logo.h"

static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t wilderness_logo_map[] = {{
{c_array(data)}
}};

const lv_image_dsc_t wilderness_logo = {{
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.cf = LV_COLOR_FORMAT_RGB565_SWAPPED,
    .header.flags = 0,
    .header.w = {SIZE},
    .header.h = {SIZE},
    .header.stride = {SIZE * 2},
    .header.reserved_2 = 0,
    .data_size = sizeof(wilderness_logo_map),
    .data = wilderness_logo_map,
    .reserved = NULL,
}};
''',
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
