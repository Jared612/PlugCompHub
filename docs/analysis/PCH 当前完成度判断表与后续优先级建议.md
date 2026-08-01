# PCH 当前完成度判断表与后续优先级建议

## 说明

本文基于当前仓库代码实际情况，对 PCH 插件架构做三件事：

1. 判断现在已经完成了什么
2. 判断还缺什么
3. 给出后续开发优先级排序

本文不是泛泛的架构建议，而是结合当前仓库源码得出的结论。

---

## 一、总体判断

当前 PCH 项目已经不是“从零开始的架构草图”，而是已经具备了一个可继续演进的插件平台基础。

更准确地说，当前状态是：

- 核心插件运行时已经成形
- 组件注册、对象创建、插件装载、消息分发这些关键机制已经具备
- 网络、SQLite、线程池三类插件都已经有了可运行雏形
- 技术路线整体是对的

但同时也要明确：

- 现在更像“架构基础 + 功能验证版”
- 还没有完全进入“接口稳定 + 横向扩展 + 工程化收口”的阶段

所以结论不是“要不要重写”，而是：

- 不需要推翻
- 需要收口
- 需要补齐
- 需要按优先级继续扩展

---

## 二、当前已经完成了什么

## 2.1 核心运行时已经完成的部分

### 1. 插件运行时骨架已完成

你已经具备了插件平台最关键的几个核心管理器：

- `PluginManager`：负责插件加载、注册、卸载控制
- `ComponentManager`：负责组件注册表
- `ObjectManager`：负责对象创建、命名、查找、删除
- `MessageCenter`：负责同步/异步消息派发
- `LoggerManager`：负责日志
- `Environment`：负责环境变量访问

对应代码：

- [core/src/plugin/pluginManager.cpp](/d:/Projects/PlugCompHub/core/src/plugin/pluginManager.cpp:43)
- [core/src/component/componentManager.cpp](/d:/Projects/PlugCompHub/core/src/component/componentManager.cpp:28)
- [core/src/component/objectManager.cpp](/d:/Projects/PlugCompHub/core/src/component/objectManager.cpp:174)
- [core/src/messagecenter.cpp](/d:/Projects/PlugCompHub/core/src/messagecenter.cpp:166)
- [core/src/loggerManager.cpp](/d:/Projects/PlugCompHub/core/src/loggerManager.cpp:19)
- [core/src/environment.cpp](/d:/Projects/PlugCompHub/core/src/environment.cpp:15)

### 2. 核心装配流程已完成

`pchcoreStart()` 已经能够把核心组件装配起来，`pchcoreStop()` 已经具备相对明确的停止与释放顺序。

对应代码：

- [core/src/coreinit.cpp](/d:/Projects/PlugCompHub/core/src/coreinit.cpp:46)
- [core/src/coreinit.cpp](/d:/Projects/PlugCompHub/core/src/coreinit.cpp:152)

### 3. 宿主调用入口已完成

宿主通过 `pch::api` 访问运行时，这套入口已经成形：

- `Initialize`
- `CreateObject`
- `CreateNamedObject`
- `FindObject`
- `DeleteObject`
- `Terminate`

对应代码：

- [core/include/core.h](/d:/Projects/PlugCompHub/core/include/core.h:22)
- [core/src/coreapi.cpp](/d:/Projects/PlugCompHub/core/src/coreapi.cpp:51)
- [core/src/coreapi.cpp](/d:/Projects/PlugCompHub/core/src/coreapi.cpp:199)

### 4. 插件宏机制已完成

组件注册宏、插件导出宏、组件表导出机制都已经具备。

对应代码：

- [core/include/componentinfo.h](/d:/Projects/PlugCompHub/core/include/componentinfo.h:22)
- [core/include/componentinfo.h](/d:/Projects/PlugCompHub/core/include/componentinfo.h:125)
- [core/include/plugininfo.h](/d:/Projects/PlugCompHub/core/include/plugininfo.h:19)

---

## 2.2 已有插件能力

### 1. 网络插件已完成“第一阶段”

`network` 已经具备以下基础能力：

- Asio 事件循环对象
- HTTP Server
- HTTP Client
- WebSocket Server
- WebSocket Client

