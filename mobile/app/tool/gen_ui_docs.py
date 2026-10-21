#!/usr/bin/env python3
"""Regenerate docs/handoff/strings.md (all zh/en string keys and where they are used)
and docs/handoff/screen-map.md (screen/widget file -> string keys and widget Keys).
Run from anywhere: python3 app/tool/gen_ui_docs.py   (standard library only)."""
import glob, json, os, re

APP = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUT = os.path.abspath(os.path.join(APP, '..', 'docs', 'handoff'))
I18N = os.path.join(APP, 'assets', 'i18n')


def load(name):
    with open(os.path.join(I18N, name), encoding='utf-8') as f:
        return json.load(f)


zh, en, azh, aen = load('zh.json'), load('en.json'), load('app_zh.json'), load('app_en.json')
files = sorted(glob.glob(os.path.join(APP, 'lib', '**', '*.dart'), recursive=True))
srcs = {os.path.relpath(f, os.path.join(APP, 'lib')): open(f, encoding='utf-8').read() for f in files}

# Interpolated keys such as 'preset.${p.name}' or 'help.$id.title' become regexes.
dyn = {}
for f, s in srcs.items():
    for lit in re.findall(r"'([a-z0-9_]+\.[A-Za-z0-9_.${} +]*\$[A-Za-z0-9_.${} +]*)'", s):
        rx = re.sub(r'\\\$\\\{[^}]*\\\}|\\\$[A-Za-z_]\w*', '[A-Za-z0-9_]+', re.escape(lit))
        dyn.setdefault(rx, set()).add(f)


def users(key):
    used = {f for f, s in srcs.items() if "'%s'" % key in s}
    dynamic = set()
    for rx, fs in dyn.items():
        if re.fullmatch(rx, key):
            dynamic |= fs
    text = ', '.join(sorted(used))
    extra = sorted(dynamic - used)
    if extra:
        text += (', ' if text else '') + ', '.join(extra) + '（动态键）'
    return text or '—'


def esc(s):
    return s.replace('|', '\\|').replace('\n', '<br>')


out = ['# 界面文案全表（自动生成，请勿手改）', '',
       '> 由 `python3 app/tool/gen_ui_docs.py` 从 `app/assets/i18n/{zh,en,app_zh,app_en}.json` 生成，共 %d 个键（设计稿表 %d + App 专用 %d）。'
       % (len(zh) + len(azh), len(zh), len(azh)),
       '> “使用位置”是 `app/lib` 里以字符串字面量出现该键的文件；`preset.${p.name}`、`help.$id.title`、`converting.stage.${st.name}` 这类拼接键标为“动态键”。'
       '“—”表示没有直接引用：多数是设计稿为 iOS 原生分享面板等预留的文案，或通过 `${i + 1}` 之类表达式拼出的键（如 `onboarding.2.step1..3`）。', '',
       '改文案：设计稿表改 `design/ui/strings_*.json` 后运行 `app/tool/sync_strings.sh`；App 专用表改 `app/tool/app_strings.py` 后运行 `python3 tool/app_strings.py`；'
       '最后重新运行本脚本。详见 [recipes.md R2](recipes.md#r2-新增或修改一条界面文案)。', '']
for title, a, b in (('设计稿文案表（zh.json / en.json，来源 design/ui）', zh, en),
                    ('App 专用文案表（app_zh.json / app_en.json，来源 tool/app_strings.py）', azh, aen)):
    out += ['## ' + title, '', '| 键 | 中文 | English | 使用位置 |', '|---|---|---|---|']
    for k in sorted(a):
        out.append('| `%s` | %s | %s | %s |' % (k, esc(a[k]), esc(b.get(k, '**缺失**')), users(k)))
    out.append('')
with open(os.path.join(OUT, 'strings.md'), 'w', encoding='utf-8') as f:
    f.write('\n'.join(out) + '\n')

m = ['# 页面 / 组件 → 文案键与 widget Key（自动生成，请勿手改）', '',
     '> 由 `python3 app/tool/gen_ui_docs.py` 扫描 `app/lib/screens` 和 `app/lib/widgets` 生成。文案键指 `t(\'...\')` 的字面参数（带 `$` 的是拼接键）；'
     'widget Key 指 `Key(\'...\')`，测试用 `find.byKey` 定位。文案内容见 [strings.md](strings.md)。', '']
for f in sorted(srcs):
    if not (f.startswith('screens/') or f.startswith('widgets/')):
        continue
    s = srcs[f]
    keys = sorted(set(re.findall(r"t\(\s*'([A-Za-z0-9_.${} +]+)'", s)))
    wk = sorted(set(re.findall(r"Key\('([^']+)'\)", s)))
    m += ['## `app/lib/%s`' % f, '',
          '- 文案键（%d）：%s' % (len(keys), '、'.join('`%s`' % k for k in keys) or '无'),
          '- widget Key（%d）：%s' % (len(wk), '、'.join('`%s`' % k for k in wk) or '无'), '']
with open(os.path.join(OUT, 'screen-map.md'), 'w', encoding='utf-8') as f:
    f.write('\n'.join(m) + '\n')
print('strings.md: %d keys; screen-map.md: %d files' % (len(zh) + len(azh), sum(1 for x in srcs if x.startswith(('screens/', 'widgets/')))))
