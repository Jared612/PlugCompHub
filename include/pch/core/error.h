/**
 * @file error.h
 * @brief 定义 PCH 各模块共用的整型错误码宏。
 * @details 接口返回、参数校验、日志等场景统一用这些常量表达成败与原因。
 */

#pragma once
/******************************************************************************
 * 错误码定义说明
 * 001  - SUCCESS（成功）
 * 1**  - FAILED（失败）
 * 2**  - NULLPTR（空指针）
 * 3**  - NOTFOUND（未找到）
 * 4**  - ADYEXIST（已存在）
 * 5**  - NOTALLOW（不允许）
 * 6**  - INVALID（无效）
 * 7**  - BADADDR（地址错误）
 * 8**  - OUTOFMEM（内存不足）
 * 9**  - IOERROR（IO 错误）
 *****************************************************************************/

#define PCH_SUCCESS                           0    // 运行成功
#define PCH_FAILED                           -100  // 运行失败
#define PCH_LOADPLUGIN_FAILED                -101  // 加载插件失败
#define PCH_LOADCOM_FAILED                   -102  // 加载组件失败
#define PCH_OPENFILE_FAILED                  -103  // 打开配置文件失败
#define PCH_NULLPTR                          -200  // 空指针错误
#define PCH_OBJECT_NULLPTR                   -201  // 查找或获取对象为空，可能是对象名错误或对象未注册
#define PCH_COMPONENT_NULLPTR                -202  // 组件信息为空
#define PCH_PLUGIN_NULLPTR                   -203  // 插件为空，可能是插件加载失败
#define PCH_OBJMANAGER_NULLPTR               -204  // 对象管理器为空
#define PCH_COMMANAGER_NULLPTR               -205  // 组件管理器为空
#define PCH_PLUGMANAGER_NULLPTR              -206  // 插件管理器为空
#define PCH_PARAM_NULLPTR                    -207  // 函数调用参数错误（空指针）
#define PCH_NOTFOUND                         -300  // 未找到错误
#define PCH_OBJECT_NOTFOUND                  -301  // 对象未找到，可能是对象名错误或对象未创建
#define PCH_HANDLER_NOTFOUND                 -302  // 消息处理器未找到，可能未注册
#define PCH_PLUGIN_NOTFOUND                  -303  // 插件未找到，可能未注册
#define PCH_COMPONENT_NOTFOUND               -304  // 组件未找到，可能未注册
#define PCH_MESSAGEHANDLER_NOTFOUND          -305  // 组件未实现 `handleMessage` 方法
#define PCH_ADYEXIST                         -400  // 已存在错误
#define PCH_OBJECT_ADYEXIST                  -401  // 对象已存在，将返回 nullptr
#define PCH_PLUGIN_ADYEXIST                  -402  // 插件已存在
#define PCH_COMPONNET_ADYEXIST               -403  // 组件已存在
#define PCH_ERROR_ADYEXIST                   -404  // 错误码已存在
#define PCH_NOTALLOW                         -500  // 不允许错误
#define PCH_INVALID                          -600  // 无效错误
#define PCH_ERRCODE_INVAlID                  -601  // 无效错误码
#define PCH_FILE_INVALID                     -602  // 文件无效，无法打开或解析
#define PCH_PARAM_INVALID                    -603  // 调用函数时传入的参数无效，可能为空或为空字符串
#define PCH_COMPONENT_INVALID                -604  // 组件信息无效，可能其成员变量为空或为空字符串
#define PCH_BADADDR                          -700  // 地址错误
#define PCH_OUTOFMEM                         -800  // 内存不足错误
#define PCH_IOERROR                          -901  // IO 错误