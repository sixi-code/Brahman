const fs = require('fs');
const vm = require('vm');
const path = require('path');

const root = 'D:/programs/stm32/Brahman/ui-sim';
const src = fs.readFileSync(path.join(root, 'assets/js/pages.js'), 'utf8')
  + '\n;globalThis.__PAGES = PAGES; globalThis.__TITLE = PAGE_TITLE; globalThis.__NAV = PAGES_WITH_NAV;';

class FakeBlob { constructor(a) { this.size = Buffer.byteLength(String(a[0]), 'utf8'); } }
const sandbox = { console, Math, Date, String, Number, Array, Object, Buffer, Blob: FakeBlob };
vm.createContext(sandbox);
vm.runInContext(src, sandbox, { filename: 'pages.js' });

const PAGES = sandbox.__PAGES;
const ctx = {
  dateText: '9月11日周五 丙午年', hhmm: '17:09', hms: '17:09:51', now: new Date(),
  tile: 0, fsPath: '0:/', clockTab: 0, swTime: '00:12.48', calcExpr: '', txtContent: 'x',
  uac1: false, logOn: true, logMode: 0,
  note: { view: 'list', page: 0, name: '', text: '', dirty: false, dlg: null, ask: '', err: '', msg: '', input: '', exitOk: false },
  dac: {}, canvasList: { page: 0, dlg: null, msg: '' },
  words: { view: 0, idx: 0, mode: 0, book: 0, listPage: 0, shown: false },
  usb: { idx: 0, running: false },
  memSeries: Array.from({ length: 24 }, (_, i) => 50 + i), memMode: 'CCM RAM', ccmUsed: 21.4, sramUsed: 63.7, cpu: 34,
  song: { name: '夜航星', fmt: 'flac', cur: 73, total: 262 }, playing: true, mode: 0, liked: false, showLyrics: false,
  spectrum: Array.from({ length: 20 }, () => 0.5), stickL: [0, 0], stickR: [0, 0], keyL: false, keyR: false,
  cursor: { x: 120, y: 120 }, battery: 78, voltage: 3.92, charging: 0, saving: true,
  headphone: false, tfInited: true, usbPlugged: true, volume: 3, rightMode: 0, showPercent: true,
  bright: 200, hdpVol: 180, spkVol: 160, hdpOut: false, musicActive: true, usbActive: false,
  pw: { bright: true, hdp: true, spk: true }, ccOpen: false, songIdx: 0,
};

const out = [];
for (const n of Object.keys(PAGES)) {
  try {
    const html = String(PAGES[n](ctx));
    const undefinedLeak = /undefined|NaN|\[object Object\]/.test(html);
    out.push({ page: n, ok: true, len: html.length, suspicious: undefinedLeak });
  } catch (e) {
    out.push({ page: n, ok: false, err: e.message });
  }
}
console.log(JSON.stringify({
  pages: Object.keys(PAGES).length,
  titles: Object.keys(sandbox.__TITLE).length,
  nav: sandbox.__NAV.size,
  results: out,
}, null, 1));
