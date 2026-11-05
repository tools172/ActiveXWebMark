# Markdown 主题实现技术方案（跨语言参考）

本文以 ActiveXWebMark 的 C++ 实现为参考，但**数据模型、派生公式和导入映射与语言无关**。
目标读者是其他 Markdown 阅读器/编辑器项目的开发者：即使使用 Electron、Rust、
Python、C# 或 Qt，也可以按本文实现一套兼容的主题系统。

本文与面向用户的 [themes.md](themes.md) 分工：

- `themes.md`：主题怎么用、原生 JSON 怎么写、支持哪些导入格式。
- 本文：**为什么这样设计、数据怎么流动、每个颜色怎么算、各层怎么实现、移植时注意什么**。

---

## 1. 设计目标与取舍

### 1.1 目标

1. **Markdown-first**：主题的视觉身份由正文、标题、链接、代码、引用、表格等
   Markdown 元素决定。
2. **一个数据源，三种输出**：同一份颜色同时驱动
   - Markdown 预览（CSS）；
   - 代码编辑器（Scintilla 或其他编辑器）；
   - 应用外壳（窗口、标题栏、标签、侧栏、滚动条、菜单）。
3. **可导入**：外部主题统一转成应用自己的语义模型，而不是在核心代码里解析
   各家的 CSS/JSON。
4. **可编辑**：提供带真实预览的设置界面，内置主题只读、复制后可改。
5. **可测试**：颜色解析、派生、格式转换、存储、导入都能脱离 GUI 单测。

### 1.2 为什么不用 CSS 作为权威模型

CSS 是 Markdown 渲染层的事实标准，但它不适合作为唯一数据源：

- CSS 是“规则”，不是“数据”；用颜色选择器改一个颜色，需要可靠地定位并重写规则。
- 原生窗口、编辑器、滚动条通常不消费 CSS。
- 各应用的 CSS 变量名和选择器不同；直接复用外部 CSS 会绑定到对方的 DOM。
- 某些引擎（如 IE11）不支持 CSS 自定义属性，最终仍需把变量解析成具体颜色。
- 任意 CSS 有注入和网络请求风险。

因此：**CSS 是表现层和导入源，不是权威模型**。

### 1.3 为什么不用 VS Code Color Theme JSON 作为权威模型

- 它有几百个 workbench color key，偏代码编辑器；Markdown 排版（字体、字号、
  行高、标题风格）完全不在其中。
- Markdown 的 blockquote/table/heading 语义没有直接对应项。
- 如果把它的 key 当作用户界面，用户很难理解。
- 它适合作为**导入适配器**和 token 颜色的参考，不适合作为 Markdown 阅读器的
  主模型。

### 1.4 结论

采用“**Markdown 语义 JSON + 表现层 + 导入适配器**”的三层结构。核心模型小而稳定，
外部格式通过适配器进出。

---

## 2. 总体架构

```
   Typora/Obsidian/MarkText CSS ─┐
   VS Code Color Theme JSON ────┼──► 导入适配器 ──► ThemeSource（权威模型）
   TextMate .tmTheme ───────────┤                         │
   Base16/Base24 YAML ──────────┤                         │ BuildDefinition()
   VSIX 主题包 ─────────────────┘                         │
                                                          ▼
                                              ThemeDefinition（完整配色）
                                                          │
                    ┌─────────────────────────────────────┼─────────────────────┐
                    ▼                                     ▼                     ▼
           预览 CSS（Markdown）                  编辑器配色（Scintilla）   原生窗口 Palette
```

### 2.1 三层职责

| 层 | 职责 | 是否允许用户手改 |
|---|---|---|
| 权威模型 `ThemeSource` | 存 Markdown 语义色、token、排版、元数据 | 可以，JSON 文件 |
| 表现层 | 由 `BuildDefinition()` 生成 CSS / 编辑器色 / 窗口色 | 不手改；额外 CSS 是例外 |
| 导入适配器 | 外部格式 → `ThemeSource`，返回映射警告 | 不适用 |

### 2.2 模块边界建议

- 权威模型、派生、导入、存储：**纯逻辑模块**，不依赖 GUI，方便单测。
- 设置界面：只读写 `ThemeSource`，通过“应用主题”回调通知主程序。
- 主程序：负责加载用户主题、按 id 解析、应用三路输出、持久化 `ui.theme`。

---

## 3. 权威数据模型

### 3.1 ThemeSource

`ThemeSource` 是**规范化后的完整数据**：每个颜色都有值，派生逻辑没有歧义。
导入器/JSON 读取器先产出 `ThemeSourceDraft`（可选字段），再由
`FinalizeSource()` 补全。

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `schemaVersion` | int | 是 | 当前为 1 |
| `id` | string | 否 | 稳定 id；缺失时由名称+哈希生成 |
| `name` | string | 是 | 显示名，允许 UTF-8 |
| `author` | string | 否 | 作者；用户主题为空时显示“本地导入” |
| `license` | string | 否 | 导入源的许可证，展示用 |
| `source` | string | 否 | 来源描述，如 `VS Code Marketplace: publisher.ext@1.2.3` |
| `dark` | bool | 否 | 缺省按背景亮度推断 |
| `family` | enum | 否 | 缺省按强调色色相推断 |
| `background` | color | 是 | Markdown 页面背景 |
| `text` | color | 是 | 正文 |
| `muted` | color | 否 | 次级文字 |
| `accent` | color | 否 | 强调色；缺省时可用 link |
| `link` | color | 否 | 链接；缺省取 accent |
| `border` | color | 否 | 通用边框 |
| `hr` | color | 否 | 分隔线；缺省取 border |
| `strong` / `emphasis` | color | 否 | 粗体/斜体；缺省取正文 |
| `listMarker` | color | 否 | 列表标记；缺省取 muted |
| `headings[6]` | color[] | 否 | 6 级标题色 |
| `blockquoteText` / `blockquoteBorder` | color | 否 | 引用；缺省 muted / border |
| `codeBackground` / `codeText` / `codeBorder` | color | 否 | 代码块 |
| `tableBorder` / `tableHeadBackground` | color | 否 | 表格 |
| `selection` | color | 否 | 选中背景，允许 alpha |
| `findMatch` / `findCurrent` / `findText` | color | 否 | 查找高亮 |
| `sidebarBackground` / `sidebarHover` | color | 否 | 侧栏 |
| `danger` | color | 否 | 错误/危险 |
| `tokens` | object | 否 | 10 组 Prism token 色 |
| `typography` | object | 否 | 字体、字号、行高、宽度、内边距 |
| `extraCss` | string | 否 | 高级 Markdown CSS，经过严格过滤 |
| `ui` | object | 否 | 可选窗口外壳覆盖 |
| `overrides` | map | 否 | 高级稀疏覆盖（内置主题导出/精确往返） |

