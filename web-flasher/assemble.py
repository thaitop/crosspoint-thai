#!/usr/bin/env python3
"""Turn downloaded release firmware into the files the web flasher serves.

Usage: assemble.py <site_dir> <tag>

The site directory holds devices.json plus, per build, <build>-firmware.bin and
(for split devices) <build>-firmware.factory.bin. For split builds this writes
<build>-boot.bin (bootloader + partition table, up to nvs) and <build>-app.bin (from
the end of nvs: otadata + app), so an install keeps the device secret. It then
writes one ESP Web Tools manifest per device and deletes the factory images.
"""

import json
import sys
from pathlib import Path

from check_site import APP_PART_OFFSET, expected_parts, find_nvs, load_devices


def split_factory(site: Path, build: str) -> list[str]:
    path = site / f"{build}-firmware.factory.bin"
    if not path.is_file():
        return [f"missing {path.name}"]
    image = path.read_bytes()
    nvs = find_nvs(image)
    if nvs is None:
        return [f"{path.name}: no nvs partition found"]
    offset, size = nvs
    if offset + size != APP_PART_OFFSET:
        return [f"{path.name}: nvs ends at {offset + size:#x}, manifests expect {APP_PART_OFFSET:#x}"]
    (site / f"{build}-boot.bin").write_bytes(image[:offset])
    (site / f"{build}-app.bin").write_bytes(image[offset + size :])
    return []


def manifest(device: dict, tag: str) -> dict:
    return {
        "name": f"CrossPoint ภาษาไทย – {device['name']}",
        "version": tag,
        "new_install_prompt_erase": False,
        "new_install_improv_wait_time": 0,
        "builds": [{"chipFamily": device["chip"], "parts": expected_parts(device)}],
    }


def assemble(site: Path, tag: str) -> list[str]:
    problems = []
    split_builds = set()
    for device in load_devices(site):
        if device["layout"] == "split" and device["build"] not in split_builds:
            split_builds.add(device["build"])
            problems += split_factory(site, device["build"])
        text = json.dumps(manifest(device, tag), ensure_ascii=False, indent=2) + "\n"
        (site / f"{device['id']}.json").write_text(text, encoding="utf-8")
    for factory in site.glob("*-firmware.factory.bin"):
        factory.unlink()
    return problems


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    problems = assemble(Path(sys.argv[1]), sys.argv[2])
    for problem in problems:
        print(f"FAIL: {problem}", file=sys.stderr)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
