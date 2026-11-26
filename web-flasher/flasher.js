// Thai install dialog around ESP Web Tools' flash(). flash() owns chip
// detection, build selection, writing and reset; this file owns the UI.
import { layoutProblem, TABLE_OFFSET, TABLE_SIZE } from "./partitions.js";

const FLASH_MODULE = "https://esm.sh/esp-web-tools@10.5.2/dist/flash.js?deps=esptool-js@0.7.1";
const ESPTOOL_MODULE = "https://esm.sh/esptool-js@0.7.1";

const dialog = document.getElementById("flash-dialog");
const view = dialog.querySelector("[data-view]");
let busy = false;

const ERRORS = {
  failed_initialize: {
    title: "เชื่อมต่อเครื่องไม่ได้",
    hint: "ถอดสายแล้วเสียบใหม่ ปิดโปรแกรมอื่นที่อาจใช้พอร์ตนี้อยู่ แล้วลองอีกครั้ง",
  },
  not_supported: {
    title: "เครื่องไม่ตรงกับรุ่นที่เลือก",
    hint: null, // filled in with the detected chip
  },
  failed_firmware_download: {
    title: "ดาวน์โหลดไฟล์เฟิร์มแวร์ไม่สำเร็จ",
    hint: "ตรวจการเชื่อมต่ออินเทอร์เน็ต แล้วลองอีกครั้ง",
  },
  picco_table: {
    title: "เครื่องนี้เป็น Picco / BOOX Tiles",
    hint: "เครื่องที่เสียบมี partition ของ Picco ถ้าติดตั้งด้วยปุ่มนี้ bootloader และโหมดกู้คืนของผู้ผลิตจะหายไปถาวร เลือกรุ่น Onyx Picco / BOOX Tiles แทน ยังไม่มีอะไรถูกเขียนลงเครื่อง",
  },
  not_picco: {
    title: "เครื่องนี้ไม่ใช่ Picco / BOOX Tiles",
    hint: "เครื่องที่เสียบไม่มี partition แบบ Picco ถ้าติดตั้งด้วยปุ่มนี้เครื่องจะบูตไม่ขึ้น เลือกรุ่นที่ตรงกับเครื่องแทน ยังไม่มีอะไรถูกเขียนลงเครื่อง",
  },
  write_failed: {
    title: "เขียนเฟิร์มแวร์ไม่สำเร็จ",
    hint: "เครื่องยังกู้ได้ อย่าเพิ่งถอดสาย กดลองอีกครั้งเพื่อติดตั้งใหม่",
  },
};

function render(html) {
  view.innerHTML = html;
  const focus = view.querySelector("[autofocus]") || view.querySelector("button");
  if (focus) focus.focus();
}

function escapeText(text) {
  const span = document.createElement("span");
  span.textContent = String(text);
  return span.innerHTML;
}

function showConfirm(device) {
  render(`
    <h2>ติดตั้งลง ${device.name}</h2>
    <p>เมื่อกดเริ่ม browser จะให้เลือกพอร์ต เลือกรายการที่เป็นเครื่องของคุณ แล้วกด เชื่อมต่อ</p>
    <p class="fd-note">การตั้งค่าและหนังสือในเครื่องยังอยู่ครบ เครื่องจะรีสตาร์ทเองเมื่อเสร็จ</p>
    <div class="fd-actions">
      <button class="btn btn-outline" data-action="close">ยกเลิก</button>
      <button class="btn" data-action="start" autofocus>เลือกพอร์ตและเริ่ม</button>
    </div>`);
}

const STEPS = [
  ["initializing", "เชื่อมต่อเครื่อง"],
  ["preparing", "ดาวน์โหลดไฟล์เฟิร์มแวร์"],
  ["writing", "เขียนเฟิร์มแวร์"],
];

