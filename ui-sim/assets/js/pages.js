/* ==========================================================================
   pages.js —— 页面与组件渲染
   所有坐标 / 尺寸 / 颜色均取自固件 GUI_Unit 源码，1:1 还原。
   ========================================================================== */

/** 生成一个图标节点（PNG alpha 作 mask，color 控制重着色）
 *  mask-image 必须写在内联样式里：相对路径才能相对文档解析。 */
function ic(name, size, color, extra) {
  const u = `assets/icons/${name}.png`;
  const c = color ? `color:${color};` : '';
  return `<i class="ic" style="-webkit-mask-image:url('${u}');mask-image:url('${u}');` +
         `width:${size}px;height:${size}px;${c}${extra || ''}"></i>`;
}

/* 桌面 APP 注册表 —— 顺序与 desktop_unit.c 的 app_icons[] 完全一致 */
const APP_LIST = [
  { ic: 'app_task_manager', label: '任务管理', page: 'mem' },
  { ic: 'app_music',        label: '音乐播放', page: 'music' },
  { ic: 'app_usb',          label: 'USB 控制', page: 'usb' },
  { ic: 'app_note',         label: '备忘录',   page: 'note' },
  { ic: 'app_video',        label: '视频',     page: 'file', path: '0:/MEDIA' },
  { ic: 'app_game',         label: '游戏',     page: 'file', path: '0:/GAME' },
  { ic: 'app_setting',      label: '设置',     page: 'settings' },
  { ic: 'app_drawing',      label: '画板',     page: 'canvas_list' },
  { ic: 'app_file_manager', label: '文件管理', page: 'file' },
  { ic: 'app_clock',        label: '时钟',     page: 'clock' },
  { ic: 'app_calendar',     label: '日历',     page: 'calendar' },
  { ic: 'app_words',        label: '单词本',   page: 'words' },
  { ic: 'app_calculator',   label: '计算器',   page: 'calculator' },
  { ic: 'app_cmd',          label: '终端',     page: 'cmd' },
  { ic: 'app_lots',         label: '抽签',     page: 'lots' },
  { ic: 'app_album',        label: '相册',     page: 'album' },
  { ic: 'app_cmd',          label: '串口',     page: 'serial' },
];

/* 页面标题（导航栏用）—— 取自各 GUI_Page/*.c 的 Create_Navigation_Bar("…") 实参 */
const PAGE_TITLE = {
  about: '关于', album: '相册', calculator: '计算器', calendar: '日历',
  canvas_list: '画板', clock: '闹钟时钟', debug: '系统日志', es9018: 'DAC设置',
  file: '文件管理', keytest: '外设状态', logctrl: '日志设置', lots: '今日运势',
  mem: '任务管理器', note: '记事本', note_list: '记事本', settings: '设置',
  text: '文件管理', timeset: '日期与时间', usb: 'USB设置', words: '背单词',
};

/* 有顶部导航栏的页面（GUI_Page 里调用了 Create_Navigation_Bar 的 18 个页面） */
const PAGES_WITH_NAV = new Set([
  'about', 'album', 'calculator', 'calendar', 'canvas_list', 'clock', 'debug',
  'es9018', 'file', 'keytest', 'logctrl', 'lots', 'mem', 'note', 'note_list',
  'settings', 'text', 'timeset', 'usb', 'words',
]);

/* 既不建导航栏、也不建状态栏的页面（全屏自绘） */
const PAGES_NO_STATUS = new Set(['start', 'canvas', 'fontupdate']);

/* ======================================================================
   各页面渲染函数：返回 240x210 内容区（不含导航栏/状态栏）的 HTML
   ====================================================================== */
const PAGES = {};

/* ------------------------------ 起始页（开机页） ------------------------------ */
/* start_unit.c 的 Update_Start_Unit() 每帧把 top_img 按光标位置重定位：
     offset = (cursor - 120) / 12   （C 整数除法，向零截断），限幅 ±10
     pos    = (40 + offset_x, 94 + offset_y)
   光标由左摇杆驱动（本工程把「光标移动」放在左摇杆，右摇杆是确定键）。
   角色立绘的漂移就是这一屏唯一的动效 —— 固件里没有任何 lv_anim 逐帧动画。 */
function startOffset(st) {
  const lim = (v) => Math.min(10, Math.max(-10, Math.trunc((Math.trunc(v) - 120) / 12)));
  return { ox: lim(st.cursor.x), oy: lim(st.cursor.y) };
}

PAGES.start = (st) => {
  const { ox, oy } = startOffset(st);
  return `
<div class="pg pg-start full" data-act="to-desktop" title="点击进入桌面">
  <img class="wp" src="assets/icons/pic_back.png" alt="">
  <div class="t-date">${st.dateText}</div>
  <div class="t-time">${st.hhmm}</div>
  <img class="t-top" src="assets/icons/pic_top.png" alt=""
       style="left:${40 + ox}px;top:${94 + oy}px">
</div>`;
};

/* ------------------------------ 桌面 ------------------------------ */
PAGES.desktop = (st) => {
  const perPage = 12, cols = 4, rows = 3;
  const IW = 40, GAP = 16, SX = 16, SY = 16;
  let html = `
  <div class="pg pg-desktop no-nav">
    <img class="wp" src="assets/icons/pic_back.png" alt="">
    <div class="tiles" id="tiles" style="transform:translateX(${-st.tile * 240}px)">`;

  for (let p = 0; p < 2; p++) {
    html += '<div class="tile">';
    for (let i = 0; i < perPage; i++) {
      const idx = p * perPage + i;
      const app = APP_LIST[idx];
      if (!app) continue;
      const r = Math.floor(i / cols), c = i % cols;
      const x = SX + c * (IW + GAP), y = SY + r * (IW + GAP);
      html += `<button class="app-ic" style="left:${x}px;top:${y}px"
                 data-app="${idx}" title="${app.label}">
                 ${ic(app.ic, 36, 'var(--c-app-icon)')}
               </button>`;
    }
    html += '</div>';
  }
  html += `</div>
    <div class="dots">
      <i class="${st.tile === 0 ? 'on' : ''}"></i><i class="${st.tile === 1 ? 'on' : ''}"></i>
    </div>
  </div>`;
  return html;
};