### 3.2 颜色表示

- 8-bit sRGB + 8-bit alpha：`{r,g,b,a}`。
- 解析支持：`#rgb`、`#rgba`、`#rrggbb`、`#rrggbbaa`、`rgb()`、`rgba()`、
  少量命名色（black/white/red/green/blue/yellow/orange/purple/gray/grey/silver/
  navy/teal/transparent）。
- 输出 CSS：alpha=255 用 `#rrggbb`，否则 `rgba(r,g,b,a)`。
- **原生 UI/编辑器没有 alpha 时**，先用 `Over(fg,bg)` 压平再交给它们。

### 3.3 原生 JSON 示例

```json
{
  "schemaVersion": 1,
  "id": "example-ocean-dark",
  "name": "Ocean Dark",
  "author": "Example",
  "license": "MIT",
  "dark": true,
  "markdown": {
    "background": "#0f1720",
    "text": "#d7e0ea",
    "muted": "#8fa2b5",
    "accent": "#3aa0ff",
    "link": "#5ab0ff",
    "border": "#243241",
    "headings": ["#ffffff", "#e6eef7", "#d7e0ea", "#d7e0ea", "#d7e0ea", "#9fb0c2"],
    "blockquote": { "text": "#9fb0c2", "border": "#3aa0ff" },
    "code": { "background": "#131e29", "text": "#d7e0ea", "border": "#243241" },
    "table": { "border": "#243241", "headBackground": "#131e29" },
    "selection": "rgba(58,160,255,0.25)"
  },
  "tokens": {
    "comment": "#6b7d90", "keyword": "#ff7ab2", "string": "#8ce99a"
  },
  "typography": {
    "bodyFont": "'Segoe UI',sans-serif",
    "codeFont": "Consolas,monospace",
    "bodySize": 15,
    "lineHeight": 1.75,
    "maxWidth": 980,
    "padding": [32, 40, 64],
    "codeSize": 13
  }
}
```

完整 Schema 见 [awmtheme.schema.json](awmtheme.schema.json)。

### 3.4 版本与兼容

- `schemaVersion: 1`。
- 读取时忽略未知字段（前向兼容）。
- `schemaVersion > 1` 明确拒绝，并提示当前支持版本。
- 保存时写规范 JSON；未知字段不保证保留（编辑会规范化）。
- 文件大小上限 2 MB；UTF-8 BOM 应被容忍并剥离。

### 3.5 id 与文件命名

- id 必须稳定，用于 `ui.theme`、菜单命中和运行时解析。
- 用户新建主题时生成：`user-<ascii-slug>-<8hex>`。
  - slug 只保留 ASCII 字母/数字，其他字符折叠为 `-`，最多 32 字符。
  - hash 用 FNV-1a 等确定性算法。
- 文件名：`<id>.awmtheme.json`，id 中非 `[A-Za-z0-9._-]` 的字符替换为 `_`。
- id 冲突：
  - 与内置主题冲突：重新生成。
  - 导入时与已有用户主题冲突：重新生成，**不覆盖**。
  - 编辑已有用户主题：保留原 id。
- 显示名允许重复；导入时自动追加 ` (2)`、` (3)`。

---

## 4. 颜色基础运算

以下算法与语言无关，建议作为独立工具函数并单测。

### 4.1 线性插值 Blend

```
Blend(a, b, t):          # t=0 返回 a，t=1 返回 b
    t = clamp(t, 0, 1)
    return {
        r: round(a.r*(1-t) + b.r*t),
        g: round(a.g*(1-t) + b.g*t),
        b: round(a.b*(1-t) + b.b*t),
        a: 255
    }
```

### 4.2 Alpha 合成 Over

```
Over(fg, bg):            # 把 fg 按其 alpha 压到 bg 上，结果不透明
    if fg.a == 255: return fg (opaque)
    if fg.a == 0:   return bg (opaque)
    alpha = fg.a / 255
    return {
        r: round(fg.r*alpha + bg.r*(1-alpha)),
        g: round(fg.g*alpha + bg.g*(1-alpha)),
        b: round(fg.b*alpha + bg.b*(1-alpha)),
        a: 255
    }
```

### 4.3 相对亮度与对比度

```
Channel(c):               # c in 0..255
    c = c / 255
    return c <= 0.04045 ? c/12.92 : ((c+0.055)/1.055)^2.4

Luminance(c):
    return 0.2126*Channel(c.r) + 0.7152*Channel(c.g) + 0.0722*Channel(c.b)

Contrast(a, b):
    la = Luminance(a); lb = Luminance(b)
    return (max(la,lb) + 0.05) / (min(la,lb) + 0.05)
```

### 4.4 对比度兜底 EnsureContrast

```
EnsureContrast(fg, bg, minRatio):
    fg.a = 255
    if Contrast(fg, bg) >= minRatio: return fg

    # 关键：选中“白色/黑色中对比度更高”的方向。
    # 中灰背景上纯白可能达不到 4.5，纯黑却可以。
    toward = Contrast(white, bg) >= Contrast(black, bg) ? white : black
    for step in 1..100:
        candidate = Blend(fg, toward, step/100)
        if Contrast(candidate, bg) >= minRatio: return candidate
    return toward
```

**常见坑**：如果固定“浅背景向黑、深背景向白”，在中灰背景上可能永远达不到目标；
必须按白/黑的可用对比度选择方向。

