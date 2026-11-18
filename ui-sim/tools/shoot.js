// shoot.js —— 通过 CDP 驱动 headless Edge，逐页截图。
// 用法: node tools/shoot.js [port] [outDir]
const fs = require('fs');
const path = require('path');

const PORT = Number(process.argv[2] || 9222);
const OUT = process.argv[3] || 'tools/shots';

async function cdp() {
  const list = await (await fetch('http://127.0.0.1:' + PORT + '/json/list')).json();
  const page = list.find((t) => t.type === 'page');
  if (!page) throw new Error('no page target');
  const ws = new WebSocket(page.webSocketDebuggerUrl);
  await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
  let id = 0;
  const pending = new Map();
  ws.onmessage = (ev) => {
    const m = JSON.parse(ev.data);
    if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); }
  };
  const send = (method, params = {}) => new Promise((res) => {
    const mid = ++id;
    pending.set(mid, res);
    ws.send(JSON.stringify({ id: mid, method, params }));
  });
  return { send, close: () => ws.close() };
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

(async () => {
  fs.mkdirSync(OUT, { recursive: true });
  const { send, close } = await cdp();

  await send('Page.enable');
  await send('Runtime.enable');
  await send('Emulation.setDeviceMetricsOverride', {
    width: 1500, height: 1200, deviceScaleFactor: 1, mobile: false,
  });

  const evalJs = async (expr) => {
    const r = await send('Runtime.evaluate', { expression: expr, returnByValue: true, awaitPromise: true });
    if (r.result && r.result.exceptionDetails) {
      const d = r.result.exceptionDetails;
      throw new Error('eval failed [' + (d.exception && d.exception.description) + '] expr=' + expr.slice(0, 160));
    }
    return r.result && r.result.result ? r.result.result.value : undefined;
  };
  const q = (sel) => JSON.stringify(sel);
  const click = (sel) => evalJs('document.querySelector(' + q(sel) + ').click()');

  const goto = async (url) => { await send('Page.navigate', { url }); await sleep(1200); };
  const shot = async (file, clip) => {
    const params = { format: 'png', captureBeyondViewport: true };
    if (clip) params.clip = { ...clip, scale: 1 };
    const r = await send('Page.captureScreenshot', params);
    if (!r.result || !r.result.data) throw new Error('shot failed ' + file);
    fs.writeFileSync(path.join(OUT, file), Buffer.from(r.result.data, 'base64'));
  };
  const box = (sel) => evalJs('(() => { const e = document.querySelector(' + q(sel) + ');' +
    ' if (!e) return null; const r = e.getBoundingClientRect();' +
    ' return { x: Math.round(r.x), y: Math.round(r.y), width: Math.round(r.width), height: Math.round(r.height) }; })()');
  const goPage = async (p) => {
    await click('[data-go="' + p + '"]');
    await sleep(350);
  };

  const log = [];
  const colors = new Set();

  await goto('http://127.0.0.1:8931/index.html?t=' + Date.now());

  const fullH = await evalJs('document.documentElement.scrollHeight');
  await shot('00_full.png', { x: 0, y: 0, width: 1500, height: Math.min(fullH || 1200, 1600) });

  const dev = await box('.device');
  if (dev) await shot('01_device.png', dev);
  const scr = await box('.screen');
  log.push('screen=' + JSON.stringify(scr));

  const pages = ['start', 'desktop', 'music', 'settings', 'file', 'clock', 'calendar',
    'calculator', 'about', 'album', 'note', 'canvas_list', 'canvas', 'mem', 'es9018',
    'words', 'usb', 'cmd', 'serial', 'keytest', 'logctrl', 'debug', 'fontupdate', 'timeset'];

  for (const p of pages) {
    await goPage(p);
    if (scr) await shot('pg_' + p + '.png', scr);
    const t = await evalJs('document.querySelector(".nav-title") ? document.querySelector(".nav-title").textContent : ""');
    log.push(p + ':' + t);
  }

  await goPage('desktop');
  await evalJs('document.dispatchEvent(new KeyboardEvent("keydown",{key:"ArrowRight"}))');
  await sleep(350);
  if (scr) await shot('pg_desktop2.png', scr);

  await goPage('music');
  await click('[data-act="toggle-lyrics"]');
  await sleep(350);
  if (scr) await shot('pg_music_lyrics.png', scr);

  await click('.sbar .hit-m');
  await sleep(350);
  if (scr) await shot('ov_cc.png', scr);
  await click('.cc-close');
  await sleep(200);

  await goPage('calculator');
  for (const k of ['7', '×', '8', '=']) { await click('[data-key="' + k + '"]'); await sleep(90); }
  if (scr) await shot('pg_calculator.png', scr);
  log.push('calc=' + (await evalJs('document.querySelector(".calc-disp").innerText')));

  await goPage('canvas_list');
  await click('[data-act="cv-next"]');     // 名过长的那条在第 2 页
  await sleep(250);
  await click('[data-canvas="5"]');
  await sleep(300);
  if (scr) await shot('pg_canvas_tip.png', scr);
  await click('[data-act="cv-tip-ok"]');
  await sleep(200);

  await goPage('words');
  await click('[data-act="wd-list"]');
  await sleep(300);
  if (scr) await shot('pg_words_list.png', scr);

  await goPage('usb');
  await click('[data-act="usb-start"]');
  await sleep(300);
  if (scr) await shot('pg_usb_on.png', scr);

  await goPage('es9018');
  const rows = await evalJs('document.querySelectorAll(".dac-row").length');
  log.push('dacRows=' + rows);

  console.log(JSON.stringify({ ok: true, files: fs.readdirSync(OUT).length, log }, null, 1));
  close();
  process.exit(0);
})().catch((e) => { console.error('FAIL', e.message); process.exit(1); });
