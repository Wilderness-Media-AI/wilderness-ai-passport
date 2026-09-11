#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${script_dir}/build"
app_dir="${build_dir}/WildernessMic.app"
contents_dir="${app_dir}/Contents"

mkdir -p "${contents_dir}/MacOS"
cp "${script_dir}/Info.plist" "${contents_dir}/Info.plist"

swiftc -warnings-as-errors "${script_dir}/WildernessMic.swift" \
  -framework AppKit \
  -framework AudioToolbox \
  -framework AVFoundation \
  -framework CoreBluetooth \
  -o "${contents_dir}/MacOS/WildernessMic"

codesign --force --deep --sign - "${app_dir}"
plutil -lint "${contents_dir}/Info.plist"
codesign --verify --deep --strict "${app_dir}"
echo "Mac companion: PASS (${app_dir})"
