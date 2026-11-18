/* ==========================================================================
   verify_parallax.js —— 开机页视差动效的离线验证
   不需要浏览器：用最小 DOM 桩把 pages.js + app.js 跑起来，检查
     1) startOffset() 的整数运算与固件 start_unit.c 一致（含向零截断、限幅）
     2) PAGES.start 渲染出的角色立绘初值 = (40,94)
     3) 拖动左摇杆后，20ms 定时器把光标积分进 ST.cursor 并写回立绘位置
   运行： node tools/verify_parallax.js
   ========================================================================== */
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const read = (p) => fs.readFileSync(path.join(ROOT, p), 'utf8');

let pass = 0, fail = 0;
function ok(cond, label, extra) {
  if (cond) { pass++; console.log(`  \u2713 ${label}`); }
  else { fail++; console.log(`  \u2717 ${label}${extra ? '  -> ' + extra : ''}`); }
}

/* ------------------------------ DOM 桩 ------------------------------ */
function mkEl(id) {
  const el = {
    id, dataset: {}, textContent: '', _handlers: {},
    style: {},
    classList: { toggle() {}, add() {}, remove() {}, contains: () => false },
    addEventListener(t, fn) { (this._handlers[t] = this._handlers[t] || []).push(fn); },
    removeEventListener() {},
    setPointerCapture() {}, releasePointerCapture() {},
    getBoundingClientRect: () => ({ left: 100, top: 500, width: 40, height: 40 }),
    querySelector: () => mkEl(id + '-q'),
    querySelectorAll: () => [],
    closest: () => null,
    contains: () => false,
    appendChild() {},
    set innerHTML(v) { this._html = v; },
    get innerHTML() { return this._html || ''; },
  };
  return el;
}

const els = {};
['layer-page', 'layer-nav', 'layer-status', 'layer-overlay', 'meta', 'page-list',
 'stick-l', 'stick-r', 'btn-wkup', 'mu-slider'].forEach((k) => { els[k] = mkEl(k); });

/* #stick-l / #stick-r 的 knob 需要 querySelector('span').style */
els['stick-l'].querySelector = () => mkEl('stick-l-span');
els['stick-r'].querySelector = () => mkEl('stick-r-span');
els['stick-l'].querySelectorAll = () => [];
els['stick-r'].querySelectorAll = () => [];

/* 角色立绘：由 #layer-page.innerHTML 的赋值派生出来 */
let topEl = null, topInit = null;
Object.defineProperty(els['layer-page'], 'innerHTML', {
  set(html) {
    this._html = html;
    const m = /class="t-top"[\s\S]*?style="left:(-?\d+)px;top:(-?\d+)px"/.exec(html);
    if (m) {
      topInit = { left: +m[1], top: +m[2] };
      if (!topEl) topEl = mkEl('t-top');
      topEl.style.left = topInit.left + 'px';
      topEl.style.top = topInit.top + 'px';
    } else {
      topEl = null;
    }
  },
  get() { return this._html || ''; },
});

const docListeners = {};
const document = {
  documentElement: mkEl('html'),
  getElementById: (id) => els[id] || null,
  querySelector: (sel) => {
    if (sel === '.pg-start .t-top') return topEl;
    if (sel[0] === '#') return els[sel.slice(1)] || null;
    return null;
  },
  querySelectorAll: () => [],
  addEventListener(t, fn) { (docListeners[t] = docListeners[t] || []).push(fn); },
  removeEventListener() {},
};

/* 定时器：只记录，不真跑，由用例手动步进 */
const intervals = [];
const sandbox = {
  document,
  console: { log() {}, warn() {}, error() {} },
  setInterval: (fn, ms) => { intervals.push({ fn, ms }); return intervals.length; },
  clearInterval() {},
  setTimeout: (fn, ms) => { intervals.push({ fn, ms, once: true }); return intervals.length; },
  clearTimeout() {},
  getComputedStyle: () => ({ getPropertyValue: (k) => (k === '--s' ? '2.6' : '') }),
  Date, Math, JSON, Object, Array, String, Number, Boolean, Error, Function, RegExp,
};
sandbox.window = sandbox;
sandbox.globalThis = sandbox;
vm.createContext(sandbox);

