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

同一主题会在三个分类中各出现一次，当前主题在所有视图中同时显示勾选。主题 id 保存在
`%LOCALAPPDATA%\ActiveXWebMark\settings.json` 的 `ui.theme` 中；旧版本保存的 id 仍然有效。

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

生成器只使用 Python 标准库，构建过程不调用它。`--check` 用于 CI：若提交的
`theme_catalog_generated.inc` 与重新生成的结果不一致则失败。

新增主题时：

1. 在 `generate.py` 的 `THEMES` 表中加入 `(id, 名称, 明暗, 作者/组织, 色系, 来源类型, 源文件)`；
2. 重新生成并审查 `.inc`；
3. 更新本文件的目录表与 `tests/core/theme_catalog_test.cpp` 的数量断言。

## 自定义主题（后续版本）

当前版本只预留数据模型，不实现用户主题加载器/编辑器。规划中的用户主题格式是带版本的
语义 JSON（`*.awmtheme.json`，`schemaVersion: 1`），只暴露约 20 个语义色和 10 组 token
颜色，其余内部配色由 `BuildTheme()` 派生并统一做对比度兜底。VS Code 主题 JSON、
MarkText/Typora/Obsidian 的 CSS 变量、Base16/Base24 调色板将作为导入适配层。

Markdown 主题目前没有统一标准：CommonMark/GFM 只定义语法；Typora、Obsidian、MarkText
各用一套 CSS 变量；`github-markdown-css` 只是 GitHub 风格的 CSS 复刻。编辑器主题方面，
VS Code Color Theme JSON 是事实标准，TextMate `.tmTheme` 是旧式语法主题格式，Base16/Base24
是调色板事实标准，W3C DTCG（2025.10 Community Group Final Report）是通用设计令牌规范。
