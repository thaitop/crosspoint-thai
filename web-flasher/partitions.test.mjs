// Run: node --test web-flasher/
import assert from "node:assert/strict";
import { test } from "node:test";
import { layoutProblem, parsePartitions, TABLE_SIZE } from "./partitions.js";

function table(entries) {
  const bytes = new Uint8Array(TABLE_SIZE).fill(0xff);
  const view = new DataView(bytes.buffer);
  entries.forEach(([type, subtype, offset, size], i) => {
    const pos = i * 32;
    bytes.set([0xaa, 0x50, type, subtype], pos);
    view.setUint32(pos + 4, offset, true);
    view.setUint32(pos + 8, size, true);
  });
  return bytes;
}

// partitions.csv (every split device)
const OURS = table([
  [0x01, 0x02, 0x9000, 0x5000],
  [0x01, 0x00, 0xe000, 0x2000],
  [0x00, 0x10, 0x10000, 0x640000],
  [0x00, 0x11, 0x650000, 0x640000],
]);
// partitions_picco.csv (vendor layout)
const PICCO = table([
  [0x01, 0x02, 0x9000, 0xc000],
  [0x01, 0x00, 0x15000, 0x2000],
  [0x00, 0x00, 0x20000, 0xc0000],
  [0x00, 0x10, 0xe0000, 0xe70000],
]);
const BLANK = new Uint8Array(TABLE_SIZE).fill(0xff);

test("parses entries until the first non-entry", () => {
  assert.equal(parsePartitions(OURS).length, 4);
  assert.deepEqual(parsePartitions(PICCO)[3], { type: 0, subtype: 0x10, offset: 0xe0000, size: 0xe70000 });
  assert.equal(parsePartitions(BLANK).length, 0);
});

test("split install is refused on a Picco table", () => {
  assert.equal(layoutProblem(PICCO, "split"), "picco_table");
});

test("split install goes ahead on our table, a blank chip or another vendor table", () => {
  assert.equal(layoutProblem(OURS, "split"), null);
  assert.equal(layoutProblem(BLANK, "split"), null);
});

test("Picco install needs the Picco table", () => {
  assert.equal(layoutProblem(PICCO, "app-only"), null);
  assert.equal(layoutProblem(OURS, "app-only"), "not_picco");
  assert.equal(layoutProblem(BLANK, "app-only"), "not_picco");
});
