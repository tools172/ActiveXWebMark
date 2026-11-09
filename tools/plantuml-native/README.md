# PlantUML headless 原生包

应用本地 PlantUML 模式使用的官方 GraalVM 原生构建。它不是 Java 环境，用户机器
不需要安装 Java。

## 固定版本

见 `manifest.json`：

- 版本：1.2026.4
- 文件：`plantuml-headless-win-amd64-1.2026.4.zip`
- SHA-256：`99ec178b41beb80a3d9b18dff43fa22c6211ae6578e9d1a8902ae277d33fd383`
- 主程序：`plantuml-headless.exe`

## 下载 / 校验 / 暂存

```powershell
powershell -ExecutionPolicy Bypass -File tools\plantuml-native\fetch.ps1
```

脚本会：

1. 下载 zip（3 次重试）到 `build\plantuml-native\cache\`；
2. 校验 SHA-256；
3. 解压并检查 `plantuml-headless.exe`；
4. 复制 exe + DLL + `LICENSE.plantuml` 到 `build\plantuml-native\staging\`；
5. 运行 `plantuml-headless.exe -version` 做冒烟测试。

成功后：

- 应用构建（CMake POST_BUILD）会把 staging 复制到 exe 旁的 `plantuml\`；
- NSIS / WiX 安装包从 staging 打入 `plantuml\`；
- 应用本地模式调用 `<exe>\plantuml\plantuml-headless.exe`。

## 失败行为

任何一步失败都会打印：

```text
PlantUML native download FAILED
Stage:        download | checksum | extract | smoke-test
Version:      ...
URL:          ...
Expected SHA: ...
Actual SHA:   ...
Cached zip:   ...
Log:          ...
Hint:         ... -ZipPath <本地zip>
```

并以退出码 1 结束。打包入口会在失败时停止，不会生成安装包；NSIS 脚本还会检查
`plantuml-headless.exe` 是否存在，WiX 在 `Source` 缺失时也会构建失败。

## 离线 / 镜像

```powershell
# 手动下载后指定本地 zip（仍校验 SHA-256）
tools\plantuml-native\fetch.ps1 -ZipPath D:\plantuml-headless.zip

# 或使用镜像
tools\plantuml-native\fetch.ps1 -Url https://mirror.example/plantuml-headless.zip

# 只校验缓存
tools\plantuml-native\fetch.ps1 -VerifyOnly

# 强制重新下载
tools\plantuml-native\fetch.ps1 -Force
```

环境变量 `AWM_PLANTUML_NATIVE_URL` 可替代 `-Url`。

## 许可证

PlantUML 采用 GPLv3 或更高版本。`LICENSE.plantuml` 是 GPLv3 全文，
`license.txt` 是上游项目声明（含第三方组件说明），两者都会随 staging/安装包
一起分发。分发本程序前请确认与项目自身许可证兼容。
