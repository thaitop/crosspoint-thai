# Web Flasher for Every Device Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The Pages web flasher installs the Thai firmware on all 9 supported devices, with firmware built by GitHub Actions on every `th-*` release.

**Architecture:** One device list (`web-flasher/devices.json`) drives a site assembler (`assemble.py`: split images, write manifests), the site checker (`check_site.py`) and a consistency check against the static page. A new `release-thai.yml` builds all 9 firmwares from a matrix (8 rows on the release tag, 1 row on `thai-eink4plus` with pioarduino 6.2.0), attaches them, and calls `pages-flasher.yml` as a reusable workflow. The page gets a radio-group device picker that shows one install panel at a time; `flasher.js` flash code is unchanged.

**Tech Stack:** GitHub Actions, PlatformIO (pioarduino core 6.1.19 / 6.2.0), Python 3 stdlib (`unittest`, no new deps), static HTML/CSS/JS, ESP Web Tools 10.5.2 (already pinned).

**Spec:** `docs/superpowers/specs/2026-11-26-web-flasher-all-devices-design.md`

## Global Constraints

- Device ids, names, chips, branches and envs (one source: `web-flasher/devices.json`):

  | id | name | chip | layout | sd_update | photo | ref | env |
  | --- | --- | --- | --- | --- | --- | --- | --- |
  | `x4pro` | Xteink X4 Pro | ESP32-S3 | split | true | `x4-pro.png` | tag | `x4pro-gh_release` |
  | `eink4plus` | Metalio E-Ink4-Plus | ESP32-S31 | split | true | `ink4-plus.png` | `thai-eink4plus` | `metalio_eink4_plus` |
  | `x3-x4` | Xteink X3 / X4 | ESP32-C3 | split | true | — | tag | `gh_release` |
  | `x4c` | Xteink X4 Classic (X4C) | ESP32-S3 | split | true | — | tag | `x4c-gh_release` |
  | `sticky` | Seeed Sticky | ESP32-S3 | split | true | — | tag | `sticky-gh_release` |
  | `papermono` | M5Stack Paper Mono | ESP32-S3 | split | true | — | tag | `papermono-gh_release` |
  | `metalio_eink4` | Metalio E-Ink 4 | ESP32-S3 | split | true | — | tag | `metalio_eink4-gh_release` |
  | `eego_a4` | EEGO Reader A4 | ESP32-S3 | split | true | — | tag | `eego_a4-gh_release` |
  | `picco` | Onyx Picco / BOOX Tiles | ESP32-S3 | app-only | false | — | tag | `picco-gh_release` |

- `split`: manifest parts `<id>-boot.bin`@`0` and `<id>-app.bin`@`57344` (0xE000); boot part's NVS must end at 0xE000.
- `app-only` (Picco): manifest has exactly one part `picco-firmware.bin`@`917504` (0xE0000). Never write at 0x0 on Picco.
- Never put `*-factory.bin` in the site.
- Manifests keep `new_install_prompt_erase: false`, `new_install_improv_wait_time: 0`, `version` = release tag.
- Live site serves the latest non-pre-release `th-*` release only.
- Release assets: `<id>-firmware.bin` for all 9, `<id>-firmware.factory.bin` for the 8 split devices.
- Page: no "tested / untested" labels; Paper Green look from `web-flasher/DESIGN.md`, no new colors; all copy in Thai; relative URLs only.
- Python: stdlib only, runs on `python3` (3.10+) in CI and locally.
- Commits only after the owner approves each one (`CLAUDE.md`, Human ownership). Never push.

## Review Focus