### 4.5 派生中的对比度下限

| 对象 | 下限 |
|---|---|
| 用户主题 Markdown 正文 / 窗口主文字 / 编辑器正文 | 4.5 |
| 用户主题 muted / 次级文字 / 引用文字 / 标题 / token | 3.0 |
| 内置主题（历史兼容） | 正文 4.0；次级 3.0 |

导入主题在 `BuildDefinition(..., userDefined=true)` 中统一兜底；内置主题保持原有
手调颜色，不做额外 4.5 调整，避免视觉回归。

---

## 5. 派生算法

派生分两段：

1. `FinalizeSource(draft)`：把可选的 Markdown 字段补全。
2. `BuildDefinition(source)`：从 Markdown 色派生预览、窗口、编辑器、token 兜底。

### 5.1 FinalizeSource：Markdown 字段缺省值

| 字段 | 缺省值 |
|---|---|
| background | `#ffffff` |
| text | `#1f2328` |
| dark | `Luminance(background) < 0.5` |
| accent | `accent` 或 `link` 或 `#0969da` |
| link | accent |
| muted | `Over(text@50%, background)` |
| border | `Blend(background, text, 0.15)` |
| hr | border |
| strong / emphasis | text |
| listMarker | muted |
| headings[0..4] | text |
| headings[5] | muted |
| blockquoteText / blockquoteBorder | muted / border |
| codeBackground | `Blend(background, text, dark ? 0.10 : 0.06)` |
| codeText | text |
| codeBorder | `Blend(codeBackground, text, 0.10)` |
| tableBorder | border |
| tableHeadBackground | codeBackground |
| selection | `accent@25%` |
| findMatch / findCurrent | `accent@40%` / `accent@60%` |
| findText | text |
| sidebarBackground / sidebarHover | `Blend(background, text, 0.04)` / `0.08` |
| danger | `#d64545` |
| typography | 内置字体栈、15px、1.75、980、32/40/64、13 |
| family | 按 accent 色相推断 |

#### Token 缺省值

如果主题没有提供 `tokens`，按下列规则生成，并对 `codeBackground` 做 3.0 兜底：

```
comment       = Blend(text, background, 0.55)
punctuation   = Blend(text, background, 0.10)
tag           = accent
property      = Blend(accent, text, 0.35)
booleanNumber = Blend(accent, text, 0.20)
string        = Blend(accent, text, 0.55)
operator      = Blend(text, background, 0.20)
function      = accent
keyword       = Blend(accent, text, 0.10)
regex         = Blend(accent, background, 0.25)
```

#### 色系推断

把 accent 转 HSL：

- 饱和度 < 0.12 → Neutral。
- 色相：<15° 或 ≥345° → RedPink；<70° → Warm；<165° → Green；<195° → Cyan；
  <255° → Blue；<290° → Purple；其余 RedPink。
- Multicolor 只能由主题显式指定。

### 5.2 BuildTheme：预览 / 窗口 / 编辑器

下面以 `pageBg = background`、`pageText = text` 为前提。所有 `Over` 结果不透明。

#### 预览

| 目标 | 公式 |
|---|---|
| pageBg | background |
| pageText | `EnsureContrast(Over(text, pageBg), pageBg, 4.0)`；用户主题再 4.5 |
| mutedText | `Over(muted, pageBg)`；用户主题再 3.0 |
| border | `Over(border, pageBg)` |
| link | link |
| inlineCodeBg / inlineCodeText | codeBackground / codeText |
| codeBlockBg / codeBlockText | codeBackground / codeText |
| codeBlockBorder | `Blend(codeBackground, pageText, 0.10)` |
| blockquoteText / blockquoteBorder | `Over(blockquoteText, pageBg)` / blockquoteBorder |
| tableBorder / tableHeadBg | tableBorder / codeBackground |
| hr | `Over(hr, pageBg)` |
| strong / em | `Over(strong/em, pageBg)` |
| listMarker | listMarker |
| selectionBg | `Over(selection, pageBg)` |
| findMatchBg / findCurrentBg | `Over(findMatch/findCurrent, pageBg)` |
| findText | 若对 findMatchBg < 4.5，则用 `EnsureContrast(pageText, findMatchBg, 4.5)` |
| headings[6] | 原样；用户主题 3.0 兜底 |

#### 窗口外壳

| 目标 | 公式 |
|---|---|
| surface | pageBg |
| windowBg | `Over(sidebarBackground, pageBg)` |
| navBg / titleBarBg | windowBg |
| titleBarInactive | `Blend(windowBg, pageBg, 0.35)` |
| tabHover / buttonHover | `Over(sidebarHover, windowBg)` |
| buttonPressed | `Blend(tabHover, pageText, 0.08)` |
| divider | tableBorder |
| textPrimary | pageText（用户主题 4.5） |
| textSecondary | `Blend(pageText, pageBg, 0.15)`（用户主题 3.0） |
| accent | accent |
| accentSoft | `Blend(pageBg, accent, 0.12)` |
| gutterBg | `Blend(pageBg, pageText, 0.03)` |
| currentLine | `Blend(pageBg, accent, 0.06)` |
| selection | `Over(selection, pageBg)` |
| danger | danger |
| dirtyDot | accent |

#### 编辑器

| 目标 | 来源 |
|---|---|
| background | pageBg |
| text | pageText（用户主题 4.5） |
| heading | headings[1] |
| emphasis | pageText |
| strong | strong |
| code | inlineCodeText |
| codeBlock | codeBlockText |
| link / image | link |
| listMarker | listMarker |
| blockQuote | blockquoteText |
| html | mutedText |
| hrule | hr |
| comment | mutedText |
| reference | chrome.accent |
| gutterBg | chrome.gutterBg |
| gutterText | chrome.textSecondary |
| currentLine | chrome.currentLine |
| selection | chrome.selection |
| selectionText | pageText |
| caret | chrome.accent |

#### 滚动条

```
track = pageBackground
thumb = Blend(pageBackground, pageText, 0.28)
arrow = chrome.textSecondary
```

