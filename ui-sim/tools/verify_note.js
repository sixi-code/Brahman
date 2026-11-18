/* ==========================================================================
   verify_note.js —— 记事本页的离线校验（不需要浏览器）
   把 pages.js 的渲染结果里出现的**每个坐标/尺寸/文案**逐条对回 note_unit.c。
   用法： node tools/verify_note.js
   ========================================================================== */
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const src = fs.readFileSync(path.join(__dirname, '..', 'assets', 'js', 'pages.js'), 'utf8');
vm.runInThisContext(src, { filename: 'pages.js' });
/* 列表 / 正文框 / 弹窗的几何有一部分写在 CSS 里（与本模拟器既有做法一致），
   所以数值断言分两处查：HTML 里的行内样式 + style.css */
const css = fs.readFileSync(path.join(__dirname, '..', 'assets', 'css', 'style.css'), 'utf8');

let pass = 0;
const fails = [];
function ok(cond, msg) { cond ? pass++ : fails.push(msg); }

/** 造一个只含 note 的渲染上下文 */
const st = (note) => ({ note });

/* ---------------- 列表视图：第 1 页 ---------------- */
const list0 = PAGES.note(st({ view: 'list', page: 0, name: '', text: '', dlg: null }));
ok(list0.includes('class="pg pg-note"'), '列表视图缺 .pg.pg-note 根节点');
ok(list0.includes('left:42px;width:58px">1/3<'), '页码不是 1/3（12 篇 / 每页 5 ⇒ 3 页）');
ok(!list0.includes('data-act="nt-prev"'), '首页不该出现上一页箭头（固件到边界直接隐藏）');
ok(list0.includes('data-act="nt-next"'), '首页缺下一页箭头');
ok(list0.includes('style="left:102px;top:2px;width:30px;height:24px" data-act="nt-next"'),
   '下一页箭头应为 left:102px top:2px 30x24（note_unit.c 的 btn_next）');
ok(list0.includes('style="left:174px;top:2px;width:56px;height:24px" data-act="nt-new"'),
   '「新建」应为 left:174px top:2px 56x24（TOP_RIGHT -10,2）');
ok(css.includes('left:2px;top:32px;width:236px;height:146px'),
   '列表容器应为 left:2px top:32px 236x146（list_box TOP_MID 0,32）');

