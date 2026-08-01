#include "application.h"
#include "componentinfo.h"
#include "plugininfo.h"

PCH_REGISTER_COMPONENT(pch::Application, "cpp.pch.application")

PCH_PLUGIN_INFO_BEGIN("pchapplication", "1.0.0", pchapplication)
PCH_PLUGIN_INFO(desc, "PlugCompHub 插件组件平台应用框架插件")
PCH_PLUGIN_INFO_END()

PCH_COMPONENT_EXPORT_TABLE_BEGIN(pchapplication)
PCH_COMPONENT_EXPORT_TABLE_ITEM(pch::Application)
PCH_COMPONENT_EXPORT_TABLE_END()