预览滚动条使用同一组公式，输出 IE 的 `scrollbar-*-color` 属性；其他引擎可用
`::-webkit-scrollbar` 或 CSS 变量重写。

### 5.3 BuildDefinition 伪代码

```
BuildDefinition(source, userDefined):
    spec = fill ThemeSpec from source          # 见 5.1 的字段
    theme = BuildTheme(spec)                   # 见 5.2
    theme.typography = source.typography
    theme.extraCss = Sanitize(source.extraCss)
    apply source.ui overrides to theme.chrome
    re-derive theme.editor from preview/chrome
    apply source.overrides                     # 高级稀疏覆盖
    if userDefined:
        ensure preview.pageText / chrome.textPrimary / editor.text >= 4.5
        ensure preview.mutedText / blockquoteText / chrome.textSecondary >= 3.0
        ensure tokens >= 3.0; headings >= 3.0
    return theme
```

---

## 6. 表现层

### 6.1 预览 CSS

#### 6.1.1 结构

生成一段**纯 CSS 字符串**，插入预览 HTML 的 `<style>`：

1. `html, body` 重置。
2. `body`：背景、正文色、字体、字号、行高。
3. `.markdown-body`：最大宽度、居中、内边距、换行。
4. 标题、段落、列表、链接、强调、代码、引用、表格、图片、`hr`、`kbd`。
5. `::selection`。
6. IE 滚动条属性。
7. 查找高亮 class。
8. Prism token 规则。
9. 主题 `extraCss`（过滤后）。
10. 调用方的运行时 `extraCss`。

#### 6.1.2 排版

- 正文字体、字号、行高、最大宽度、内边距、代码字号来自 `typography`。
- 缺省：Segoe UI 栈、Consolas 栈、15px、1.75、980px、32/40/64、13px。
- 标题字号固定：h1 2em、h2 1.55em、h3 1.3em、h4 1.12em、h5 1em、h6 0.92em。
- h1/h2 有 1px 底边框，颜色用 `preview.border`。
- 字体名插入前做保守校验，拒绝 `;{}<>@\()` 等可终止声明或引入函数的字符。

#### 6.1.3 Prism token 选择器

| 分组 | 选择器 |
|---|---|
| comment | `.token.comment,.token.prolog,.token.doctype,.token.cdata` |
| punctuation | `.token.punctuation` |
| tag | `.token.selector,.token.tag` |
| property | `.token.property,.token.constant,.token.symbol,.token.deleted` |
| booleanNumber | `.token.boolean,.token.number` |
| string | `.token.string,.token.char,.token.attr-name,.token.attr-value,.token.builtin,.token.inserted` |
| operator | `.token.operator,.token.entity,.token.url,.language-css .token.string,.style .token.string` |
| function | `.token.atrule,.token.function,.token.class-name` |
| keyword | `.token.keyword` |
| regex | `.token.regex,.token.important,.token.variable` |

另外固定：`.token.important,.token.bold{font-weight:bold}`、
`.token.italic{font-style:italic}`、`.token.entity{cursor:help}`、
`.token.namespace{opacity:.7}`。

#### 6.1.4 IE11 / 旧引擎注意

- **不要输出 `var()`、`:root`、`--*`**；直接输出具体颜色。
- 滚动条使用旧属性：
  `scrollbar-face-color`、`scrollbar-track-color`、`scrollbar-arrow-color`、
  `scrollbar-shadow-color`、`scrollbar-highlight-color`、`scrollbar-3dlight-color`、
  `scrollbar-darkshadow-color`。
- 预览文档需要 `<!DOCTYPE html>` + `X-UA-Compatible: IE=edge`，否则会退回旧文档模式。

#### 6.1.5 运行时额外 CSS

预览构建器可以在主题 CSS 之后追加调用方提供的 CSS；主题自带的 `extraCss`
先追加，调用方的再追加，后者可覆盖前者。

`extraCss` 的安全策略见第 11 节：**只允许原生格式携带**，外部导入的 CSS 永远
不作为原始 CSS 执行。

### 6.2 编辑器配色

把 `ThemeDefinition.editor` 映射到具体编辑器：

- Scintilla：`SCI_STYLESET*`（前景/背景/字体/粗斜体），以及 caret、selection、
  margin、current line。
- Monaco/CodeMirror：把同一组语义色映射到 token 主题和编辑器样式。
- 其他：至少映射背景、正文、标题、强调、代码、链接、引用、列表、行号、
  当前行、选中、光标。

**关键**：编辑器颜色必须来自同一份 `ThemeDefinition`，不要另外维护一套。

### 6.3 原生窗口 Palette

以 Win32 为例，`ChromeColors` 映射到 18 个 `COLORREF`：

| Palette | ChromeColors |
|---|---|
| windowBg | windowBg |
| surface | surface |
| navBg | navBg |
| titleBarBg / titleBarInactive | 同名 |
| tabHover / buttonHover / buttonPressed | 同名 |
| divider | divider |
| textPrimary / textSecondary | 同名 |
| accent / accentSoft | 同名 |
| gutterBg / currentLine / selection | 同名 |
| danger / dirtyDot | 同名 |

- GDI 没有 alpha；所有颜色先 `Over` 压平。
- 滚动条用同一组公式；导航树/列表的滚动条轨道应跟随侧栏背景，而不是文档背景。
- 深色主题可调用平台的暗色模式接口；不可用时保持系统样式，不要伪造。

---

## 7. 原生文件格式与存储

### 7.1 位置

```
%LOCALAPPDATA%\ActiveXWebMark\themes\<id>.awmtheme.json
```

- 允许 `AWM_SETTINGS_DIR` 之类的环境变量覆盖，便于测试/便携运行。
- 设置文件只存 `ui.theme`（id），不存主题内容。
- 主题目录不存在时自动创建；启动时扫描，坏文件跳过并记录。

### 7.2 原子写入

保存时先写同目录临时文件，再原子替换目标文件（如 `MoveFileEx` +
`MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`）。避免崩溃留下半截 JSON。

### 7.3 加载顺序（重要）