/* 模拟目录共 12 篇 ⇒ 3 页：第 1 页 5 条（都可编辑）、第 3 页 2 条（都是不可编辑的） */
const items0 = list0.match(/class="nt-item/g) || [];
ok(items0.length === 5, `首页应列 5 条（NOTE_PAGE_SIZE=5），实为 ${items0.length}`);
ok(!list0.includes('class="nt-item bad"'), '首页不该有不可编辑条目（dump / 长名在第 3 页）');

/* ---------------- 列表视图：第 3 页（末页） ---------------- */
const list2 = PAGES.note(st({ view: 'list', page: 2, name: '', text: '', dlg: null }));
ok(list2.includes('left:42px;width:58px">3/3<'), '末页页码应为 3/3');
ok(list2.includes('data-act="nt-prev"'), '末页缺上一页箭头');
ok(!list2.includes('data-act="nt-next"'), '末页不该出现下一页箭头');
ok(((list2.match(/class="nt-item/g) || []).length) === 2, '末页应为 2 条（12 篇的最后两篇）');
ok((list2.match(/class="nt-item bad"/g) || []).length === 2, '末页两条都应是不可编辑的（过大 / 名过长）');
ok(list2.includes('（过大）dump'), '过大条目应带「（过大）」**前缀**');
ok(!list2.includes('dump（过大）'), '过大标注不该放后缀（名字本身超宽时会被截断）');
ok(list2.includes('（名过长）这是一个特别特别长的笔记名字用来触发名过长分支'),
   '名字过长条目应带「（名过长）」前缀');
ok(css.includes('.nt-item.bad{color:var(--c-grey)}'), '不可编辑条目应置灰');

/* 页码越界应被夹到末页（固件同口径：page 只在合法范围内） */
const clamped = PAGES.note(st({ view: 'list', page: 99, name: '', text: '', dlg: null }));
ok(clamped.includes('>3/3<'), '页码越界应夹到末页 3/3');

/* ---------------- 编辑视图 ---------------- */
const edit = PAGES.note(st({ view: 'edit', name: '备忘录', text: '正文内容', dlg: null }));
ok(edit.includes('style="left:10px;top:2px;width:56px;height:24px" data-act="nt-list"'),
   '「列表」按钮应为 left:10px top:2px 56x24');
ok(edit.includes('style="left:72px;width:158px">备忘录'), '笔记名应为 left:72px 宽 158px（label_name）');
ok(css.includes('left:10px;top:32px;width:220px;height:146px'),
   '正文框应为 left:10px top:32px 220x146（ta_note TOP_MID 0,32）');
ok(edit.includes('contenteditable="true"'), '正文框应可编辑');
ok(!edit.includes('nt-save'), '编辑视图不该有页内保存键（保存统一走离开确认框）');
const noName = PAGES.note(st({ view: 'edit', name: '', text: '', dlg: null }));
ok(noName.includes('>未命名<'), '没起名时应显示「未命名」');

/* ---------------- 弹窗：还没保存（94px） ---------------- */
const ask = noteDialogHTML(st({ dlg: 'ask', msg: '', note: undefined }));
ok(ask.includes('style="top:30px;height:94px"'), '确认框应为 top:30px 高 94px');
ok(ask.includes('left:12px;top:12px;width:216px'), '确认框正文应为 left:12px top:12px 宽 216px');
ok(ask.includes('style="left:8px;top:58px;width:68px;height:26px" data-act="nt-ask-save"'),
   '「保存」应为 left:8px top:58px 68x26');
ok(ask.includes('style="left:86px;top:58px;width:68px;height:26px" data-act="nt-ask-drop"'),
   '「不保存」应为 left:86px');
ok(ask.includes('style="left:164px;top:58px;width:68px;height:26px" data-act="nt-ask-stay"'),
   '「取消」应为 left:164px');
ok(ask.includes('要保存吗?'), '确认框缺默认文案');

/* ---------------- 弹窗：起名（86px） ---------------- */
const name = noteDialogHTML(st({ dlg: 'name', err: '', input: '', note: undefined }));
ok(name.includes('style="top:30px;height:86px"'), '起名弹窗应为 top:30px 高 86px');
ok(name.includes('left:10px;top:4px;width:220px'), '起名弹窗标题行应为 left:10px top:4px 宽 2200px 内');
ok(name.includes('style="top:26px"') && name.includes('data-nt-input'), '名称输入框应在 top:26px');
ok(name.includes('style="left:10px;top:56px;width:60px;height:26px" data-act="nt-name-cancel"'),
   '「取消」应为 left:10px top:56px 60x26');
ok(name.includes('style="left:170px;top:56px;width:60px;height:26px" data-act="nt-name-ok"'),
   '「确定」应为 left:170px（TOP_RIGHT -10）');
ok(name.includes('给笔记起个名字'), '起名弹窗缺默认标题');
const nameErr = noteDialogHTML(st({ dlg: 'name', err: '名称已存在', input: '备忘录', note: undefined }));
ok(nameErr.includes('class="nt-dlg-msg err"') && nameErr.includes('名称已存在'),
   '起名弹窗出错时标题行应变红并显示原因');
ok(nameErr.includes('value="备忘录"'), '起名弹窗出错后应保留已输入的名称');

/* ---------------- 弹窗：提示（过大 / 名过长） ---------------- */
const tip = noteDialogHTML(st({ dlg: 'tip', msg: '文件过大\n只能编辑 4 KB 以内的笔记', note: undefined }));
ok(tip.includes('style="top:30px;height:86px"'), '提示弹窗应为 top:30px 高 86px');
ok(tip.includes('style="left:84px;top:52px;width:72px;height:26px" data-act="nt-tip-ok"'),
   '「知道了」应为 left:84px top:52px 72x26（TOP_MID 0,52）');
ok(noteDialogHTML(st({ dlg: null, note: undefined })) === '', '无弹窗时应渲染空串');

/* ---------------- 汇总 ---------------- */
console.log(`记事本页离线校验：通过 ${pass} 项，失败 ${fails.length} 项`);
if (fails.length) {
  fails.forEach((f) => console.log('  ✗ ' + f));
  process.exit(1);
}
console.log('  ✓ 全部坐标 / 尺寸 / 文案均与 note_unit.c 对得上');