- A device id present in `devices.json` but missing from `index.html` (or the reverse) → `check_site.py` must fail, not deploy a dead button. (Task 1 test `test_page_devices_match_list`.)
- A factory image whose NVS does not end at 0xE000 (e.g. someone builds Picco as split) → `assemble.py` must exit non-zero naming the file. (Task 1 test `test_split_rejects_other_nvs_end`.)
- One matrix build fails (e.g. X3/X4 overflows flash) → no assets uploaded, no deploy, release left without bins and the run red. (Task 2 gate `test "$(ls dist | wc -l)" -eq 17`.)
- Opening `…/#install-picco` directly, or with JS off → that panel shows / all panels show, and the install button still works. (Task 3 manual checks.)
- Browser without Web Serial → every panel hides its button and shows the "unsupported" text, as today. (Task 3 manual check.)

---

### Task 1: Device list, site assembler and checker

**Files:**
- Create: `web-flasher/devices.json`
- Create: `web-flasher/assemble.py` (replaces `split_factory.py`)
- Delete: `web-flasher/split_factory.py`, `web-flasher/x4pro.json`, `web-flasher/eink4plus.json` (manifests are now generated)
- Modify: `web-flasher/check_site.py` (whole device section, `:13-118`)
- Test: `web-flasher/test_site.py`

**Interfaces:**
- Produces:
  - `devices.json`: JSON array of `{"id": str, "name": str, "chip": str, "layout": "split"|"app-only", "sd_update": bool, "photo": str|null}` in the table order.
  - `check_site.load_devices(site_or_src: Path) -> list[dict]` reads `devices.json` from that directory.
  - `check_site.APP_PART_OFFSET = 0xE000`, `check_site.PICCO_APP_OFFSET = 0xE0000`, `check_site.find_nvs(image: bytes) -> tuple[int, int] | None` (unchanged).
  - `check_site.expected_parts(device: dict) -> list[dict]` — manifest parts per layout.
  - `check_site.check_site(site: Path) -> list[str]` — problems, empty when ready.
  - `assemble.assemble(site: Path, tag: str) -> list[str]` — problems; on success the site holds only deployable files.
  - CLI: `python3 web-flasher/assemble.py <site_dir> <tag>` exit 0/1; `python3 web-flasher/check_site.py <site_dir>` exit 0/1 (unchanged).
  - `index.html` contract checked here: each device has exactly one element with `data-device="<id>"` and `data-flash="<id>.json"`.

- [ ] **Step 1: Write failing tests in `web-flasher/test_site.py` (`unittest`)**

  Helper `fake_factory(nvs_offset=0x9000, nvs_size=0x5000, total=2_000_000) -> bytes`: zero bytes with a partition-table entry at 0x8000 (`b"\xaa\x50\x01\x02" + struct.pack("<II", nvs_offset, nvs_size)` padded to 32 bytes). Helper `make_site(tmp)` copies `web-flasher/` (html, css, js, devices.json, images) into `tmp`, adds `docs/images/thai/*.png` to `tmp/images`, writes `fake_factory()` as `<id>-firmware.factory.bin` and 2 MB `<id>-firmware.bin` for each device per Global Constraints.

  Tests:
  - `test_assembled_site_passes`: `assemble(site, "th-test") == []` and `check_site(site) == []`.
  - `test_split_manifest_parts`: `x4pro.json` builds == `[{"chipFamily": "ESP32-S3", "parts": [{"path": "x4pro-boot.bin", "offset": 0}, {"path": "x4pro-app.bin", "offset": 57344}]}]`, `version == "th-test"`.
  - `test_picco_app_only`: `picco.json` parts == `[{"path": "picco-firmware.bin", "offset": 917504}]`; no `picco-boot.bin`, no `picco-app.bin` in site.
  - `test_x3_x4_chip`: `x3-x4.json` `chipFamily == "ESP32-C3"`.
  - `test_no_factory_images_left`: no `*factory*` file in site after assemble.
  - `test_split_rejects_other_nvs_end`: x4pro factory built with `nvs_size=0xC000` → `assemble` returns a problem containing `"x4pro-firmware.factory.bin"`.
  - `test_missing_firmware_fails`: delete `sticky-firmware.bin` after assemble → `check_site` problems contain `"missing sticky-firmware.bin"`.
  - `test_page_devices_match_list`: remove the `data-device="papermono"` element's attribute from site `index.html` → problem contains `"papermono"`; add a fake `data-device="nope"` → problem contains `"nope"`.
  - `test_dev_version_rejected`: a manifest with `"version": "dev"` fails (existing rule kept).