/* ------------------------------ 加载 ------------------------------ */
vm.runInContext(read('assets/js/pages.js'), sandbox, { filename: 'pages.js' });
vm.runInContext(read('assets/js/app.js'), sandbox, { filename: 'app.js' });

/* ------------------------------ 1) 偏移公式 ------------------------------ */
console.log('\n1) startOffset() 与 start_unit.c 的整数运算一致');
const off = (x, y) => sandbox.startOffset({ cursor: { x, y } });
const eq = (a, b) => a.ox === b.ox && a.oy === b.oy;
ok(eq(off(120, 120), { ox: 0, oy: 0 }), '中心 (120,120) -> (0,0)');
ok(eq(off(132, 131), { ox: 1, oy: 0 }), '(132,131) -> (1,0)  [11/12 截断为 0]');
ok(eq(off(239, 0), { ox: 9, oy: -10 }), '(239,0) -> (9,-10)');
ok(eq(off(0, 239), { ox: -10, oy: 9 }), '(0,239) -> (-10,9)');
ok(eq(off(108, 108), { ox: -1, oy: -1 }), '(108,108) -> (-1,-1)  [-12/12]');
ok(eq(off(1000, -500), { ox: 10, oy: -10 }), '越界值被限幅到 ±10');
ok(eq(off(131.9, 131.9), { ox: 0, oy: 0 }), '浮点先向零截断成 131 -> (0,0)');

/* ------------------------------ 2) 初值渲染 ------------------------------ */
console.log('\n2) PAGES.start 渲染出的立绘初值');
ok(topInit !== null, '起始页里存在 .t-top');
ok(topInit && topInit.left === 40, `初值 left=40 (实际 ${topInit && topInit.left})`);
ok(topInit && topInit.top === 94, `初值 top=94 (实际 ${topInit && topInit.top})`);

/* ------------------------------ 3) 摇杆 -> 光标 -> 立绘 ------------------------------ */
console.log('\n3) 拖左摇杆：20ms 定时器积分光标并写回立绘');
const tick20 = intervals.find((i) => i.ms === 20);
ok(!!tick20, '找到 20ms 定时器（对应 LV_INDEV_DEF_READ_PERIOD）');

const fire = (t, ev) => (els['stick-l']._handlers[t] || []).forEach((fn) => fn(ev));
const step = (n) => { for (let i = 0; i < n; i++) tick20.fn(); };
const pos = () => `${topEl && topEl.style.left}/${topEl && topEl.style.top}`;

/* 摇杆中心 (120,520)；clientX=140 -> dx=+12(满偏) -> 摇杆值 +127 -> 每拍 +7.62px */
fire('pointerdown', { clientX: 140, clientY: 520, pointerId: 1 });
ok(els['stick-l']._handlers.pointermove && els['stick-l']._handlers.pointermove.length > 0,
   'pointerdown 后成功挂上 pointermove');

step(10);
ok(pos() === '46px/94px', `右满偏 10 拍 -> left=46 top=94 (实际 ${pos()})`);
step(10);
ok(pos() === '49px/94px', `继续到 20 拍：光标触顶 239，left=49 (实际 ${pos()})`);

/* 换一档：向上满偏，检验负偏移的向零截断（-77/12 = -6，不是 -7） */
const before = pos();
fire('pointerdown', { clientX: 120, clientY: 500, pointerId: 1 });
step(10);
ok(pos() === '49px/88px',
   `上满偏 10 拍 -> top=88（-77/12 截断为 -6） (实际 ${pos()})`);
step(10);
ok(pos() === '49px/84px', `继续到触底 0 -> top=84 (实际 ${pos()})`);

/* 松手：摇杆自回中，光标停住不再变化 */
fire('pointerup', { clientX: 120, clientY: 500, pointerId: 1 });
const held = pos();
step(30);
ok(pos() === held, `松手后光标停止（立绘停在 ${held}）`);

/* 非开机页不写立绘 */
console.log('\n4) 只在开机页生效');
const layer = els['layer-page'];
layer.innerHTML = '<div class="pg pg-desktop">x</div>';
step(5);
ok(topEl === null, '切到非开机页后 .t-top 消失（syncStartOffset 空转）');

console.log(`\n结果：${pass} 通过 / ${fail} 失败`);
process.exit(fail ? 1 : 0);
