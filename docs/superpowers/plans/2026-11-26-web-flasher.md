# One-click Web Flasher Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A good-looking Thai GitHub Pages site with one Flash button per device. It always installs the latest `th-*` release.

**Architecture:** The static files live in `web-flasher/`: one HTML page, two ESP Web Tools manifests, and one CSS file. A GitHub Actions workflow downloads the latest `th-*` release assets. It copies the assets and `docs/images/thai/` next to the page, stamps the version into the manifests, checks the site, and deploys it to Pages.

**Tech Stack:** Plain HTML/CSS, ESP Web Tools 10.5.2 (pinned, from unpkg), Noto Sans Thai (Google Fonts), GitHub Actions (`actions/upload-pages-artifact@v3`, `actions/deploy-pages@v4`), `gh` CLI.

**Spec:** `docs/superpowers/specs/2026-11-26-web-flasher-design.md`

## Global Constraints

- No firmware, build, or `platformio.ini` changes.
- ESP Web Tools pinned at `https://unpkg.com/esp-web-tools@10.5.2/dist/web/install-button.js?module`.
- Manifests: X4 Pro `chipFamily: "ESP32-S3"`, E-Ink4-Plus `chipFamily: "ESP32-S31"`. Each has one part `{ "path": "<device>-firmware.factory.bin", "offset": 0 }` and `new_install_prompt_erase: false`.
- Asset names, exactly: `x4pro-firmware.bin`, `x4pro-firmware.factory.bin`, `eink4plus-firmware.bin`, `eink4plus-firmware.factory.bin`.
- Every URL in the site is relative, because Pages serves it under `/crosspoint-thai/`.
- Page copy in plain Thai. Look: off-white paper, near-black ink, one accent color, Noto Sans Thai, and real screenshots from `docs/images/thai/`.
- Commits need the owner's approval. Push only when the owner says so.

## Review Focus

1. A release is missing one of the four assets, or the workflow runs before the uploads finish. The workflow must fail and keep the current site, not deploy a broken button. (Task 2, check step)
2. The browser has no Web Serial (Safari, Firefox, phones). The page shows a Thai message saying "use desktop Chrome/Edge", not a dead button. The SD download still works. (Task 1, `slot="unsupported"`)
3. A release whose tag is not `th-*` is published. The workflow skips it. (Task 2, `if:`)
4. The site is served under the `/crosspoint-thai/` subpath. The manifests and bins must still load, so no URL may start with `/`. (Task 1 check + Task 2 check)
5. The page is opened on a narrow phone screen. The layout stacks into one column with no sideways scroll. (Task 1, 375 px screenshot)

---

### Task 1: Page, manifests, and local preview

**Files:**
- Create: `web-flasher/index.html`, `web-flasher/style.css`, `web-flasher/x4pro.json`, `web-flasher/eink4plus.json`, `web-flasher/check_site.py`

**Interfaces:**
- Produces: `python3 web-flasher/check_site.py <site_dir>`. It exits 0 when `<site_dir>` has `index.html`, both manifests (valid JSON with the constraints above, `version` not `"dev"`), all four bins (size > 1 MB), and `images/` with the six screenshots. It also checks that no `src=`/`href=` in `index.html` starts with `/`. Otherwise it prints each problem and exits 1.
- Produces: in the manifests, the literal `"version": "dev"`, which Task 2 replaces with the tag.

- [ ] **Step 1:** Write `check_site.py` (stdlib only), then run it on an empty temp dir. Expected: exit 1, listing every missing file.
- [ ] **Step 2:** Write both manifests (`name`: "CrossPoint ภาษาไทย – X4 Pro" / "… – E-Ink4-Plus", `version`: "dev", `new_install_prompt_erase`: false, one build each per Global Constraints).
- [ ] **Step 3:** Load the `impeccable` skill and build `index.html` + `style.css` in the order the spec's Look section gives: hero → screenshot strip (`images/*.png`) → two device cards → 1-2-3 steps → footer (links to the repo and releases). Each card holds `<esp-web-install-button manifest="x4pro.json">` (or `eink4plus.json`) with a custom `slot="activate"` button. It also has a Thai `slot="unsupported"` message, and X4 Pro also gets an SD-update link to `x4pro-firmware.bin` with the three SD steps from the spec. Add a version label that is filled in from the manifest's `version` by a small `fetch` script.
- [ ] **Step 4:** Assemble a local preview: copy `web-flasher/*` into the scratchpad `site/`, copy `docs/images/thai/*.png` into `site/images/`, and run `gh release download th-2026.11.26 -R thaitop/crosspoint-thai -D site/` (with `GH_TOKEN=$(gh auth token --user thaitop)`). Set `version` to `th-2026.11.26`. Run `check_site.py site`. Expected: exit 0.
- [ ] **Step 5:** Serve with `python3 -m http.server` from a parent dir so the page sits under `/crosspoint-thai/`. Open it in Chrome via chrome-devtools. Take screenshots at 1280 px and 375 px. Expected: no console errors, both install buttons render, and no sideways scroll at 375 px. Show the screenshots to the owner and change things until the owner approves the look.
- [ ] **Step 6 (owner):** In desktop Chrome on the preview, flash the E-Ink4-Plus over USB. Expected: the chip is detected as ESP32-S31, the flash completes, the device boots the Thai build, and settings are kept.
- [ ] **Step 7:** Commit `web-flasher/` after the owner approves: `feat: Thai one-click web flasher page`.

### Task 2: Pages deploy workflow and README link

**Files:**
- Create: `.github/workflows/pages-flasher.yml`
- Modify: `README.md` (install section near line 112)

**Interfaces:**
- Consumes: `web-flasher/*` and `check_site.py <site_dir>` from Task 1.

- [ ] **Step 1:** Write the workflow:
  - Triggers: `release: {types: [published]}` and `workflow_dispatch`. Job `if: github.event_name == 'workflow_dispatch' || startsWith(github.event.release.tag_name, 'th-')`.
  - Permissions: `contents: read`, `pages: write`, `id-token: write`. Concurrency group `pages`.
  - Steps: checkout → tag = the event's tag, or on dispatch `gh release list --exclude-drafts --exclude-pre-releases --json tagName -q '[.[] | select(.tagName | startswith("th-"))][0].tagName'` → assemble `_site` the same way as Task 1 Step 4 (`gh release download "$tag" -p '*.bin' -D _site`) → replace `"version": "dev"` with the tag in both manifests → `python3 web-flasher/check_site.py _site` → upload-pages-artifact (`path: _site`) → a `deploy` job with `environment: github-pages` using deploy-pages.
- [ ] **Step 2:** Lint the workflow with `uvx --from actionlint-py actionlint .github/workflows/pages-flasher.yml`. Expected: no output.
- [ ] **Step 3:** In README, put a line at the top of the install section that links `https://thaitop.github.io/crosspoint-thai/` as the easy way to install (Thai, one sentence).
- [ ] **Step 4:** Commit (with the owner's approval): `ci: deploy web flasher to GitHub Pages on th-* releases`.
- [ ] **Step 5 (owner):** Settings → Pages → Source "GitHub Actions". Then push (on the owner's instruction) and run the workflow from the Actions tab.
- [ ] **Step 6:** Check the live site: `curl -sI` returns 200 for the page, both manifests, and the four bins. The bins' `content-length` must match the release sizes (`gh release view th-2026.11.26 --json assets`).
- [ ] **Step 7 (owner):** From the live page, download `x4pro-firmware.bin` and update the X4 Pro over SD. Expected: it boots the Thai build.