- [ ] **Step 2: Run and confirm failure**

  Run: `python3 -m unittest discover -s web-flasher -p 'test_*.py' -v`
  Expected: FAIL / ImportError (`assemble` missing).

- [ ] **Step 3: Write `devices.json`, `assemble.py`, rewrite `check_site.py`**

  - `assemble`: for each device: split → read `<id>-firmware.factory.bin`, check NVS ends at `APP_PART_OFFSET` (error names the file), write `<id>-boot.bin` (`[:nvs_offset]`) and `<id>-app.bin` (`[nvs_end:]`); app-only → nothing to split. Write `<id>.json` (`name` = "CrossPoint ภาษาไทย – <name>", fields per Global Constraints). Delete every `*-firmware.factory.bin`. Delete `<id>-firmware.bin` for split devices with `sd_update == false` (none today; keep the rule).
  - `check_site`: keep existing checks (files, absolute URLs, version, prompt/improv flags, boot part NVS, `MIN_BIN_BYTES`). Per device: manifest parts == `expected_parts`, chip matches, every part file exists and ≥ `MIN_BIN_BYTES`, `<id>-firmware.bin` exists when `sd_update` or `app-only`, `images/<photo>` exists when set, page contract (regex `data-device="([^"]+)"` set == device id set, each with `data-flash="<id>.json"`). `IMAGES` keeps the 6 screenshots; device photos come from the list.
  - Index.html will not yet have `data-device` — `test_page_devices_match_list` and `test_assembled_site_passes` stay red until Task 3. Mark both `@unittest.expectedFailure` now and remove the marks in Task 3.

- [ ] **Step 4: Run tests**

  Run: `python3 -m unittest discover -s web-flasher -p 'test_*.py' -v`
  Expected: all pass (2 expected failures).

- [ ] **Step 5: Ask the owner to approve, then commit**

  `git add web-flasher/ && git commit -m "feat(web-flasher): device list drives manifests and site check"` (with the Co-Authored-By trailer).

### Task 2: Release build workflow and Pages wiring

**Files:**
- Create: `.github/workflows/release-thai.yml`
- Modify: `.github/workflows/pages-flasher.yml` (triggers, Assemble site step)
- Modify: `.github/workflows/release.yml:14` (job `if`)

**Interfaces:**
- Consumes: `assemble.py <site> <tag>`, `check_site.py <site>`, `devices.json` (Task 1).
- Produces: `pages-flasher.yml` callable with `on: workflow_call` (no inputs; it picks the latest non-pre-release `th-*` itself) and `workflow_dispatch`.

- [ ] **Step 1: `release.yml`** — add `if: startsWith(github.event.release.tag_name, 'v')` to job `build-release`.

