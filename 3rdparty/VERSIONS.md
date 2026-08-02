# Third-Party Versions（vendored 依赖锁定）

本目录下所有第三方组件均为源码树内 vendored，版本锁定如下：

| 组件 | 版本 | 形态 | 许可/版权 |
|---|---|---|---|
| Boost（裁剪头文件版） | 1.90.0 | `boost/`（asio/beast/system 等模块） | BSL-1.0 |
| cpptoml | 未发布版本标记 | 单头文件 `cpptoml/cpptoml.h` | MIT，Chase Geigle；建议后续锁定上游 commit |
| nlohmann/json | 3.9.1 | 单头文件 `nlohmann/json.hpp` | MIT，Niels Lohmann |
| spdlog | 1.14.1 | 完整源码 `spdlog/` | MIT，Gabi Melman；vendored 自 commit `27cb4c7` |
| SQLite | 3.46.1 | amalgamation `sqlite3/` | Public Domain（作者声明） |

升级任何依赖时，请同步更新本表，并在 CHANGELOG 中记录版本变化。