/* ------------------------------ 音乐播放器 ------------------------------ */
PAGES.music = (st) => {
  const s = st.song;
  const fmt = (v) => `${String(Math.floor(v / 60)).padStart(2, '0')}:${String(v % 60).padStart(2, '0')}`;
  const modeIc = ['music_order', 'music_random', 'music_cycle'][st.mode];
  const modeTint = ['#bdbdbd', '#bdbdbd', '#bdbdbd'][st.mode];

  let bars = '';
  for (let i = 0; i < 20; i++) {
    bars += `<i data-bar="${i}" style="top:${114 - i * 6}px"></i>`;
  }

  const lrc = ['当星轨划过无声的海', '我把名字写进夜风里', '（副歌）', '夜航星 带我回家', '穿过光年以外的回答'];
  const lyricHTML = lrc.map((t, i) =>
    `<p class="${i === 2 ? 'cur' : ''}" style="top:${i === 2 ? 48 : i < 2 ? 48 - (2 - i) * 20 : 48 + (i - 2) * 20}px">${t}</p>`
  ).join('');

  return `
  <div class="pg pg-music no-nav">
    <div class="mu-album" data-act="toggle-lyrics">
      ${ic('app_music', 60, '#ffffff', 'position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);opacity:.85')}
      <div class="tag">0:/COVER · 占位封面</div>
    </div>
    <div class="mu-spec" data-act="toggle-lyrics">${bars}</div>
    <div class="mu-lyrics ${st.showLyrics ? 'show' : ''}" data-act="toggle-lyrics">${lyricHTML}</div>

    <button class="mu-btn" style="top:10px"  data-act="music-exit" title="退出">${ic('music_exit', 24, '#ffb74d')}</button>
    <button class="mu-btn" style="top:42px"  data-act="music-file" title="播放列表">${ic('music_menu', 24, '#bdbdbd')}</button>
    <button class="mu-btn" style="top:74px"  data-act="music-mode" title="播放模式">${ic(modeIc, 24, modeTint)}</button>
    <button class="mu-btn" style="top:106px" data-act="music-like" title="喜欢">${ic('music_like', 24, st.liked ? '#ef5350' : '#bdbdbd')}</button>

    <div class="mu-name"><span>${s.name}</span></div>

    <div class="mu-ctrl">
      <button class="mu-btn" style="left:0"  data-act="music-prev">${ic('music_before', 24, '#bdbdbd')}</button>
      <button class="mu-btn" style="left:34px" data-act="music-play">${ic(st.playing ? 'music_suspend' : 'music_playing', 24, '#bdbdbd')}</button>
      <button class="mu-btn" style="left:68px" data-act="music-next">${ic('music_next', 24, '#bdbdbd')}</button>
    </div>

    <div class="mu-fmt">${s.fmt}</div>

    <div class="mu-prog">
      <span class="t cur">${fmt(s.cur)}</span>
      <div class="slider" id="mu-slider" style="position:absolute;left:40px;top:5px;width:140px">
        <div class="ind" style="width:${(s.cur / s.total * 100).toFixed(1)}%"></div>
        <div class="knob" style="left:${(s.cur / s.total * 100).toFixed(1)}%"></div>
      </div>
      <span class="t tot">${fmt(s.total)}</span>
    </div>
  </div>`;
};

/* ------------------------------ 设置 ------------------------------ */
const SETTINGS_ITEMS = [
  { t: '输入设备监控', page: 'keytest' },
  { t: '日志设置', page: 'logctrl' },
  { t: '关于本机', page: 'about' },
  { t: 'DAC 设置', page: 'es9018' },
  { t: '日期与时间', page: 'timeset' },
  { t: '提升 UAC 兼容性', sw: 'uac1' },
];
PAGES.settings = (st) => `
  <div class="pg pg-settings">
    <div class="set-list">
      ${SETTINGS_ITEMS.map((it) => it.sw
        ? `<div class="set-item" data-act="toggle-uac">${it.t}
             <span class="sw ${st.uac1 ? 'on' : ''}"><i></i></span></div>`
        : `<div class="set-item" data-page="${it.page}">${it.t}<span class="arrow">›</span></div>`
      ).join('')}
    </div>
  </div>`;

/* ------------------------------ 文件管理 ------------------------------ */
const FM_FILES = {
  '0:/': [
    { d: 'MUSIC', t: 'dir' }, { d: 'MEDIA', t: 'dir' }, { d: 'GAME', t: 'dir' },
    { d: 'COVER', t: 'dir' }, { d: 'LYRICS', t: 'dir' }, { d: 'FONT', t: 'dir' },
    { d: 'SYSTEM', t: 'dir' },
  ],
  '0:/MUSIC': [
    { d: '夜航星.flac', t: 'file' }, { d: 'Sundial.mp3', t: 'file' },
    { d: 'Blue Hour.wav', t: 'file' }, { d: '暮色回响.ape', t: 'file' },
    { d: 'Nocturne.aac', t: 'file' }, { d: '海边书.ogg', t: 'file' },
  ],
  '0:/MEDIA': [{ d: 'demo.png', t: 'file' }, { d: 'loop.gif', t: 'file' },
               { d: 'sample.jpg', t: 'file' }, { d: 'clip.avi', t: 'file' }],
  '0:/GAME': [{ d: 'contra.nes', t: 'file' }, { d: 'tank.nes', t: 'file' }],
};
PAGES.file = (st) => {
  const path = st.fsPath || '0:/';
  const list = FM_FILES[path] || FM_FILES['0:/'];
  return `
  <div class="pg pg-file">
    <div class="fm-head">${ic('file_exit_icon', 16, '#000')}<span>${path}</span></div>
    <div class="fm-list">
      ${path === '0:/' ? `
        <div class="fm-drive">${ic('file_folder_icon', 16, '#000')}<span>SD:</span>
          <div class="bar" style="width:112px"></div></div>
        <div class="fm-drive">${ic('file_folder_icon', 16, '#000')}<span>Flash:</span>
          <div class="bar" style="width:64px;background:#9e9e9e"></div></div>` : ''}
      ${list.map((f) => `<div class="fm-row" data-file="${f.d}">
          ${ic(f.t === 'dir' ? 'file_folder_icon' : 'file_file_icon', 16, '#000')}
          <span>${f.d}</span></div>`).join('')}
    </div>
    <div class="fm-acts">
      <button>复制</button><button class="paste">粘贴</button><button class="del">删除</button>
    </div>
  </div>`;
};

