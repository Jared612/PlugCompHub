/**
 * @file error.h
 * @brief 定义 PCX 各模块共用的整型错误码宏。
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

#define PCX_SUCCESS                           0    // 运行成功
#define PCX_FAILED                           -100  // 运行失败
#define PCX_LOADPLUGIN_FAILED                -101  // 加载插件失败
#define PCX_LOADCOM_FAILED                   -102  // 加载组件失败
#define PCX_OPENFILE_FAILED                  -103  // 打开配置文件失败
#define PCX_NULLPTR                          -200  // 空指针错误
#define PCX_OBJECT_NULLPTR                   -201  // 查找或获取对象为空，可能是对象名错误或对象未注册
#define PCX_COMPONENT_NULLPTR                -202  // 组件信息为空
#define PCX_PLUGIN_NULLPTR                   -203  // 插件为空，可能是插件加载失败
#define PCX_OBJMANAGER_NULLPTR               -204  // 对象管理器为空
#define PCX_COMMANAGER_NULLPTR               -205  // 组件管理器为空
#define PCX_PLUGMANAGER_NULLPTR              -206  // 插件管理器为空
#define PCX_PARAM_NULLPTR                    -207  // 函数调用参数错误（空指针）
#define PCX_NOTFOUND                         -300  // 未找到错误
#define PCX_OBJECT_NOTFOUND                  -301  // 对象未找到，可能是对象名错误或对象未创建
#define PCX_HANDLER_NOTFOUND                 -302  // 消息处理器未找到，可能未注册
#define PCX_PLUGIN_NOTFOUND                  -303  // 插件未找到，可能未注册
#define PCX_COMPONENT_NOTFOUND               -304  // 组件未找到，可能未注册
#define PCX_MESSAGEHANDLER_NOTFOUND          -305  // 组件未实现 `handleMessage` 方法
#define PCX_ADYEXIST                         -400  // 已存在错误
#define PCX_OBJECT_ADYEXIST                  -401  // 对象已存在，将返回 nullptr
#define PCX_PLUGIN_ADYEXIST                  -402  // 插件已存在
#define PCX_COMPONNET_ADYEXIST               -403  // 组件已存在
#define PCX_ERROR_ADYEXIST                   -404  // 错误码已存在
#define PCX_NOTALLOW                         -500  // 不允许错误
#define PCX_INVALID                          -600  // 无效错误
#define PCX_ERRCODE_INVAlID                  -601  // 无效错误码
#define PCX_FILE_INVALID                     -602  // 文件无效，无法打开或解析
#define PCX_PARAM_INVALID                    -603  // 调用函数时传入的参数无效，可能为空或为空字符串
#define PCX_COMPONENT_INVALID                -604  // 组件信息无效，可能其成员变量为空或为空字符串
#define PCX_BADADDR                          -700  // 地址错误
#define PCX_OUTOFMEM                         -800  // 内存不足错误
#define PCX_IOERROR                          -901  // IO 错误