# One-click web flasher on GitHub Pages (short design)

## Intent

People who own an Xteink X4 Pro or a Metalio E-Ink4-Plus should be able to
install the Thai firmware from a browser with one button. They should not need
to install PlatformIO or esptool, or pick a "Custom .bin" in the upstream
flasher.

## Goals / non-goals

- Goal: one Thai page with one "Flash" button per device. The button always
  installs the latest `th-*` release.
- Goal: an SD-card path for X4 Pro units without USB flashing. The page links
  `x4pro-firmware.bin` and gives short steps (rename it to `firmware.bin`, put
  it on the SD card root, then Settings → General → SD Card Firmware Update).
- Non-goal: changing firmware or build output, auto-detecting the device, a
  Thai flashing dialog, or older releases.
- Anti-goal: a page that someone has to update by hand on every release.

## Approaches

- **A. Static page + ESP Web Tools (chosen).** The `<esp-web-install-button>`
  per device reads a manifest that writes `*-firmware.factory.bin` at offset
  0x0. Little code and no build step. The flashing dialog is in English.
- B. Custom flasher on `esptool-js`, like upstream. Fully Thai, can detect the
  device, but needs much more code to own and maintain.
- Rejected: linking to the upstream flasher's Custom .bin option, because it is
  not one click.

Evidence that the Plus works with A: upstream's flasher maps
`metalio_plus` to `ESP32-S31` and bundles `esptool-js`. `esptool-js` 0.7.1
has `targets/esp32s31`. ESP Web Tools 10.5.2 depends on `esptool-js ^0.7.0`
and matches builds by `esploader.chip.CHIP_NAME`. This still needs a test on
the owner's Plus.

## Design

- `web-flasher/index.html`: the Thai page. It loads ESP Web Tools 10.5.2 from a
  pinned CDN URL and has two install buttons, the SD download link with steps,
  and a note that Web Serial needs desktop Chrome or Edge.
- `web-flasher/x4pro.json`, `web-flasher/eink4plus.json`: ESP Web Tools
  manifests with `chipFamily` `ESP32-S3` (`platformio.ini:394`) and `ESP32-S31`, one part each:
  `{path: "<device>-firmware.factory.bin", offset: 0}`. The page hides
  `new_install_prompt_erase`, so a flash does not erase user settings.
- `.github/workflows/pages-flasher.yml`: runs on `release: published` for tags
  `th-*`, and on `workflow_dispatch`. It downloads the latest `th-*` release
  assets with `gh release download`, copies them and the screenshots in
  `docs/images/thai/` next to the page, writes the
  release tag into the manifests' `version`, and deploys with
  `actions/upload-pages-artifact` + `actions/deploy-pages`. GitHub Releases
  files cannot be fetched by the browser (no CORS headers), so they must be
  served from Pages.
- One-time setup by the owner: Settings → Pages → Source "GitHub Actions".
- README: link the page at the top of the install section.

## Look (owner requirement: the page must look good)

Builder's call, for the owner to review on a preview before deploy:

- Paper and ink: an off-white page, near-black text, one accent color, and
  plenty of space. This echoes an e-ink screen.
- A Thai web font that pairs with the firmware's UI font (Noto Sans Thai).
- A short hero ("ภาษาไทยบน e-reader ของคุณ"), then real device screenshots
  from `docs/images/thai/` (home calendar, reader, sleep screen), then one
  card per device with its big Flash button and the SD option.
- 1-2-3 steps (plug in USB → press Flash → choose the port). It works on
  mobile for reading, and says that flashing needs desktop Chrome or Edge.
- Use the `impeccable` skill for the visual pass and preview the page locally
  in a browser before the first deploy.

## Left to the builder

The exact wording (plain Thai, short). Whether the workflow reads
the version from the release event or `gh release list`.

## Verification

- Run the workflow with `workflow_dispatch`. Check that the Pages URL serves
  the page, both manifests, and all four `.bin` files with the release sizes.
- Owner, in desktop Chrome: flash the E-Ink4-Plus over USB from the page, and
  check that the device boots the Thai build and keeps its settings.
- Owner: download `x4pro-firmware.bin` from the page and update over SD.
