# Changelog

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