- [ ] **Step 2: `release-thai.yml`**

  - `on: release: [published]` and `workflow_dispatch: inputs: tag (required string)`. `TAG = inputs.tag || github.event.release.tag_name`. Job-level `if: startsWith(TAG, 'th-')`. `permissions: contents: read`.
  - Job `build`, `strategy.fail-fast: false`, matrix `include` rows `{id, env, ref, core}` from the Global Constraints table (`ref` = `TAG` except eink4plus `thai-eink4plus`; `core` = `6.1.19` except eink4plus `6.2.0`). Steps copied from `release.yml:47-104` (checkout with `ref: matrix.ref == 'tag' && TAG || matrix.ref`, `submodules: recursive`; setup-python 3.13; uv; outer core `v${{ matrix.core }}.zip` + the same pinned deps; penv pin `pioarduino==${{ matrix.core }}`; build with the optional `FREEINK_LCP_RELEASE_KEY` block) minus the version-validation step. Then copy `.pio/build/<env>/firmware.bin` → `<id>-firmware.bin` and, if the factory file exists and `id != picco`, `firmware.factory.bin` → `<id>-firmware.factory.bin`; for eink4plus write `git rev-parse --short HEAD` to `eink4plus-commit.txt`. Upload all with `upload-artifact@v6` (`name: fw-<id>`, `compression-level: 0`, `retention-days: 1`).
  - Job `publish` (`needs: build`, `contents: write`): `download-artifact@v4` `pattern: fw-*`, `merge-multiple: true` into `dist`. Gate: each of the 9 `<id>-firmware.bin` exists and `test "$(ls dist/*.bin | wc -l)" -eq 17`. `gh release upload "$TAG" dist/*.bin --clobber`. Append `\n\nE-Ink4-Plus: <sha>` to release notes with `gh release view --json body` + `gh release edit --notes`. Output `deploy=true` only if the release is not a pre-release and `TAG` equals the latest non-pre-release `th-*` (same `gh release list` query as `pages-flasher.yml` "Pick release").
  - Job `pages` (`needs: publish`, `if: needs.publish.outputs.deploy == 'true'`): `uses: ./.github/workflows/pages-flasher.yml`, `permissions: {contents: read, pages: write, id-token: write}`.

- [ ] **Step 3: `pages-flasher.yml`**

  - `on:` → `workflow_call:` + `workflow_dispatch:` (drop `release`). Remove the job-level `if`. "Pick release" always uses the latest-non-pre-release query (drop `EVENT_TAG`).
  - "Assemble site": copy `index.html style.css flasher.js favicon.svg devices.json` and `fonts/`, images as today, `gh release download "$TAG" -p '*.bin' -D _site`, then `python3 web-flasher/assemble.py _site "$TAG"`. Remove the split loop, the `rm` line and the `sed` line.

- [ ] **Step 4: Static checks**

  Run: `actionlint .github/workflows/release-thai.yml .github/workflows/pages-flasher.yml .github/workflows/release.yml` (install with `brew install actionlint` if missing; otherwise `uvx --from yamllint yamllint` for syntax only).
  Expected: no errors.

- [ ] **Step 5: Ask the owner to approve, then commit**

  `git commit -m "ci: build all devices on th-* releases and deploy the flasher"`.

  CI run itself is verified in Task 4 (needs push, which only the owner can authorize).

### Task 3: Device picker page and disclaimer

**Files:**
- Modify: `web-flasher/index.html` (meta description `:9`, lead `:51-54`, install section `:116-195`, footer `:200`)
- Modify: `web-flasher/flasher.js` (append picker code after `:191`)
- Modify: `web-flasher/style.css` (`.devices`/`.device` rules `:508-560`, mobile `:806-825`)
- Modify: `web-flasher/test_site.py` (remove the two `expectedFailure` marks)

**Interfaces:**
- Consumes: page contract from Task 1 (`data-device`, `data-flash` per device).
- Produces: nothing other tasks use.

- [ ] **Step 1: Markup**

  - Meta description and lead: replace the two model names with "ทุกรุ่นที่ CrossPoint รองรับ" (lead may list makers: Xteink, Metalio, Seeed, M5Stack, EEGO, Onyx).
  - After the steps list: `<fieldset class="picker">` with `<legend>เลือกรุ่นของเครื่อง</legend>` and 9 `<label><input type="radio" name="device" value="<id>"> <img or svg> <span>name</span></label>` in table order. Devices without a photo use one inline SVG outline (24×24, stroke style of the wordmark icon).
  - Prompt `<p class="picker-empty" data-picker-empty>เลือกรุ่นของเครื่องด้านบน</p>`.
  - 9 `<article class="device" id="install-<id>" data-device="<id>" data-flash="<id>.json" data-name="<short name>" data-chip="<chip>">` panels built like today's X4 Pro card: photo/outline, `<h3>`, chip line (chip only — drop "ทดสอบบนเครื่องจริงแล้ว"), one short Thai line, install button, unsupported text, and the SD `<details>` with `href="<id>-firmware.bin" download="firmware.bin"` for every device with `sd_update: true`. Picco's unsupported text and body link to Releases instead of the SD block.
  - Replace the after-notes line "หน้านี้รองรับเฉพาะ…" (`:186`); keep the USB-lock and back-to-stock links.
  - Disclaimer `<section class="disclaimer" aria-labelledby="disclaimer-title">` before the footer: heading "ข้อควรทราบ", 3 points per spec §6 (as-is/own risk; not every model tried on a real device, report on `https://github.com/thaitop/crosspoint-thai/issues`; you can return to stock with `https://crosspointreader.com/install` or the vendor tool). Footer affiliation line lists Xteink, Metalio, Seeed, M5Stack, EEGO, Onyx; no duplicate in the disclaimer.

