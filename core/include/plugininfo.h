/**
 * @file plugininfo.h
 * @brief 插件侧数据结构和宏：如何向主机程序导出插件信息和组件表
 * @details 动态库插件使用此处的宏生成标准入口点，用于框架加载和注册
 */

#pragma once
#include <functional>
#include <string>
#include <atomic>
#include "coreexport.h"


PCH_BEGIN_NAMESPACE

struct PluginInfo
{
	pch::ComponentInfo** componentInfo; // 插件导出的组件信息表（以 nullptr 结尾）
	const char* name;					// 插件名称
	const char* version;				// 插件版本
	const char* description;			// 插件描述信息（通常为键值对文本）
};

/******************************************************************************
 * 插件信息导出宏定义
 * 用于生成插件信息入口 pluginfo() 和初始化入口 pluginit()
 *****************************************************************************/
#ifdef __PCH_SYS_WINDOWS
#define PCH_PLUGIN_INFO_BEGIN_3(NAME, VERSION,tableName) \
extern pch::ComponentInfo* tableName##_PCH_componentInfoTable[]; \
extern "C" __PCH_EXPORT pch::PluginInfo* pluginfo() { \
	static pch::PluginInfo pinfo = { \
			tableName##_PCH_componentInfoTable, \
			NAME, \
			VERSION, \
			"{" 

#define PCH_PLUGIN_INFO_BEGIN_2(NAME, VERSION) \
extern pch::ComponentInfo* _PCH_componentInfoTable[]; \
extern "C" __PCH_EXPORT pch::PluginInfo* pluginfo() { \
	static pch::PluginInfo pinfo = { \
			_PCH_componentInfoTable, \
			NAME, \
			VERSION, \
			"{" 

// 在 Windows 上也通过 VARGS 分发 2/3 参数，消除与 Linux/macOS 的平台差异
#define PCH_PLUGIN_INFO_BEGIN(...) PCH_EXPAND(CONCAT(PCH_PLUGIN_INFO_BEGIN_,VARGS(__VA_ARGS__))(__VA_ARGS__))
#else
#define PCH_PLUGIN_INFO_BEGIN_3(NAME, VERSION,tableName) \
extern __attribute__((visibility("hidden"))) pch::ComponentInfo* tableName##_PCH_componentInfoTable[]; \
extern "C" __PCH_EXPORT pch::PluginInfo* pluginfo() { \
	static pch::PluginInfo pinfo = { \
			tableName##_PCH_componentInfoTable, \
			NAME, \
			VERSION, \
			"{" 

#define PCH_PLUGIN_INFO_BEGIN_2(NAME, VERSION) \
extern __attribute__((visibility("hidden"))) pch::ComponentInfo* _PCH_componentInfoTable[]; \
extern "C" __PCH_EXPORT pch::PluginInfo* pluginfo() { \
	static pch::PluginInfo pinfo = { \
			_PCH_componentInfoTable, \
			NAME, \
			VERSION, \
			"{" 

#define PCH_PLUGIN_INFO_BEGIN(...) CONCAT(PCH_PLUGIN_INFO_BEGIN_,VARGS(__VA_ARGS__))(__VA_ARGS__)
#endif



/******************************************************************************
 * 插件描述拼接和结束宏
 * - PCH_PLUGIN_INFO(key,value)：拼接键值对为 "key:value," 的描述片段
 * - PCH_PLUGIN_INFO_END()：关闭描述，返回 pinfo，并导出 pluginit / pluginexit
 *   pluginit 将主机传入的 objManager 写入插件自身的 pch::api 单例
 *   pluginexit 由 PluginManager 在主机广播 SystemShutdown 时回调
 *              将同一 pch::api::_objectManager 置为 nullptr，防止插件
 *              在主机销毁 ObjectManager 后持有悬空指针
 *****************************************************************************/
#define PCH_PLUGIN_INFO(key,value) #key":"#value","
#define PCH_PLUGIN_INFO_END() "\"end\":0 }"};return &pinfo; }\
extern "C" __PCH_EXPORT void pluginit(void* objManager) { \
struct Priv:public pch::api{static void set(void*p1){Priv*p=(Priv*)&get();p->_objectManager=(pch::IObjectManager*)p1;}};Priv::set(objManager);}\
extern "C" __PCH_EXPORT void pluginexit() { \
struct Priv:public pch::api{static void clear(){Priv*p=(Priv*)&get();p->_objectManager=nullptr;}};Priv::clear();}

 /******************************************************************************
  * 插件组件导出表宏定义
  * 用于构造以 nullptr 结尾的组件信息数组
  *****************************************************************************/
#ifdef __PCH_SYS_WINDOWS
#define PCH_COMPONENT_EXPORT_TABLE_BEGIN(tableName) \
	pch::ComponentInfo* tableName##_PCH_componentInfoTable[] = {
#else
#define PCH_COMPONENT_EXPORT_TABLE_BEGIN(tableName) \
	__attribute__((visibility("hidden"))) pch::ComponentInfo* tableName##_PCH_componentInfoTable[] = {
#endif

#define PCH_COMPONENT_EXPORT_TABLE_ITEM(CLASSNAME) \
	(pch::ComponentInfo*)&pch::ComponentTmpl<CLASSNAME>::componentInfo,

#define PCH_COMPONENT_EXPORT_TABLE_END() nullptr };

#define PCH_GET_COMPONENT_TABLE(tableName) tableName##_PCH_componentInfoTable
#define EXTERN_NPC_COMPONENT_TABLE(tableName) extern pch::ComponentInfo* tableName##_PCH_componentInfoTable[]

/******************************************************************************
 * 宏说明（针对本文件中的插件导出宏）
 * 1) PCH_COMPONENT_EXPORT_TABLE_BEGIN/ITEM/END：
 *    - 定义一个 pch::ComponentInfo* 数组，每一项为各组件 ComponentInfo
 *    - 末尾的 nullptr 是必需的，用于主机"以 nullptr 结尾"的遍历
 *
 * 2) PCH_PLUGIN_INFO_BEGIN/.../END：
 *    - 生成 extern "C" 导出函数 pluginfo()，返回静态 PluginInfo
 *    - PluginInfo 持有组件表指针、插件名称、版本、描述字符串
 *    - PCH_PLUGIN_INFO_END 同时导出 pluginit(void*)，将主机的 objectManager
 *      注入 pch::api 单例，供插件运行时访问主机对象系统
 *
 * 3) PCH_GET_COMPONENT_TABLE / EXTERN_NPC_COMPONENT_TABLE：
 *    - 前者按约定名称获取组件表符号
 *    - 后者跨编译单元声明组件表
 *****************************************************************************/

/******************************************************************************
 * 宏展开等价示例（用于理解，不参与编译）
 *
 * 示例用户代码：
 *   PCH_COMPONENT_EXPORT_TABLE_BEGIN(demo)
 *   PCH_COMPONENT_EXPORT_TABLE_ITEM(MyComponent)
 *   PCH_COMPONENT_EXPORT_TABLE_ITEM(OtherComponent)
 *   PCH_COMPONENT_EXPORT_TABLE_END()
 *
 *   PCH_PLUGIN_INFO_BEGIN("DemoPlugin", "1.0.0", demo)
 *   PCH_PLUGIN_INFO(author, abc)
 *   PCH_PLUGIN_INFO(license, MIT)
 *   PCH_PLUGIN_INFO_END()
 *
 * 大致等价于：
 *
 *   pch::ComponentInfo* demo_PCH_componentInfoTable[] = {
 *       (pch::ComponentInfo*)&pch::ComponentTmpl<MyComponent>::componentInfo,
 *       (pch::ComponentInfo*)&pch::ComponentTmpl<OtherComponent>::componentInfo,
 *       nullptr
 *   };
 *
 *   extern "C" __PCH_EXPORT pch::PluginInfo* pluginfo() {
 *       static pch::PluginInfo pinfo = {
 *           demo_PCH_componentInfoTable,
 *           "DemoPlugin",
 *           "1.0.0",
 *           "{" "author:abc," "license:MIT," "\"end\":0 }"
 *       };
 *       return &pinfo;
 *   }
 *
 *   extern "C" __PCH_EXPORT void pluginit(void* objManager) {
 *       struct Priv : public pch::api {
 *           static void set(void* p1) {
 *               Priv* p = (Priv*)&get();
 *               p->_objectManager = (pch::IObjectManager*)p1;
 *           }
 *       };
 *       Priv::set(objManager);
 *   }
 *****************************************************************************/
PCH_END_NAMESPACE