必须在读取 `ui.theme` **之前**加载用户主题目录：

```
settings = load_settings()
load_user_themes(themes_dir)       # 注入主题目录
view_settings = load_view_settings(settings)  # ui.theme 现在能解析
```

否则用户主题 id 会被当成未知 id，回退到默认主题，并在退出时把 `ui.theme`
改写为默认 id。

### 7.4 运行时重载

- 主题目录重载后，内置主题索引必须保持不变；用户主题只替换尾部。
- 菜单命令 id 常用“基址 + 目录下标”；重载后必须重建菜单，或使用代际号
  （generation counter）在菜单弹出前检查并重建。
- 跨 UI 边界只传主题 id，不长期持有主题对象指针/引用；重载会使旧引用失效。
- 当前主题被删除时，回退默认主题并持久化。

---

## 8. 导入适配器

### 8.1 接口

```
ImportResult ImportThemeFile(path):
    for importer in registry:
        if importer.CanImport(path):
            return importer.Import(path)
    return error("unsupported")
```

`ImportResult` 应包含：

- `themes: ImportedTheme[]`（一个 VSIX 可能含多个主题）
- `warnings: string[]`（未映射的 key、近似映射、被忽略的构造）
- `error: string`（文件级失败）

**只翻译，不执行**：导入器不把外部 CSS 直接塞进预览。

### 8.2 VS Code Color Theme

#### 8.2.1 文件形态

- JSON/JSONC；支持 `//`、`/* */`、尾逗号。
- `colors`：workbench color key → 颜色。
- `tokenColors`：数组或指向 `.tmTheme` 的路径。
- `semanticTokenColors`：可选。
- `include`：相对路径继承；父在前、子覆盖；限制递归深度并检测环。
- `type`：`light` / `dark` / `hc-black`。

#### 8.2.2 颜色映射

| VS Code key | ThemeSource |
|---|---|
| `editor.background` | background |
| `editor.foreground` | text |
| `textLink.foreground` / `textLink.activeForeground` | link |
| `focusBorder` / `activityBarBadge.background` / `button.background` / `progressBar.background` | accent |
| `errorForeground` / `editorError.foreground` | danger |
| `descriptionForeground` / `disabledForeground` | muted |
| `textCodeBlock.background` / `editorWidget.background` | codeBackground |
| `editorWidget.border` / `panel.border` / `editorGroup.border` | border / tableBorder |
| `sideBar.background` | sidebarBackground |
| `list.hoverBackground` / `tab.hoverBackground` | sidebarHover |
| `editor.selectionBackground` | selection |
| `editor.findMatchBackground` / `editor.findMatchHighlightBackground` | findCurrent / findMatch |
| `editor.findMatchForeground` | findText |

未被映射的 key 不需要报错；只对“缺失关键颜色”给出警告。

#### 8.2.3 Token scope → 10 个分组

代表性 scope：

| 分组 | 代表 scope |
|---|---|
| comment | comment |
| punctuation | punctuation |
| tag | entity.name.tag |
| property | variable.other |
| booleanNumber | constant.numeric |
| string | string |
| operator | keyword.operator |
| function | entity.name.function |
| keyword | keyword |
| regex | string.regexp |

简化匹配规则（适合主题导入，不要求完整 TextMate 语义）：

1. 按逗号拆分 `scope`，忽略以 `-` 开头的排除项。
2. 取选择器最后一个空格分隔的组件作为具体 scope。
3. 与代表 scope 做“段前缀”匹配（相等，或一方是另一方加 `.` 的前缀）。
4. 具体度 = 段数 × 100 + 长度；同分时后出现者优先。
5. 未命中任何规则的分组用第 5.1 节的 token 缺省公式补全。

`fontStyle` 可忽略（本实现只用 bold/italic 固定规则）。

#### 8.2.4 JSONC 处理

写一个**字符串感知**的预处理：

- 扫描时跟踪 `inString` 和转义；字符串内的 `//` 不能当注释。
- 移除 `//` 到行尾、`/* ... */`。
- 再扫描一次，去掉 `}`/`]` 前的尾逗号。
- 不要用正则一把梭。

### 8.3 VSIX

VSIX 本质是 ZIP：

1. 用 ZIP 库（本实现用 vendored miniz）读取中央目录。
2. 读取 `extension/package.json`：
   ```json
   "contributes": {
     "themes": [
       { "label": "My Theme", "uiTheme": "vs-dark", "path": "./themes/my.json" }
     ]
   }
   ```
3. 按 `path` 读取主题 JSON，支持 `include`。
4. 没有 `contributes.themes` 时，回退扫描 `extension/themes/**/*.json|jsonc`。
5. 保留 `publisher`、`name`、`version`、`license` 到 `source`/`license`。

安全限制（建议值）：

| 限制 | 值 |
|---|---|
| ZIP 文件 | ≤ 32 MB |
| 条目数 | ≤ 2048 |
| 单条目解压 | ≤ 8 MB |
| 解压总量 | ≤ 32 MB |
| 路径 | 拒绝 `..`、绝对路径、盘符、UNC |
| 加密条目 | 拒绝 |

可以解压到独立临时目录（带路径校验）再导入，导入后清理；也可以纯内存读取。
无论哪种，都不要相信 ZIP 内的路径和大小。

### 8.4 TextMate `.tmTheme`

- XML plist；只取 `settings` 数组、全局 `background/foreground/selection`、
  各规则的 `scope` + `settings.foreground`。
- 只解析需要的标签（dict/array/key/string/true/false），不必实现完整 XML。
- 拒绝 `<!ENTITY`，限制文件大小（如 2 MB）和嵌套深度。
- 全局 bg/fg 缺失时，用默认值或当前主题作为基底。
- token 规则套用 8.2.3 的分组。

### 8.5 Base16 / Base24

YAML 调色板：

```yaml
system: "base16"
name: "Scheme"
author: "Author"
variant: "dark"
palette:
  base00: "101010"   # background
  base01: "181818"
  base03: "888888"   # comment/muted
  base05: "eeeeee"   # text
  base08: "ff5555"
  base0B: "55ff55"
  base0D: "3399ff"
  base0E: "aa66ff"
```

