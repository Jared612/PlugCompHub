#include "filelogger.h"
#include "core.h"
#include "componentinfo.h"
#include "plugininfo.h"

PCH_REGISTER_COMPONENT(pch::FileLogger, "cpp.pch.filelogger")

PCH_PLUGIN_INFO_BEGIN("pchlogger", "0.1.0", pchlogger)
PCH_PLUGIN_INFO(desc, "PlugCompHub Plugin Component Platform file logger plugin")
PCH_PLUGIN_INFO_END()

PCH_COMPONENT_EXPORT_TABLE_BEGIN(pchlogger)
PCH_COMPONENT_EXPORT_TABLE_ITEM(pch::FileLogger)
PCH_COMPONENT_EXPORT_TABLE_END()
