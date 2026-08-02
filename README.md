# PlugCompHub（PCH）

PlugCompHub（简称 PCH）是一个微内核式 C++ 插件组件平台：宿主进程加载 `pch` 核心动态库后，即可动态装载插件、注册组件、创建/查找对象，并通过消息中心进行同步/异步通信。

## 特性

- 插件运行时：`PluginManager` 动态加载/卸载插件，按 `pluginfo` / `pluginit` 约定解析导出符号
- 组件模型：`ComponentManager` 维护组件注册表，`ObjectManager` 管理具名/匿名对象生命周期
- 消息中心：同步 `sendMessage`、异步 `postMessage`、组播/广播
- 可选插件：网络（cpp-httplib HTTP/WebSocket 客户端）、SQLite（含 KV ORM）、线程池、文件日志（spdlog）、应用骨架（TOML 配置驱动）
- 公共接口只暴露 `pch::` 接口，第三方库类型不泄露到头文件
- 公共头统一集中在 `include/pch/`，伞头 `pch.h` 可一键引入核心接口
- CMake 安装包：`find_package(PCH)` 即可使用

## 目录结构

```text
PlugCompHub/
├── include/pch/   # 统一公共头（core/ network/ logger/ sqlite/ threadpool/ application/）
├── core/          # 核心运行时（pch）
├── plugins/       # 可选插件
│   ├── network/   #   网络插件（pchnetwork）
│   ├── logger/    #   文件日志插件（pchlogger）
│   ├── sqlite/    #   SQLite 插件（pchsqlite）
│   ├── threadpool/#   线程池插件（pchthreadpool）
│   └── application/#  应用骨架插件（pchapplication）
├── docs/          # 功能规范与历史分析
└── 3rdparty/      # vendored 第三方依赖
```

## 构建

```bash
cmake -S . -B build -A x64        # Windows / Visual Studio
cmake --build build --config Debug
```

Linux / macOS：

```bash
cmake -S . -B build
cmake --build build
```

安装到 `dist/`：

```bash
cmake --install build --config Debug --prefix dist
```

Windows 默认统一使用静态 CRT（`PCH_USE_STATIC_CRT=ON`），保证 core、插件、宿主
跨 DLL 运行库一致；如需动态 CRT：`cmake -DPCH_USE_STATIC_CRT=OFF`。
完整的工具链/ABI、生命周期与并发契约见 [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)。

## 快速开始

业务集成方式：包含伞头 `pch.h`（或按需 `core/core.h`、`threadpool/ithreadpool.h` 等），调用 `pch::api::Initialize()` 启动内核，再通过 `pch::api::FindObject(PCH_DEFAULT_PLUGINMANAGER)` 加载插件、`pch::api::CreateNamedObject()` 创建对象：

```cpp
pch::api::Initialize("pch.dll");
auto* pm = static_cast<pch::IPluginManager*>(
    pch::api::FindObject(PCH_DEFAULT_PLUGINMANAGER));
pm->loadPlugin("pchthreadpool.dll");
auto* tp = static_cast<pch::IThreadPool*>(
    pch::api::CreateNamedObject("cpp.pch.threadpool", "my.tp"));
```

## 开发约定

- 公共接口头统一放在 `include/pch/<module>/`，实现放在各模块 `src/`；`src/` 不对外暴露
- 源码内包含公共头使用前缀写法，例如 `#include "core/core.h"`、`#include "network/ihttpclient.h"`
- 接口命名 `I*`，公共头文件名统一小写（`ihttpclient.h` 等）
- 组件 ID 统一 `cpp.pch.<component>`，插件库名统一 `pch<module>`
- 新增源文件需显式加入对应 `CMakeLists.txt`

## 文档

- [1.0 功能实现说明](docs/1.0功能实现.md)
- [历史分析文档](docs/analysis/)

## 许可证

[MIT License](LICENSE)
