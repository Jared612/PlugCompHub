# PlugCompHub（PCH）

PlugCompHub（简称 PCH）是一个微内核式 C++ 插件组件平台：宿主进程加载 `pch` 核心动态库后，即可动态装载插件、注册组件、创建/查找对象，并通过消息中心进行同步/异步通信。

## 特性

- 插件运行时：`PluginManager` 动态加载/卸载插件，按 `pluginfo` / `pluginit` 约定解析导出符号
- 组件模型：`ComponentManager` 维护组件注册表，`ObjectManager` 管理具名/匿名对象生命周期
- 消息中心：同步 `sendMessage`、异步 `postMessage`、组播/广播
- 可选插件：网络（cpp-httplib HTTP/WebSocket 客户端）、SQLite（含 KV ORM）、线程池、文件日志（spdlog）、应用骨架（TOML 配置驱动）
- 公共接口只暴露 `pch::` 接口，第三方库类型不泄露到头文件
- CMake 安装包：`find_package(PCH)` 即可使用

## 目录结构

```text
PlugCompHub/
├── core/          # 核心运行时（pch）
├── plugins/       # 可选插件
│   ├── network/   #   网络插件（pchnetwork）
│   ├── logger/    #   文件日志插件（pchlogger）
│   ├── sqlite/    #   SQLite 插件（pchsqlite）
│   └── threadpool/#   线程池插件（pchthreadpool）
├── application/   # 应用骨架插件（pchapplication）
├── examples/      # 示例程序
├── tests/         # CTest 冒烟测试
├── docs/          # 功能规范与历史分析
└── 3rdparty/      # vendored 第三方依赖
```

## 构建

```bash
cmake -S . -B build -A x64        # Windows / Visual Studio
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Linux / macOS：

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

安装到 `dist/`：

```bash
cmake --install build --config Debug --prefix dist
```

## 快速开始

构建后直接运行 `build/bin/Debug/` 下的示例：

```bash
example_threadpool
example_sqlite
example_logger
example_net
example_application
```

业务集成方式：包含 `core.h`，调用 `pch::api::Initialize()` 启动内核，再通过 `pch::api::FindObject(PCH_DEFAULT_PLUGINMANAGER)` 加载插件、`pch::api::CreateNamedObject()` 创建对象。

## 开发约定

- 公共接口头文件放在各模块 `include/`，实现放在 `src/`；`src/` 不对外暴露
- 接口命名 `I*`，公共头文件名统一小写（`ihttpclient.h` 等）
- 组件 ID 统一 `cpp.pch.<component>`，插件库名统一 `pch<module>`
- 新增源文件需显式加入对应 `CMakeLists.txt`

## 文档

- [1.0 功能实现说明](docs/1.0功能实现.md)
- [历史分析文档](docs/analysis/)

## 许可证

待定（开源许可确认后将在此补充并添加 LICENSE 文件）。
