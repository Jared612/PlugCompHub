/**
 * @file pcxplugin.h
 * @brief 插件侧的数据结构与宏：如何把插件信息、组件表导出给主程序。
 * @details 动态库插件用这里的宏生成标准入口，供框架加载与注册。
 */

#pragma once
#include <functional>
#include <string>
#include <atomic>
#include "pcxexport.h"


PCX_BEGIN_NAMESPACE

struct PluginInfo
{
	pcx::ComponentInfo** componentInfo; // 插件导出的组件信息表（以 nullptr 结尾）
	const char* name;					// 插件名称
	const char* version;				// 插件版本号
	const char* description;			// 插件描述信息（通常为键值对文本）
};

/******************************************************************************
 * 插件信息导出宏定义
 * 用于生成插件信息入口 pluginfo() 与初始化入口 pluginit()
 *****************************************************************************/
#ifdef __PCX_SYS_WINDOWS
#define PCX_PLUGIN_INFO_BEGIN(NAME, VERSION,tableName) \
extern pcx::ComponentInfo* tableName##_pcx_componentInfoTable[]; \
extern "C" __PCX_EXPORT pcx::PluginInfo* pluginfo() { \
	static pcx::PluginInfo pinfo = { \
			tableName##_pcx_componentInfoTable, \
			NAME, \
			VERSION, \
			"{" 
#else
#define PCX_PLUGIN_INFO_BEGIN_3(NAME, VERSION,tableName) \
extern __attribute__((visibility("hidden"))) pcx::ComponentInfo* tableName##_pcx_componentInfoTable[]; \
extern "C" __PCX_EXPORT pcx::PluginInfo* pluginfo() { \
	static pcx::PluginInfo pinfo = { \
			tableName##_pcx_componentInfoTable, \
			NAME, \
			VERSION, \
			"{" 

#define PCX_PLUGIN_INFO_BEGIN_2(NAME, VERSION) \
extern __attribute__((visibility("hidden"))) pcx::ComponentInfo* _pcx_componentInfoTable[]; \
extern "C" __PCX_EXPORT pcx::PluginInfo* pluginfo() { \
	static pcx::PluginInfo pinfo = { \
			_pcx_componentInfoTable, \
			NAME, \
			VERSION, \
			"{" 

#define PCX_PLUGIN_INFO_BEGIN(...) CONCAT(PCX_PLUGIN_INFO_BEGIN_,VARGS(__VA_ARGS__))(__VA_ARGS__)
#endif



/******************************************************************************
 * 插件描述拼接与收尾宏
 * - PCX_PLUGIN_INFO(key,value)：把键值对拼成 "key:value," 的描述片段。
 * - PCX_PLUGIN_INFO_END()：补齐描述结尾，返回 pinfo，并导出 pluginit / pluginexit。
 *   pluginit  把宿主传入的 objManager 写入插件自己这份 `pcx::api` 单例；
 *   pluginexit 在宿主广播 `SystemShutdown` 时由 `PluginManager` 反向调用，
 *             把同一份 `pcx::api::_objectManager` 置回 nullptr，避免宿主销毁
 *             `ObjectManager` 之后插件仍持有悬挂指针。
 *****************************************************************************/
#define PCX_PLUGIN_INFO(key,value) #key":"#value","
#define PCX_PLUGIN_INFO_END() "\"end\":0 }"};return &pinfo; }\
extern "C" __PCX_EXPORT void pluginit(void* objManager) { \
struct Priv:public pcx::api{static void set(void*p1){Priv*p=(Priv*)&get();p->_objectManager=(pcx::IObjectManager*)p1;}};Priv::set(objManager);}\
extern "C" __PCX_EXPORT void pluginexit() { \
struct Priv:public pcx::api{static void clear(){Priv*p=(Priv*)&get();p->_objectManager=nullptr;}};Priv::clear();}

 /******************************************************************************
  * 插件组件导出表宏定义
  * 用于构造组件信息数组并以 nullptr 作为结束标记
  *****************************************************************************/
#ifdef __PCX_SYS_WINDOWS
#define PCX_COMPONENT_EXPORT_TABLE_BEGIN(tableName) \
	pcx::ComponentInfo* tableName##_pcx_componentInfoTable[] = {
#else
#define PCX_COMPONENT_EXPORT_TABLE_BEGIN(tableName) \
	__attribute__((visibility("hidden"))) pcx::ComponentInfo* tableName##_pcx_componentInfoTable[] = {
#endif

#define PCX_COMPONENT_EXPORT_TABLE_ITEM(CLASSNAME) \
	(pcx::ComponentInfo*)&pcx::ComponentTmpl<CLASSNAME>::componentInfo,

#define PCX_COMPONENT_EXPORT_TABLE_END() nullptr };

#define PCX_GET_COMPONENT_TABLE(tableName) tableName##_pcx_componentInfoTable
#define EXTERN_NPC_COMPONENT_TABLE(tableName) extern pcx::ComponentInfo* tableName##_pcx_componentInfoTable[]

/******************************************************************************
 * 宏理解说明（针对本文件插件导出相关宏）
 * 1) PCX_COMPONENT_EXPORT_TABLE_BEGIN/ITEM/END：
 *    - 定义一个 pcx::ComponentInfo* 数组，数组项为各组件的 ComponentInfo。
 *    - 末尾强制追加 nullptr，供宿主按“空指针终止”方式遍历。
 *
 * 2) PCX_PLUGIN_INFO_BEGIN/.../END：
 *    - 生成 extern "C" 导出函数 pluginfo()，返回静态 PluginInfo。
 *    - PluginInfo 中保存组件表指针、插件名、版本、描述字符串。
 *    - PCX_PLUGIN_INFO_END 同时导出 pluginit(void*)，把宿主的 objectManager
 *      注入到 pcx::api 单例内部，供插件运行时访问宿主对象系统。
 *
 * 3) PCX_GET_COMPONENT_TABLE / EXTERN_NPC_COMPONENT_TABLE：
 *    - 前者用于按约定名字取得组件表符号；
 *    - 后者用于跨编译单元声明该组件表。
 *****************************************************************************/

/******************************************************************************
 * 宏展开等价示例（便于理解，不参与编译逻辑）
 *
 * 例如用户代码：
 *   PCX_COMPONENT_EXPORT_TABLE_BEGIN(demo)
 *   PCX_COMPONENT_EXPORT_TABLE_ITEM(MyComponent)
 *   PCX_COMPONENT_EXPORT_TABLE_ITEM(OtherComponent)
 *   PCX_COMPONENT_EXPORT_TABLE_END()
 *
 *   PCX_PLUGIN_INFO_BEGIN("DemoPlugin", "1.0.0", demo)
 *   PCX_PLUGIN_INFO(author, abc)
 *   PCX_PLUGIN_INFO(license, MIT)
 *   PCX_PLUGIN_INFO_END()
 *
 * 大致等价于：
 *
 *   pcx::ComponentInfo* demo_pcx_componentInfoTable[] = {
 *       (pcx::ComponentInfo*)&pcx::ComponentTmpl<MyComponent>::componentInfo,
 *       (pcx::ComponentInfo*)&pcx::ComponentTmpl<OtherComponent>::componentInfo,
 *       nullptr
 *   };
 *
 *   extern "C" __PCX_EXPORT pcx::PluginInfo* pluginfo() {
 *       static pcx::PluginInfo pinfo = {
 *           demo_pcx_componentInfoTable,
 *           "DemoPlugin",
 *           "1.0.0",
 *           "{" "author:abc," "license:MIT," "\"end\":0 }"
 *       };
 *       return &pinfo;
 *   }
 *
 *   extern "C" __PCX_EXPORT void pluginit(void* objManager) {
 *       struct Priv : public pcx::api {
 *           static void set(void* p1) {
 *               Priv* p = (Priv*)&get();
 *               p->_objectManager = (pcx::IObjectManager*)p1;
 *           }
 *       };
 *       Priv::set(objManager);
 *   }
 *****************************************************************************/
PCX_END_NAMESPACE
