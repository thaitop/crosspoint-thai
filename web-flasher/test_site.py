"""Tests for assemble.py and check_site.py.

Run: python3 -m unittest discover -s web-flasher -p 'test_*.py' -v
"""

import json
import re
import shutil
import struct
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from assemble import assemble  # noqa: E402
from check_site import check_site, load_devices  # noqa: E402

BIN_BYTES = 2_000_000


def fake_factory(nvs_offset=0x9000, nvs_size=0x5000, total=BIN_BYTES) -> bytes:
    image = bytearray(total)
    entry = b"\xaa\x50\x01\x02" + struct.pack("<II", nvs_offset, nvs_size)
    image[0x8000 : 0x8000 + 32] = entry.ljust(32, b"\0")
    return bytes(image)


def make_site(tmp: Path) -> Path:
    site = tmp / "site"
    site.mkdir()
    for name in ("index.html", "style.css", "flasher.js", "partitions.js", "favicon.svg", "devices.json"):
        shutil.copy(HERE / name, site / name)
    shutil.copytree(HERE / "images", site / "images")
    for png in (HERE.parent / "docs" / "images" / "thai").glob("*.png"):
        shutil.copy(png, site / "images" / png.name)
    for device in load_devices(HERE):
        (site / f"{device['build']}-firmware.bin").write_bytes(bytes(BIN_BYTES))
        if device["layout"] == "split":
            (site / f"{device['build']}-firmware.factory.bin").write_bytes(fake_factory())
    return site


class SiteTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.site = make_site(Path(self._tmp.name))

    def tearDown(self):
        self._tmp.cleanup()

    def manifest(self, device_id):
        return json.loads((self.site / f"{device_id}.json").read_text(encoding="utf-8"))

    def test_assembled_site_passes(self):
        self.assertEqual(assemble(self.site, "th-test"), [])
        self.assertEqual(check_site(self.site), [])

    def test_split_manifest_parts(self):
        assemble(self.site, "th-test")
        manifest = self.manifest("x4pro")
        self.assertEqual(manifest["version"], "th-test")
        self.assertEqual(
            manifest["builds"],
            [
                {
                    "chipFamily": "ESP32-S3",
                    "parts": [
                        {"path": "x4pro-boot.bin", "offset": 0},
                        {"path": "x4pro-app.bin", "offset": 57344},
                    ],
                }
            ],
        )

    def test_picco_app_only(self):
        assemble(self.site, "th-test")
        self.assertEqual(self.manifest("picco")["builds"][0]["parts"], [{"path": "picco-firmware.bin", "offset": 917504}])
        self.assertFalse((self.site / "picco-boot.bin").exists())
        self.assertFalse((self.site / "picco-app.bin").exists())

    def test_x3_and_x4_share_one_build(self):
        assemble(self.site, "th-test")
        for device_id in ("x3", "x4"):
            build = self.manifest(device_id)["builds"][0]
            self.assertEqual(build["chipFamily"], "ESP32-C3")
            self.assertEqual([p["path"] for p in build["parts"]], ["x3-x4-boot.bin", "x3-x4-app.bin"])
        self.assertFalse((self.site / "x3-boot.bin").exists())

    def test_no_factory_images_left(self):
        assemble(self.site, "th-test")
        self.assertEqual(list(self.site.glob("*factory*")), [])

    def test_split_rejects_other_nvs_end(self):
        (self.site / "x4pro-firmware.factory.bin").write_bytes(fake_factory(nvs_size=0xC000))
        problems = assemble(self.site, "th-test")
        self.assertTrue(any("x4pro-firmware.factory.bin" in p for p in problems), problems)

    def test_missing_firmware_fails(self):
        assemble(self.site, "th-test")
        (self.site / "sticky-firmware.bin").unlink()
        self.assertIn("missing sticky-firmware.bin", check_site(self.site))

    def test_page_devices_match_list(self):
        assemble(self.site, "th-test")
        index = self.site / "index.html"
        html = index.read_text(encoding="utf-8")
        self.assertIn('data-device="papermono"', html)
        html = html.replace('data-device="papermono"', "", 1)
        html = html.replace("</main>", '<div data-device="nope" data-flash="nope.json"></div></main>', 1)
        index.write_text(html, encoding="utf-8")
        problems = check_site(self.site)
        self.assertTrue(any("papermono" in p for p in problems), problems)
        self.assertTrue(any("nope" in p for p in problems), problems)

    def test_page_chip_and_layout_match_list(self):
        assemble(self.site, "th-test")
        index = self.site / "index.html"
        html = index.read_text(encoding="utf-8")
        html = html.replace('data-flash="x3.json" data-name="X3" data-chip="ESP32-C3"', 'data-flash="x3.json" data-name="X3" data-chip="ESP32-S3"', 1)
        html = html.replace('data-chip="ESP32-S3" data-layout="app-only"', 'data-chip="ESP32-S3" data-layout="split"', 1)
        index.write_text(html, encoding="utf-8")
        problems = check_site(self.site)
        self.assertTrue(any("x3" in p and "ESP32-C3" in p for p in problems), problems)
        self.assertTrue(any("picco" in p and "app-only" in p for p in problems), problems)

    def test_every_brand_has_a_tile(self):
        assemble(self.site, "th-test")
        index = self.site / "index.html"
        index.write_text(index.read_text(encoding="utf-8").replace('value="eego"', 'value="x"', 1), encoding="utf-8")
        self.assertTrue(any("eego" in p for p in check_site(self.site)))

    def test_missing_page_image_fails(self):
        assemble(self.site, "th-test")
        (self.site / "images" / "seeed.png").unlink()
        self.assertIn("missing images/seeed.png", check_site(self.site))

    def test_partitions_module_required(self):
        assemble(self.site, "th-test")
        (self.site / "partitions.js").unlink()
        self.assertIn("missing partitions.js", check_site(self.site))

    def test_dev_version_rejected(self):
        assemble(self.site, "dev")
        self.assertTrue(any("version" in p for p in check_site(self.site)))


class ReleaseMatrixTest(unittest.TestCase):
    def test_matrix_builds_every_listed_device(self):
        workflow = (HERE.parent / ".github" / "workflows" / "release-thai.yml").read_text(encoding="utf-8")
        matrix_ids = re.findall(r"^\s*- \{ id: ([\w-]+),", workflow, re.MULTILINE)
        builds = {d["build"] for d in load_devices(HERE)}
        self.assertEqual(sorted(matrix_ids), sorted(builds))
        gate = re.search(r"for id in ([\w\s-]+); do", workflow).group(1).split()
        self.assertEqual(sorted(gate), sorted(builds))


if __name__ == "__main__":
    unittest.main()
