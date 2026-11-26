#!/usr/bin/env python3
"""Check an assembled web-flasher site before it is deployed.

Usage: check_site.py <site_dir>   (exit 0 when ready, 1 with a list of problems)
"""

import json
import re
import struct
import sys
from pathlib import Path

IMAGES = [
    "home-calendar.png",
    "home-ledger.png",
    "home-today.png",
    "reader-thai.png",
    "settings-thai.png",
    "sleep-thai-calendar.png",
]
MIN_BIN_BYTES = 1_000_000
# Split devices are written in two parts that skip NVS, so the device secret
# (protected-book keys) survives an install.
APP_PART_OFFSET = 0xE000
# Picco keeps its vendor bootloader, partition table and safeboot partition:
# only the app is written, into the stock ota_0 slot (partitions_picco.csv).
PICCO_APP_OFFSET = 0xE0000
PARTITION_TABLE_OFFSET = 0x8000


def load_devices(directory: Path) -> list[dict]:
    """Return the device list (devices.json) kept in a site or source directory."""
    return json.loads((directory / "devices.json").read_text(encoding="utf-8"))


def find_nvs(boot: bytes) -> tuple[int, int] | None:
    """Return (offset, size) of the nvs partition in an image's partition table."""
    for pos in range(PARTITION_TABLE_OFFSET, min(len(boot), PARTITION_TABLE_OFFSET + 0xC00), 32):
        entry = boot[pos : pos + 32]
        if entry[:2] != b"\xaa\x50":
            break
        if entry[2] == 0x01 and entry[3] == 0x02:
            return struct.unpack("<II", entry[4:12])
    return None


def expected_parts(device: dict) -> list[dict]:
    """Manifest parts; files are named by build, which X3 and X4 share."""
    build = device["build"]
    if device["layout"] == "app-only":
        return [{"path": f"{build}-firmware.bin", "offset": PICCO_APP_OFFSET}]
    return [
        {"path": f"{build}-boot.bin", "offset": 0},
        {"path": f"{build}-app.bin", "offset": APP_PART_OFFSET},
    ]


def check_boot_part(site: Path, build: str) -> list[str]:
    path = site / f"{build}-boot.bin"
    if not path.is_file():
        return []  # reported as a missing part
    boot = path.read_bytes()
    nvs = find_nvs(boot)
    if nvs is None:
        return [f"{path.name}: no nvs partition in the partition table"]
    offset, size = nvs
    if offset != len(boot) or offset + size != APP_PART_OFFSET:
        return [f"{path.name}: parts must stop at nvs {offset:#x} and resume at its end {offset + size:#x}"]
    return []


def check_manifest(site: Path, device: dict) -> list[str]:
    path = site / f"{device['id']}.json"
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
    parts = expected_parts(device)
    if len(builds) != 1 or builds[0].get("chipFamily") != device["chip"] or builds[0].get("parts") != parts:
        problems.append(f"{path.name}: expected one {device['chip']} build with parts {parts}")
    return problems


def check_page_devices(html: str, devices: list[dict]) -> list[str]:
    """Each listed device has exactly one install panel, and no panel is unlisted."""
    problems = []
    on_page = re.findall(r'\bdata-device="([^"]+)"', html)
    listed = [d["id"] for d in devices]
    panels = {m.group(1): m.group(0) for m in re.finditer(r'<article\b[^>]*\bdata-device="([^"]+)"[^>]*>', html)}
    for device in devices:
        device_id = device["id"]
        count = on_page.count(device_id)
        if count != 1:
            problems.append(f'index.html: expected one data-device="{device_id}" panel, found {count}')
            continue
        tag = panels.get(device_id, "")
        for attr, value in (("data-flash", f"{device_id}.json"), ("data-chip", device["chip"]), ("data-layout", device["layout"])):
            if f'{attr}="{value}"' not in tag:
                problems.append(f'index.html: panel {device_id} needs {attr}="{value}"')
    for device_id in sorted(set(on_page) - set(listed)):
        problems.append(f'index.html: data-device="{device_id}" is not in devices.json')
    brand_tiles = set(re.findall(r'name="brand" value="([^"]+)"', html))
    for brand in sorted({d["brand"] for d in devices} - brand_tiles):
        problems.append(f'index.html: no brand tile for "{brand}"')
    return problems


def check_bin(site: Path, name: str) -> list[str]:
    path = site / name
    if not path.is_file():
        return [f"missing {name}"]
    if path.stat().st_size < MIN_BIN_BYTES:
        return [f"{name}: only {path.stat().st_size} bytes"]
    return []


def check_site(site: Path) -> list[str]:
    problems = []
    for name in ("flasher.js", "partitions.js", "style.css", "devices.json"):
        if not (site / name).is_file():
            problems.append(f"missing {name}")
    if problems:
        return problems
    devices = load_devices(site)
    index = site / "index.html"
    if not index.is_file():
        problems.append("missing index.html")
    else:
        html = index.read_text(encoding="utf-8")
        for attr, url in re.findall(r'\b(src|href|manifest)="(/[^"]*)"', html):
            problems.append(f"index.html: absolute URL {attr}=\"{url}\" breaks under the Pages subpath")
        problems += check_page_devices(html, devices)
    for factory in sorted(site.glob("*factory*")):
        problems.append(f"{factory.name}: factory images wipe NVS and must not be deployed")
    images = set(IMAGES)
    if index.is_file():
        images.update(re.findall(r'\bsrc="images/([^"]+)"', index.read_text(encoding="utf-8")))
    files = set()
    for device in devices:
        problems += check_manifest(site, device)
        files.update(part["path"] for part in expected_parts(device))
        if device["layout"] == "split" and device["sd_update"]:
            files.add(f"{device['build']}-firmware.bin")
    for name in sorted(files):
        if name.endswith("-boot.bin"):  # bootloader + partition table: small by design
            problems += [f"missing {name}"] if not (site / name).is_file() else check_boot_part(site, name[: -len("-boot.bin")])
        else:
            problems += check_bin(site, name)
    for name in sorted(images):
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