- [ ] **Step 2: Picker JS in `flasher.js`**

  `selectDevice(id: string | null): void` — shows `#install-<id>`, hides other `[data-device]` panels and the empty prompt, checks the matching radio, `history.replaceState(null, "", "#install-<id>")`. On load: if `location.hash` matches `#install-<known id>`, select it (and scroll the panel into view); otherwise hide all panels and show the prompt. Radios `change` → `selectDevice`. JS off: all panels visible, prompt hidden by default in HTML (`hidden` set only by JS).

- [ ] **Step 3: CSS** — picker as a wrapping grid of tiles (min 7rem), checked tile uses the existing accent/border tokens, visible `:focus-visible` ring; panel reuses `.device` rules as a single wide card. Mobile: 3 tiles per row.

- [ ] **Step 4: Unit tests**

  Remove the two `@unittest.expectedFailure` marks. Run: `python3 -m unittest discover -s web-flasher -p 'test_*.py' -v`. Expected: all pass.

- [ ] **Step 5: Browser checks** (assemble a local site with the Task 1 `make_site` helper, or `python3 -m http.server -d <site>`; Chrome DevTools MCP)

  - Load `/`: prompt shown, no panel. Click each tile: right panel, URL hash updates, install button's manifest = `<id>.json`, SD link = `<id>-firmware.bin` (none for Picco).
  - Load `/#install-picco`: Picco panel open.
  - Keyboard only: Tab to picker, arrow keys move selection, Enter on install opens the Thai dialog.
  - JS disabled: 9 panels visible.
  - Width 375 px: no horizontal scroll; screenshot for the owner.
  - Emulate no `navigator.serial` (Firefox or override): buttons hidden, unsupported text shown.

- [ ] **Step 6: Ask the owner to approve, then commit**

  `git commit -m "feat(web-flasher): device picker for all models and disclaimer"`.

### Task 4: Handoff and live verification

- [ ] **Step 1:** Run `firmware-handoff` (`.agents/skills/firmware-handoff/SKILL.md`) — release workflows change how shipped firmware is built.
- [ ] **Step 2:** Give the owner the steps (owner pushes; agent does not):
  1. Push `thai-2.0.0`.
  2. Create a pre-release `th-test-<date>` on `thai-2.0.0` → `release-thai.yml` builds 9 rows, 17 assets attached, `pages` job skipped, live site unchanged. Delete the test pre-release and its tag afterwards.
  3. The current live release `th-2026.11.26` has only the 2-device assets, so the new `pages-flasher.yml` would fail `check_site.py` on it (no broken deploy, but no update). Either run `release-thai.yml` by workflow_dispatch with `tag: th-2026.11.26` (rebuilds and replaces its assets from the same commits, then deploys), or publish the next `th-<date>` release.
  4. From the live page, flash X4 Pro and E-Ink4-Plus; settings and books kept. Other models: first owner reports via Issues.
- [ ] **Step 3:** Update memory `web-flasher-pages.md` (9 devices, CI builds, Picco app-only).
