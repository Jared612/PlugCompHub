# PCH 插件第三方库选型建议

## 文档范围

本文基于当前 PCH 仓库的实际架构，给出三类插件的第三方库选型建议：

- 网络插件
- 数据库插件
- 线程池插件

这些建议不是泛泛而谈，而是结合当前仓库现状得出的：

- 项目已经内置了 Boost
- 现有网络模块已经走的是 Asio/Beast 路线
- 现有 SQLite 模块已经直接集成了官方 SQLite 源码
- 整体运行时是插件化架构，对 ABI、对象生命周期、卸载行为都比较敏感

## 一句话结论

- 网络插件：优先 `Boost.Asio + Boost.Beast`
- 数据库插件：
  - SQLite：优先官方 `sqlite3` C API
  - PostgreSQL：优先官方 `libpq`，如需更现代的 C++ 写法，可在插件内部包一层 `libpqxx`
  - MySQL：优先官方 `MySQL Connector/C++`
- 线程池插件：
  - 只做简单线程池：优先自己实现，或者封装 `BS::thread_pool`
  - 需要任务图和依赖调度：优先 `Taskflow`

## 1. 网络插件

### 推荐方案

使用 `Boost.Asio + Boost.Beast`。

### 为什么这样选

这是最适合当前 PCH 代码库的方案。

- 仓库里已经带了 Boost。
- 当前 `network` 模块已经和这条技术路线一致。
- `Asio` 提供的是成熟的异步 I/O 模型。
- `Beast` 正好补齐 HTTP 和 WebSocket。
- 这套组合非常适合接入你的插件对象模型、消息机制和长生命周期运行时。

### 不建议作为主方案的选择

不建议把主网络插件底座切换成 `cpp-httplib`。

原因：

- `cpp-httplib` 上手非常轻量。
- 但它官方说明就是基于阻塞式 socket I/O。
- 它更适合小工具、轻量服务、演示程序。
- 不适合作为一个可扩展插件运行时的通用网络底座。

### 网络插件选型结论

- 主运行时网络能力：`Boost.Asio + Boost.Beast`
- 小型工具型功能：可以局部使用 `cpp-httplib`，但不应作为主网络插件基础

### 官方参考

- Boost.Asio: https://www.boost.org/library/latest/asio/
- Boost.Beast: https://www.boost.org/library/latest/beast/
- Standalone Asio 文档: https://think-async.com/Asio/Documentation.html
- cpp-httplib: https://github.com/yhirose/cpp-httplib

## 2. 数据库插件

### 总体建议

不要一开始就追求“一个 ORM 打天下”。

对你现在这套架构，更合理的做法是：

- 按数据库类型拆插件
- 每种数据库插件底层使用官方客户端库
- PCH 层只统一最薄的一层数据库抽象

建议统一的能力边界：

- 连接 / 断开
- 事务
- 预处理语句
- 参数绑定
- 结果集逐行遍历
- 错误码与错误信息

不要在公共头文件里暴露第三方库类型。

## 2.1 SQLite 插件

### 推荐方案

直接使用官方 `sqlite3` C API。

### 为什么这样选

- SQLite 是嵌入式、无服务进程、部署简单。
- 当前仓库已经是这个方向。
- 稳定、可控、容易和插件 DLL 一起分发。
- 在 ABI 边界上最干净，不会引入额外包装层复杂度。

### 可选做法

如果你希望插件内部代码更现代一点，可以在插件实现内部再包一层 C++ 封装，但插件公开接口仍然应该只暴露你自己的类型。

### 官方参考

- SQLite 文档: https://sqlite.org/docs.html
- SQLite 官方源码镜像: https://github.com/sqlite/sqlite

## 2.2 PostgreSQL 插件

### 推荐方案

底层优先使用官方 `libpq`。

如果你只是想让插件内部实现代码更现代一些，可以在插件内部再包一层 `libpqxx`。

### 为什么这样选

- `libpq` 是 PostgreSQL 官方客户端库。
- 功能最全，控制最直接。
- 官方文档里明确覆盖了异步命令处理、pipeline mode 等能力。
- 对插件这种强调兼容性、控制力、可维护性的架构更合适。

### `libpqxx` 的定位

`libpqxx` 更适合作为插件内部实现层，不适合作为你的公共 ABI。

### PostgreSQL 插件选型结论

- PCH 对外接口：只暴露你自己的数据库插件接口
- 插件内部实现：
  - 优先：`libpq`
  - 可选：`libpqxx`

### 官方参考

- PostgreSQL `libpq`: https://www.postgresql.org/docs/current/libpq.html
- libpqxx: https://github.com/jtv/libpqxx