映射建议：

- base00 → background；base05 → text；base03 → muted/comment；base01 → border/
  codeBackground/sidebarBackground；base02 → selection/hover；base08 → danger；
  base0D → accent/link；base0B/0C/0E → string/operator/keyword；base09/0A/0F →
  number/emphasis/regex。
- 这是近似映射；导入结果应标注来源。

### 8.6 导入报告

每个导入主题应返回：

- 已映射的字段数；
- 近似/降级的字段；
- 未映射的外部 key（去重、限量）；
- 被忽略的 CSS/XML 构造。

设置界面/CLI 应展示前几条，避免“看起来不对但不知道原因”。

---

## 9. 主题设置界面

### 9.1 推荐布局

```
┌ 主题设置 ───────────────────────────────────────────────┐
│ 搜索[____]           │ [基本][配色][正文排版][窗口外观] │
│ ┌ 主题列表 ────────┐ │ ┌ 编辑区 ─────────────────────┐ │
│ │ 内置主题          │ │ │ 名称/作者/明暗/色系/来源     │ │
│ │ 自定义主题        │ │ │ 颜色列表（双击选色）         │ │
│ └───────────────────┘ │ │ 正文排版 / 额外 CSS          │ │
│ [新建][导入][复制]    │ └─────────────────────────────┘ │
│ [删除][导出]          │ ┌ 预览（真实 Markdown）──────┐ │
│                       │ │ + 窗口外壳色板              │ │
│                       │ └─────────────────────────────┘ │
│                [保存] [立即使用此主题] [取消]             │
└──────────────────────────────────────────────────────────┘
```

布局约束：

- 窗口最小 900×620 逻辑像素，可拖动边框缩放或最大化。
- 编辑区与 Markdown 预览之间有一条水平分割条，可上下拖动；默认上方约 58%、
  下方约 42%，并限制页面最小 160（“窗口外观”页 230）、预览最小 180 逻辑像素。
- 拖动时实时重排，但**只移动受分割条影响的控件**：分割条、Markdown 预览、
  配色/窗口外观列表高度、额外 CSS 高度和提示行；左侧列表、页签和底部按钮不参与。
- 移动/缩放子控件使用 `SWP_NOZORDER | SWP_NOCOPYBITS | SWP_NOACTIVATE`，避免
  旧位图被拷贝出来。
- **不要把 `WS_EX_COMPOSITED` 加到包含 ListView 的整窗上**：ListView 的表头是
  独立的 `SysHeader32` 子控件，在整窗合成下会持续重绘/闪烁。如果确实需要双缓冲，
  应只作用于右侧面板的独立容器，而不是整个设置窗口。
- IE/WebView 预览使用 `SetLiveBounds()` 轻量路径：拖动期间不恢复滚动位置、
  不擦除背景、不拷贝位图；松开鼠标后再用完整 `SetBounds()` 恢复滚动并做一次
  完整重绘。不要在每帧重设 IE 滚动位置或全量重排所有子控件，这两者是残影/
  抖动的主要来源。
- 保存/立即使用此主题/取消与状态提示共用底部一行：状态在左、按钮在右，
  不再单独占用一行空白；预览必须在按钮行之上结束，不要让 IE/WebView 预览
  控件覆盖按钮，否则滚动时按钮会“消失”。
- 页内的“编辑颜色”按钮与底部提示文字分行，不能重叠；同一页面只保留一个提示
  标签，切换页签时更新其文本。

### 9.2 交互语义

- **内置主题只读**；点击“复制”创建可编辑副本。
- **新建/复制**清空 id，保存时由存储层分配新 id。
- **颜色列表**：标签 + 色块 + 十六进制；双击/按钮打开系统颜色选择器；
  保留原有 alpha（选择器通常只改 RGB）。
- **正文排版**：正文字体、字号、行高、最大宽度、等宽字体、代码字号、额外 CSS。
  字体输入框占整行，数字字段使用固定窄宽度；字段带固定标签和单位，分为
  “正文 / 代码 / 高级”三组。
- **预览**：用真实 Markdown 管线 + 语法高亮渲染固定示例；窗口外壳色板用同一份
  `ChromeColors` 绘制。
- **搜索**：过滤列表，但不要在过滤时自动加载另一个主题，避免丢失未保存修改。
- **切换主题**：如果当前有未保存修改，询问是否放弃；拒绝则恢复原选择。
- **保存**：只保存当前用户主题并重载目录，**不切换当前主题**；关闭。若保存的
  正是当前主题，主程序可以刷新当前主题的三路输出，但不会切换到别的主题。
- **立即使用此主题**：保存（如需要）→ 应用并持久化 → 关闭；内置主题直接应用。
- **取消**：不保存编辑；但已导入/删除的文件保留，结果应标记“目录已变化”，
  让主程序重载。
- “窗口外观”页顶部是一个只读的迷你窗口示意（标题栏、标签、侧栏、正文、
  滚动条），下面是完整的派生颜色列表；迷你窗口不是控件/按钮，只展示派生效果。
  内置主题的配色表和窗口外观列表都必须可滚动浏览，只禁用编辑动作。
- **删除当前主题**：确认；删除文件；重载；当前主题消失时回退默认。
- **导出**：默认 `<slug>.awmtheme.json`；内置主题导出时带完整 `overrides`，
  保证重新导入后视觉一致。

### 9.3 状态管理建议

- 编辑状态是一个完整的 `ThemeSource` + `dirty` 标志。
- 列表选择只切换“当前编辑对象”；未保存时先确认。
- 预览使用 150ms 左右的防抖，避免每个按键都重建 HTML。
- 设置窗口自身使用当前应用 palette，不使用正在编辑的草稿色，避免编辑出
  不可读的界面。

---

## 10. 运行时接入

### 10.1 应用主题（按 id）

