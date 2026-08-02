# 发布流程（RELEASING）

## 发版检查清单

1. **版本号**：更新顶层 [CMakeLists.txt](../CMakeLists.txt) 的 `project(... VERSION x.y.z)`。
2. **ABI 版本**：如果本轮改动了公共接口布局（新增/删除虚函数、修改签名、
   `PluginInfo` 等跨 DLL 结构体字段），必须递增 [coreexport.h](../include/pch/core/coreexport.h)
   中的 `PCH_ABI_VERSION`；否则保持不变。
3. **CHANGELOG**：新增版本段，记录行为变化、修复、ABI 变化。
4. **全量验证**（必须全绿）：
   ```bash
   cmake -S . -B build -A x64
   cmake --build build --config Debug
   ctest --test-dir build -C Debug --output-on-failure
   cmake --install build --config Debug --prefix dist
   ```
5. **打标签**：
   ```bash
   git tag vX.Y.Z
   git push origin master --tags
   ```
6. （可选）在 GitHub 创建 Release，附上 CHANGELOG 摘要与安装包说明。

## 兼容性承诺

- 同一 ABI 版本内，已发布的公共接口不允许破坏性变更；
- 插件必须与 core 使用相同 `PCH_ABI_VERSION` 构建，否则拒绝加载；
- Windows 下 core、插件、宿主必须使用相同的 CRT 模式（默认静态 CRT）。
