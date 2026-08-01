# Changelog

## [0.1.0] - 2026-08-02

### 重构

- PCX 更名为 PCH / PlugCompHub，模块重组为 `core / network / logger / sqlite / threadpool / application`
- 移除整包 Boost，改用 cpp-httplib / spdlog / cpptoml / nlohmann / sqlite3 amalgamation（vendored）
- 新增 logger 文件日志插件

### 接口收口（P0）

- `IPluginManager` / `IComponentManager` / `ILoggerManager` / `IEnvironment` / `IPlugin` 并入公共 `interface.h`，删除内部头 `internal.h`
- 示例与测试只依赖公共接口；`core/src` 不再以 PUBLIC 方式暴露

### 工程化

- CMake：显式源文件列表、统一 `project()`、`find_package(PCH)` 安装包导出
- 新增 CTest 冒烟测试与 GitHub Actions CI 工作流
- 新增 README、第三方依赖声明；示例公共代码抽到 `example_common.h`
- 公共头集中到 `include/pch/<module>/`，新增伞头 `pch.h`，include 采用 `core/core.h` 等前缀写法
- 采用 MIT 许可证