function showProgress(device, state, percent) {
  const current = STEPS.findIndex(([id]) => id === state);
  const items = STEPS.map(([id, label], i) => {
    const status = i < current ? "done" : i === current ? "active" : "todo";
    const extra = id === "writing" && status === "active" ? ` <span class="fd-pct">${percent}%</span>` : "";
    return `<li class="fd-step is-${status}">${label}${extra}</li>`;
  }).join("");
  render(`
    <h2>กำลังติดตั้งลง ${device.name}</h2>
    <ol class="fd-steps">${items}</ol>
    <div class="fd-bar" role="progressbar" aria-label="ความคืบหน้า" aria-valuemin="0" aria-valuemax="100"
      aria-valuenow="${percent}"><span style="width: ${percent}%"></span></div>
    <p class="fd-warn">ห้ามถอดสายและห้ามปิดหน้านี้จนกว่าจะเสร็จ</p>`);
}

function updatePercent(percent) {
  const bar = view.querySelector(".fd-bar");
  const pct = view.querySelector(".fd-pct");
  if (!bar || !pct) return false;
  bar.setAttribute("aria-valuenow", percent);
  bar.firstElementChild.style.width = `${percent}%`;
  pct.textContent = `${percent}%`;
  return true;
}

function showDone(device) {
  render(`
    <h2>ติดตั้งเสร็จแล้ว</h2>
    <p>${device.name} กำลังรีสตาร์ทเข้าเฟิร์มแวร์ภาษาไทย ถอดสายได้เมื่อเครื่องแสดงหน้า Home</p>
    <ol class="fd-next">
      <li>Settings → General → Language เลือก <strong>ไทย</strong></li>
      <li>ถ้าเครื่องมีนาฬิกา ตั้งเวลาที่ Settings → General → Clock</li>
    </ol>
    <div class="fd-actions">
      <button class="btn" data-action="close" autofocus>เสร็จแล้ว</button>
    </div>`);
}

function showError(device, code, detail, chip) {
  const known = ERRORS[code] || { title: "ติดตั้งไม่สำเร็จ", hint: "ถอดสายแล้วเสียบใหม่ แล้วลองอีกครั้ง" };
  const hint =
    code === "not_supported"
      ? `เครื่องที่เสียบเป็น <span class="nb">${escapeText(chip || "ชิปรุ่นอื่น")}</span> แต่รุ่นที่เลือกคือ ${device.name} <span class="nb">(${device.chip})</span> กลับไปเลือกรุ่นที่ตรงกับเครื่อง`
      : known.hint;
  render(`
    <h2>${known.title}</h2>
    <p>${hint}</p>
    ${detail ? `<details class="fd-detail"><summary>รายละเอียดสำหรับแจ้งปัญหา</summary><code>${escapeText(detail)}</code></details>` : ""}
    <div class="fd-actions">
      <button class="btn btn-outline" data-action="close">ปิด</button>
      <button class="btn" data-action="start" autofocus>ลองอีกครั้ง</button>
    </div>`);
}

// Reads the device's partition table and returns a reason code when this
// card's layout would overwrite another model's (see partitions.js).
async function checkLayout(port, layout) {
  const { Transport, ESPLoader } = await import(ESPTOOL_MODULE);
  const transport = new Transport(port);
  try {
    const loader = new ESPLoader({ transport, baudrate: 115200 });
    await loader.main();
    const problem = layoutProblem(await loader.readFlash(TABLE_OFFSET, TABLE_SIZE), layout);
    if (problem) await loader.after(); // nothing written: reboot into the current firmware
    return problem;
  } finally {
    // After a reset the USB port may already be gone; keep the result above.
    await transport.disconnect().catch(() => {});
  }
}

