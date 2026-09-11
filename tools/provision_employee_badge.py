#!/usr/bin/env python3
"""Create a private Wilderness employee badge profile without exposing QR data."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime
from pathlib import Path


TEMPLATE_VERSION = "wilderness-employee-badge-v1"
MAX_NAME_LENGTH = 18
MAX_ROLE_LENGTH = 24
SLUG_PATTERN = re.compile(r"^[a-z0-9][a-z0-9-]{0,31}$")


def display_text(value: str, field: str, maximum: int) -> str:
    value = " ".join(value.split()).upper()
    if not value:
        raise ValueError(f"{field} must not be empty")
    if not value.isascii() or any(ord(char) < 32 or ord(char) > 126 for char in value):
        raise ValueError(f"{field} must use printable ASCII supported by the fixed font")
    if len(value) > maximum:
        raise ValueError(f"{field} is {len(value)} characters; fixed layout limit is {maximum}")
    return value


def c_string(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def run(command: list[str], cwd: Path) -> None:
    subprocess.run(command, cwd=cwd, check=True)


def pillow_python() -> str:
    """Find a Python runtime that can execute the existing Pillow asset tool."""
    candidates = [
        os.environ.get("BADGE_ASSET_PYTHON"),
        sys.executable,
        "/usr/bin/python3",
        "/opt/homebrew/bin/python3",
        "/usr/local/bin/python3",
    ]
    for candidate in candidates:
        if not candidate or not Path(candidate).is_file():
            continue
        result = subprocess.run(
            [candidate, "-c", "import PIL"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        if result.returncode == 0:
            return candidate
    raise RuntimeError(
        "no Python runtime with Pillow is available; set BADGE_ASSET_PYTHON"
    )


def validate_rgb_png(path: Path) -> None:
    """Validate the generated PNG IHDR without importing Pillow in IDF Python."""
    header = path.read_bytes()[:29]
    if len(header) != 29 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError("badge avatar preview is not a valid PNG")
    width = int.from_bytes(header[16:20], "big")
    height = int.from_bytes(header[20:24], "big")
    bit_depth = header[24]
    color_type = header[25]
    if (width, height, bit_depth, color_type) != (224, 224, 8, 2):
        raise ValueError("badge avatar must be a 224x224 8-bit RGB PNG")


def validate_profile(profile_dir: Path) -> None:
    required = (
        "badge_profile_private.h",
        "badge_avatar_private.h",
        "badge_avatar_private.c",
        "wechat_qr_private.h",
        "badge-avatar-private-224.png",
    )
    missing = [name for name in required if not (profile_dir / name).is_file()]
    if missing:
        raise ValueError("profile is incomplete: " + ", ".join(missing))

    validate_rgb_png(profile_dir / "badge-avatar-private-224.png")

    qr_header = (profile_dir / "wechat_qr_private.h").read_text(encoding="utf-8")
    if "WECHAT_QR_DATA[]" not in qr_header or "WECHAT_QR_DATA_LEN" not in qr_header:
        raise ValueError("QR header does not contain the expected private byte array")


def create_profile(args: argparse.Namespace, root: Path) -> Path:
    if not SLUG_PATTERN.fullmatch(args.slug):
        raise ValueError("slug must contain 1-32 lowercase letters, digits, or hyphens")

    name = display_text(args.name, "name", MAX_NAME_LENGTH)
    role = display_text(args.role, "role", MAX_ROLE_LENGTH)
    portrait = args.portrait.expanduser().resolve()
    qr_image = args.wechat_qr.expanduser().resolve()
    if not portrait.is_file():
        raise FileNotFoundError(f"portrait not found: {portrait}")
    if not qr_image.is_file():
        raise FileNotFoundError(f"WeChat QR image not found: {qr_image}")

    profile_root = args.output_root.expanduser().resolve() if args.output_root else root / "private-profiles"
    profile_root.mkdir(parents=True, exist_ok=True)
    destination = profile_root / args.slug
    stage = Path(tempfile.mkdtemp(prefix=f".{args.slug}-", dir=profile_root))

    try:
        portrait_copy = stage / f"source-portrait{portrait.suffix.lower() or '.jpg'}"
        qr_copy = stage / f"source-wechat-qr{qr_image.suffix.lower() or '.jpg'}"
        shutil.copy2(portrait, portrait_copy)
        shutil.copy2(qr_image, qr_copy)

        cutout = stage / "source-cutout.png"
        run(["swift", "tools/extract_portrait_foreground.swift", str(portrait_copy), str(cutout)], root)
        run(
            [pillow_python(), "tools/generate_private_avatar.py", str(cutout), "--profile-dir", str(stage)],
            root,
        )
        run(
            ["swift", "tools/extract_private_qr.swift", str(qr_copy), str(stage / "wechat_qr_private.h")],
            root,
        )

        (stage / "badge_profile_private.h").write_text(
            "#pragma once\n\n"
            f'#define BADGE_PERSON_NAME "{c_string(name)}"\n'
            f'#define BADGE_PERSON_ROLE "{c_string(role)}"\n'
            f'#define BADGE_WECHAT_LABEL "{c_string(name)} / WECHAT"\n',
            encoding="utf-8",
        )
        (stage / "profile.json").write_text(
            json.dumps(
                {
                    "template": TEMPLATE_VERSION,
                    "slug": args.slug,
                    "name": name,
                    "role": role,
                },
                ensure_ascii=True,
                indent=2,
            )
            + "\n",
            encoding="utf-8",
        )
        validate_profile(stage)

        if destination.exists():
            if not args.replace:
                raise FileExistsError(f"profile already exists: {destination}; use --replace to back it up and update it")
            backup_root = profile_root / ".backups"
            backup_root.mkdir(exist_ok=True)
            timestamp = datetime.now().astimezone().strftime("%Y%m%dT%H%M%S%z")
            backup = backup_root / f"{args.slug}-{timestamp}"
            shutil.move(str(destination), str(backup))
            print(f"Existing profile backed up locally: {backup}")

        os.replace(stage, destination)
        return destination
    except Exception:
        shutil.rmtree(stage, ignore_errors=True)
        raise


def build_profile(profile_dir: Path, root: Path) -> Path:
    if shutil.which("idf.py") is None:
        raise RuntimeError("idf.py is unavailable; activate the project ESP-IDF 5.5.3 environment")
    build_dir = root / f"build-employee-{profile_dir.name}"
    sdkconfig = build_dir / "sdkconfig"
    environment = os.environ.copy()
    environment["SDKCONFIG_DEFAULTS"] = str(root / "sdkconfig.defaults")
    subprocess.run(
        [
            "idf.py",
            "-B",
            str(build_dir),
            "-D",
            f"SDKCONFIG={sdkconfig}",
            "-D",
            f"BADGE_PROFILE_DIR={profile_dir}",
            "build",
        ],
        cwd=root,
        env=environment,
        check=True,
    )
    run(["idf.py", "-B", str(build_dir), "merge-bin", "-o", str(build_dir / "Wilderness-AI-Passport-full.bin")], root)
    run([sys.executable, "tools/verify_firmware.py", str(build_dir)], root)
    return build_dir


def main() -> int:
    parser = argparse.ArgumentParser(description="Provision one private employee badge profile")
    parser.add_argument("--slug", required=True, help="lowercase employee identifier, for example sam-huang")
    parser.add_argument("--name", required=True, help="employee display name")
    parser.add_argument("--role", required=True, help="employee display role")
    parser.add_argument("--portrait", required=True, type=Path, help="source portrait image")
    parser.add_argument("--wechat-qr", required=True, type=Path, help="WeChat QR screenshot or image")
    parser.add_argument("--replace", action="store_true", help="back up and replace an existing profile")
    parser.add_argument("--build", action="store_true", help="also build and verify employee firmware")
    parser.add_argument("--output-root", type=Path, help=argparse.SUPPRESS)
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    try:
        profile_dir = create_profile(args, root)
        print(f"Employee profile: PASS ({profile_dir})")
        print("QR payload: private and not printed")
        if args.build:
            build_dir = build_profile(profile_dir, root)
            print(f"Employee firmware: PASS ({build_dir})")
        else:
            print(
                "Build with: idf.py -B build-employee-"
                f"{args.slug} -DBADGE_PROFILE_DIR='{profile_dir}' build"
            )
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
