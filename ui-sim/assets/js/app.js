/* ==========================================================================
   app.js —— 状态机 / 状态栏 / 导航栏 / 路由 / 交互
   ========================================================================== */
(function () {
  'use strict';

  const $ = (s, r = document) => r.querySelector(s);
  const pad = (n) => String(n).padStart(2, '0');
  const clamp = (v, a, b) => Math.min(b, Math.max(a, v));
  const WEEK = ['日', '一', '二', '三', '四', '五', '六'];
  const GAN = '甲乙丙丁戊己庚辛壬癸', ZHI = '子丑寅卯辰巳午未申酉戌亥';

  /* ------------------------------ 状态 ------------------------------ */
  const ST = {
    page: 'start', history: [], tile: 0, fsPath: '0:/',

    /* RTC / 状态栏 */
    now: new Date(), rightMode: 0, showPercent: true,
    battery: 78, voltage: 3.92, charging: 0,            // 0 无 / 1 充电中 / 2 已充满
    saving: true, headphone: false, tfInited: true, usbPlugged: true,

    /* 音频与系统 */
    volume: 3,                                          // volume_icon_0..5
    cpu: 34, ram: 41, ccmUsed: 21.4, sramUsed: 63.7,
    memMode: 'CCM RAM', memSeries: Array.from({ length: 24 }, (_, i) => 20 + 18 * Math.sin(i / 2.2) + (i % 3) * 3),

    /* 控制中心 */
    ccOpen: false, hdpOut: false, musicActive: true, usbActive: false,
    pw: { bright: true, hdp: true, spk: true },
    bright: 200, hdpVol: 180, spkVol: 160,

    /* 音乐 */
    playing: true, mode: 0, liked: false, showLyrics: false,
    songIdx: 0,
    song: { name: '夜航星', fmt: 'flac 24bit 96kHz 1024kbps', cur: 73, total: 262 },

    /* 其它页面 */
    clockTab: 0, swTime: '00:12.48',
    calcExpr: '', txtContent: '',
    uac1: false, logOn: true, logMode: 0,

    /* DAC 设置 —— 对应 es9018_unit.c 的 ES9018_Config_t，键为 DAC_ROWS 下标 */
    dac: {},

    /* 画作列表 —— 对应 canvas_list_unit.c（顶栏翻页 + 提示弹窗） */
    canvasList: { page: 0, dlg: null, msg: '' },

    /* 背单词 —— 对应 words_state_t：0 卡片 / 1 目录 / 2 选书 */
    words: { view: 0, idx: 0, mode: 0, book: 0, listPage: 0, shown: false },

    /* USB 控制 —— 对应 usb_ctrl_unit.c：滚轮选中项 + 是否已启动 */
    usb: { idx: 0, running: false },

    /* DAC 设置 —— 对应 es9018_unit.c 的 es9018_config_t，键为 DAC_ROWS 下标 */
    dac: {},

    /* 画作列表 —— 对应 canvas_list_unit.c（顶栏翻页 + 提示弹窗） */
    canvasList: { page: 0, dlg: null, msg: '' },

    /* 背单词 —— 对应 words_state_t：0 卡片 / 1 目录 / 2 选书 */
    words: { view: 0, idx: 0, mode: 0, book: 0, listPage: 0, shown: false },

    /* USB 控制 —— 对应 usb_ctrl_unit.c：roller 选中项 + 是否启动 */
    usb: { idx: 0, running: false },
    stickL: [0, 0], stickR: [0, 0], keyL: false, keyR: false,
    cursor: { x: 120, y: 120 },                          // lv_port_indev.c: 初值 = 屏幕中心

    /* 记事本 —— 对应 note_unit.c 的 note_state_t */
    note: {
      view: 'list', page: 0, name: '', text: '', dirty: false,
      dlg: null,            // null / 'ask' / 'name' / 'tip'
      ask: '',              // 确认框在为哪件事准备：'' / 'leave' / 'list'
      err: '', msg: '', input: '', exitOk: false,
    },

    spectrum: Array.from({ length: 20 }, () => Math.random()),
  };

  const PLAYLIST = [
    { name: '夜航星', fmt: 'flac 24bit 96kHz 1024kbps', total: 262 },
    { name: 'Sundial', fmt: 'mp3 16bit 48kHz 320kbps', total: 198 },
    { name: 'Blue Hour', fmt: 'wav 32bit 96kHz 4608kbps', total: 305 },
    { name: '暮色回响', fmt: 'ape 16bit 48kHz 900kbps', total: 241 },
    { name: 'Nocturne', fmt: 'aac 16bit 96kHz 256kbps', total: 176 },
  ];

  /* ------------------------------ 工具 ------------------------------ */
  function dateText() {
    const d = ST.now;
    const gz = GAN[(d.getFullYear() - 4) % 10] + ZHI[(d.getFullYear() - 4) % 12];
    return `${d.getMonth() + 1}月${d.getDate()}日周${WEEK[d.getDay()]} ${gz}年`;
  }
  const hhmm = () => `${pad(ST.now.getHours())}:${pad(ST.now.getMinutes())}`;
  const hms = () => `${pad(ST.now.getHours())}:${pad(ST.now.getMinutes())}:${pad(ST.now.getSeconds())}`;
  const mmss = (v) => `${pad(Math.floor(v / 60))}:${pad(v % 60)}`;

  /** 供 pages.js 使用的渲染上下文 */
  function ctx() {
    return Object.assign({}, ST, {
      dateText: dateText(), hhmm: hhmm(), hms: hms(), now: ST.now,
    });
  }

  /* ======================================================================
     状态栏
     ====================================================================== */
  function renderStatus() {
    /* start / canvas / fontupdate 这三页固件里不创建状态栏 */
    if (PAGES_NO_STATUS.has(ST.page)) { $('#layer-status').innerHTML = ''; return; }

    const d = ST.now;
    const ds = `${pad(d.getFullYear() % 100)}.${pad(d.getMonth() + 1)}.${pad(d.getDate())}`;

    let right;
    if (ST.rightMode === 2) {
      right = `CPU:${ST.cpu}%\nRAM:${ST.ram}%`;
    } else {
      const h = d.getHours();
      const t = ST.rightMode === 1
        ? `${pad(h % 12 || 12)}:${pad(d.getMinutes())}:${pad(d.getSeconds())}${h < 12 ? 'AM' : 'PM'}`
        : `${pad(h)}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
      right = `${ds}\n${t}`;
    }

    const pct = clamp(ST.battery, 0, 100);
    const barColor = ST.charging === 1 ? 'var(--c-bat-charging)'
                   : ST.charging === 2 ? 'var(--c-bat-full)' : '#fff';

    const icons = [];
    if (ST.saving) icons.push(ic('saving_icon', 20, '#000'));
    if (ST.headphone) icons.push(ic('headphone_icon', 20, '#000'));
    if (!ST.tfInited) icons.push(ic('card_err_icon', 16, '#000', 'height:20px'));
    if (ST.usbPlugged) icons.push(ic('usb_icon', 14, '#000', 'height:20px'));
    icons.push(ic(`volume_icon_${clamp(ST.volume, 0, 5)}`, 20, '#000'));

    $('#layer-status').innerHTML = `
      <div class="sbar">
        <div class="s-line"></div>
        <div class="bat">
          <div class="bat-body"><div class="bat-bar" style="width:${pct}%;background:${barColor}"></div></div>
          <div class="bat-label">${ST.showPercent ? `${pad(pct)}%` : `${ST.voltage.toFixed(2)}V`}</div>
        </div>
        <div class="bat-term"></div>
        <div class="sbar-icons">${icons.join('')}</div>
        <div class="sbar-clock">${right}</div>
        <div class="hit hit-l" data-act="sb-left"  title="电量 / 电压"></div>
        <div class="hit hit-m" data-act="sb-mid"   title="控制中心"></div>
        <div class="hit hit-r" data-act="sb-right" title="24h / 12h / CPU"></div>
      </div>`;
  }

  /* ======================================================================
     导航栏
     ====================================================================== */
  function renderNav() {
    if (!PAGES_WITH_NAV.has(ST.page)) { $('#layer-nav').innerHTML = ''; return; }
    const title = PAGE_TITLE[ST.page] || '';
    $('#layer-nav').innerHTML = `
      <div class="nav">
        <div class="nav-line"></div>
        <div class="nav-title">${title}</div>
        <button class="b-exit"  data-act="nav-back"    title="返回">${ic('exit_icon', 20, '#000')}</button>
        <button class="b-min"   data-act="nav-desktop" title="最小化">${ic('minimize_icon', 20, '#000')}</button>
        <button class="b-close" data-act="nav-desktop" title="关闭">${ic('close_icon', 20, '#000')}</button>
      </div>`;
  }

  /* ======================================================================
     页面 / 覆盖层
     ====================================================================== */
  function renderPage(anim) {
    const fn = PAGES[ST.page] || PAGES.desktop;
    const html = fn(ctx());
    $('#layer-page').innerHTML = anim ? html.replace('class="pg', 'class="pg anim') : html;
    syncPanel();
  }

  function renderOverlay() {
    /* 控制中心与记事本弹窗都挂在这一层（对应固件的 lv_layer_top） */
    $('#layer-overlay').innerHTML = ST.ccOpen ? renderControlCenter(ST)
      : (ST.page === 'canvas_list' ? listTipHTML(ST.canvasList) : noteDialogHTML(ST));
  }

  /* ======================================================================
     记事本 —— 对应 note_unit.c：列表 / 编辑 / 三个弹窗 / 离开前询问
     ====================================================================== */
  const NT_RESET = { view: 'list', page: 0, name: '', text: '', dirty: false,
                     dlg: null, ask: '', err: '', msg: '', input: '', exitOk: false };

  /** 离开记事本页时复位（对应固件 Create_Note_Unit → note_show_list） */
  function ntReset() { Object.assign(ST.note, NT_RESET); }

  /** 页面切换前的询问（对应固件 Page_Interface_t.can_exit） */
  function canLeavePage() {
    const nt = ST.note;
    if (ST.page !== 'note') return true;
    if (nt.view !== 'edit' || !nt.dirty || nt.exitOk) return true;
    if (!nt.dlg) ntAsk('leave');        // 还没问过就弹出来，并拦下本次切换
    return false;
  }

  function ntRender() { renderPage(); renderOverlay(); }

  function ntAsk(what) {
    ST.note.ask = what;
    ST.note.dlg = 'ask';
    ST.note.msg = '';
    renderOverlay();
  }

  function ntTip(msg) { ST.note.dlg = 'tip'; ST.note.msg = msg; renderOverlay(); }

  function ntList() { ST.note.view = 'list'; ST.note.page = 0; ST.note.dlg = null; ntRender(); }

  /** 打开某篇笔记；过大 / 名字过长的不进编辑页，只提示（对应 note_item_event_cb） */
  function ntOpen(idx) {
    const f = NOTE_FILES[idx];
    if (!f) return;
    if (f.bad === 'big')  { ntTip('文件过大\n只能编辑 4 KB 以内的笔记'); return; }
    if (f.bad === 'long') { ntTip('文件名过长\n汉字别超过 16 个'); return; }
    Object.assign(ST.note, { view: 'edit', name: f.n, text: f.t || '', dirty: false, exitOk: false });
    ntRender();
  }

  function ntNew() {
    Object.assign(ST.note, { view: 'edit', name: '', text: '', dirty: false, exitOk: false });
    ntRender();
  }

  /** 名称校验（对应 note_check_name）：空 / 非法字符 / 超 48 字节 */
  function ntCheckName(name) {
    if (!name) return '请先输入名称';
    if (new Blob([name]).size > 48) return '名称太长';
    if (/["*/:<>?\\|]/.test(name) || /[\u0000-\u001f]/.test(name)) return '名称含非法字符';
    return '';
  }

  /** 落盘：更新模拟目录（对应 do_write_text + 保存后的收尾） */
  function ntSave(name) {
    const i = NOTE_FILES.findIndex((f) => f.n === name);
    if (i >= 0) NOTE_FILES[i].t = ST.note.text;
    else NOTE_FILES.push({ n: name, t: ST.note.text });

    const ask = ST.note.ask;
    ST.note.dirty = false;
    ST.note.dlg = null;
    ST.note.ask = '';
    if (ask === 'leave') { ST.note.exitOk = true; back(); }   // 存完就放行离开
    else ntList();
  }

  function goto(id, opts = {}) {
    if (!PAGES[id]) return;
    if (id !== ST.page && !canLeavePage()) return;   // 记事本有未保存改动就先问一句
    if (id !== ST.page && ST.page !== 'start' && ST.page !== 'desktop' && !opts.back)
      ST.history.push(ST.page);
    if (ST.page === 'note') ntReset();               // 离开记事本：下次进来回到列表
    if (opts.path) ST.fsPath = opts.path;
    ST.page = id;
    ST.ccOpen = false;
    renderNav(); renderStatus(); renderPage(true); renderOverlay();
  }

  function back() {
    if (ST.ccOpen) { ST.ccOpen = false; renderOverlay(); return; }
    if (!canLeavePage()) return;
    const prev = ST.history.pop();
    if (ST.page === 'note') ntReset();
    if (prev) { ST.page = prev; renderNav(); renderStatus(); renderPage(true); renderOverlay(); }
    else if (ST.page !== 'desktop') goto('desktop', { back: true });
  }

  /* ======================================================================
     控制中心交互辅助
     ====================================================================== */
  function setCc(key, val) { ST[key === 'bright' ? 'bright' : key === 'hdp' ? 'hdpVol' : 'spkVol'] = val; }

  /* ======================================================================
     事件：点击
     ====================================================================== */
  document.addEventListener('click', (e) => {
    const el = e.target.closest('[data-act],[data-app],[data-page],[data-tab],[data-key],' +
                                 '[data-pwr],[data-logmode],[data-file],[data-note],' +
                                 '[data-canvas],[data-dac],[data-dac-sw],[data-w],[data-b],.set-item');
    if (!el) return;

    /* --- 桌面 APP --- */
    if (el.dataset.app !== undefined) {
      const app = APP_LIST[+el.dataset.app];
      goto(app.page, { path: app.path });
      return;
    }
    /* --- 设置项跳转 --- */
    if (el.dataset.page) { goto(el.dataset.page); return; }
    if (el.dataset.tab !== undefined) { ST.clockTab = +el.dataset.tab; renderPage(); return; }
    if (el.dataset.logmode !== undefined) { ST.logMode = +el.dataset.logmode; renderPage(); return; }
    if (el.dataset.file) { return; }   // 文件行：仅高亮，不跳转
    if (el.dataset.note !== undefined) { ntOpen(+el.dataset.note); return; }

    /* --- 画作条目：名过长的只弹提示，不进画布（对应 canvas_list_unit.c 的 on_item） --- */
    if (el.dataset.canvas !== undefined) {
      const f = CANVAS_FILES[+el.dataset.canvas];
      if (f && f.bad === 'long') {
        ST.canvasList.dlg = 'tip';
        ST.canvasList.msg = '文件名过长\n无法打开这幅画作';
        renderOverlay();
        return;
      }
      goto('canvas');
      return;
    }

    /* --- 计算器 --- */
    if (el.dataset.key !== undefined) { calcKey(el.dataset.key); return; }

    /* --- DAC 设置：循环按钮 / 开关（对应 es9018_unit.c 的 setting_event_cb） --- */
    if (el.dataset.dac !== undefined) {
      const i = +el.dataset.dac;
      const row = DAC_ROWS[i];
      const cur = ST.dac[i] !== undefined ? ST.dac[i] : row.v;
      const at = row.opts.indexOf(cur);
      ST.dac[i] = row.opts[(at + 1) % row.opts.length];
      renderPage();
      return;
    }
    if (el.dataset.dacSw !== undefined) {
      const i = +el.dataset.dacSw;
      const row = DAC_ROWS[i];
      ST.dac[i] = !(ST.dac[i] !== undefined ? ST.dac[i] : row.v);
      renderPage();
      return;
    }

    /* --- 控制中心电源键 --- */
    if (el.dataset.pwr) {
      const k = el.dataset.pwr;
      ST.pw[k] = !ST.pw[k];
      renderOverlay();
      return;
    }

    /* --- 通用动作 --- */
    switch (el.dataset.act) {
      /* 导航栏 */
      case 'nav-back':    back(); break;
      case 'nav-desktop': goto('desktop'); break;
      case 'to-desktop':  goto('desktop'); break;
      /* 状态栏 */
      case 'sb-left':  ST.showPercent = !ST.showPercent; renderStatus(); break;
      case 'sb-mid':
        if (ST.page === 'start') goto('desktop');
        else { ST.ccOpen = !ST.ccOpen; renderOverlay(); }
        break;
      case 'sb-right': ST.rightMode = (ST.rightMode + 1) % 3; renderStatus(); break;
      /* 控制中心 */
      case 'cc-close':       ST.ccOpen = false; renderOverlay(); break;
      case 'cc-music-close': ST.musicActive = false; renderOverlay(); break;
      case 'cc-usb-close':   ST.usbActive = false; renderOverlay(); break;
      case 'cc-hdp':         ST.hdpOut = !ST.hdpOut; renderOverlay(); break;
      /* 音乐 */
      case 'music-exit':  goto('desktop'); break;
      case 'music-file':  goto('file', { path: '0:/MUSIC' }); break;
      case 'music-mode':  ST.mode = (ST.mode + 1) % 3; renderPage(); break;
      case 'music-like':  ST.liked = !ST.liked; renderPage(); break;
      case 'music-play':  ST.playing = !ST.playing; renderPage(); break;
      case 'music-prev':  stepSong(-1); break;
      case 'music-next':  stepSong(1); break;
      case 'toggle-lyrics': ST.showLyrics = !ST.showLyrics; renderPage(); break;
      /* 设置 / 其它 */
      case 'toggle-uac': ST.uac1 = !ST.uac1; renderPage(); break;
      case 'log-sw':     ST.logOn = !ST.logOn; renderPage(); break;
      case 'mem-mode':   cycleMemMode(); break;
      case 'album-prev': case 'album-next': break;

      /* 画作列表（canvas_list_unit.c：翻页 / 新建 / 提示弹窗） */
      case 'cv-prev': ST.canvasList.page = Math.max(0, ST.canvasList.page - 1); renderPage(); break;
      case 'cv-next': ST.canvasList.page += 1; renderPage(); break;
      case 'cv-new':  goto('canvas'); break;
      case 'cv-tip-ok': ST.canvasList.dlg = null; renderOverlay(); break;

      /* 背单词（words_unit.c：卡片 / 目录 / 选书三视图） */
      case 'wd-mode':
        ST.words.mode ^= 1;
        if (ST.words.mode) ST.words.idx = Math.floor(Math.random() * 10);
        renderPage(); break;
      case 'wd-show': ST.words.shown = !ST.words.shown; renderPage(); break;
      case 'wd-prev': ST.words.idx = (ST.words.idx + 9) % 10; ST.words.shown = false; renderPage(); break;
      case 'wd-next': ST.words.idx = (ST.words.idx + 1) % 10; ST.words.shown = false; renderPage(); break;
      case 'wd-books': ST.words.view = 2; renderPage(); break;
      case 'wd-book': {
        ST.words.book = +(el.dataset.b || 0);
        ST.words.view = 0; ST.words.idx = 0; ST.words.shown = false;
        renderPage(); break;
      }
      case 'wd-list': ST.words.view = 1; ST.words.listPage = 0; renderPage(); break;
      case 'wd-list-up': ST.words.listPage = Math.max(0, (ST.words.listPage || 0) - 1); renderPage(); break;
      case 'wd-list-down': ST.words.listPage = (ST.words.listPage || 0) + 1; renderPage(); break;
      case 'wd-open': ST.words.view = 0; ST.words.shown = true; renderPage(); break;

      /* USB 控制（usb_ctrl_unit.c：滚轮上下移 + 启动/终止） */
      case 'usb-up':   ST.usb.idx = (ST.usb.idx + 7) % 8; ST.usb.running = false; renderPage(); break;
      case 'usb-down': ST.usb.idx = (ST.usb.idx + 1) % 8; ST.usb.running = false; renderPage(); break;
      case 'usb-start': ST.usb.running = true; ST.usbActive = true; renderPage(); renderOverlay(); break;
      case 'usb-end':   ST.usb.running = false; ST.usbActive = false; renderPage(); renderOverlay(); break;

      /* 日志设置 */
      case 'log-open-usb': ST.logOpenUsb = !ST.logOpenUsb; renderPage(); break;
      case 'log-dump-lvgl': ST.logOn = true; ST.logMode = 2; renderPage(); break;
      /* 记事本 */
      case 'nt-prev':
        ST.note.page = Math.max(0, ST.note.page - 1); renderPage(); break;
      case 'nt-next':
        ST.note.page = Math.min(Math.ceil(NOTE_FILES.length / NT_PAGE_SIZE) - 1,
                                ST.note.page + 1); renderPage(); break;
      case 'nt-new':  ntNew(); break;
      case 'nt-list':
        if (ST.note.dirty) ntAsk('list');            // 有未保存改动先问一句
        else ntList();
        break;
      case 'nt-ask-save':
        if (ST.note.name) ntSave(ST.note.name);
        else { ST.note.dlg = 'name'; ST.note.err = ''; ST.note.input = ''; renderOverlay(); }
        break;
      case 'nt-ask-drop': {
        const ask = ST.note.ask;
        ST.note.dirty = false; ST.note.dlg = null; ST.note.ask = '';
        if (ask === 'leave') { ST.note.exitOk = true; back(); }   // 放行离开
        else ntList();
        break;
      }
      case 'nt-ask-stay':                            // 取消：留在本页，切换已被拦下
        ST.note.dlg = null; ST.note.ask = ''; renderOverlay();
        break;
      case 'nt-name-cancel':
        ST.note.dlg = null; ST.note.err = ''; ST.note.ask = '';   // 若为离开而起名，一并取消离开
        renderOverlay();
        break;
      case 'nt-name-ok': {
        const inp = document.querySelector('[data-nt-input]');
        const name = (inp ? inp.value : '').trim();
        ST.note.input = name;
        const err = ntCheckName(name);
        if (err) { ST.note.err = err; renderOverlay(); return; }
        if (NOTE_FILES.some((f) => f.n === name) && name !== ST.note.name) {
          ST.note.err = '名称已存在'; renderOverlay(); return;   // 查重：同名且不是当前这篇
        }
        ntSave(name);
        break;
      }
      case 'nt-tip-ok':
        ST.note.dlg = null; renderOverlay(); break;
      default: break;
    }
  });

  /* 记事本正文 / 名称输入框 */
  document.addEventListener('input', (e) => {
    const el = e.target;
    if (el.matches('[data-nt-edit]')) { ST.note.text = el.innerText; ST.note.dirty = true; }
    else if (el.matches('[data-nt-input]')) { ST.note.input = el.value; }
  });

  function stepSong(dir) {
    ST.songIdx = (ST.songIdx + dir + PLAYLIST.length) % PLAYLIST.length;
    ST.song = Object.assign({ cur: 0 }, PLAYLIST[ST.songIdx]);
    renderPage();
  }

  function cycleMemMode() {
    const modes = ['CCM RAM', 'SRAM', 'CPU', 'Tasks'];
    ST.memMode = modes[(modes.indexOf(ST.memMode) + 1) % modes.length];
    renderPage();
  }

  function calcKey(k) {
    if (k === 'C') { ST.calcExpr = ''; }
    else if (k === '=') {
      try {
        const v = Function('"use strict";return(' + ST.calcExpr.replace(/[^0-9+\-*/.()]/g, '') + ')')();
        ST.calcExpr = (v === undefined || Number.isNaN(v)) ? '' : String(+v.toFixed(8));
      } catch (_) { ST.calcExpr = '错误'; }
    } else if (k === '±') {
      ST.calcExpr = ST.calcExpr.startsWith('-') ? ST.calcExpr.slice(1) : '-' + ST.calcExpr;
    } else {
      if (ST.calcExpr === '错误') ST.calcExpr = '';
      ST.calcExpr += k.replace('−', '-').replace('×', '*').replace('÷', '/');
    }
    renderPage();
  }

  /* ======================================================================
     事件：滑块拖动（音乐进度 / 控制中心三个滑块）
     ====================================================================== */
  let drag = null;

  document.addEventListener('pointerdown', (e) => {
    const el = e.target.closest('.slider');
    if (!el) return;
    const host = el.closest('[data-slider]');
    const isMusic = el.id === 'mu-slider';
    if (!host && !isMusic) return;
    if (host && !ST.pw[host.dataset.slider]) return;      // 电源关闭时滑块禁用
    // 缓存矩形：拖动过程中可能发生重绘导致元素脱离文档
    drag = { el, host, isMusic, rect: el.getBoundingClientRect(), total: ST.song.total };
    try { el.setPointerCapture(e.pointerId); } catch (_) {}
    applyDrag(e.clientX);
    e.preventDefault();
  });

  document.addEventListener('pointermove', (e) => { if (drag) applyDrag(e.clientX); });

  document.addEventListener('pointerup', (e) => {
    if (!drag) return;
    applyDrag(e.clientX);
    drag = null;
    if (ST.page === 'music') renderPage();
    if (ST.ccOpen) renderOverlay();
    renderStatus();
  });

  function applyDrag(clientX) {
    const { el, host, isMusic, rect, total } = drag;
    const p = clamp((clientX - rect.left) / rect.width, 0, 1);
    const setBar = () => {
      el.querySelector('.ind').style.width = (p * 100) + '%';
      el.querySelector('.knob').style.left = (p * 100) + '%';
    };

    if (isMusic) {
      ST.song.cur = Math.round(p * total);
      setBar();
      const c = document.querySelector('.mu-prog .t.cur');
      if (c) c.textContent = mmss(ST.song.cur);
      return;
    }
    const key = host.dataset.slider;
    const v = Math.round(p * 255);
    setCc(key, v);
    setBar();
    host.querySelector('.val').textContent = String(v).padStart(3, '0');
    ST.volume = clamp(Math.round(v / 51), 0, 5);
  }

  /* ======================================================================
     事件：桌面滑动翻页
     ====================================================================== */
  (function bindDesktopSwipe() {
    let start = null, moved = false;
    const layer = $('#layer-page');
    const scale = () => parseFloat(getComputedStyle(document.documentElement)
      .getPropertyValue('--s')) || 2.6;

    layer.addEventListener('pointerdown', (e) => {
      if (ST.page !== 'desktop') return;
      start = { x: e.clientX, tile: ST.tile };
      moved = false;
    });

    layer.addEventListener('pointermove', (e) => {
      if (!start) return;
      const dx = (e.clientX - start.x) / scale();
      if (!moved && Math.abs(dx) < 6) return;      // 未达阈值：视为点击，不位移
      moved = true;
      const t = $('#tiles');
      if (t) {
        t.style.transition = 'none';
        t.style.transform = `translateX(${clamp(-start.tile * 240 + dx, -240, 0)}px)`;
      }
    });

    const finish = (e) => {
      if (!start) return;
      const dx = (e.clientX - start.x) / scale();
      const tile0 = start.tile;
      const wasMoved = moved;
      start = null; moved = false;
      if (!wasMoved) return;                       // 单击：交给 click 事件（图标点击）
      if (Math.abs(dx) > 40) ST.tile = clamp(tile0 + (dx < 0 ? 1 : -1), 0, 1);
      renderPage();
    };
    layer.addEventListener('pointerup', finish);
    layer.addEventListener('pointercancel', () => { start = null; moved = false; renderPage(); });
  })();

  /* ======================================================================
     定时器：时钟 / 频谱
     ====================================================================== */
  setInterval(() => {
    ST.now = new Date();
    renderStatus();
    if (ST.page === 'start' || ST.page === 'clock' || ST.page === 'calendar' || ST.page === 'timeset')
      renderPage();
  }, 1000);

  /* 频谱：约 16Hz，与 music_unit.c 的 ms->limit<3 节流一致 */
  setInterval(() => {
    if (ST.page !== 'music' || ST.showLyrics) return;
    if (ST.playing) {
      ST.spectrum = ST.spectrum.map((v, i) => {
        const t = 0.42 + 0.5 * Math.abs(Math.sin(Date.now() / 620 + i * 0.42));
        return v + (t - v) * 0.42;
      });
    } else {
      ST.spectrum = ST.spectrum.map((v) => v * 0.86);
    }
    const bars = document.querySelectorAll('[data-bar]');
    bars.forEach((b, i) => { b.style.width = clamp(ST.spectrum[i], 0, 1) * 60 + 'px'; });
  }, 62);

  /* 播放进度 */
  setInterval(() => {
    if (!ST.playing || ST.page !== 'music') return;
    ST.song.cur = (ST.song.cur + 1) % ST.song.total;
    const s = $('#mu-slider');
    if (s) {
      const p = ST.song.cur / ST.song.total * 100;
      s.querySelector('.ind').style.width = p + '%';
      s.querySelector('.knob').style.left = p + '%';
      const c = document.querySelector('.mu-prog .t.cur');
      if (c) c.textContent = mmss(ST.song.cur);
    }
  }, 1000);

  /* 状态栏小图标轮播（saving 闪烁、CPU 浮动） */
  setInterval(() => {
    ST.cpu = clamp(Math.round(28 + 14 * Math.abs(Math.sin(Date.now() / 4200))), 5, 99);
    ST.ram = clamp(Math.round(38 + 8 * Math.abs(Math.sin(Date.now() / 6100))), 5, 99);
    ST.memSeries = ST.memSeries.slice(1).concat([ST.cpu]);
    if (ST.rightMode === 2) renderStatus();
  }, 2000);

  /* ======================================================================
     光标 —— 左摇杆驱动（lv_port_indev.c）
       cursor += stick * 0.06，限幅 0..239；读写周期同 LV_INDEV_DEF_READ_PERIOD（20ms）
     本工程把「光标移动」放在左摇杆上，右摇杆是确定键。
     ====================================================================== */
  const stickRaw = (v) => Math.round(v / 12 * 127);   // adc.c limit_and_deadzone 限幅 ±127

  setInterval(() => {
    const sx = stickRaw(ST.stickL[0]);
    const sy = stickRaw(ST.stickL[1]);
    if (!sx && !sy) return;                            // 摇杆自回中，松手即停
    ST.cursor.x = clamp(ST.cursor.x + sx * 0.06, 0, 239);
    ST.cursor.y = clamp(ST.cursor.y + sy * 0.06, 0, 239);
    syncStartOffset();
  }, 20);

  /* 把光标偏移贴到开机页的角色立绘上（start_unit.c Update_Start_Unit 第 3 步） */
  function syncStartOffset() {
    if (ST.page !== 'start') return;
    const el = document.querySelector('.pg-start .t-top');
    if (!el) return;
    const { ox, oy } = startOffset(ST);
    el.style.left = (40 + ox) + 'px';
    el.style.top = (94 + oy) + 'px';
  }

  /* ======================================================================
     机身：摇杆 / 唤醒键
     ====================================================================== */
  (function bindHardware() {
    $('#stick-l').addEventListener('pointerdown', (e) => {
      ST.keyL = true; renderPage();
      moveStick(e, 'stick-l', 'stickL');
    });
    $('#stick-r').addEventListener('pointerdown', (e) => {
      ST.keyR = true; renderPage();
      moveStick(e, 'stick-r', 'stickR');
    });
    function moveStick(e, id, key) {
      const el = document.getElementById(id);
      const r = el.getBoundingClientRect();
      const move = (ev) => {
        const dx = clamp((ev.clientX - (r.left + r.width / 2)) / (r.width / 2), -1, 1) * 12;
        const dy = clamp((ev.clientY - (r.top + r.height / 2)) / (r.height / 2), -1, 1) * 12;
        el.querySelector('span').style.transform = `translate(${dx}px,${dy}px)`;
        ST[key] = [Math.round(dx), Math.round(dy)];
        if (ST.page === 'keytest') renderPage();
      };
      el.setPointerCapture(e.pointerId);
      el.addEventListener('pointermove', move);
      el.addEventListener('pointerup', () => {
        el.removeEventListener('pointermove', move);
        el.querySelector('span').style.transform = '';
        ST[key] = [0, 0];
        ST.keyL = ST.keyR = false;
        renderPage();
      }, { once: true });
      move(e);
    }
    $('#btn-wkup').addEventListener('click', back);
  })();

  /* ======================================================================
     键盘快捷键
     ====================================================================== */
  document.addEventListener('keydown', (e) => {
    if (e.target.isContentEditable || e.target.tagName === 'INPUT') return;
    if (e.key === 'Escape' || e.key === 'Backspace') { e.preventDefault(); back(); }
    else if (e.key === 'Enter') { ST.ccOpen = !ST.ccOpen; renderOverlay(); }
    else if (e.key === 'ArrowLeft' && ST.page === 'desktop') { ST.tile = 0; renderPage(); }
    else if (e.key === 'ArrowRight' && ST.page === 'desktop') { ST.tile = 1; renderPage(); }
  });

  /* ======================================================================
     右侧面板
     ====================================================================== */
  const PANEL_PAGES = ['start', 'desktop', 'music', 'settings', 'file', 'clock',
    'calendar', 'calculator', 'about', 'album', 'note', 'canvas_list', 'canvas', 'mem',
    'es9018', 'words', 'usb', 'cmd', 'serial', 'keytest', 'logctrl', 'debug',
    'fontupdate', 'timeset'];

  function buildPanel() {
    $('#page-list').innerHTML = PANEL_PAGES
      .map((p) => `<button data-go="${p}">${p}</button>`).join('');
    $('#page-list').addEventListener('click', (e) => {
      const b = e.target.closest('[data-go]');
      if (b) goto(b.dataset.go);
    });
  }

  function syncPanel() {
    document.querySelectorAll('#page-list [data-go]').forEach((b) => {
      b.classList.toggle('on', b.dataset.go === ST.page);
    });
    if (!ST.meta) ST.meta = $('#meta');
    ST.meta.innerHTML = `
      <dt>当前页面</dt><dd>${ST.page}${ST.page === 'desktop' ? ` · 第 ${ST.tile + 1}/2 屏` : ''}</dd>
      <dt>分辨率</dt><dd>240 × 240 · RGB565</dd>
      <dt>电量</dt><dd>${ST.battery}% · ${ST.voltage.toFixed(2)} V</dd>
      <dt>音量</dt><dd>${ST.bright} / ${ST.hdpVol} / ${ST.spkVol}</dd>
      <dt>输出</dt><dd>${ST.hdpOut ? '耳机 (HDP)' : '喇叭 (SPK)'}</dd>
      <dt>播放</dt><dd>${ST.song.name} · ${mmss(ST.song.cur)} / ${mmss(ST.song.total)}</dd>
      <dt>图标资源</dt><dd class="mono">assets/icons · 61 个</dd>`;
  }

  /* ======================================================================
     启动
     ====================================================================== */
  // 初始化页面默认文本
  ST.txtContent = 'Brahman —— 掌上多媒体终端固件。\n\n' +
    '主控 STM32F405RGT6 @168MHz，1MB Flash / 128KB SRAM / 64KB CCM；\n' +
    '1.54" IPS 240×240，SPI1 + DMA2 刷屏；\n' +
    'ES9018K2M DAC + MAX98357A 功放，I2S2 48kHz/24bit；\n' +
    '16MB W25Q128 + TF 卡（SDIO 4-bit 24MHz）。\n\n' +
    '本页内容由前端模拟，用于对照固件 GUI 实现效果。';

  buildPanel();
  renderStatus(); renderNav(); renderPage(); renderOverlay();

  // 摇杆 / 按键面板的初始值
  ST.stickL = [0, 0]; ST.stickR = [0, 0];

  console.log('%cBrahman UI Simulator', 'font-weight:700;color:#2f6fb3',
    '—— 位图资源由固件 GUI_Icons/*.c 解码而来');
})();