async function runFlash(device) {
  let port;
  try {
    port = await navigator.serial.requestPort();
  } catch {
    showConfirm(device); // picker dismissed: stay on the first step
    return;
  }

  busy = true;
  let reported = false;
  showProgress(device, "initializing", 0);
  try {
    // Download first: once the check passes the device waits in download
    // mode, so nothing between the check and flash() may fail.
    const [{ flash }, manifest] = await Promise.all([
      import(FLASH_MODULE),
      fetch(device.manifest).then((r) => r.json()),
    ]);
    let problem;
    try {
      problem = await checkLayout(port, device.layout);
    } catch (err) {
      busy = false;
      showError(device, "failed_initialize", err && err.message);
      return;
    }
    if (problem) {
      busy = false;
      showError(device, problem);
      return;
    }
    await flash(
      (event) => {
        if (event.state === "writing") {
          const percent = event.details.percentage;
          if (!updatePercent(percent)) showProgress(device, "writing", percent);
        } else if (event.state === "initializing" || event.state === "preparing") {
          const step = STEPS.findIndex(([id]) => id === event.state) + (event.details.done ? 1 : 0);
          showProgress(device, STEPS[step][0], 0);
        } else if (event.state === "finished") {
          busy = false;
          showDone(device);
        } else if (event.state === "error") {
          busy = false;
          reported = true;
          showError(device, event.details.error, event.message, event.chipFamily);
        }
      },
      port,
      device.manifest,
      manifest,
      false,
    );
  } catch (err) {
    busy = false;
    // flash() resets the device after reporting an error; a failure there
    // must not replace the specific message already shown.
    if (!reported) showError(device, "unknown", err && err.message);
  }
}

function open(device) {
  showConfirm(device);
  dialog.onclick = (event) => {
    const action = event.target.closest("[data-action]")?.dataset.action;
    if (action === "close") dialog.close();
    if (action === "start") runFlash(device);
  };
  dialog.showModal();
}

// Esc and the close event must not interrupt a running write.
dialog.addEventListener("cancel", (event) => {
  if (busy) event.preventDefault();
});
window.addEventListener("beforeunload", (event) => {
  if (busy) event.preventDefault();
});

const supported = "serial" in navigator && window.isSecureContext;
for (const card of document.querySelectorAll("[data-flash]")) {
  const button = card.querySelector("[data-flash-start]");
  const message = card.querySelector("[data-flash-unsupported]");
  if (!supported) {
    button.hidden = true;
    message.hidden = false;
    continue;
  }
  const device = {
    manifest: card.dataset.flash,
    name: card.dataset.name,
    chip: card.dataset.chip,
    layout: card.dataset.layout,
  };
  button.addEventListener("click", () => open(device));
}

// Device picker: choose a brand, then a model; one install panel at a time.
// A #install-<id> link selects both. Without JS every model and panel shows.
const panels = [...document.querySelectorAll("[data-device]")];
const brandRadios = [...document.querySelectorAll('.picker input[name="brand"]')];
const deviceRadios = [...document.querySelectorAll('.picker input[name="device"]')];
const deviceStep = document.querySelector("[data-picker-devices]");
const emptyPrompt = document.querySelector("[data-picker-empty]");

function brandOf(radio) {
  return radio.closest("[data-brand]").dataset.brand;
}

function showPanel(id) {
  let shown = null;
  for (const panel of panels) {
    panel.hidden = panel.dataset.device !== id;
    if (!panel.hidden) shown = panel;
  }
  emptyPrompt.hidden = shown !== null;
  for (const radio of deviceRadios) radio.checked = radio.value === id;
  if (shown) history.replaceState(null, "", `#install-${id}`);
  else if (location.hash.startsWith("#install-")) history.replaceState(null, "", "#install");
  return shown;
}

function selectBrand(brand) {
  for (const radio of brandRadios) radio.checked = radio.value === brand;
  const models = deviceRadios.filter((radio) => brandOf(radio) === brand);
  for (const radio of deviceRadios) radio.closest("label").hidden = brandOf(radio) !== brand;
  deviceStep.hidden = !brand;
  // A brand with one model needs no second choice.
  return showPanel(models.length === 1 ? models[0].value : null);
}

function selectDevice(id) {
  const radio = deviceRadios.find((r) => r.value === id);
  if (!radio) return null;
  selectBrand(brandOf(radio));
  return showPanel(id);
}

for (const radio of brandRadios) radio.addEventListener("change", () => selectBrand(radio.value));
for (const radio of deviceRadios) radio.addEventListener("change", () => showPanel(radio.value));

function selectFromHash() {
  const linked = location.hash.match(/^#install-(.+)$/);
  const panel = linked && selectDevice(linked[1]);
  if (panel) panel.scrollIntoView({ block: "start" });
  return panel;
}
window.addEventListener("hashchange", selectFromHash);
if (!selectFromHash()) selectBrand(null);
