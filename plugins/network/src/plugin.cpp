#include "core/componentinfo.h"
#include "core/core.h"
#include "core/plugininfo.h"

#include "httpclient.h"
#include "websocketclient.h"

PCH_REGISTER_COMPONENT(pch::HttpClient, "cpp.pch.httpclient")
PCH_REGISTER_COMPONENT(pch::WebSocketClient, "cpp.pch.websocketclient")

PCH_PLUGIN_INFO_BEGIN("pchnetwork", "0.1.0", pchnetwork)
PCH_PLUGIN_INFO(desc, "PlugCompHub network plugin (cpp-httplib backend)")
PCH_PLUGIN_INFO_END()

PCH_COMPONENT_EXPORT_TABLE_BEGIN(pchnetwork)
PCH_COMPONENT_EXPORT_TABLE_ITEM(pch::HttpClient)
PCH_COMPONENT_EXPORT_TABLE_ITEM(pch::WebSocketClient)
PCH_COMPONENT_EXPORT_TABLE_END()