/* ------------------------------ 时钟 ------------------------------ */
const CK_TABS = ['时钟', '计时', '秒表', '闹钟'];
PAGES.clock = (st) => {
  const tab = st.clockTab;
  let body = '';
  if (tab === 0) {
    body = `<div class="ck-clock"><div class="big">${st.hms}</div>
            <div class="sub">${st.dateText}</div></div>`;
  } else if (tab === 1 || tab === 3) {
    const roll = (n, unit, x) => `
      <div class="roller" style="left:${x}px">
        <div class="sel"></div>
        <div class="col" style="top:0px">${String((n + 1) % 24).padStart(2, '0')}</div>
        <div class="col" style="top:20px">${String(n).padStart(2, '0')}</div>
        <div class="col" style="top:40px">${String((n + 23) % 24).padStart(2, '0')}</div>
      </div>
      <span class="unit" style="left:${x + 49}px">${unit}</span>`;
    body = `<div style="position:absolute;left:24px;top:34px">${roll(0, '时', 0)}${roll(25, '分', 78)}${roll(10, '秒', 156)}</div>
            <button class="big-btn" style="position:absolute;left:70px;top:118px;width:100px;height:22px">
              ${tab === 1 ? '开始' : '保存'}</button>`;
  } else {
    body = `<div style="position:absolute;left:60px;top:44px;font-size:40px;font-variant-numeric:tabular-nums">${st.swTime}</div>
            <button class="big-btn" style="position:absolute;left:30px;top:110px;width:80px;height:24px">开始</button>
            <button class="big-btn" style="position:absolute;left:130px;top:110px;width:80px;height:24px">复位</button>`;
  }
  return `
  <div class="pg pg-clock">
    <div class="ck-body">${body}</div>
    <div class="ck-nav">
      ${CK_TABS.map((t, i) => `<button class="${i === tab ? 'on' : ''}" data-tab="${i}">${t}</button>`).join('')}
    </div>
  </div>`;
};

/* ------------------------------ 日历 ------------------------------ */
PAGES.calendar = (st) => {
  const y = st.now.getFullYear(), m = st.now.getMonth(), d = st.now.getDate();
  const first = new Date(y, m, 1).getDay();
  const days = new Date(y, m + 1, 0).getDate();
  let cells = ['日', '一', '二', '三', '四', '五', '六'].map((w) => `<div class="hd">${w}</div>`).join('');
  for (let i = 0; i < first; i++) cells += '<div></div>';
  for (let i = 1; i <= days; i++) cells += `<div class="${i === d ? 'sel' : ''}">${i}</div>`;
  return `
  <div class="pg">
    <div class="cal-head">
      <span class="ym">${y} 年 ${m + 1} 月</span>
      <span class="cal-nav"><button>‹</button><button>›</button></span>
    </div>
    <div class="cal-grid">${cells}</div>
  </div>`;
};

/* ------------------------------ 计算器 ------------------------------ */
const CALC_KEYS = [
  ['C', 'op'], ['(', 'op'], [')', 'op'], ['÷', 'op'],
  ['7', ''], ['8', ''], ['9', ''], ['×', 'op'],
  ['4', ''], ['5', ''], ['6', ''], ['−', 'op'],
  ['1', ''], ['2', ''], ['3', ''], ['+', 'op'],
  ['±', ''], ['0', ''], ['.', ''], ['=', 'eq'],
];
PAGES.calculator = (st) => `
  <div class="pg">
    <div class="calc-disp">${st.calcExpr || '0'}</div>
    <div class="calc-grid">
      ${CALC_KEYS.map(([k, c]) => `<button class="${c}" data-key="${k}">${k.replace('−', '-').replace('×', '*').replace('÷', '/')}</button>`).join('')}
    </div>
  </div>`;

/* ------------------------------ 关于本机 ------------------------------ */
/* 与 about_unit.c 1:1：品牌标识是「描边环 + 字面」两张 142x22 的 1bit 掩膜叠加
   （同画布同偏移，对应固件的 img_edge / img_icon），容器 .about-logo 定位 top=9px。
   7 行信息 y=34 起行距 20。设备ID 为模拟值 —— 真机读 0x1FFF7A10 的 96 位 UID。
   每行段落数组 [文本, 是否黑色]，与固件的 #808080/#000000 recolor 对应。 */
PAGES.about = () => `
  <div class="pg">
    <div class="about-logo">
      ${ic('brahman_icon_edge', 142, '#0b2a5a', 'width:142px;height:22px')}
      ${ic('brahman_icon', 142, '#ffd900', 'width:142px;height:22px;position:absolute;left:0;top:0')}
    </div>
    ${[[['设备名称: ', 0], ['Brahman', 1]],
       [['设备ID: ', 0], ['XXXXXXXX 05421W7', 1]],
       [['处理器: ', 0], ['STM32F405RGT6', 1]],
       [['运行内存: ', 0], ['192KB', 1], [' 板载储存: ', 0], ['16MB', 1]],
       [['电池容量: ', 0], ['2000mAh', 1]],
       [['软件版本: ', 0], ['V1.0', 1]],
       [['构建: ', 0], ['ARMCC5', 1]]]
      .map((segs, i) => `<div class="about-row" style="top:${34 + i * 20}px">
        <span class="bar"></span><span class="txt">${segs.map((s) =>
          `<span class="${s[1] ? 'k' : ''}">${s[0]}</span>`).join('')}</span></div>`).join('')}
  </div>`;

/* ------------------------------ 相册 ------------------------------ */
PAGES.album = () => `
  <div class="pg pg-album">
    <div class="holder">${ic('app_video', 64, '#ffffff', 'opacity:.35')}</div>
    <div class="zone" style="left:0"  data-act="album-prev"></div>
    <div class="zone" style="right:0" data-act="album-next"></div>
  </div>`;

/* ------------------------------ 记事本 ------------------------------ */
/* 模拟 TF 卡上的 0:/Note/ 目录。bad 标记对应固件里两类"只列出、点了弹提示、
   不让进编辑页"的文件：'big' = 超过 NOTE_FILE_MAX，'long' = 名字超过 48 字节 */
const NOTE_FILES = [
  { n: '备忘录', t: '备忘 · 阶段 7：Tasks 装配（8 个任务）\n· 复查 L6218E 未定义符号\n\n· 记事本：列表 / 分页 / 命名保存 / 退出确认 / 写盘走后台任务' },
  { n: '开发日志', t: '09-27  file_unit / file_page / fileop_task 落地\n10-01  music_unit 全链落地\n10-02  album / settings / mem / note，记事本重做' },
  { n: '面试要点', t: '1. TLSF 接管 pvPortMalloc：bsc 池可 DMA、ccm 池不可\n2. FatFs 挂两个逻辑盘：DEV_SD / DEV_USB\n3. LVGL 刷新：lv_task → 脏区合并 → SPI1+DMA2' },
  { n: '购物清单', t: '牛奶\n鸡蛋\n打印纸\nSD 读卡器' },
  { n: '引脚备忘', t: 'PB14 / PB15 = OTG_HS (AF14)\nPA0 = WKUP\nPC0 / PC1 = 右摇杆\nPB0 / PB1 = 左摇杆' },
  { n: 'TODO', t: '- 分页\n- 命名保存（查重）\n- 退出确认\n- 写盘并入 fileop_task' },
  { n: '调试记录', t: 'L6218E: Undefined symbol page_music_interface\n→ 直跑 CLI 的 --rebuild 会静默跳文件，门禁之外要另查日志' },
  { n: '歌词草稿', t: '夜航星 · 第一段\n（待补）' },
  { n: '灵感', t: '摇杆做光标、右摇杆做确定' },
  { n: '读书笔记', t: '《计算机程序的构造和解释》第 1 章：用过程抽象替代状态' },
  { n: 'dump', bad: 'big' },
  { n: '这是一个特别特别长的笔记名字用来触发名过长分支', bad: 'long' },
];

