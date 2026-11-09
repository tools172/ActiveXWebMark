# 主题目录

ActiveXWebMark 内置 53 个主题。`视图 > 主题` 提供三种查找方式：

```
主题
├── 浅色主题
├── 深色主题
├── 按作者/来源
│   ├── GitHub
│   ├── Microsoft
│   └── ...
└── 按色系
    ├── 中性灰
    ├── 蓝色系
    └── ...
```

同一主题会在三个分类中各出现一次，当前主题在所有视图中同时显示勾选。存在用户主题时，
菜单顶部还会出现“自定义主题”；菜单底部有“主题设置...”，用于创建、导入和编辑主题。
主题 id 保存在 `%LOCALAPPDATA%\ActiveXWebMark\settings.json` 的 `ui.theme` 中；
旧版本保存的 id 仍然有效。

色系名称对应关系：`Neutral` = 中性灰，`Blue` = 蓝色系，`Cyan` = 青色系，
`Green` = 绿色系，`Warm` = 暖色系，`RedPink` = 红粉系，`Purple` = 紫罗兰，
`Multicolor` = 多彩霓虹。

## 配色数据来源

配色值以本地参考源码为准，由 `tools/theme-codegen/generate.py` 生成
`src/core/theme/theme_catalog_generated.inc`：

- **MarkText v0.23.0**（MIT）：主题 CSS 变量与 `prismjs/*.theme.css`。
- **Microsoft VS Code 1.144.0**（MIT）：`extensions/theme-*` 中的内置主题 JSON/JSONC。
- **GitHub Primer**：GitHub Light 为项目原有的手调配色；GitHub Dark / Dark Dimmed 依据
  Primer 的 dark / dimmed 色板手工整理。

“作者/组织”列是主题配色的原始作者或组织，不是抓取仓库。VS Code 内置主题中有一部分是
第三方配色的移植版，出处见各节说明。

## 原有主题（6 个）

| id | 名称 | 明暗 | 色系 | 作者/组织 |
|---|---|---|---|---|
| `github-light` | GitHub Light | Light | Blue | GitHub |
| `ulysses` | Ulysses | Light | Cyan | Ran Luo (MarkText) |
| `solarized-light` | Solarized Light | Light | Blue | Ethan Schoonover |
| `one-dark` | One Dark | Dark | Blue | Atom (GitHub) |
| `dracula` | Dracula | Dark | Purple | Dracula Theme |
| `nord` | Nord | Dark | Blue | Arctic Ice Studio |

## MarkText 配色（28 个）

| id | 名称 | 明暗 | 色系 | 作者/组织 |
|---|---|---|---|---|
| `cadmium-light` | Cadmium Light | Light | Green | Ran Luo (MarkText) |
| `ayu-light` | Ayu Light | Light | Blue | Dempfi (Ayu) |
| `catppuccin-latte` | Catppuccin Latte | Light | Blue | Catppuccin |
| `everforest-light` | Everforest Light | Light | Green | Sainnhe (Everforest) |
| `graphite` | Graphite Light | Light | Blue | Ran Luo (MarkText) |
| `gruvbox-light` | Gruvbox Light | Light | Cyan | Pavel Pertsev (morhetz) |
| `rose-pine-dawn` | Rosé Pine Dawn | Light | Purple | Rosé Pine |
| `tokyo-night-light` | Tokyo Night Light | Light | Blue | Enkia (Tokyo Night) |
| `ayu-dark` | Ayu Dark | Dark | Cyan | Dempfi (Ayu) |
| `ayu-mirage` | Ayu Mirage | Dark | Warm | Dempfi (Ayu) |
| `catppuccin-mocha` | Catppuccin Mocha | Dark | Blue | Catppuccin |
| `cyberdream` | Cyberdream | Dark | Multicolor | Scott McKendry |
| `cadmium-dark` | Cadmium Dark | Dark | Blue | Ran Luo (MarkText) |
| `everforest-dark` | Everforest Dark | Dark | Green | Sainnhe (Everforest) |
| `gruvbox-dark` | Gruvbox Dark | Dark | Cyan | Pavel Pertsev (morhetz) |
| `horizon-dark` | Horizon Dark | Dark | RedPink | Jonathan Olaleye |
| `kanagawa` | Kanagawa | Dark | Blue | Rebelot (Kanagawa) |
| `material-dark` | Material Dark | Dark | Warm | Mattia Astorino (equinusocio) |
| `monokai-pro` | Monokai Pro | Dark | Warm | Monokai (Wimer Hazenberg) |
| `nightfox` | Nightfox | Dark | Blue | EdenEast |
| `oxocarbon-dark` | Oxocarbon Dark | Dark | Blue | Nyoom Engineering (IBM Carbon) |
| `palenight` | Palenight | Dark | Blue | Mattia Astorino (equinusocio) |
| `rose-pine` | Rosé Pine | Dark | Purple | Rosé Pine |
| `rose-pine-moon` | Rosé Pine Moon | Dark | Purple | Rosé Pine |
| `solarized-dark` | Solarized Dark | Dark | Blue | Ethan Schoonover |
| `synthwave-84` | Synthwave '84 | Dark | Multicolor | Robb Owen |
| `tokyo-night` | Tokyo Night | Dark | Blue | Enkia (Tokyo Night) |
| `tokyo-night-storm` | Tokyo Night Storm | Dark | Blue | Enkia (Tokyo Night) |

