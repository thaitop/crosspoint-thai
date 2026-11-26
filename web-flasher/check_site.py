#!/usr/bin/env python3
"""Check an assembled web-flasher site before it is deployed.

Usage: check_site.py <site_dir>   (exit 0 when ready, 1 with a list of problems)
"""

import json
import re
import struct
import sys
from pathlib import Path

DEVICES = {"x4pro": "ESP32-S3", "eink4plus": "ESP32-S31"}
IMAGES = [
    "home-calendar.png",
    "home-ledger.png",
    "home-today.png",
    "reader-thai.png",
    "settings-thai.png",
    "sleep-thai-calendar.png",
    "x4-pro.png",
    "ink4-plus.png",
]
MIN_BIN_BYTES = 1_000_000
# The flash is written in two parts that skip NVS, so the device secret
# (protected-book keys) survives an install.
APP_PART_OFFSET = 0xE000
PARTITION_TABLE_OFFSET = 0x8000


def find_nvs(boot: bytes) -> tuple[int, int] | None:
    """Return (offset, size) of the nvs partition in an image's partition table."""
    for pos in range(PARTITION_TABLE_OFFSET, min(len(boot), PARTITION_TABLE_OFFSET + 0xC00), 32):
        entry = boot[pos : pos + 32]
        if entry[:2] != b"\xaa\x50":
            break
        if entry[2] == 0x01 and entry[3] == 0x02:
            return struct.unpack("<II", entry[4:12])
    return None


def check_boot_part(site: Path, device: str) -> list[str]:
    path = site / f"{device}-boot.bin"
    if not path.is_file():
        return [f"missing {path.name}"]
    boot = path.read_bytes()
    nvs = find_nvs(boot)
    if nvs is None:
        return [f"{path.name}: no nvs partition in the partition table"]
    offset, size = nvs
    if offset != len(boot) or offset + size != APP_PART_OFFSET:
        return [f"{path.name}: parts must stop at nvs {offset:#x} and resume at its end {offset + size:#x}"]
    return []


def check_manifest(site: Path, device: str, chip: str) -> list[str]:
    path = site / f"{device}.json"
    if not path.is_file():
        return [f"missing {path.name}"]
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as err:
        return [f"{path.name}: invalid JSON ({err})"]
    problems = []
    if manifest.get("version") in (None, "", "dev"):
        problems.append(f"{path.name}: version not set to a release tag")
    if manifest.get("new_install_prompt_erase") is not False:
        problems.append(f"{path.name}: new_install_prompt_erase must be false")
    if manifest.get("new_install_improv_wait_time") != 0:
        problems.append(f"{path.name}: new_install_improv_wait_time must be 0 (firmware has no Improv)")
    builds = manifest.get("builds", [])
    expected_parts = [
        {"path": f"{device}-boot.bin", "offset": 0},
        {"path": f"{device}-app.bin", "offset": APP_PART_OFFSET},
    ]
    if len(builds) != 1 or builds[0].get("chipFamily") != chip or builds[0].get("parts") != expected_parts:
        problems.append(f"{path.name}: expected one {chip} build with parts {expected_parts}")
    return problems


def check_site(site: Path) -> list[str]:
    problems = []
    for name in ("flasher.js", "style.css"):
        if not (site / name).is_file():
            problems.append(f"missing {name}")
    index = site / "index.html"
    if not index.is_file():
        problems.append("missing index.html")
    else:
        html = index.read_text(encoding="utf-8")
        for attr, url in re.findall(r'\b(src|href|manifest)="(/[^"]*)"', html):
            problems.append(f"index.html: absolute URL {attr}=\"{url}\" breaks under the Pages subpath")
    for device, chip in DEVICES.items():
        problems += check_manifest(site, device, chip)
        problems += check_boot_part(site, device)
        names = [f"{device}-app.bin"] + (["x4pro-firmware.bin"] if device == "x4pro" else [])
        for name in names:
            path = site / name
            if not path.is_file():
                problems.append(f"missing {name}")
            elif path.stat().st_size < MIN_BIN_BYTES:
                problems.append(f"{name}: only {path.stat().st_size} bytes")
    for name in IMAGES:
        if not (site / "images" / name).is_file():
            problems.append(f"missing images/{name}")
    return problems


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    problems = check_site(Path(sys.argv[1]))
    for problem in problems:
        print(f"FAIL: {problem}")
    if not problems:
        print("OK: site is ready")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