const NT_PAGE_SIZE = 5;   // 固件 note_unit.c 的 NOTE_PAGE_SIZE

/** 记事本页面 —— 列表视图 / 编辑视图，坐标全部取自 note_unit.c */
PAGES.note = (st) => (st.note.view === 'edit' ? ntEditHTML(st.note) : ntListHTML(st.note));

/** 列表视图：顶栏「< 页码 >  新建」+ 每页 5 条 */
function ntListHTML(nt) {
  const max = Math.max(0, Math.ceil(NOTE_FILES.length / NT_PAGE_SIZE) - 1);
  const cur = Math.min(nt.page, max);
  const rows = NOTE_FILES.slice(cur * NT_PAGE_SIZE, cur * NT_PAGE_SIZE + NT_PAGE_SIZE);
  const mark = (f) => (f.bad === 'big' ? '（过大）' : f.bad === 'long' ? '（名过长）' : '');
  return `
  <div class="pg pg-note">
    ${cur === 0 ? '' : `<button class="nt-btn" style="left:10px;top:2px;width:30px;height:24px" data-act="nt-prev">&lt;</button>`}
    <div class="nt-pager" style="left:42px;width:58px">${cur + 1}/${max + 1}</div>
    ${cur >= max ? '' : `<button class="nt-btn" style="left:102px;top:2px;width:30px;height:24px" data-act="nt-next">&gt;</button>`}
    <button class="nt-btn" style="left:174px;top:2px;width:56px;height:24px" data-act="nt-new">新建</button>
    <div class="nt-list">
      ${rows.length === 0
        ? `<div class="nt-empty">还没有笔记<br>点右上角「新建」写第一篇</div>`
        : rows.map((f, i) => `<div class="nt-item${f.bad ? ' bad' : ''}" data-note="${cur * NT_PAGE_SIZE + i}">
             <span>${mark(f)}${f.n}</span></div>`).join('')}
    </div>
  </div>`;
}

/** 编辑视图：顶栏「列表」+ 当前笔记名，下面是正文框（无页内保存键） */
function ntEditHTML(nt) {
  return `
  <div class="pg pg-note">
    <button class="nt-btn" style="left:10px;top:2px;width:56px;height:24px" data-act="nt-list">列表</button>
    <div class="nt-name" style="left:72px;width:158px">${nt.name || '未命名'}</div>
    <div class="nt-edit" contenteditable="true" spellcheck="false" data-nt-edit>${nt.text}</div>
  </div>`;
}

/** 记事本弹窗：对应固件挂在 lv_layer_top 上的那几个，用屏幕坐标（对话框顶边 y=30） */
function noteDialogHTML(st) {
  const nt = st.note;
  if (!nt.dlg) return '';
  if (nt.dlg === 'ask') {
    const msg = nt.msg || '这篇笔记还没保存\n要保存吗?';
    return `
    <div class="nt-dlg" style="top:30px;height:94px">
      <div class="nt-dlg-msg" style="left:12px;top:12px;width:216px;text-align:center;white-space:pre-line">${msg}</div>
      <button class="nt-btn" style="left:8px;top:58px;width:68px;height:26px" data-act="nt-ask-save">保存</button>
      <button class="nt-btn" style="left:86px;top:58px;width:68px;height:26px" data-act="nt-ask-drop">不保存</button>
      <button class="nt-btn" style="left:164px;top:58px;width:68px;height:26px" data-act="nt-ask-stay">取消</button>
    </div>`;
  }
  if (nt.dlg === 'name') {
    return `
    <div class="nt-dlg" style="top:30px;height:86px">
      <div class="nt-dlg-msg${nt.err ? ' err' : ''}" style="left:10px;top:4px;width:220px">${nt.err || '给笔记起个名字'}</div>
      <input class="nt-dlg-in" style="top:26px" value="${nt.input || ''}" data-nt-input>
      <button class="nt-btn" style="left:10px;top:56px;width:60px;height:26px" data-act="nt-name-cancel">取消</button>
      <button class="nt-btn" style="left:170px;top:56px;width:60px;height:26px" data-act="nt-name-ok">确定</button>
    </div>`;
  }
  return `
  <div class="nt-dlg" style="top:30px;height:86px">
    <div class="nt-dlg-msg" style="left:12px;top:10px;width:216px;text-align:center;white-space:pre-line">${nt.msg || ''}</div>
    <button class="nt-btn" style="left:84px;top:52px;width:72px;height:26px" data-act="nt-tip-ok">知道了</button>
  </div>`;
}

/* ------------------------------ 内存 / 任务监控 ------------------------------ */
PAGES.mem = (st) => {
  const series = st.memSeries;
  const pts = series.map((v, i) => `${(i / (series.length - 1) * 218).toFixed(1)},${(98 - v * 96 / 100).toFixed(1)}`).join(' ');
  const rows = [
    ['CCM用', `${st.ccmUsed}KB`], ['CCM空', `${(62.3 - st.ccmUsed).toFixed(1)}KB`],
    ['SRAM用', `${st.sramUsed}KB`], ['BSC堆', '98KB'],
    ['CPU', `${st.cpu}%`], ['任务', '8'],
  ];
  return `
  <div class="pg">
    <div class="mem-wrap">
      <div class="mem-title" data-act="mem-mode">${st.memMode}</div>
      <div class="mem-chart">
        <svg viewBox="0 0 220 100" preserveAspectRatio="none">
          <polyline points="${pts}" fill="none" stroke="#2196f3" stroke-width="1.2"/>
        </svg>
      </div>
      <div class="mem-grid">
        <div class="mem-tbl">${rows.slice(0, 3).map((r) => `<div><span>${r[0]}</span><span>${r[1]}</span></div>`).join('')}</div>
        <div class="mem-tbl">${rows.slice(3, 6).map((r) => `<div><span>${r[0]}</span><span>${r[1]}</span></div>`).join('')}</div>
      </div>
    </div>
  </div>`;
};

/* ------------------------------ DAC 设置 ------------------------------ */
/* 与 es9018_unit.c 一致：18 行配置，行高 20。
   cyc=循环按钮(110x20, #EEEEEE)  sl=滑条(110x16)  sw=开关(40x16)
   opts / max 取自固件的 filter_opts、iir_opts、ch_opts、stop_div_opts 与各滑块量程。 */
