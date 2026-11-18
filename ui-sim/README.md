# Brahman UI 模拟器

用前端（HTML/CSS/JS）把 Brahman 固件的 LVGL 界面 1:1 还原出来，用于在真机屏幕点亮之前
先看 UI 效果、核对布局与配色。

## 运行

本目录是纯静态站点，**必须用 HTTP 打开**（`file://` 下部分资源会被浏览器安全策略拦掉）：

```bash
cd ui-sim
python -m http.server 8931
# 浏览器访问 http://127.0.0.1:8931/index.html
```

Windows 也可以直接双击 `Start.cmd`。

## 它做了什么

- **坐标体系完全沿用固件**：屏幕就是 240×240，容器用 `transform: scale(var(--s))` 放大呈现，
  所以 CSS 里出现的每个数值（`left:206px`、`height:30px`、`font-size:12px`…）都是从
  `ware/GUI/GUI_Unit/*.c` 的 `lv_obj_set_pos/size` 直接抄过来的，可以逐行对照。
- **图标是固件里的真实位图**：`assets/icons/*.png` 由 `tools/extract_icons.py` 从
  `ware/GUI/GUI_Icons/*.c` 解码生成（`LV_IMG_CF_ALPHA_1BIT` / `ALPHA_4BIT` /
  `TRUE_COLOR` / `TRUE_COLOR_ALPHA`，color depth 16）。前端用 CSS `mask-image` +
  `background-color: currentColor` 复现 `lv_obj_set_style_img_recolor()` 的重着色行为。
- **页面装配按固件的真实组合**：哪一页有导航栏、哪一页有状态栏，取自各
  `GUI_Page/*.c` 里是否调用 `Create_Navigation_Bar()` / `Create_Status_Bar()`
  （见 `pages.js` 的 `PAGES_WITH_NAV` / `PAGES_NO_STATUS`），导航栏标题也用的是
  `Create_Navigation_Bar("…")` 的原始字符串（如 `关于`、`日志设置`、`画板`）。

## 重新生成图标

```bash
python -m venv envs/default            # 任意带 Pillow 的环境
pip install pillow
python tools/extract_icons.py
```

脚本会重写 `assets/icons/` 并输出 `manifest.js`（图标名 / 尺寸 / 格式清单）。

## 复刻范围

当前实现了 **25 个可渲染页面**，覆盖 `GUI_Page` 里所有有独立布局的界面：

| 页面 | 说明 |
|------|------|
| `start` | 壁纸 `pic_back` + 角色 `pic_top` + `lv_font_56` 粉色时钟；角色随光标做 ±10px 视差（见下） |
| `desktop` | `pic_back` 壁纸 + tileview 双屏 + 4×3 图标网格（40×40 / 圆角10 / 底 `#C6E6E8` / 图标 `#FFB3B3`） |
| `music` | 专辑封面 120×120 + 20 条频谱 + 歌词视图 + 进度条 + 侧边 4 个功能键 |
| `settings` | 6 项列表（5 项跳页 + UAC 兼容性开关），文案取自 `settings_unit.c` |
| `file` | 盘符占用条 + 目录/文件列表 + 复制/粘贴/删除 |
| `clock` | 时钟 / 计时 / 秒表 / 闹钟 四标签 |
| `note` | 记事本：列表（每页 5 条 + 翻页，过大/名过长置灰只提示）+ 编辑器 + 三个弹窗（未保存确认 / 起名查重 / 提示） |
| `canvas_list` | 画作列表（`canvas_list_unit.c`）：每页 5 条、顶栏 `<` `页码` `>` + 「新建」，名过长的条目点了只弹「文件名过长」；空态与提示文案与固件一致 |
| `canvas` | 画板：40×40 画布 + 浮动工具面板（9 个图标）+ 色环 + 缩略图 |
| `es9018` | DAC 设置：18 行配置与 `es9018_unit.c` 一一对应（数字滤波器 / IIR 带宽 / 旁路 IIR / DPLL 带宽 / 锁定信号时钟数 / 音量渐变 / 平滑自动静音 / 静音时间与等级 / 两路静音 / 通道映射 / 旁路 THD / C2-C3 谐波 / 失锁行为），行高与 `y_ofs += 30` 一致；循环按钮与开关可点击 |
| `words` | 背单词（`words_unit.c`）三视图：卡片（选书 / 顺序-随机切换 / 目录 / 点卡片显隐释义 / 不熟-认识）、单词目录（每页 6 条）、选择词书 |
| `usb` | USB 控制：左侧 100×90 滚轮（8 项与 `func_list[]` 顺序一致）+ 上下移，右侧 110×60 状态卡 + 启动/终止 |
| `logctrl` | 日志设置：调试总开关 / 实时输出 TSDB·USB·LVGL / 储存输出 Open USB·Dump LVGL |
| `keytest` | 双摇杆 100×130 面板 + 圆环 + 十字线 + 坐标，机身摇杆拖动实时联动 |
| `mem` | TLSF 双池曲线 + 6 格统计（CCM 用/空、SRAM 用、BSC 堆、CPU、任务数），点标题切换曲线源 |
| 其它 | `calendar` / `calculator` / `about` / `album` / `lots` / `text` / `cmd` / `serial` / `debug` / `fontupdate` / `timeset` 按各自 `GUI_Unit` 的布局与配色还原 |
| 控制中心 | 200×100 面板（关闭键 / 耳机-喇叭竖向开关 / 3 个电源键 + 3 条 0–255 滑块）+ 音乐、USB 场景行向上堆叠 |

