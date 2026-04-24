#include "pcxthreadpool.h"
#include "internal.h"
#include "pcx.h"
#include "pcxcomponent.h"

PCX_REGISTER_COMPONENT(pcx::PcxThreadPool, "cpp.pcx.threadpool")

PCX_PLUGIN_INFO_BEGIN("pcxthreadpool", "0.1.0", pcxthreadpool)
PCX_PLUGIN_INFO(desc, "Plucomx Plugin Component Platform thread pool plugin")
PCX_PLUGIN_INFO_END()

PCX_COMPONENT_EXPORT_TABLE_BEGIN(pcxthreadpool)
PCX_COMPONENT_EXPORT_TABLE_ITEM(pcx::PcxThreadPool)
PCX_COMPONENT_EXPORT_TABLE_END()
