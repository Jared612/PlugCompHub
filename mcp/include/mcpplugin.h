/**
 * @file mcpplugin.h
 * @brief 插件侧的数据结构与宏：如何把插件信息、组件表导出给主程序。
 * @details 动态库插件用这里的宏生成标准入口，供框架加载与注册。
 */

#pragma once
#include <functional>
#include <string>
#include <atomic>
#include "mcpexport.h"


MCP_BEGIN_NAMESPACE

struct PluginInfo
{
	mcp::ComponentInfo** componentInfo; // 插件导出的组件信息表（以 nullptr 结尾）
	const char* name;					// 插件名称
	const char* version;				// 插件版本号
	const char* description;			// 插件描述信息（通常为键值对文本）
};

/******************************************************************************
 * 插件信息导出宏定义
 * 用于生成插件信息入口 pluginfo() 与初始化入口 pluginit()
 *****************************************************************************/
#ifdef __MCP_SYS_WINDOWS
#define MCP_PLUGIN_INFO_BEGIN(NAME, VERSION,tableName) \
extern mcp::ComponentInfo* tableName##_mcp_componentInfoTable[]; \
extern "C" __MCP_EXPORT mcp::PluginInfo* pluginfo() { \
	static mcp::PluginInfo pinfo = { \
			tableName##_mcp_componentInfoTable, \
			NAME, \
			VERSION, \
			"{" 
#else
#define MCP_PLUGIN_INFO_BEGIN_3(NAME, VERSION,tableName) \
extern __attribute__((visibility("hidden"))) mcp::ComponentInfo* tableName##_mcp_componentInfoTable[]; \
extern "C" __MCP_EXPORT mcp::PluginInfo* pluginfo() { \
	static mcp::PluginInfo pinfo = { \
			tableName##_mcp_componentInfoTable, \
			NAME, \
			VERSION, \
			"{" 

#define MCP_PLUGIN_INFO_BEGIN_2(NAME, VERSION) \
extern __attribute__((visibility("hidden"))) mcp::ComponentInfo* _mcp_componentInfoTable[]; \
extern "C" __MCP_EXPORT mcp::PluginInfo* pluginfo() { \
	static mcp::PluginInfo pinfo = { \
			_mcp_componentInfoTable, \
			NAME, \
			VERSION, \
			"{" 

#define MCP_PLUGIN_INFO_BEGIN(...) CONCAT(MCP_PLUGIN_INFO_BEGIN_,VARGS(__VA_ARGS__))(__VA_ARGS__)
#endif



/******************************************************************************
 * 插件描述拼接与收尾宏
 * - MCP_PLUGIN_INFO(key,value)：把键值对拼成 "key:value," 的描述片段。
 * - MCP_PLUGIN_INFO_END()：补齐描述结尾，返回 pinfo，并导出 pluginit。
 *   pluginit 会把宿主传入的 objManager 写入 mcp::api 单例中。
 *****************************************************************************/
#define MCP_PLUGIN_INFO(key,value) #key":"#value","
#define MCP_PLUGIN_INFO_END() "\"end\":0 }"};return &pinfo; }\
extern "C" __MCP_EXPORT void pluginit(void* objManager) { \
struct Priv:public mcp::api{static void set(void*p1){Priv*p=(Priv*)&get();p->_objectManager=(mcp::IObjectManager*)p1;}};Priv::set(objManager);}

 /******************************************************************************
  * 插件组件导出表宏定义
  * 用于构造组件信息数组并以 nullptr 作为结束标记
  *****************************************************************************/
#ifdef __MCP_SYS_WINDOWS
#define MCP_COMPONENT_EXPORT_TABLE_BEGIN(tableName) \
	mcp::ComponentInfo* tableName##_mcp_componentInfoTable[] = {
#else
#define MCP_COMPONENT_EXPORT_TABLE_BEGIN(tableName) \
	__attribute__((visibility("hidden"))) mcp::ComponentInfo* tableName##_mcp_componentInfoTable[] = {
#endif

#define MCP_COMPONENT_EXPORT_TABLE_ITEM(CLASSNAME) \
	(mcp::ComponentInfo*)&mcp::ComponentTmpl<CLASSNAME>::componentInfo,

#define MCP_COMPONENT_EXPORT_TABLE_END() nullptr };

#define MCP_GET_COMPONENT_TABLE(tableName) tableName##_mcp_componentInfoTable
#define EXTERN_NPC_COMPONENT_TABLE(tableName) extern mcp::ComponentInfo* tableName##_mcp_componentInfoTable[]

/******************************************************************************
 * 宏理解说明（针对本文件插件导出相关宏）
 * 1) MCP_COMPONENT_EXPORT_TABLE_BEGIN/ITEM/END：
 *    - 定义一个 mcp::ComponentInfo* 数组，数组项为各组件的 ComponentInfo。
 *    - 末尾强制追加 nullptr，供宿主按“空指针终止”方式遍历。
 *
 * 2) MCP_PLUGIN_INFO_BEGIN/.../END：
 *    - 生成 extern "C" 导出函数 pluginfo()，返回静态 PluginInfo。
 *    - PluginInfo 中保存组件表指针、插件名、版本、描述字符串。
 *    - MCP_PLUGIN_INFO_END 同时导出 pluginit(void*)，把宿主的 objectManager
 *      注入到 mcp::api 单例内部，供插件运行时访问宿主对象系统。
 *
 * 3) MCP_GET_COMPONENT_TABLE / EXTERN_NPC_COMPONENT_TABLE：
 *    - 前者用于按约定名字取得组件表符号；
 *    - 后者用于跨编译单元声明该组件表。
 *****************************************************************************/

/******************************************************************************
 * 宏展开等价示例（便于理解，不参与编译逻辑）
 *
 * 例如用户代码：
 *   MCP_COMPONENT_EXPORT_TABLE_BEGIN(demo)
 *   MCP_COMPONENT_EXPORT_TABLE_ITEM(MyComponent)
 *   MCP_COMPONENT_EXPORT_TABLE_ITEM(OtherComponent)
 *   MCP_COMPONENT_EXPORT_TABLE_END()
 *
 *   MCP_PLUGIN_INFO_BEGIN("DemoPlugin", "1.0.0", demo)
 *   MCP_PLUGIN_INFO(author, abc)
 *   MCP_PLUGIN_INFO(license, MIT)
 *   MCP_PLUGIN_INFO_END()
 *
 * 大致等价于：
 *
 *   mcp::ComponentInfo* demo_mcp_componentInfoTable[] = {
 *       (mcp::ComponentInfo*)&mcp::ComponentTmpl<MyComponent>::componentInfo,
 *       (mcp::ComponentInfo*)&mcp::ComponentTmpl<OtherComponent>::componentInfo,
 *       nullptr
 *   };
 *
 *   extern "C" __MCP_EXPORT mcp::PluginInfo* pluginfo() {
 *       static mcp::PluginInfo pinfo = {
 *           demo_mcp_componentInfoTable,
 *           "DemoPlugin",
 *           "1.0.0",
 *           "{" "author:abc," "license:MIT," "\"end\":0 }"
 *       };
 *       return &pinfo;
 *   }
 *
 *   extern "C" __MCP_EXPORT void pluginit(void* objManager) {
 *       struct Priv : public mcp::api {
 *           static void set(void* p1) {
 *               Priv* p = (Priv*)&get();
 *               p->_objectManager = (mcp::IObjectManager*)p1;
 *           }
 *       };
 *       Priv::set(objManager);
 *   }
 *****************************************************************************/
MCP_END_NAMESPACE
