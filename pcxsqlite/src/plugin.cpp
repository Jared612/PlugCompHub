#include "pcxcomponent.h"
#include "pcx.h"
#include "pcxplugin.h"

#include "pcxsqlite.h"
#include "pcxsqliteorm.h"

PCX_REGISTER_COMPONENT(pcx::PcxSqlite, "cpp.pcx.sqlite")
PCX_REGISTER_COMPONENT(pcx::PcxSqliteOrm, "cpp.pcx.sqliteorm")

PCX_PLUGIN_INFO_BEGIN("pcxsqlite", "0.1.0", pcxsqlite)
PCX_PLUGIN_INFO(desc, "Plucomx Plugin Component Platform sqlite plugin")
PCX_PLUGIN_INFO_END()

PCX_COMPONENT_EXPORT_TABLE_BEGIN(pcxsqlite)
PCX_COMPONENT_EXPORT_TABLE_ITEM(pcx::PcxSqlite)
PCX_COMPONENT_EXPORT_TABLE_ITEM(pcx::PcxSqliteOrm)
PCX_COMPONENT_EXPORT_TABLE_END()
