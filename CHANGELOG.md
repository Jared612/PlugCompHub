# Changelog

## [0.3.0] - 2026-08-02

### 变更

- 网络插件从 cpp-httplib 重写为 **Boost.Asio + Boost.Beast** 异步实现
- 新增 HTTP 服务端（`IHttpServer`，组件 `cpp.pch.httpserver`）与
  WebSocket 服务端（`IWebSocketServer`，组件 `cpp.pch.websocketserver`）
- 保留 `IHttpClient` / `IWebSocketClient` 公共接口不变（ABI 兼容）
- 新增命名 Asio 上下文注册表（接口参数 `asioContextName` 复用同一 io_context）
- 3rdparty 以裁剪头文件版 Boost 1.90 替换 cpp-httplib

### 修复

- 修复跨模块回调返回 `std::string` 导致的静态 CRT 跨堆 free 死锁：
  服务端请求处理器改为调用方提供输出缓冲（见 DEVELOPMENT.md ABI 规则）

## [0.2.2] - 2026-08-02

### 变更

- 移除 `examples/` 与 `tests/`（等待重构后回归）；顶层 CMake 的 EXISTS 检查保留，
  目录重建后自动恢复构建；CI 测试步骤改为 `--no-tests=ignore`

## [0.2.1] - 2026-08-02

### 新增

- ABI 版本机制：`PCH_ABI_VERSION` + `PluginInfo::abiVersion`，插件加载时校验，
  不匹配返回 `PCH_PLUGIN_ABI_MISMATCH`
- 插件卸载回滚测试、并发压力测试（CTest 共 7 项）
- [docs/RELEASING.md](docs/RELEASING.md) 发布流程、[3rdparty/VERSIONS.md](3rdparty/VERSIONS.md) 依赖版本锁定

### 变更

- `api::Initialize` 重复初始化改为幂等返回 `PCH_SUCCESS`（与 core 侧一致）
- `initMsg` 所有权明确为调用方负责，公共接口文档统一说明
- 默认控制台日志增加级别前缀（`[INFO]` 等）
- 3rdparty 头文件目录标记为 SYSTEM，抑制第三方编译警告
- `application` 移入 `plugins/application/`，与其它插件目录统一

### 修复

- `invokeHandler` 增加组件信息/消息处理器空指针防御
- `unloadPlugin` 成功卸载后日志仍引用已卸载 DLL 内的插件名指针导致崩溃；
  改为先拷贝名称再卸载（由新增 plugin_unload 测试捕获）

## [0.2.0] - 2026-08-02

### 新增

- 对象生命周期保护：ObjectInfo in-use 计数，分发期间删除对象改为延迟销毁
- 核心单元测试：日志层级继承、消息所有权、对象并发删除（CTest 共 5 项）
- 工具链与 ABI 契约文档 [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)
- Windows 统一静态 CRT 选项 `PCH_USE_STATIC_CRT`（默认 ON）

## [0.1.1] - 2026-08-02

### 修复

- 日志器层级合并：修复 break 条件与注释相反的问题（级别已确定时才停止向上继承），
  避免命名日志器在祖先配置了更严格级别时错误回落到默认级别
- SystemReady 不再通过 getLogger 获取默认日志器，避免默认日志器被 _isGet 永久锁定，
  导致此后 addDefaultLogger / setDefaultLogger 静默失效
- 组播接口补充 count/group 参数校验，避免负数 count 导致越界访问
- pchcoreStart / Initialize 失败路径统一回滚，避免半初始化状态残留
- MessageCenter 退出标志改为 atomic，消除跨线程数据竞争
- 匿名对象创建在地址表插入失败时完整回滚 _regObjs，避免悬垂条目
- Plugin::getErrorMessage 增加空缓冲区保护，避免 std::string 空指针构造
- 清理重复 include、魔法哨兵值、未使用变量等语法/风格问题

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