```
ApplyTheme(id, persist):
    definition = ResolveTheme(id)      # 内置 + 用户目录
    ui.setPalette(MakePalette(definition))
    set_native_color_mode(definition.dark)
    for each document view:
        view.SetTheme(id)
        view.ApplyTheme()              # 预览/编辑器/滚动条
    titlebar/tabstrip/update dialog: update palette
    invalidate window
    if persist: settings["ui.theme"] = id
```

- 隐藏标签页可延迟到激活时重建预览，避免一次切换重渲染所有文档。
- 预览 HTML 内嵌主题 CSS；主题变化时清空缓存并重渲染。
- 编辑器配色可每次全量重设（Scintilla 开销很小）。

### 10.2 菜单

- 菜单项 id = 基址 + 主题目录下标。
- 主题目录变化后，菜单项必须重建；不要在菜单跟踪（`TrackPopupMenu`）过程中
  修改目录。
- 分组建议：自定义主题 / 浅色 / 深色 / 按作者 / 按色系 / 主题设置。
- 同一主题可出现在多个分组；勾选状态要跨分组同步。

---

## 11. 安全

### 11.1 额外 CSS

本实现的原生 `extraCss` 是严格模式：

- 上限 16 KB。
- 拒绝 `<`、`>`、`\`、`@`、`(`、`)`、`/*`、`*/` 和控制字符。
- 这样直接阻断 `@import`、`url()`、`expression()`、`var()`、标签闭合注入和
  转义绕过。
- 被拒绝的整段 CSS 丢弃，并返回原因。
- **外部导入的 VS Code/VSIX/tmTheme/Base16 主题永远不携带原始 CSS。**

如果确实需要 `calc()`、渐变、`:not()` 等函数，建议后续改为函数白名单，
而不是放开整个 `()`。

### 11.2 文件与网络

- 原生 JSON ≤ 2 MB；tmTheme ≤ 2 MB；Base16 ≤ 1 MB；VSIX 见 8.3。
- ZIP 路径穿越、绝对路径、盘符、加密条目一律拒绝。
- XML 拒绝 `<!ENTITY`，限制深度和大小。
- 主题文件只从本地读取；不支持在线市场时不要偷偷发网络请求。
- 原子写入，避免半截文件。

### 11.3 外部 CSS 不执行

导入器只提取颜色/排版数据，不把外部 CSS 注入预览。这是“可导入”和“可安全
导入”的分界线。应用特有 DOM 的装饰（callout、伪元素、布局）无法通用还原，
应明确告诉用户。

---

## 12. 可访问性与质量

- 用户主题正文/主 UI 文字 ≥ 4.5，次级文字/标题/token ≥ 3.0。
- 深色主题不要用纯黑背景 + 纯白文字；轻微抬高背景/压低文字更舒适。
- 悬停/选中/焦点要有可区分但不刺眼的色阶。
- 导入主题必须做对比度兜底；不要假设外部主题一定可读。
- 颜色不是唯一信息载体：脏标记、错误、选中还应有形状/位置/文字提示。
- 高对比度模式下，原生控件可保留系统样式，不要强行覆盖。

---

## 13. 测试策略

### 13.1 单元测试（无 GUI）

| 测试 | 内容 |
|---|---|
| 颜色解析 | `#rgb/#rgba/#rrggbb/#rrggbbaa/rgb()/rgba()/命名色`、非法输入 |
| 派生 | 缺省字段、dark/family、token 缺省、对比度下限 |
| 原生 JSON | 最小/完整、未知字段、schemaVersion 2、UTF-8、BOM、超限 |
| 往返 | 内置主题 `FromDefinition → BuildDefinition` 逐字段一致 |
| 存储 | 保存/加载/删除、坏文件跳过、id 冲突、导入不覆盖 |
| VS Code | JSONC、include 链/环、colors 映射、token scope、semantic tokens |
| tmTheme | plist 解析、全局色、scope 规则、实体拒绝 |
| Base16 | YAML 键、缺项、明暗推断 |
| VSIX | 合成包、多主题、路径穿越、超大条目、加密条目 |
| CSS | 生成内容、无 `var()/:root/--`、extraCss 顺序、sanitize |
| 对比度 | 中灰背景、白/黑方向选择、4.5/3.0 下限 |

### 13.2 手动冒烟

- 切换全部内置主题，检查预览、编辑器、持久化、重启恢复。
- 用隔离的 `AWM_SETTINGS_DIR` 启动，放入一个用户主题，检查它被加载并应用。
- 打开主题设置，检查创建/取消、改名/保存、“立即使用此主题”、导入/删除/导出；
  并检查内置主题的配色表可滚动、窗口外观页可滚动、保存不会自动切换主题。
- 检查窗口可缩放、分割条可拖动并**实时**改变预览高度、松开后保持最终尺寸、
  预览区随窗口变大、预览不覆盖底部按钮、“编辑颜色”按钮与提示不重叠
  （可用 `theme-layout-smoke.ps1`）。
- 在 100%/150%/200% DPI 下检查设置窗口和预览。
- 检查删除当前主题后的回退。

### 13.3 回归重点

- 内置主题的颜色/索引不能变（菜单 id 依赖索引）。
- 生成的主题数据（如 codegen 产物）不能被无关改动污染。
- Debug 构建要跑一遍：Release 下侥幸通过的未定义行为（悬垂视图、未初始化）
  会在 Debug 断言中暴露。

---

## 14. 移植清单

### 14.1 最小可用版本（MVP）

1. 定义 `ThemeSource`/`ThemeDefinition` 和颜色类型。
2. 实现 `ParseColor`、`Blend`、`Over`、`Luminance`、`Contrast`、`EnsureContrast`。
3. 实现 `FinalizeSource`（第 5.1 节）。
4. 实现 `BuildDefinition`（第 5.2 节）。
5. 生成预览 CSS（第 6.1 节）。
6. 映射编辑器配色和窗口 palette（第 6.2/6.3 节）。
7. 用户主题目录 + 启动加载 + `ui.theme` 持久化。
8. 做一个最小的设置界面：列表、颜色、排版、预览、保存。

### 14.2 再逐步加入

