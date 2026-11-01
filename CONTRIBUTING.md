# 贡献指南（Contributing）

## 分支模型

- `main`：稳定/发布分支，始终可构建、可发布。只通过 PR 从 `dev` 合入（或热修复）。
- `dev`：开发集成分支，日常开发都汇入这里。
- 功能/修复分支：`feature/<名称>`、`fix/<名称>`，从 `dev` 切出，完成后 PR 合回 `dev`。

```
main  ←(PR)←  dev  ←(PR)←  feature/xxx
                            fix/yyy
```

## 开发流程

```bash
# 1. 从最新的 dev 切功能分支
git checkout dev
git pull
git checkout -b feature/xxx

# 2. 开发 + 本地验证
cmake --build --preset vs2022 --config Release
ctest --preset vs2022

# 3. 提交并推送
git add -A
git commit -m "feat: <描述>"
git push -u origin feature/xxx

# 4. 在 GitHub 上发起 PR：feature/xxx → dev，等 CI 通过后合入
```

## 发布流程

1. `dev` 稳定后，发起 PR：`dev` → `main`，CI 通过后合入。
2. 在 `main` 上打版本 tag 并推送：

   ```bash
   git checkout main
   git pull
   git tag v0.1.0
   git push origin v0.1.0
   ```

3. GitHub Actions 自动构建并生成 WiX MSI 与 NSIS Setup，附到对应 Release。

## 提交规范

建议使用 Conventional Commits 格式：`<type>: <描述>`

| type | 含义 |
|------|------|
| feat | 新功能 |
| fix | 修复 |
| docs | 文档 |
| refactor | 重构 |
| test | 测试 |
| build | 构建/依赖（CMake、vcpkg） |
| ci | CI/CD（GitHub Actions） |

## 分支保护（仓库管理员在 GitHub 设置）

建议在 Settings → Branches 里对 `main`（和 `dev`）开启：

- Require a pull request before merging
- Require status checks to pass（勾选 CI）
- 禁止直接 push（Allow force push 关闭）

## 本地构建与安装包

见 [README.md](README.md) 的「构建」与「本地生成安装包」。
