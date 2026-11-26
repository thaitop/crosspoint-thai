// Checks the partition table already on a device before an install, so the
// wrong card cannot overwrite another model's layout. ESP Web Tools only
// matches the chip family, and seven models share ESP32-S3.

export const TABLE_OFFSET = 0x8000;
export const TABLE_SIZE = 0xc00;
// Picco's stock table keeps its app (ota_0) here; ours puts ota_0 at 0x10000.
const PICCO_APP_OFFSET = 0xe0000;

export function parsePartitions(table) {
  const view = new DataView(table.buffer, table.byteOffset, table.byteLength);
  const parts = [];
  for (let pos = 0; pos + 32 <= table.length; pos += 32) {
    if (table[pos] !== 0xaa || table[pos + 1] !== 0x50) break;
    parts.push({
      type: table[pos + 2],
      subtype: table[pos + 3],
      offset: view.getUint32(pos + 4, true),
      size: view.getUint32(pos + 8, true),
    });
  }
  return parts;
}

function hasPiccoApp(parts) {
  return parts.some((p) => p.type === 0x00 && p.subtype === 0x10 && p.offset === PICCO_APP_OFFSET);
}

// Returns null when the install may go ahead, or a reason code:
// "picco_table"   - a split install would erase Picco's vendor bootloader and recovery
// "not_picco"     - an app-only install at 0xE0000 would land inside another model's app
export function layoutProblem(table, layout) {
  const piccoApp = hasPiccoApp(parsePartitions(table));
  if (layout === "app-only") return piccoApp ? null : "not_picco";
  return piccoApp ? "picco_table" : null;
}

// Picco is the only model whose layout a wrong card can destroy, and it is
// an ESP32-S3; ESP Web Tools already rejects a card for another chip family.
// Skipping other chips also avoids READ_FLASH, which hangs on the S31 stub.
export function needsLayoutCheck(chip) {
  return chip === "ESP32-S3";
}