对应代码：

- [network/include/iasiocontext.h](/d:/Projects/PlugCompHub/network/include/iasiocontext.h:12)
- [network/include/ihttpserver.h](/d:/Projects/PlugCompHub/network/include/ihttpserver.h:50)
- [network/include/ihttpclient.h](/d:/Projects/PlugCompHub/network/include/ihttpclient.h:11)

这说明网络插件方向已经被验证，不需要再重新选底层路线。

### 2. SQLite 插件已完成“第一阶段”

`sqlite` 已经具备两类能力：

- 原始 SQLite 访问封装
- 简单 KV ORM 封装

对应代码：

- [sqlite/include/isqlite.h](/d:/Projects/PlugCompHub/sqlite/include/isqlite.h:14)
- [sqlite/include/isqliteorm.h](/d:/Projects/PlugCompHub/sqlite/include/isqliteorm.h:14)
- [sqlite/CMakeLists.txt](/d:/Projects/PlugCompHub/sqlite/CMakeLists.txt:12)

这说明嵌入式数据库插件这条路线已经被验证。

### 3. 线程池插件已完成“第一阶段”

`threadpool` 已经具备基础线程池能力：

- 启动线程
- 投递任务
- 等待所有任务完成
- 停止线程池
- 查询线程数和任务数

对应代码：

- [threadpool/include/ithreadpool.h](/d:/Projects/PlugCompHub/threadpool/include/ithreadpool.h:14)

这说明“插件化执行器对象”这条路径已经打通。

### 4. 应用层启动封装已完成基础版本

`application` 已经提供了基于配置驱动的统一启动/停止入口。

对应代码：

- [application/include/iapplication.h](/d:/Projects/PlugCompHub/application/include/iapplication.h:18)

这意味着你已经不只是有一个底层框架，而是已经开始具备“应用接入层”。

---

## 2.3 已经完成的工程判断

可以明确认为目前已经完成的，不只是代码文件数量，而是下面这些架构目标：

- 已经完成插件平台最小闭环
- 已经完成三类典型插件的首轮验证
- 已经完成宿主到插件再到对象的基本调用链
- 已经完成插件注册与对象管理的基本机制
- 已经具备继续扩展数据库和网络插件的基础

---

## 三、当前还缺什么

## 3.1 架构层还缺的内容

### 1. 公共接口边界还不够稳定

虽然你已经有 `ipch...` 接口，但仍然存在两个问题：

- 接口粒度还偏“功能能用”，还不是“长期稳定 ABI”
- 部分接口语义还不够硬，例如线程安全边界、停止后是否允许再次调用、异常传播规则

这不是“没有接口”，而是“接口还没有定型”。

### 2. 生命周期语义还缺统一文档和统一约束

目前代码里已经能看出你在修补生命周期问题，但这些语义还主要存在于实现代码里，不在统一规范里。

尤其需要统一的内容包括：

- 插件 `load/unload` 条件
- 对象 `create/delete` 责任边界
- `start/stop` 是否幂等
- 停止过程中未完成任务如何处理
- 异常是否允许跨插件接口传播

### 3. 插件能力分层还不够清晰

例如网络插件当前已经有 HTTP/WS，但未来边界还没完全定清：

- 只做 HTTP / WebSocket？
- 是否暴露 TCP / UDP？
- 是否纳入 SSL/TLS？
- 是否提供 timer 能力？

同样，线程池插件也存在边界问题：

- 只是简单线程池？
- 是否支持 future？
- 是否支持取消？
- 是否要升级成任务图执行器？

---

## 3.2 插件能力层还缺的内容

### 1. 缺 PostgreSQL 插件

当前只有 SQLite。

这只验证了“嵌入式数据库插件”路线，还没有验证“外部数据库客户端插件”路线。

建议新增：

- `pchpgsql`

### 2. 缺 MySQL 插件

如果项目目标是通用插件平台，而不是只做本地 SQLite，那么 MySQL 也是很自然的一条扩展线。

建议新增：

- `pchmysql`

### 3. 网络插件缺更完整的工程能力