const DAC_ROWS = [
  { t: 'cyc', n: '数字滤波器',     v: 'Slow Rolloff', opts: ['Fast Rolloff', 'Slow Rolloff', 'Minimum Phase'] },
  { t: 'cyc', n: 'IIR 滤波器带宽', v: '1.3605 fs',    opts: ['1.0757 fs', '1.1338 fs', '1.3605 fs', '1.5873 fs'] },
  { t: 'sw',  n: '旁路 IIR 滤波',  v: false },
  { t: 'sl',  n: 'DPLL 带宽',      v: 5,   max: 15 },
  { t: 'cyc', n: '锁定信号时钟数', v: '3276',         opts: ['18364', '8192', '5461', '4096', '3276', '2730', '2340', '2048'] },
  { t: 'sl',  n: '音量渐变速率',   v: 0,   max: 7 },
  { t: 'sw',  n: '平滑自动静音',   v: true },
  { t: 'sl',  n: '自动静音时间',   v: 104, max: 255 },
  { t: 'sl',  n: '自动静音等级',   v: 5,   max: 127 },
  { t: 'sw',  n: '静音通道 1',     v: false },
  { t: 'sw',  n: '静音通道 2',     v: true },
  { t: 'cyc', n: '通道 1 映射',    v: 'Left',         opts: ['Left', 'Right'] },
  { t: 'cyc', n: '通道 2 映射',    v: 'Right',        opts: ['Left', 'Right'] },
  { t: 'sw',  n: '旁路 THD 补偿',  v: false },
  { t: 'sl',  n: 'C2 谐波补偿',    v: 0,   max: 2000 },
  { t: 'sl',  n: 'C3 谐波补偿',    v: 0,   max: 2000 },
  { t: 'sw',  n: '失锁时强制静音', v: true },
  { t: 'sw',  n: '失锁时输出变低', v: true },
];

PAGES.es9018 = (st) => {
  const dac = st.dac || {};
  return `
  <div class="pg">
    <div class="dac-wrap">
      <div class="dac-h"><span class="t">ES9018K2M 芯片高级配置</span></div>
      <div class="dac-line"></div>
      ${DAC_ROWS.map((r, i) => {
        const v = dac[i] !== undefined ? dac[i] : r.v;
        if (r.t === 'cyc') return `<div class="dac-row">
            <span class="n">${r.n}</span>
            <span class="cyc" data-dac="${i}">${v}</span></div>`;
        if (r.t === 'sl') return `<div class="dac-row">
            <span class="n">${r.n}</span>
            <span class="sl"><span class="slider"><span class="ind"
              style="width:${(v / r.max * 100).toFixed(0)}%"></span></span>
              <span class="dv">${v}</span></span></div>`;
        return `<div class="dac-row">
            <span class="n">${r.n}</span>
            <span class="sw ${v ? 'on' : ''}" data-dac-sw="${i}"><i></i></span></div>`;
      }).join('')}
      <div style="height:10px"></div>
    </div>
  </div>`;
};

/* ------------------------------ 画作列表 ------------------------------ */
/* 对应 canvas_list_unit.c 的 List_Cfg_t：
     dir=0:/Canvas  ext=.bmp  page_size=5  name_max=48  file_max=CANVAS_FILE_MAX
     空态"还没有画作\n点右上角「新建」画一张"  动作按钮="新建"
     条目过大 / 名过长：置灰，点了只弹提示不进画布
   list_unit.c 几何：顶栏 32 高，条目 26 高、间隔 2、宽 232，内容区 240x180。 */
const CANVAS_FILES = [
  { n: '月亮', t: '40x40' },
  { n: '小猫', t: '40x40' },
  { n: '山', t: '40x40' },
  { n: '星星', t: '40x40' },
  { n: 'test', t: '40x40' },
  { n: '一个特别特别长的画作名字用来触发名过长分支', bad: 'long' },
];
const CV_PAGE_SIZE = 5;

PAGES.canvas_list = (st) => {
  const cv = st.canvasList || { page: 0, dlg: null, msg: '' };
  const max = Math.max(0, Math.ceil(CANVAS_FILES.length / CV_PAGE_SIZE) - 1);
  const cur = Math.min(cv.page, max);
  const rows = CANVAS_FILES.slice(cur * CV_PAGE_SIZE, cur * CV_PAGE_SIZE + CV_PAGE_SIZE);
  const mark = (f) => (f.bad === 'long' ? '（名过长）' : f.bad === 'big' ? '（过大）' : '');
  return `
  <div class="pg pg-list">
    ${cur === 0 ? '' : `<button class="ls-btn" style="left:8px;top:3px;width:26px;height:26px"
        data-act="cv-prev">&lt;</button>`}
    <div class="ls-pager" style="left:40px;width:60px">${cur + 1}/${max + 1}</div>
    ${cur >= max ? '' : `<button class="ls-btn" style="left:100px;top:3px;width:26px;height:26px"
        data-act="cv-next">&gt;</button>`}
    <button class="ls-btn" style="left:178px;top:3px;width:56px;height:26px"
      data-act="cv-new">新建</button>
    <div class="ls-list">
      ${rows.length === 0
        ? '<div class="ls-empty">还没有画作<br>点右上角「新建」画一张</div>'
        : rows.map((f, i) => `<div class="ls-item${f.bad ? ' bad' : ''}"
             data-canvas="${cur * CV_PAGE_SIZE + i}"><span>${mark(f)}${f.n}</span></div>`).join('')}
    </div>
  </div>`;
};

/* 列表页的提示弹窗（list_unit.c 的 List_Open_Tip_Dlg：高 86，按钮 72x26 居中 y=52） */
function listTipHTML(cv) {
  if (!cv || cv.dlg !== 'tip') return '';
  return `
  <div class="nt-dlg" style="top:30px;height:86px">
    <div class="nt-dlg-msg" style="left:12px;top:10px;width:216px;text-align:center;white-space:pre-line">${cv.msg || ''}</div>
    <button class="nt-btn" style="left:84px;top:52px;width:72px;height:26px" data-act="cv-tip-ok">知道了</button>
  </div>`;
}

