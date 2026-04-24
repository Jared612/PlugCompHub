#include "pcxapplication.h"
#include "pcxcomponent.h"
#include "pcxplugin.h"

PCX_REGISTER_COMPONENT(pcx::PcxApplication, "cpp.pcx.application")

PCX_PLUGIN_INFO_BEGIN("pcxapplication", "1.0.0", pcxapplication)
PCX_PLUGIN_INFO(desc, "Plucomx Plugin Component Platform application framework plugin")
PCX_PLUGIN_INFO_END()

PCX_COMPONENT_EXPORT_TABLE_BEGIN(pcxapplication)
PCX_COMPONENT_EXPORT_TABLE_ITEM(pcx::PcxApplication)
PCX_COMPONENT_EXPORT_TABLE_END()
