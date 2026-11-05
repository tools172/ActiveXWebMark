# 示例主题

这两个文件是可编辑的起点，不是内置主题：

- `ocean-dark.awmtheme.json`：完整的深色主题，覆盖 Markdown 颜色、token、
  排版和少量额外 CSS。
- `paper-light.awmtheme.json`：最小浅色主题，只写背景/正文/强调色与排版，
  其余颜色由应用按规则派生。

使用方式：

1. 打开“视图 → 主题 → 主题设置...”。
2. 点击“导入...”，选择其中一个文件。
3. 在主题设置中调整，点击“确定”保存并应用。

也可以把文件复制到
`%LOCALAPPDATA%\ActiveXWebMark\themes\`，重启应用后直接出现在主题菜单中。
字段说明见 [../themes.md](../themes.md) 和
[../awmtheme.schema.json](../awmtheme.schema.json)；实现原理与移植说明见
[../theme-implementation.md](../theme-implementation.md)。
