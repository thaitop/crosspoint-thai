# Web flasher for every device — design

Date: 2026-11-26. Branch: `thai-2.0.0` (default branch of `thaitop/crosspoint-thai`).
Builds on [2026-11-26-web-flasher-design.md](2026-11-26-web-flasher-design.md).

## Intent

People who own other CrossPoint devices asked for the Thai firmware. The owner
wants the web flasher (https://thaitop.github.io/crosspoint-thai/) to install on
every device the firmware supports, not only X4 Pro and E-Ink4-Plus.

## Goals

- One-click USB install for all 9 devices:

  | id (asset prefix) | Name on page | Chip | Source branch | PlatformIO env |
  | --- | --- | --- | --- | --- |
  | `x4pro` | Xteink X4 Pro | ESP32-S3 | `thai-2.0.0` | `x4pro-gh_release` |
  | `eink4plus` | Metalio E-Ink4-Plus | ESP32-S31 | `thai-eink4plus` | `metalio_eink4_plus` |
  | `x3-x4` | Xteink X3 / X4 | ESP32-C3 | `thai-2.0.0` | `gh_release` |
  | `x4c` | Xteink X4C | ESP32-S3 | `thai-2.0.0` | `x4c-gh_release` |
  | `sticky` | Sticky | ESP32-S3 | `thai-2.0.0` | `sticky-gh_release` |
  | `papermono` | PaperMono | ESP32-S3 | `thai-2.0.0` | `papermono-gh_release` |
  | `metalio_eink4` | Metalio E-Ink4 | ESP32-S3 | `thai-2.0.0` | `metalio_eink4-gh_release` |
  | `eego_a4` | EEGO A4 | ESP32-S3 | `thai-2.0.0` | `eego_a4-gh_release` |
  | `picco` | Onyx Picco | ESP32-S3 | `thai-2.0.0` | `picco-gh_release` |

  Envs and device ids follow `.github/workflows/release.yml:19-43`.
- GitHub Actions builds all 9 firmwares when a `th-*` release is published and
  attaches them to that release; then the Pages site redeploys with them.
- A disclaimer at the bottom of the page.
- Installing keeps settings, books and the NVS device secret (same rule as today).

## Non-goals

- No "tested / untested" labels on devices (owner's decision). The disclaimer
  covers the risk instead.
- No change to firmware code. If a device's build fails or overflows in CI,
  that is a separate task.
- Upstream `v*` releases and `release.yml`'s build matrix stay as they are.

## Anti-goals

- Writing anything at 0x0 or over NVS on Picco: it must keep its vendor
  bootloader, partition table, otadata and `safeboot` recovery partition
  (`scripts/app_only_upload.py`).
- Flashing `*-factory.bin` at 0x0 on any device (wipes NVS).
- A long page where nine identical buttons make it easy to flash the wrong model.

## Approaches considered

Layout:
- **A. Pick the device first (chosen).** A row of 9 device tiles; selecting one
  shows a single install panel for it. Short page, hard to hit the wrong button.
  Costs a little JS to switch panels.
- B. Nine cards in a 3×3 grid. Least code, but a very long page on phones and
  nine look-alike install buttons.

Firmware source:
- **ก. CI builds on `th-*` release (chosen).** No manual 9× builds. Plus needs its
  own job (other branch, newer pioarduino platform/core).
- ข. Owner builds 18 files locally and uploads them. No CI work, but 9 builds per
  release.

## Design

### 1. Release build workflow (new: `.github/workflows/release-thai.yml`)

- Trigger: `release: published` with `startsWith(tag_name, 'th-')`, plus
  `workflow_dispatch` with a `tag` input (rebuild an existing release).
- Job `build` (matrix of the 8 `thai-2.0.0` devices): checkout the release tag
  with `submodules: recursive`; same Python / uv / pioarduino 6.1.19 setup and
  penv pin as `release.yml:62-91`; same optional `FREEINK_LCP_RELEASE_KEY`
  handling (`release.yml:93-104`). No version check against `platformio.ini`
  (th tags are dates). Upload `<id>-firmware.bin` and, for all but Picco,
  `<id>-firmware.factory.bin` (from `.pio/build/<env>/`) as artifacts.
- Job `build-plus`: checkout `thai-eink4plus` (see "Plus revision" below), recursive
  submodules (`.gitmodules` points at the public fork `thaitop/freeink-sdk`), build
  `metalio_eink4_plus` with the pioarduino core version that branch needs
  (6.2.0, see its CI or `platformio.ini`), upload `eink4plus-firmware.bin` and
  `eink4plus-firmware.factory.bin`.
- Job `publish` (needs both): download artifacts, assert all 9 `-firmware.bin`
  exist, `gh release upload --clobber`, then start the Pages deploy (call
  `pages-flasher.yml` as a reusable workflow with the tag, or `gh workflow run`).
- `fail-fast: false`, but `publish` only runs if every build succeeds, so the site
  never ships with a device missing.

Plus revision: build the tip of `thai-eink4plus` at release time and append
"E-Ink4-Plus: `<sha>`" to the release notes, so the notes still list both commits
as today. (Builder may use a separate tag instead if simpler; record the sha either way.)

### 2. Stop `release.yml` failing on th tags

Add `if: startsWith(github.event.release.tag_name, 'v')` to its `build-release`
job. Today it fails on every `th-*` release (run 45778114370).

### 3. Pages workflow (`pages-flasher.yml`)

- Stop triggering on `release: published` (bins are not attached yet at that
  moment); trigger from `release-thai.yml` and `workflow_dispatch` only.
- The site always serves the latest non-pre-release `th-*` release (keep the
  existing "Pick release" step). `release-thai.yml` starts a deploy only for a
  non-pre-release, and only if that tag is the latest one, so rebuilding an old
  tag or testing a pre-release never changes the live page. *(Claude's call; owner to check.)*
- Loop over all 9 ids. For every id except Picco, split the factory image with
  `split_factory.py` into `<id>-boot.bin` + `<id>-app.bin`. Keep each
  `<id>-firmware.bin` for the SD-card download. Delete factory images.
- Set `"version"` in every manifest to the tag.

### 4. Per-device flash layout

- `web-flasher/devices.json` (new) is the single list of devices: id, name,
  chip family, photo, short description, and `layout`: `"split"` or `"app-only"`.
  Used by `check_site.py`, the workflow loop (via a tiny Python/jq read), and the
  page JS. Builder may instead keep the list in `check_site.py` and generate the
  rest from it — one source of truth is what matters.
- `split` devices: manifest parts `<id>-boot.bin`@0x0 and `<id>-app.bin`@0xE000
  (all share `partitions.csv`: NVS 0x9000–0xE000).
- Picco (`app-only`): manifest has one part `picco-firmware.bin` at 0xE0000
  (ota_0 in `partitions_picco.csv`). Nothing else is written, so the vendor
  bootloader, partition table, otadata, NVS and safeboot stay.
  This is exactly what the documented install does: `pio run -e picco -t upload`
  writes only the app at 0xE0000 (`platformio.ini:688-697`,
  `scripts/app_only_upload.py`), so the web install matches it.
- One manifest per device `web-flasher/<id>.json`, `chipFamily` from the table.
- `check_site.py` validates every device: manifest parts match its layout,
  files exist and are > `MIN_BIN_BYTES`, split boot parts contain a partition
  table whose NVS ends at 0xE000, every image listed exists.

### 5. Page (`index.html`, `flasher.js`, `style.css`)

- Install section: keep the 3 steps, then a device picker (radio-group semantics,
  keyboard accessible, Paper Green look from `web-flasher/DESIGN.md`, no new
  colors), then one panel for the selected device: photo, name, chip, short
  line, install button, SD-card fallback with `<id>-firmware.bin` downloaded as
  `firmware.bin`. SD fallback is shown for every device except Picco:
  `SdFirmwareUpdateActivity` is in both branches (`SettingsActivity.cpp:147`),
  but Picco's stock table has only one OTA slot, so an in-device update has
  nowhere to write. Picco's panel instead links to Releases. Device list
  carries a `sd_update` flag; `check_site.py` requires `<id>-firmware.bin` only
  where it is true. *(Claude's call; owner to check.)*
- No device is preselected; the panel says "เลือกรุ่นของเครื่องด้านบน" until
  one is chosen. Selection is kept in the URL hash (`#install-picco`) so links
  can point at a model.
- `flasher.js` reads device data from the selected panel (same `data-*` fields
  as today), so the flash dialog code is unchanged.
- Hero lead and meta description: say "ทุกรุ่นที่ CrossPoint รองรับ" instead of
  naming two models.
- Remove the note "หน้านี้รองรับเฉพาะ X4 Pro และ E-Ink4-Plus …"; keep the
  USB-lock and "go back to original firmware" links.
- Photos: X4 Pro and Plus keep their photos. The other 7 use a simple device
  outline icon (SVG, same stroke style as the wordmark) until real photos exist.

### 6. Disclaimer (bottom of page, above footer)

Short Thai text, plain tone. Content (wording is the builder's, owner reviews):
- Firmware is provided as is, without warranty; install at your own risk.
- Not every model has been tried on a real device by the developer; if
  something goes wrong, report it on GitHub Issues (link).
- You can always return to the original firmware with CrossPoint's web flasher
  (link) or the vendor's tool.
- Not affiliated with Xteink, Metalio, Onyx, or other makers (already in footer;
  move or keep, don't duplicate).

## What is left to the builder

- Rule for the release workflow's "assert all 9" gate: Picco uploads only
  `picco-firmware.bin` (no factory image).
- Exact workflow wiring between `release-thai.yml` and `pages-flasher.yml`.
- Where the device list lives, as long as there is one.
- Picker markup/CSS details within the existing design system.
- Disclaimer wording.

## Verification

- `python3 web-flasher/check_site.py _site` on a locally assembled site.
- `workflow_dispatch` of `release-thai.yml` against a test `th-*` pre-release;
  check 9 assets attached and that no deploy ran. Then run `pages-flasher.yml`
  build job's assemble + `check_site.py` steps locally against those assets.
- Open the page in Chrome: pick each device, check manifest URL and SD link;
  keyboard-only selection; phone width.
- Hardware: owner flashes X4 Pro and Plus from the deployed page (settings kept).
  Other devices: first owner reports via Issues.