/* ------------------------------ 画板 ------------------------------ */
PAGES.canvas = () => `
  <div class="pg full" style="background:#808080">
    <!-- 缩略图 -->
    <div style="position:absolute;left:10px;top:10px;width:40px;height:40px;
                border:1px solid #555;background:#fff;box-shadow:0 2px 8px rgba(0,0,0,.35)"></div>
    <!-- 浮动工具面板 120x120 -->
    <div style="position:absolute;left:10px;top:60px;width:120px;height:120px;background:#fff;
                border-radius:11px;border:1px solid #808080;padding:8px;
                display:grid;grid-template-columns:repeat(3,30px);gap:4px;align-content:start">
      ${['canvas_before', 'canvas_exit', 'canvas_next',
         'canvas_small', 'canvas_big', 'canvas_save',
         'canvas_pencil', 'canvas_rubber', 'canvas_load'].map((n) =>
        `<button style="width:30px;height:30px;border:0;border-radius:3px;background:#ccc;
                 opacity:.9;display:flex;align-items:center;justify-content:center;padding:0">
           ${ic(n, 20, '#000')}</button>`).join('')}
    </div>
    <!-- 色环 -->
    <div style="position:absolute;left:136px;top:126px;width:96px;height:96px;border-radius:50%;
                box-shadow:0 3px 10px rgba(0,0,0,.4);
                background:conic-gradient(#f66,#fc6,#6f6,#6ff,#66f,#f6f,#f66)">
      <div style="position:absolute;left:26px;top:26px;width:44px;height:44px;border-radius:50%;
                  background:#fff"></div>
    </div>
  </div>`;

/* ------------------------------ 背单词 ------------------------------ */
/* 与 words_unit.c 一致，三个子视图（ws->flashcard_cont / list_cont / book_cont）：
     0 卡片：顶栏三按钮（选书 26x26、模式 56x26 居中、目录 26x26），中间单词 + 音标 + 释义，底部 不熟/认识 各 65x26
     1 目录：顶栏 26 高，每页 6 条，条目 230x40
     2 选书：顶栏 26 高 + 词书列表 */
const WORDS_DB = [
  { w: 'nocturne',  p: 'ˈnɒktɜːn', m: 'n. 夜曲；夜景画', def: '一种抒情、沉静的器乐曲，多用于钢琴。' },
  { w: 'twilight',  p: 'ˈtwaɪlaɪt', m: 'n. 暮色；黄昏', def: '日落之后天色未全黑的那段时间。' },
  { w: 'resonance', p: 'ˈrezənəns', m: 'n. 共鸣；共振', def: '物体在特定频率下振幅显著增大的现象。' },
  { w: 'serene',    p: 'səˈriːn', m: 'adj. 宁静的', def: '平静而不受打扰的状态。' },
  { w: 'cascade',   p: 'kæˈskeɪd', m: 'n. 小瀑布；层叠', def: '水沿阶梯状落下，或事物连续层叠出现。' },
  { w: 'aperture',  p: 'ˈæpətʃə', m: 'n. 光圈；孔径', def: '镜头中控制进光量的开口。' },
  { w: 'firmware',  p: 'ˈfɜːmweə', m: 'n. 固件', def: '固化在硬件非易失存储器中的程序。' },
  { w: 'latency',   p: 'ˈleɪtənsi', m: 'n. 时延', def: '从输入到产生响应之间的时间。' },
  { w: 'disperse',  p: 'dɪˈspɜːs', m: 'v. 分散；驱散', def: '使向不同方向散开。' },
  { w: 'harbour',   p: 'ˈhɑːbə', m: 'n. 港口', def: '船只停泊避风的水域。' },
];
const WORD_BOOKS = ['CET-6 核心词汇', '考研高频词', 'TOEFL 精选'];

PAGES.words = (st) => {
  const w = st.words || { view: 0, idx: 0, mode: 0, book: 0, listPage: 0, shown: false };
  const item = WORDS_DB[w.idx];

  if (w.view === 1) {                       /* 单词目录：每页 6 条 */
    const per = 6;
    const max = Math.ceil(WORDS_DB.length / per) - 1;
    const cur = Math.min(w.listPage || 0, max);
    const rows = WORDS_DB.slice(cur * per, cur * per + per);
    return `
    <div class="pg pg-words">
      <div class="wd-top">
        <button class="ls-btn" style="left:2px" data-act="wd-list-up">&lt;</button>
        <span class="t">${cur + 1} / ${max + 1}</span>
        <button class="ls-btn" style="right:2px" data-act="wd-list-down">&gt;</button>
      </div>
      <div class="wd-list">
        ${rows.map((it, i) => `<div class="wd-row" data-act="wd-open" data-w="${cur * per + i}">
            <b>${it.w}</b> ${it.m}</div>`).join('')}
      </div>
    </div>`;
  }

  if (w.view === 2) {                       /* 选书 */
    return `
    <div class="pg pg-words">
      <div class="wd-top"><span class="t" style="left:0;width:240px">选择词书</span></div>
      <div class="wd-list">
        ${WORD_BOOKS.map((b, i) => `<div class="wd-row book ${i === w.book ? 'on' : ''}"
            data-act="wd-book" data-b="${i}">${b}</div>`).join('')}
        ${WORD_BOOKS.length ? '' : '<div class="ls-empty">0:/WORDLIST 下没有词书</div>'}
      </div>
    </div>`;
  }

  /* 卡片视图：顶栏三按钮 + 释义显隐 */
  return `
  <div class="pg pg-words">
    <button class="wd-btn" style="left:2px;top:5px;width:26px;height:26px" data-act="wd-books">书</button>
    <button class="wd-btn mode" data-act="wd-mode">${w.mode ? '随机' : '顺序'}</button>
    <button class="wd-btn" style="right:2px;top:5px;width:26px;height:26px" data-act="wd-list">目</button>

    <div class="wd-card" data-act="wd-show">
      <div class="w">${item.w}</div>
      <div class="p">[${item.p}]</div>
      <div class="m ${w.shown ? '' : 'hid'}">${item.m}</div>
      <div class="d ${w.shown ? '' : 'hid'}">${item.def}</div>
    </div>

    <button class="wd-act" style="left:55px" data-act="wd-prev">不熟</button>
    <button class="wd-act" style="left:130px" data-act="wd-next">认识</button>
  </div>`;
};

/* ------------------------------ USB 控制 ------------------------------ */
/* 与 usb_ctrl_unit.c 一致：左侧 100x90 滚轮 + 上下移按钮(100x30)，
   右侧 110x60 状态卡 + 启动/终止按钮(110x40，淡蓝 0x74C0FC / 淡红 0xFF8A80)。
   滚轮 8 项与 func_list[] 顺序严格一致。 */
const USB_FUNCS = ['虚拟U盘', '解码耳放', '电脑副屏', '模拟手柄', '模拟键盘', '模拟鼠标', 'U盘读取', '外设输入'];