## VS Code 内置配色（17 个）

| id | 名称 | 明暗 | 色系 | 作者/组织 |
|---|---|---|---|---|
| `vscode-2026-dark` | VS Code Dark 2026 | Dark | Cyan | Microsoft |
| `vscode-2026-light` | VS Code Light 2026 | Light | Blue | Microsoft |
| `vscode-dark-plus` | VS Code Dark+ | Dark | Blue | Microsoft |
| `vscode-light-plus` | VS Code Light+ | Light | Blue | Microsoft |
| `vscode-dark-modern` | VS Code Dark Modern | Dark | Blue | Microsoft |
| `vscode-light-modern` | VS Code Light Modern | Light | Blue | Microsoft |
| `vscode-dark-vs` | VS Code Dark (Visual Studio) | Dark | Neutral | Microsoft |
| `vscode-light-vs` | VS Code Light (Visual Studio) | Light | Neutral | Microsoft |
| `vscode-high-contrast-black` | VS Code Dark High Contrast | Dark | Neutral | Microsoft |
| `vscode-high-contrast-light` | VS Code Light High Contrast | Light | Neutral | Microsoft |
| `vscode-abyss` | Abyss | Dark | Blue | Ofer Reichman |
| `vscode-kimbie-dark` | Kimbie Dark | Dark | RedPink | Jan T. Sott |
| `vscode-monokai` | VS Code Monokai | Dark | RedPink | Monokai (Wimer Hazenberg) |
| `vscode-monokai-dimmed` | VS Code Monokai Dimmed | Dark | RedPink | Monokai (Wimer Hazenberg) |
| `vscode-quiet-light` | Quiet Light | Light | Purple | Ian Beck (One Crayon) |
| `vscode-red` | Red | Dark | RedPink | 1ClickComputing |
| `vscode-tomorrow-night-blue` | Tomorrow Night Blue | Dark | Blue | Chris Kempson |

说明：

- **Abyss**：原始配色收录于 Ofer Reichman 的 `oferei/sublime-colors`；VS Code 版本由
  gerane 移植。
- **Red**：来自 Colorsublime 的 `red.tmTheme`（1ClickComputing），VS Code 内置版本为
  Microsoft 维护的转换。
- **Kimbie Dark**：Jan T. Sott 的 Kimbie 配色。
- **Quiet Light**：Ian Beck（onecrayon）为 MacRabbit Espresso 设计，后移植到 VS Code。
- **Tomorrow Night Blue**：Chris Kempson 的 Tomorrow 配色系列。

## GitHub Primer（2 个）

| id | 名称 | 明暗 | 色系 | 作者/组织 |
|---|---|---|---|---|
| `github-dark` | GitHub Dark | Dark | Blue | GitHub |
| `github-dark-dimmed` | GitHub Dark Dimmed | Dark | Blue | GitHub |

项目原有的 `github-light`（作者 GitHub）仍为默认主题。

## 许可与署名

- MarkText 与 Microsoft VS Code 均以 MIT 许可证发布；本项目的生成数据取自它们的主题
  文件，并保留原始作者/组织与出处。
- 大多数社区配色项目（Catppuccin、Dracula、Everforest、Gruvbox、Nord、Rosé Pine、
  Solarized、Tokyo Night 等）采用 MIT 许可证；Monokai Pro 是 Monokai 的品牌配色，
  MarkText 以 MIT 分发其 CSS 表示。
- 用户自行导入的市场主题只在本机读取，不随安装包分发。

## 生成器

