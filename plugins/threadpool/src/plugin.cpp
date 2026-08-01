#include "threadpool.h"
#include "core/interface.h"
#include "core/core.h"
#include "core/componentinfo.h"

PCH_REGISTER_COMPONENT(pch::ThreadPool, "cpp.pch.threadpool")

PCH_PLUGIN_INFO_BEGIN("pchthreadpool", "0.1.0", pchthreadpool)
PCH_PLUGIN_INFO(desc, "PlugCompHub Plugin Component Platform thread pool plugin")
PCH_PLUGIN_INFO_END()

PCH_COMPONENT_EXPORT_TABLE_BEGIN(pchthreadpool)
PCH_COMPONENT_EXPORT_TABLE_ITEM(pch::ThreadPool)
PCH_COMPONENT_EXPORT_TABLE_END()