## 2.3 MySQL 插件

### 推荐方案

使用官方 `MySQL Connector/C++`。

### 为什么这样选

- 它是 MySQL 官方 C++ 客户端库。
- 这是 MySQL 场景下最直接、最稳妥、最容易跟随上游演进的选择。
- 对 MySQL 私有特性、协议和版本变化的适配路径也最清晰。

### 官方参考

- MySQL Connector/C++ 文档: https://dev.mysql.com/doc/connector-cpp/8.0/en/
- MySQL Connector/C++ 仓库: https://github.com/mysql/mysql-connector-cpp

## 2.4 统一数据库访问库

### 可选方案

`SOCI`

### 建议

不建议作为这个项目的第一选择。

### 原因

- 它确实可以统一多个数据库后端。
- 但会明显增加构建复杂度。
- 还会把数据库差异隐藏在另一层抽象下面，调试和排障成本更高。
- 对插件卸载、ABI 清晰度、后端特性控制来说，并不是最优解。

只有当你的真实目标是“故意牺牲部分后端特性，换一层统一数据库 API”时，才适合考虑它。

### 参考

- SOCI: https://github.com/SOCI/soci

## 3. 线程池插件

### 第一建议

如果只是做一个基础线程池插件，优先考虑两种做法：

- 自己基于标准库实现
- 使用 `BS::thread_pool`

### 为什么优先考虑自己实现

你的 PCH 是插件化运行时，不是普通业务工程。

这类架构通常更关心：

- 生命周期是否可控
- 插件卸载是否安全
- ABI 是否稳定
- 线程退出和任务清空语义是否完全可控

如果只是基础线程池，自己用标准库实现往往是最稳的：

- `std::thread`
- `std::mutex`
- `std::condition_variable`
- `std::future`
- `std::packaged_task`

优点：

- ABI 完全在自己控制之下
- 停止、清队列、等待完成这些语义最容易和插件生命周期对齐
- 不会把第三方调度模型硬塞进 PCH 运行时

## 3.1 轻量线程池

### 推荐方案

`BS::thread_pool`

### 为什么这样选

- 轻量
- 接入简单
- 现代 C++
- 很适合“任务入队 + 等待完成”这类基础需求

### 参考

- BS thread pool: https://github.com/bshoshany/thread-pool

## 3.2 任务图 / 依赖调度

### 推荐方案

`Taskflow`

### 为什么这样选

只有在你明确需要下面这些能力时，才建议上 `Taskflow`：

- 任务图
- 依赖边
- work-stealing
- 结构化并行调度

它不是简单线程池，而是更高一层的执行模型。通常应该单独做成新的插件或单独的模块，而不是偷偷塞进一个最小线程池接口后面。

### 参考

- Taskflow: https://taskflow.github.io/taskflow/
- Taskflow Executor 文档: https://taskflow.github.io/taskflow/ExecuteTaskflow.html

## 4. 针对当前仓库的最合适组合

如果按当前仓库继续发展，最实际的组合是：

1. `network`
   - `Boost.Asio`
   - `Boost.Beast`
2. `sqlite`
   - 官方 `sqlite3`
3. 新增 `pchpgsql`
   - 官方 `libpq`
4. 新增 `pchmysql`
   - 官方 `MySQL Connector/C++`
5. `threadpool`
   - 第一阶段自己实现
   - 如果以后确实要任务图，再单独增加任务调度插件

## 5. 最重要的一条架构规则

不要把第三方库类型暴露到 PCH 插件公共头文件中。

公共头文件只应该暴露：

- PCH 自己的接口
- 简单配置结构
- PCH 错误码
- PCH 回调类型

第三方库类型只应存在于插件实现内部。

这样做的好处：

- ABI 更稳定
- 更容易替换实现
- 后续迁移成本更低
- 构建依赖边界更清晰

例如：

- 你以后把 PostgreSQL 插件内部从 `libpq` 改成 `libpqxx`，不需要改公共头文件
- 你以后把线程池内部从自研实现改成 `Taskflow`，也不应该影响插件使用方

## 6. 最终建议

如果目标是基于当前 PCH 架构，做一个实用、可维护、可扩展的插件体系，那么建议是：

- 网络继续使用 `Boost.Asio + Boost.Beast`
- 嵌入式数据库继续使用官方 `sqlite3`
- PostgreSQL 使用 `libpq`
- MySQL 使用 `MySQL Connector/C++`
- 线程池先保持简单、先掌握在自己手里

比“选哪个库”更重要的是下面这条：

- 第三方依赖必须藏在插件边界之后
- PCH 公共接口必须和具体厂商库解耦

