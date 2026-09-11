#!/usr/bin/env python3
"""Generate the fixed Chinese Wilderness services page as an RGB565 asset."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


WIDTH = 240
HEIGHT = 320
BLACK = "#090909"
CARD = "#111111"
PAPER = "#E8E8E3"
GREEN = "#A8FF60"
DEFAULT_FONT = Path("/System/Library/Fonts/STHeiti Light.ttc")


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


def text(draw: ImageDraw.ImageDraw, xy: tuple[int, int], value: str,
         font: ImageFont.FreeTypeFont, fill: str) -> None:
    draw.text(xy, value, font=font, fill=fill, anchor="la")


def card(draw: ImageDraw.ImageDraw, box: tuple[int, int, int, int],
         number: str, name: str, number_font: ImageFont.FreeTypeFont,
         name_font: ImageFont.FreeTypeFont) -> None:
    draw.rectangle(box, fill=CARD, outline=PAPER, width=1)
    left, top, right, bottom = box
    text(draw, (left + 8, top + 7), number, number_font, GREEN)
    name_box = draw.textbbox((0, 0), name, font=name_font)
    name_width = name_box[2] - name_box[0]
    name_height = name_box[3] - name_box[1]
    name_x = left + (right - left + 1 - name_width) // 2
    name_y = top + 34 - name_height // 2
    text(draw, (name_x, name_y), name, name_font, PAPER)


def render(font_path: Path) -> Image.Image:
    image = Image.new("RGB", (WIDTH, HEIGHT), BLACK)
    draw = ImageDraw.Draw(image)
    title_font = ImageFont.truetype(str(font_path), 22)
    number_font = ImageFont.truetype(str(font_path), 14)
    name_font = ImageFont.truetype(str(font_path), 18)
    wide_name_font = ImageFont.truetype(str(font_path), 17)

    text(draw, (16, 27), "我们的服务", title_font, PAPER)
    draw.rectangle((16, 58, 223, 60), fill=GREEN)
    card(draw, (16, 76, 115, 139), "01", "品牌社交", number_font, name_font)
    card(draw, (124, 76, 223, 139), "02", "直播", number_font, name_font)
    card(draw, (16, 148, 115, 211), "03", "AIGC 视频", number_font, name_font)
    card(draw, (124, 148, 223, 211), "04", "品牌 TVC", number_font, name_font)
    card(draw, (16, 220, 223, 283), "05", "SaaS 产品教程视频", number_font, wide_name_font)
    return image


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--font", type=Path, default=DEFAULT_FONT)
    args = parser.parse_args()

    root = args.root.resolve()
    font_path = args.font.resolve()
    if not font_path.is_file():
        raise FileNotFoundError(f"Chinese font not found: {font_path}")

    image = render(font_path)
    preview = root / "assets" / "images" / "wilderness-services-cn-240x320.png"
    header = root / "main" / "wilderness_services_cn.h"
    source = root / "main" / "wilderness_services_cn.c"
    preview.parent.mkdir(parents=True, exist_ok=True)
    image.save(preview)

    data = rgb565_bytes(image)
    header.write_text(
        '#pragma once\n\n#include "lvgl.h"\n\nextern const lv_image_dsc_t wilderness_services_cn;\n',
        encoding="utf-8",
    )
    source.write_text(
        f'''#include "wilderness_services_cn.h"

static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t wilderness_services_cn_map[] = {{
{c_array(data)}
}};

const lv_image_dsc_t wilderness_services_cn = {{
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.cf = LV_COLOR_FORMAT_RGB565_SWAPPED,
    .header.flags = 0,
    .header.w = {WIDTH},
    .header.h = {HEIGHT},
    .header.stride = {WIDTH * 2},
    .header.reserved_2 = 0,
    .data_size = sizeof(wilderness_services_cn_map),
    .data = wilderness_services_cn_map,
    .reserved = NULL,
}};
''',
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