```powershell
python -I tools/theme-codegen/generate.py `
  --marktext-root F:/3rd_source/tools/editor/markdown/marktext/src/v0.23.0/marktext `
  --vscode-root F:/3rd_source/tools/editor/vscode/src/1.144.0/vscode
```

生成器只使用 Python 标准库，构建过程不调用它。`--check` 可用于本地校验：
若提交的 `theme_catalog_generated.inc` 与重新生成的结果不一致则返回非零。
当前 CI 只构建并运行单元测试，不执行 `--check`。

新增主题时：

1. 在 `generate.py` 的 `THEMES` 表中加入 `(id, 名称, 明暗, 作者/组织, 色系, 来源类型, 源文件)`；
2. 重新生成并审查 `.inc`；
3. 更新本文件的目录表与 `tests/core/theme_catalog_test.cpp` 的数量断言。

## 自定义主题

从“视图 → 主题 → 主题设置...”打开主题设置窗口。内置主题为只读；点击“复制”
创建可编辑的自定义主题。实现原理、派生公式和跨语言移植说明见
[theme-implementation.md](theme-implementation.md)。

设置窗口提供：

- **基本**：名称、作者、明暗、色系、来源/许可证；
- **配色**：Markdown 语义色、6 级标题色、引用/代码/表格/查找/侧栏色，以及
  10 组代码 token 色；
- **正文排版**：正文字体、字号、行高、最大宽度，以及等宽字体、代码字号和额外
  CSS；字段带固定标签与单位，并分为“正文 / 代码 / 高级”三组；
- **窗口外观**：页内先显示一个迷你窗口示意（标题栏、标签、侧栏、正文、
  滚动条），下面只读列出窗口外壳、编辑器和滚动条的全部派生颜色；内置主题的
  配色表和该列表都能滚动浏览，只是编辑按钮禁用；
- **实时预览**：用真实 Markdown 管线 + Prism 渲染示例；编辑区与 Markdown
  预览之间有一条可拖动分割条，拖动时实时调整上下比例；窗口只移动受影响的
  控件并避免 IE 预览反复重排的抖动；窗口可缩放/Maximize，预览区会随窗口变大，
  底部操作按钮与预览区分离；
- **新建 / 复制 / 导入 / 删除 / 导出**；
- **保存 / 立即使用此主题 / 关闭**：“保存”写入主题文件并保持窗口打开，便于
  继续调整；“立即使用此主题”保存（如可保存）并立即应用到主窗口，窗口同样保持
  打开；“关闭”结束编辑，有未保存修改时先确认。内置主题没有可保存内容，“保存”
  禁用，可直接“立即使用此主题”。

### 存储位置

`%LOCALAPPDATA%\ActiveXWebMark\themes\<id>.awmtheme.json`