PAGES.usb = (st) => {
  const usb = st.usb || { idx: 0, running: false };
  const i = usb.idx;
  const prev = (i + USB_FUNCS.length - 1) % USB_FUNCS.length;
  const next = (i + 1) % USB_FUNCS.length;
  const statusText = usb.running ? `${USB_FUNCS[i]} 已启动` : '未启动';
  return `
  <div class="pg" style="background:#f0f2f5">
    <button class="usb-nav" style="left:10px;top:10px" data-act="usb-up">▲ 上移</button>
    <div class="usb-roller">
      <div class="usb-row" style="top:0">${USB_FUNCS[prev]}</div>
      <div class="usb-row sel">${USB_FUNCS[i]}</div>
      <div class="usb-row" style="top:60px">${USB_FUNCS[next]}</div>
    </div>
    <button class="usb-nav" style="left:10px;top:140px" data-act="usb-down">▼ 下移</button>

    <div class="usb-card">
      <div class="k">USB 功能</div>
      <div class="v ${usb.running ? 'on' : ''}">${statusText}</div>
    </div>
    <button class="usb-start" data-act="usb-start">启动</button>
    <button class="usb-end" data-act="usb-end">终止</button>
  </div>`;
};

/* ------------------------------ 终端 ------------------------------ */
PAGES.cmd = () => `
  <div class="pg no-nav">
    <div style="position:absolute;left:20px;top:16px;width:200px;height:60px;background:#fff;
                border:2px solid #bbdefb;border-radius:8px;padding:8px">
      <div style="font-size:12px;color:#888">USB CDC 虚拟串口</div>
      <div style="font-size:12px;margin-top:4px">已连接 · 115200</div>
    </div>
    <div style="position:absolute;left:20px;top:88px;width:200px;height:60px;background:#e3f2fd;
                border:2px solid #2196f3;border-radius:8px;padding:8px">
      <div style="font-size:12px;color:#1565c0">直接模式（无 USB）</div>
      <div style="font-size:12px;margin-top:4px">Letter Shell 本地终端</div>
    </div>
    <div style="position:absolute;left:20px;top:162px;font-size:12px;color:#888">
      选择通道后进入命令行 →</div>
  </div>`;

/* ------------------------------ 抽签 ------------------------------ */
PAGES.lots = () => `
  <div class="pg" style="background:transparent">
    <div style="position:absolute;right:4px;top:0;width:180px;height:180px;background:#fff;
                border-radius:10px;display:flex;align-items:center;justify-content:center;
                box-shadow:0 6px 18px -8px rgba(0,0,0,.3)">
      <div style="text-align:center">
        <div style="font-size:56px">🎲</div>
        <div style="font-size:12px;color:#888">NFC LOT 抽签</div>
      </div>
    </div>
  </div>`;

/* ------------------------------ 文本 / 串口 / 其它（轻量） ------------------------------ */
PAGES.text = (st) => `
  <div class="pg" style="background:#f0f0f0">
    <div style="position:absolute;left:0;top:0;width:240px;height:160px;background:#fff;
                border:1px solid #e0e0e0;font-size:12px;padding:6px;overflow-y:auto;
                white-space:pre-wrap;line-height:1.7">${st.txtContent}</div>
    <div style="position:absolute;left:0;top:160px;width:240px;height:20px;background:#e0e0e0;
                display:flex;align-items:center;justify-content:space-between;padding:0 6px">
      <button class="big-btn" style="width:30px;height:20px;border-radius:3px">‹</button>
      <span style="font-size:12px">第 12 / 86 页</span>
      <button class="big-btn" style="width:30px;height:20px;border-radius:3px">›</button>
    </div>
  </div>`;

PAGES.keytest = (st) => {
  const pad = (label, lx, vals) => `
    <div style="position:absolute;left:${lx}px;top:22px;width:100px;height:126px;
                border:1px solid #ddd;border-radius:8px;background:#fff">
      <div style="text-align:center;font-size:12px;color:#888;padding-top:4px">${label}</div>
      <div style="position:absolute;left:5px;top:24px;width:90px;height:90px;border-radius:50%;
                  border:2px solid #e0e0e0">
        <div style="position:absolute;left:50%;top:0;bottom:0;width:1px;background:#eee"></div>
        <div style="position:absolute;top:50%;left:0;right:0;height:1px;background:#eee"></div>
        <div style="position:absolute;width:12px;height:12px;border-radius:50%;background:#2196f3;
                    left:${44 + vals[0]}px;top:${44 + vals[1]}px"></div>
      </div>
      <div style="position:absolute;left:0;bottom:4px;width:100px;text-align:center;font-size:12px">
        X ${vals[0]} / Y ${vals[1]}</div>
    </div>`;
  return `<div class="pg" style="background:#fff">
    ${pad('左摇杆 (PB0/PB1)', 15, st.stickL)}
    ${pad('右摇杆 (PC0/PC1)', 125, st.stickR)}
    <div style="position:absolute;left:15px;top:154px;display:flex;gap:6px;align-items:center">
      <span style="font-size:12px;color:#888">中键</span>
      <span style="display:inline-block;width:50px;height:20px;border-radius:4px;
                   background:${st.keyL ? '#2196f3' : '#e0e0e0'}"></span>
      <span style="display:inline-block;width:50px;height:20px;border-radius:4px;
                   background:${st.keyR ? '#2196f3' : '#e0e0e0'}"></span>
    </div></div>`;
};

/* ------------------------------ 日志设置 ------------------------------ */
/* 与 log_ctrl_unit.c 一致：三组控件
     1) 「开启调试功能:」+ 60x25 开关
     2) 「实时日志输出方式」+ TSDB / USB / LVGL 三个 60x25 按钮（选中 #808080）
     3) 「储存日志输出方式」+ 「Open USB」「Dump LVGL」两个 95x25 按钮 */
PAGES.logctrl = (st) => `
  <div class="pg">
    <div class="lg-row" style="top:12px">
      <span class="k">开启调试功能:</span>
      <span class="sw ${st.logOn ? 'on' : ''}" data-act="log-sw"
        style="position:relative;display:inline-block;width:60px;height:25px;border-radius:4px;
               background:${st.logOn ? '#2196f3' : '#e0e0e0'}">
        <i style="position:absolute;top:2px;left:${st.logOn ? '37px' : '2px'};width:21px;height:21px;
                  border-radius:4px;background:#fff;transition:left .16s"></i></span>
    </div>

    <div class="lg-row" style="top:52px"><span class="k">实时日志输出方式</span></div>
    <div class="lg-row" style="top:74px;gap:6px">
      ${['TSDB', 'USB', 'LVGL'].map((m, i) =>
        `<button class="lg-btn ${st.logMode === i ? 'on' : ''}" data-logmode="${i}">${m}</button>`).join('')}
    </div>

    <div class="lg-row" style="top:110px"><span class="k">储存日志输出方式</span></div>
    <div class="lg-row" style="top:132px;gap:6px">
      <button class="lg-btn wide" data-act="log-open-usb">Open USB</button>
      <button class="lg-btn wide" data-act="log-dump-lvgl">Dump LVGL</button>
    </div>
  </div>`;

