#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 --port /dev/cu.usbmodemXXXX --app path/to/Wilderness-AI-Passport.bin --expected-mac aa:bb:cc:dd:ee:ff [--baud 921600]" >&2
}

port=""
app_image=""
expected_mac=""
baud="921600"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --port) port="${2:-}"; shift 2 ;;
        --app) app_image="${2:-}"; shift 2 ;;
        --expected-mac) expected_mac="${2:-}"; shift 2 ;;
        --baud) baud="${2:-}"; shift 2 ;;
        *) usage; exit 2 ;;
    esac
done

if [[ -z "${port}" || -z "${app_image}" || -z "${expected_mac}" || ! -c "${port}" ||
      ! -f "${app_image}" || ! "${baud}" =~ ^[0-9]+$ ]]; then
    usage
    exit 2
fi

app_image="$(cd -- "$(dirname -- "${app_image}")" && pwd)/$(basename -- "${app_image}")"
app_size="$(stat -f '%z' "${app_image}" 2>/dev/null || stat -c '%s' "${app_image}")"
if (( app_size <= 0 || app_size > 0x300000 )); then
    echo "ERROR: application size ${app_size} is outside the protected 3 MB partition" >&2
    exit 1
fi
if [[ "$(od -An -tx1 -N1 "${app_image}" | tr -d '[:space:]')" != "e9" ]]; then
    echo "ERROR: application does not start with the ESP image magic byte" >&2
    exit 1
fi

if command -v esptool.py >/dev/null 2>&1; then
    esptool=(esptool.py)
elif python3 -m esptool version >/dev/null 2>&1; then
    esptool=(python3 -m esptool)
else
    echo "ERROR: esptool is unavailable; activate ESP-IDF 5.5.3 first" >&2
    exit 1
fi

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
timestamp="$(date '+%Y%m%dT%H%M%S%z')"
backup_dir="${repo_root}/device-backups/${timestamp}"
mkdir -p "${backup_dir}"
esptool_device=("${esptool[@]}" --chip esp32c3 --port "${port}" --baud "${baud}")

"${esptool_device[@]}" read_mac | tee "${backup_dir}/device.txt"
if ! grep -Fqi "MAC: ${expected_mac}" "${backup_dir}/device.txt"; then
    echo "ERROR: connected device does not match expected MAC ${expected_mac}; nothing was written" >&2
    exit 1
fi
"${esptool_device[@]}" read_flash 0x0 0x800000 "${backup_dir}/full-flash-pre.bin"
"${esptool_device[@]}" read_flash 0x356000 0x4000 "${backup_dir}/cardid-pre.bin"

"${esptool_device[@]}" write_flash --flash_size 8MB 0x10000 "${app_image}"
"${esptool_device[@]}" verify_flash 0x10000 "${app_image}"
"${esptool_device[@]}" read_flash 0x356000 0x4000 "${backup_dir}/cardid-post.bin"

if ! cmp -s "${backup_dir}/cardid-pre.bin" "${backup_dir}/cardid-post.bin"; then
    echo "ERROR: protected cardid changed; stop using this device and preserve ${backup_dir}" >&2
    exit 1
fi

shasum -a 256 "${backup_dir}/cardid-pre.bin" | tee "${backup_dir}/cardid.sha256"
echo "Safe application flash: PASS"
echo "Backup: ${backup_dir}"
echo "Protected cardid: byte-for-byte unchanged"