设置 `AWM_SETTINGS_DIR` 时放在该目录的 `themes\` 子目录，便于测试和便携运行。
`settings.json` 的 `ui.theme` 只保存主题 id；主题内容不写进设置文件。

### 原生格式

`*.awmtheme.json`，`schemaVersion: 1`。完整 Schema 见
[awmtheme.schema.json](awmtheme.schema.json)，示例见 [examples/](examples/)。

最小主题只需要：

```json
{
  "schemaVersion": 1,
  "name": "My Theme",
  "markdown": {
    "background": "#ffffff",
    "text": "#1f2328",
    "accent": "#0969da"
  }
}
```

其余颜色按以下规则派生：

| 目标 | 规则 |
|---|---|
| muted | text 以 50% 不透明度压到 background 上 |
| border / hr | background 向 text 混合 15% |
| 代码背景 | background 向 text 混合 6%（深色 10%） |
| 代码边框 | 代码背景向 text 混合 10% |
| 侧栏背景 | background 向 text 混合 4% |
| 侧栏悬停 | background 向 text 混合 8% |
| 选中 | accent + alpha 0.25 |
| 查找匹配 / 当前 | accent + alpha 0.40 / 0.60 |
| 窗口背景 | 侧栏背景压平到 Markdown 背景 |
| 窗口文字 | 正文色；次级文字向背景混合 15% |
| 强调色 / 脏标记 | accent |
| 分隔线 | 表格边框 |
| 滚动条滑块 | background 向 text 混合 28% |
| 编辑器 | 由标题/代码/链接/引用等 Markdown 色直接映射 |

未列出的字段：link 缺省取 accent；strong/emphasis 取正文色；codeText 取正文色；
blockquoteText 取 muted；tableHeadBackground 取代码背景；listMarker 取 muted；
findText 取正文色；标题 1–5 级取正文色、6 级取 muted；danger 缺省为红色。

`dark` 缺省时按背景亮度推断；`family` 缺省时按强调色色相推断。用户主题的
对比度按 WCAG 兜底：正文/窗口主文字/编辑器正文 ≥ 4.5，muted/次级文字/
引用文字/标题/token ≥ 3.0。

设置窗口保存时只会写入你实际设置过的颜色，仍等于派生默认值的字段会被省略。
在设置窗口里修改 `background` / `text` / `accent` 时，未在本轮手动改过的结构性
颜色（侧栏、窗口外壳、代码块、边框、引用、表格、弱化文字、查找色等）会随之
重新派生，色系和明暗也会重新推断；标题、Token、粗体/斜体等身份色保持不变。

可选的 `ui` 段用于单独覆盖窗口外壳色（`windowBg`、`surface`、`titleBarBg`、
`accent`、`selection` 等）；不填就使用上表派生值。`overrides` 是内置主题导出/
精确往返用的高级稀疏覆盖，普通主题不需要。

`extraCss` 是高级逃生舱：v1 严格拒绝 `<`、`>`、`\`、`@`、`(`、`)`、CSS 注释和
控制字符，整段上限 16 KB。这样可以阻断 `@import`、`url()`、`expression()`、
`var()` 和标签闭合注入。被拒绝的内容不会写入预览，并在设置窗口/导入结果中提示。

### 导入

主题设置窗口的“导入...”支持：

| 格式 | 说明 |
|---|---|
| `*.awmtheme.json` | 原生格式；直接读取 |
| VS Code Color Theme `.json` / `.jsonc` | `colors`、`tokenColors`、`semanticTokenColors`、`include` 继承链；JSONC 注释和尾逗号 |
| VS Code `.vsix` | 读取 `extension/package.json` 的 `contributes.themes`，支持包内多主题；保留 publisher/version/license |
| TextMate `.tmTheme` | XML plist；读取全局背景/正文/选中和 scope 规则 |
| Base16 / Base24 `.yaml` / `.yml` | 读取 `palette.base00`–`base0F`、名称、作者、明暗 |

导入器只做“翻译”，不会执行外部主题里的 CSS。VSIX 使用内置的 miniz 读取 ZIP，
限制：文件 ≤ 32 MB、条目 ≤ 2048、单条目解压 ≤ 8 MB、总量 ≤ 32 MB，并拒绝路径
穿越和加密条目。导入的主题统一转成原生 JSON，写入用户主题目录；源包信息和
许可证保存在 `source` / `license` 字段中，供界面展示。

暂不支持：在线浏览/搜索 Marketplace、Typora/Obsidian/MarkText 的 CSS 变量主题、
主题包内的字体/图片资源。这些格式和在线市场可以在后续版本作为新的导入适配器
加入，不需要改动核心渲染管线。

### 为什么以语义 JSON 为源头

Markdown 主题目前没有统一标准：CommonMark/GFM 只定义语法；Typora、Obsidian、
MarkText 各用一套 CSS 变量；`github-markdown-css` 只是 GitHub 风格的 CSS 复刻。
编辑器主题方面，VS Code Color Theme JSON 是事实标准，TextMate `.tmTheme` 是旧式
语法主题格式，Base16/Base24 是调色板事实标准，W3C DTCG（2025.10 Community Group
Final Report）是通用设计令牌规范。

本应用同时要驱动预览、Scintilla 和 Win32 自绘窗口，因此以 Markdown 语义色为
唯一数据源，生成预览 CSS、编辑器配色和窗口 palette；CSS 只作为表现层和导入源。
这样既能保持三路一致，又不会把内部模型绑死在某一家的主题 key 上。

### 测试

- `tests/core/theme_custom_test.cpp`：颜色解析、派生、原生 JSON、存储、VS Code/
  Base16/tmTheme/VSIX 导入、内置主题逐字段往返。
- `tests/manual/theme-smoke.ps1`：切换内置主题，验证预览、编辑器、持久化。
- `tests/manual/theme-editor-smoke.ps1`：用隔离的 `AWM_SETTINGS_DIR` 启动，
  验证自定义主题生效、主题设置窗口可创建/取消、配色/派生列表可滚动、保存/应用
  按钮状态。
- `tests/manual/theme-layout-smoke.ps1`：验证窗口可缩放、预览不覆盖底部按钮、
  “编辑颜色”按钮与提示不重叠、放大窗口后预览区随之变大。