当前 `network` 已经有基础功能，但未来如果面向实际业务，通常还会缺：

- TLS/SSL
- 超时控制
- 连接池或 session 管理
- 统一错误模型
- 更明确的路由匹配规则
- 更明确的回调线程语义

### 4. 线程池插件缺更明确的行为定义

当前接口够做 demo 和基础使用，但距离长期稳定能力还差一些约束：

- `exec` 在 `stop` 后如何表现
- `waitForAllDone` 与 `stop` 的关系
- 任务抛异常后如何处理
- 队列满载策略
- 是否支持返回值

---

## 3.3 工程化层还缺的内容

### 1. 缺系统化测试

仓库里已经有 example，但 example 不等于测试。

当前更像“示例驱动验证”，还不是“测试驱动保障”。

建议后续补：

- 核心运行时单元测试
- 插件装载/卸载测试
- 生命周期回归测试
- 并发安全测试
- 网络/数据库插件集成测试

### 2. 缺接口规范文档

你现在已经到了需要补“规范文档”的阶段，而不只是功能说明。

例如需要单独写清楚：

- 插件开发规范
- 插件导出规则
- 对象命名规则
- 消息 ownership 规则
- 卸载时行为约束

### 3. 缺版本化意识

目前接口还没有明显的版本治理设计。

后续如果插件越来越多，就会需要考虑：

- 公共接口版本号
- 插件兼容矩阵
- ABI 兼容策略

---

## 四、优先级怎么排

下面给的是我建议的优先级，不是按“最想做什么”，而是按“最能降低后续返工风险”来排。

## P0：先做接口收口

这是最高优先级。

### 目标

把现有三个插件的公共接口先收紧、定型：

- `network`
- `sqlite`
- `threadpool`

### 原因

如果公共接口没定稳，就继续加新插件，后面返工成本会很高。

### 要做的事

- 统一命名风格
- 明确线程安全语义
- 明确 `start/stop/open/close` 语义
- 明确异常处理与错误返回方式
- 禁止第三方类型泄露到公共头文件

---

## P1：补一个外部数据库插件

这是第二优先级。

### 推荐优先做

- `pchpgsql`

### 原因

你已经用 `sqlite` 验证了“本地嵌入式数据库插件”。

下一步最有价值的是验证：

- 外部数据库客户端
- 外部连接生命周期
- 网络数据库错误模型
- 配置驱动连接

这比继续丰富 SQLite 更有架构价值。

---

## P2：增强网络插件工程能力

这是第三优先级。

### 建议优先补的方向

- TLS/SSL
- 超时控制
- 更明确的错误模型
- 更明确的回调线程语义

### 原因

当前 `network` 已经证明“能做”，下一步要证明“能稳定用于实际功能”。

---

## P3：完善线程池插件行为定义

这是第四优先级。

### 要重点补的内容

- 停止语义
- 等待语义
- 任务异常处理
- 任务返回值支持策略

### 原因

线程池当前已经够验证架构，但还不够作为长期基础组件。

---

## P4：补测试与规范文档

这是第五优先级，但不能一直拖。

### 需要补的内容

- 自动化测试
- 插件开发规范
- 生命周期文档
- 错误处理文档

### 原因

插件数量一旦增长，没有规范和测试，后续维护成本会快速上升。

---

## 五、推荐开发顺序

如果按当前项目实际情况继续推进，我建议的顺序是：

1. 收口现有接口
2. 明确生命周期规则
3. 新增 `pchpgsql`
4. 增强 `network` 工程能力
5. 完善 `threadpool`
6. 最后补系统测试和插件规范文档

---

## 六、最终结论

当前 PCH 项目已经完成了最关键的一步：

- 它不是“只有想法”
- 也不是“只有 demo”
- 而是已经有了一个可继续扩展的插件平台基础

现在项目的主要矛盾已经不是：

- 选什么库
- 要不要推翻

而是：

- 现有接口怎么收口
- 生命周期怎么定规矩
- 下一类插件先补哪一类

简化成一句话：

当前代码已经满足“继续往前做”的条件，下一步重点应从“验证可行性”切换到“稳定接口、补齐能力、控制返工”。