## 开机页的动效（视差）

固件里**没有任何逐帧动画**（应用层搜不到一处 `lv_anim_*`，背光也只是直接设占空比），
所谓「开机动画」就是 `start_unit.c` 这一屏，它唯一的动效是角色立绘 `pic_top` 跟着光标漂移：

```c
// start_unit.c  Update_Start_Unit()  —— 每帧执行
int16_t offset_x = ((int16_t)cursor_x - 120) / 12;   // C 整数除法，向零截断
int16_t offset_y = ((int16_t)cursor_y - 120) / 12;
if (offset_x > 10) offset_x = 10;   /* ... 限幅 ±10 ... */
lv_obj_set_pos(top_img, 40 + offset_x, 94 + offset_y);
```

光标由摇杆驱动，**本工程把「光标移动」放在左摇杆上**（`ware/lib/LVGL/bsp/lv_port_indev.c`；
右摇杆在本工程是确定键）：

```c
cursor_x += g_key_L_X * 0.06f;    // g_key_L_X ∈ [-128,127]，adc.c 限幅 + ±19 死区
cursor_y += -g_key_L_Y * 0.06f;
/* 限幅 0..239 */
```

前端按同一套算法复现（`pages.js` 的 `startOffset()`、`app.js` 的 20ms 定时器，
周期同 `LV_INDEV_DEF_READ_PERIOD = 20`）。光标初值 `(120,120)`，
所以立绘的行程是 `(30,84) ~ (50,104)`。

（立绘画布 160×151，左上角基准 `(40,94)`：水平居中；实心墨迹在画布内为 y 9~139，
故静止时人物在屏上 y 103~233，上飘到极限时头顶 93，仍在 `lv_font_56` 时钟的 y=86 底边之下留 7px 余量。）

验证：`node tools/verify_parallax.js`（离线 DOM 桩，18 项断言，不需要浏览器）。

## 与固件的差异（有意为之）

1. **数据是模拟的**：时间用系统时钟；电量、电压、CPU、RAM、频谱、歌词、文件列表都是
   造的数据，固件里这些分别来自 RTC、ADC、TLSF/CPU 统计、`spectrum_dsp`、`.lrc` 与 FatFs。
2. **桌面页码指示点**：固件 `desktop_unit.c` 没有页码点，这里加了两个小圆点作为模拟器的
   操作提示（否则不知道有第二屏）。
3. **中文字体**：固件用的是 TF 卡里的 `Font8/12/16/24 + UNIGBK` 点阵字库，前端用系统
   无衬线字体按同名磅值（12/16/24/40/56）渲染。
