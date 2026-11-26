#!/usr/bin/env python3
"""Split a factory image into the two parts the web flasher writes.

Usage: split_factory.py <device-firmware.factory.bin> <out_prefix>

Writes <out_prefix>-boot.bin (bootloader + partition table, up to nvs) and
<out_prefix>-app.bin (from the end of nvs: otadata + app). Skipping nvs keeps
the device secret, so protected books still open after an install.
"""

import sys
from pathlib import Path

from check_site import APP_PART_OFFSET, find_nvs


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    image = Path(sys.argv[1]).read_bytes()
    nvs = find_nvs(image)
    if nvs is None:
        print(f"{sys.argv[1]}: no nvs partition found", file=sys.stderr)
        return 1
    offset, size = nvs
    if offset + size != APP_PART_OFFSET:
        print(f"{sys.argv[1]}: nvs ends at {offset + size:#x}, manifests expect {APP_PART_OFFSET:#x}", file=sys.stderr)
        return 1
    Path(f"{sys.argv[2]}-boot.bin").write_bytes(image[:offset])
    Path(f"{sys.argv[2]}-app.bin").write_bytes(image[offset + size :])
    return 0


if __name__ == "__main__":
    sys.exit(main())
