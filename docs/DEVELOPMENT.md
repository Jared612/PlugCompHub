# 开发规范（Development Guide）

本文定义 PlugCompHub（PCH）core 的工程契约：工具链与 ABI、对象生命周期、
并发模型和测试要求。新增插件或修改 core 时请先阅读本文。

## 1. 工具链与 ABI 契约

- 支持平台：Windows x64（MSVC）、Linux（GCC/Clang）、macOS（Clang）；标准为 C++14。
- **CRT 必须一致**：Windows 下默认 `PCH_USE_STATIC_CRT=ON`，所有配置统一使用
  静态 CRT（Release `/MT`、Debug `/MTd`）。宿主程序、core、所有插件必须使用
  相同的 CRT 模式构建；混用 `/MT` 与 `/MD` 会导致跨 DLL 的 `new/delete`、
  `std::string`、`std::function` 崩溃。
- 需要动态 CRT 时统一用 `cmake -DPCH_USE_STATIC_CRT=OFF` 配置整个工程，
  禁止只对单个插件切换。
- 跨 DLL 边界只应传递：内置类型、同一工具链下的纯虚接口指针、
  `const char*`（生命周期按接口注释约定）。
- **禁止在跨模块回调中按值返回 `std::string` 等持有堆内存的对象**：静态 CRT 下
  每个模块有独立堆，调用方分配的字符串由实现方释放会触发跨堆 free（调试堆下会
  死锁）。需要回传文本时使用"调用方提供输出缓冲区"的签名（如 `IHttpServer::RequestHandler`）。
- 公共接口（`include/pch/` 下的头文件）一旦发布：
  - 不得删除或修改已存在函数的签名；
  - 不得在已发布接口的类中新增虚函数（会破坏派生类 vtable 布局），
    需要扩展时升级版本号并新增接口。
- **ABI 版本校验**：跨 DLL 的结构体（如 `PluginInfo`）增删字段、接口类增删虚函数时，
  必须递增 `PCH_ABI_VERSION`；core 加载插件时会校验插件侧 ABI 版本，
  不匹配返回 `PCH_PLUGIN_ABI_MISMATCH` 并拒绝加载。
- 插件必须与 core 使用同一编译器系列、同一 C++ 标准、同一 CRT 模式构建。

## 2. 对象生命周期语义

- 对象创建后登记名称表、地址表、全局列表三份索引；`deleteObject` / `deleteObjectByName`
  会立即从索引中移除对象（对 `FindObject` 立刻不可见）。
- 对象正在被消息分发时（in-use 计数 > 0），删除请求会被标记为"待删除"，
  实际销毁延迟到最后一个使用者 `releaseObject` 之后——同步消息分发、异步工作线程、
  初始化消息分发均已接入该保护。
- 框架退出（`tearDown`）会强制销毁所有对象，包括等待延迟删除的对象；
  因此正常退出路径不需要业务侧逐个删除。
- 接口返回的指针（如 `IEnvironment::get`、`IObjectManager::getObjectName`）
  指向内部存储，仅保证调用线程内、下一次修改/删除前有效；需要长期持有请自行拷贝。
- `CreateObject` / `CreateNamedObject` 的 `initMsg` 只用于初始化投递，
  由**调用方负责释放**（消息中心不接管所有权）；这与 `sendMessage` 的
  "消息中心负责释放"语义不同，使用时注意区分。
- `Initialize` 幂等：core 侧和宿主侧 `api::Initialize` 对重复初始化都返回
  `PCH_SUCCESS`；`Terminate` 之后可以再次 `Initialize`。

## 3. 并发模型

- 对象管理器的三张索引表由一把 `std::recursive_mutex` 保护，读改写在同一临界区完成。
- 消息中心：单一工作线程消费异步队列；异步路径按对象名二次解析，
  同步/异步分发均持有 in-use 计数，删除操作不会打断正在进行的分发。
- 日志全局入口 `WriteLog` 使用 `std::atomic` 指针，日志器析构与写日志并发安全。
- 宿主侧 `Initialize` / `Terminate` 以及插件卸载（`unloadPlugin`）要求调用方串行化，
  框架未实现自动引用计数。
- 网络插件（pchnetwork）的 Asio 上下文线程生命周期为进程级：建议在进程启动时加载、
  进程退出时随 CRT 收尾统一停止，**不要热卸载 pchnetwork**；优雅停机（会话登记 +
  取消 + join）规划在后续版本。

## 4. 测试

```bash
ctest --test-dir build -C Debug --output-on-failure
```

> 当前 `examples/` 与 `tests/` 已移除，等待重构回归；上述命令在无用例时不会失败
> （CI 已加 `--no-tests=ignore`）。修改 core 业务逻辑时，仍要求为新增行为补充对应测试。
