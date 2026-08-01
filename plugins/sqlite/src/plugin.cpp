#include "core/componentinfo.h"
#include "core/core.h"
#include "core/plugininfo.h"

#include "sqlite.h"
#include "sqliteorm.h"

PCH_REGISTER_COMPONENT(pch::Sqlite, "cpp.pch.sqlite")
PCH_REGISTER_COMPONENT(pch::SqliteOrm, "cpp.pch.sqliteorm")

PCH_PLUGIN_INFO_BEGIN("pchsqlite", "0.1.0", pchsqlite)
PCH_PLUGIN_INFO(desc, "PlugCompHub Plugin Component Platform sqlite plugin")
PCH_PLUGIN_INFO_END()

PCH_COMPONENT_EXPORT_TABLE_BEGIN(pchsqlite)
PCH_COMPONENT_EXPORT_TABLE_ITEM(pch::Sqlite)
PCH_COMPONENT_EXPORT_TABLE_ITEM(pch::SqliteOrm)
PCH_COMPONENT_EXPORT_TABLE_END()