PAGES.debug = () => `
  <div class="pg" style="background:#fff;overflow:hidden">
    <div style="position:absolute;left:5px;top:5px;right:5px;font-size:12px;line-height:16px;
                font-family:ui-monospace,Consolas,monospace">
      ${Array.from({ length: 10 }, (_, i) =>
        `<div>[${String(1024 + i * 37).padStart(5, '0')}] lvgl: inv_area ${i % 3 === 0 ? 'ok' : 'skip'} ${i * 7 % 23}ms</div>`
      ).join('')}
    </div>
    <button class="mu-btn" style="left:212px;top:152px">${ic('exit_icon', 20, '#000')}</button>
  </div>`;

PAGES.fontupdate = () => `
  <div class="pg full" style="background:#fff">
    <div style="position:absolute;left:20px;top:40px;width:200px;height:160px;background:#fff;
                border-radius:15px;box-shadow:0 6px 20px rgba(204,204,204,.9)">
      <div style="position:absolute;left:0;top:22px;width:200px;text-align:center;
                  font-size:24px;color:#222">字体升级中</div>
      <div style="position:absolute;left:5px;top:64px;width:190px;height:14px;background:#e0e0e0">
        <div style="width:64%;height:14px;background:#00c853"></div>
      </div>
      <div style="position:absolute;left:0;top:86px;width:200px;text-align:center;
                  font-size:16px;color:#00c853">64%</div>
      <div style="position:absolute;left:0;top:112px;width:200px;text-align:center;
                  font-size:12px;color:#555">正在写入 W25Q128…</div>
      <div style="position:absolute;left:0;top:132px;width:200px;text-align:center;
                  font-size:12px;color:#888">Font24.bin</div>
    </div>
  </div>`;

PAGES.serial = () => `
  <div class="pg no-nav">
    <div style="position:absolute;left:0;top:0;width:240px;height:32px;background:#fff;
                display:flex;align-items:center;gap:6px;padding:0 6px">
      <button class="big-btn" style="width:46px;height:28px">打开</button>
      <button class="big-btn" style="width:70px;height:28px">115200</button>
      <span style="font-size:12px;color:#a00000">已关闭</span>
    </div>
    <div style="position:absolute;left:0;top:32px;width:240px;height:138px;background:#fff;
                border:1px solid #eee;font-size:12px;padding:4px;overflow:auto;
                font-family:ui-monospace,Consolas,monospace;line-height:1.6">
      <div>[RX] 0x1F 0x8B 0x08 0x00 0x00 0x00 0x00 0x00</div>
      <div>[DBG] spi3 dma xfer 4096B ok</div>
      <div>[RX] AT+VERSION</div>
      <div>[TX] Brahman / FW 1.0.0</div>
    </div>
    <div style="position:absolute;left:0;top:170px;width:240px;height:40px;background:#fff;
                display:flex;align-items:center;gap:6px;padding:0 6px">
      <button class="big-btn" style="width:50px;height:36px;background:var(--c-blue);color:#fff">发送</button>
      <input style="width:178px;height:36px;font-size:12px;padding:0 6px;border:1px solid #ddd;
             border-radius:4px;font-family:inherit" placeholder="输入指令…">
    </div>
  </div>`;

PAGES.timeset = (st) => {
  const cols = [['年', 2026], ['月', 9], ['日', 11], ['时', st.now.getHours()], ['分', st.now.getMinutes()], ['秒', st.now.getSeconds()]];
  return `<div class="pg">
    <div style="position:absolute;left:12px;top:16px;display:flex">
      ${cols.map(([u, v], i) => `
        <div class="roller" style="position:absolute;left:${i * 38}px;width:22px">
          <div class="sel"></div>
          <div class="col" style="top:0">${String(v + 1).slice(-2)}</div>
          <div class="col" style="top:20px">${String(v).slice(-2)}</div>
          <div class="col" style="top:40px">${String(Math.max(0, v - 1)).slice(-2)}</div>
        </div>
        <span style="position:absolute;left:${i * 38 + 24}px;top:23px;width:13px;
                     font-size:11px;color:#666">${u}</span>`).join('')}
    </div>
    <button class="big-btn" style="position:absolute;left:70px;top:110px;width:100px;height:22px">保存</button>
  </div>`;
};

/* ======================================================================
   控制中心（overlay 层）
   ====================================================================== */
function renderControlCenter(st) {
  /* 三个滑块的当前值：亮度 / 耳放音量 / 喇叭音量（注意 hdpOut 是耳机-喇叭开关，不是音量） */
  const ccVal = (k) => k === 'bright' ? st.bright : k === 'hdp' ? st.hdpVol : st.spkVol;
  let html = `<div class="cc-cover" data-act="cc-close"></div>`;

  /* 场景行向上堆叠：音乐行 y=60，USB 行 y=20；主面板固定 (20,100)。
     对应 control_center.c 里 current_offset_y 从 100 起每次 -40。 */
  if (st.musicActive)
    html += `<div class="cc-row" style="top:60px">${st.playing ? '播放' : '暂停'}：${st.song.name}
      <button class="cc-close" data-act="cc-music-close">${ic('control_exit', 16, '#000')}</button></div>`;
  if (st.usbActive)
    html += `<div class="cc-row" style="top:20px">USB：MSC 运行中
      <button class="cc-close" data-act="cc-usb-close">${ic('control_exit', 16, '#000')}</button></div>`;

  html += `
  <div class="cc-panel cc-anim">
    <button class="cc-close" data-act="cc-close">${ic('control_exit', 16, '#000')}</button>

    <div class="vswitch cc-switch" data-act="cc-hdp">
      <div class="knob ${st.hdpOut ? 'bot' : 'top'}"></div>
    </div>

    <button class="pwr cc-pwr-1 ${st.pw.bright ? 'on' : ''}" data-pwr="bright">
      ${ic('control_light', 16)}</button>
    <button class="pwr cc-pwr-2 ${st.pw.hdp ? 'on' : ''}" data-pwr="hdp">
      ${ic('control_headphone', 16)}</button>
    <button class="pwr cc-pwr-3 ${st.pw.spk ? 'on' : ''}" data-pwr="spk">
      ${ic('control_speaker', 16)}</button>

    ${[['bright', 'top:10px'], ['hdp', 'top:40px'], ['spk', 'top:70px']].map(([k, pos]) => `
      <div class="cc-slider" data-slider="${k}" style="${pos}">
        <div class="slider ${st.pw[k] ? '' : 'dis'}">
          <div class="ind" style="width:${(ccVal(k) / 255 * 100).toFixed(1)}%"></div>
          <div class="knob" style="left:${(ccVal(k) / 255 * 100).toFixed(1)}%"></div>
        </div>
        <div class="val">${String(ccVal(k)).padStart(3, '0')}</div>
      </div>`).join('')}
  </div>`;
  return html;
}