9. VS Code JSON/JSONC 导入（含 include）。
10. tmTheme、Base16/Base24。
11. VSIX。
12. 导出、复制、删除、搜索。
13. 更丰富的语义字段/高级主题包（`theme.json` + `theme.css` + assets）。
14. 在线市场 provider（独立于格式适配器）。

### 14.3 移植常见坑

- **中灰背景的对比度方向**：纯白可能达不到 4.5，必须比较白/黑两者的可用对比度。
- **alpha**：原生 UI/编辑器先压平；CSS 可以保留 alpha。
- **CSS 变量**：旧引擎不支持；最终要输出具体颜色。
- **导入顺序**：`include` 父在前、子覆盖；tokenColors 后出现者优先。
- **scope 匹配**：不要试图 100% 复刻 TextMate；用代表性 scope + 具体度评分，
  并报告近似结果。
- **id/文件名**：用户可见名允许重复，id 必须唯一且稳定。
- **主题目录重载**：内置索引保持稳定；菜单重建；不要跨重载持有对象引用。
- **启动顺序**：先加载用户主题，再解析 `ui.theme`。
- **编码**：UTF-8，容忍 BOM；Windows 路径用宽字符/UTF-8 转换。
- **字符串生命周期**（C++ 等）：不要把临时字符串的视图保存下来；
  解析颜色时尤其容易踩。
- **DPI**：设置窗口和预览要能随 DPI 缩放；至少不要写死像素。
- **不要执行外部 CSS**：只提取数据；应用特有 DOM 的装饰无法通用还原。
- **ListView 表头**：报告视图的表头是独立的 `SysHeader32` 子控件。给 ListView
  加 `WS_CLIPCHILDREN`，并在 `WM_MOUSEWHEEL`/`WM_VSCROLL`/方向键滚动后强制
  重绘表头，否则滚轮滚动后表头文字可能被列表客户区覆盖。不要把
  `WS_EX_COMPOSITED` 加到包含 ListView 的整窗上，否则表头会持续闪烁。
- **关闭自定义对话框**：先 `ShowWindow(SW_HIDE)` 再 `DestroyWindow()`，避免
  IE/WebView 预览在销毁时最后一帧白闪。

---

## 15. 参考实现（C++）

| 文件 | 作用 |
|---|---|
| `src/core/theme/theme_catalog.h/.cpp` | `ThemeDefinition`、派生、目录、对比度 |
| `src/core/theme/theme_source.h/.cpp` | `ThemeSource`、`FinalizeSource`、`ParseColor`、sanitize |
| `src/core/theme/theme_json.h/.cpp` | 原生 JSON 读写、JSONC 预处理 |
| `src/core/theme/theme_store.h/.cpp` | 用户目录加载/保存/删除、id 分配 |
| `src/core/theme/theme_import.h/.cpp` | 格式分派 |
| `src/core/theme/vscode_theme_import.cpp` | VS Code JSON/JSONC/include/tokenColors |
| `src/core/theme/tmtheme_import.cpp` | TextMate plist |
| `src/core/theme/base16_import.cpp` | Base16/Base24 YAML |
| `src/core/theme/vsix_import.cpp` | VSIX + miniz |
| `src/core/markdown/previewcss.cpp` | 预览 CSS 生成 |
| `src/core/markdown/previewhtml.cpp` | 预览 HTML 包装、extraCss 顺序 |
| `src/app/themeapply.cpp` | `ThemeDefinition` → Win32 Palette / 滚动条 |
| `src/app/themedialog.h/.cpp` | 主题设置窗口 |
| `src/app/mainwindow.cpp` | 启动加载、菜单、应用、持久化 |
| `docs/awmtheme.schema.json` | 原生格式 Schema |
| `docs/examples/` | 示例主题 |
| `tests/core/theme_custom_test.cpp` | 模型/JSON/存储/导入测试 |
| `tests/manual/theme-smoke.ps1` | 内置主题冒烟 |
| `tests/manual/theme-editor-smoke.ps1` | 用户主题/设置窗口冒烟 |
| `tests/manual/theme-layout-smoke.ps1` | 设置窗口缩放/分割条/重叠布局冒烟 |

---

## 附录 A：原生格式完整示例

见 `docs/examples/ocean-dark.awmtheme.json`（完整）和
`docs/examples/paper-light.awmtheme.json`（最小）。

## 附录 B：VS Code color key → 语义色

见 8.2.2。未列出的 key 不强制映射；可以保留在导入警告中。

## 附录 C：Token scope → 分组

见 8.2.3。

## 附录 D：生成 CSS 节选

```css
body{background:#0f1720;color:#d7e0ea;font-family:'Segoe UI',sans-serif;
font-size:15px;line-height:1.75;}
.markdown-body{max-width:980px;margin:0 auto;padding:32px 40px 64px;}
h1{color:#ffffff;font-size:2em;border-bottom:1px solid #243241;padding-bottom:.3em;}
a{color:#5ab0ff;text-decoration:none;}
code{font-family:Consolas,monospace;font-size:90%;background:#131e29;color:#d7e0ea;
padding:2px 5px;border-radius:4px;}
pre{background:#131e29;border:1px solid #243241;border-radius:6px;padding:14px 16px;}
blockquote{border-left:4px solid #3aa0ff;color:#9fb0c2;margin-left:0;padding:0 16px;}
th,td{border:1px solid #243241;padding:6px 13px;}
::selection{background:rgba(58,160,255,0.25);}
.token.keyword{color:#ff7ab2;}
.token.string{color:#8ce99a;}
```

## 附录 E：许可与归属

- 本项目的内置主题配色参考 MarkText（MIT）与 VS Code（MIT），出处见
  [themes.md](themes.md)。
- 用户导入的市场主题只在本机读取/保存，不随安装包分发；界面展示
  `source`/`license`。
- vendored miniz 为 MIT/public-domain；使用时保留其 LICENSE。

---

**一句话总结**：以 Markdown 语义为唯一源头；颜色用确定性公式派生到预览、
编辑器和窗口；CSS/外部主题格式都是适配层；导入只翻译、不执行；对比度、
体积和路径安全在核心层统一兜底。