4. **专辑封面**：固件读 `0:/COVER/<曲名>.bmp`，这里没有封面文件，用渐变占位并标注。
5. **状态栏音量图标**：固件 `Update_Volume_Icon()` 固定用 `volume_icon_0`，这里按
   控制中心滑杆的值在 `volume_icon_0..5` 之间切换，便于对照全部 6 张位图。
6. **农历/节气**：只按公式算了干支纪年（与固件 `lunar.c` 的结果一致），农历月日、
   节气、节日未实现。
7. **DAC 滑条不可拖**：固件里 6 条滑条可拖动取值，这里只渲染当前值（滑块位置与数值），
   循环按钮与开关可点；改滑条值请直接在 `app.js` 的 `ST.dac` 里写下标对应的值。

## 操作

| 操作 | 效果 |
|------|------|
| 起始页点击任意处 | 进入桌面 |
| 起始页拖动**左摇杆** | 移动光标，角色立绘随之漂移（松手摇杆自回中，光标停住） |
| 桌面图标 | 打开对应应用（画板先进画作列表） |
| 导航栏 ‹ / ▣ / ✕ | 返回 / 回桌面 / 回桌面 |
| 状态栏左侧 | 电量 ↔ 电压 |
| 状态栏中间 | 打开控制中心（起始页则进桌面） |
| 状态栏右侧 | 24h → 12h → CPU/RAM 循环 |
| 音乐页封面/频谱 | 切换 频谱 ↔ 歌词 |
| 桌面左右拖动 / ← → | 翻页 |
| 机身双摇杆拖动 | 联动「外设状态」页的坐标显示 |
| **记事本** | 列表页：`<`/`>` 翻页、点条目打开、点「新建」起空笔记；置灰条目点击只弹提示<br>编辑页：点「列表」回列表（有改动先弹 保存/不保存/取消）；导航栏返回同样会拦；保存没名字时先弹起名框（重名报「名称已存在」） |
| **画作列表** | `<`/`>` 翻页、「新建」进空白画布、点名过长的条目只弹「文件名过长」 |
| **DAC 设置** | 点循环按钮切下一档、点开关切换；滑条只读 |
| **背单词** | 「书」换词书、「顺序/随机」切模式、「目」进目录、点卡片显隐释义、不熟/认识前后翻 |
| **USB 控制** | ▲▼ 换功能（8 项）、启动/终止切换状态卡；启动后控制中心出现 USB 行 |
| **日志设置** | 总开关、TSDB/USB/LVGL 实时通道、Open USB / Dump LVGL |
| `Esc` / `Backspace` | 返回 |
| `Enter` | 控制中心 |

## 截图与回归检查

`tools/shoot.js` 通过 Chrome DevTools Protocol 驱动无头浏览器逐页截图，不依赖 puppeteer：

```bash
python -m http.server 8931 --bind 127.0.0.1          # 1) 静态服务
"C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe" \
  --headless=new --no-sandbox --disable-gpu \
  --remote-debugging-port=9222 --remote-allow-origins='*' \
  --user-data-dir=.edgeprof --window-size=1500,1200   # 2) headless Edge
node tools/shoot.js 9222 tools/shots                  # 3) 截图
```

输出 `.screen`（240×240 屏，2.6 倍缩放后 624×624）、`.device` 机身图和整页图。
只做回归检查、不需要图片时跑 `node tools/render_check.js` 即可（纯离线，秒级，逐页检查
`undefined` / `NaN` / `[object Object]` 之类的渲染异常）。

## 目录

```
ui-sim/
├── index.html                 机身 + 侧栏
├── Start.cmd                  Windows 一键起静态服务
├── assets/
│   ├── css/style.css          全部布局（数值取自固件）
│   ├── js/pages.js            25 个页面的渲染函数
│   ├── js/app.js              状态机 / 导航 / 交互
│   └── icons/                 从 GUI_Icons/*.c 解码出的 PNG + manifest.js
└── tools/
    ├── extract_icons.py       位图解码脚本（从 GUI_Icons/*.c 还原 PNG）
    ├── render_check.js        全页面离线渲染检查
    ├── verify_parallax.js     开机页视差离线断言（18 项）
    ├── verify_note.js         记事本页离线断言（42 项）
    └── shoot.js               全页面截图（CDP 驱动 headless Edge）
```
