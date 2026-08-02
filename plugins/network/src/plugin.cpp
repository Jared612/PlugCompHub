#include "core/componentinfo.h"
#include "core/core.h"
#include "core/plugininfo.h"

#include "httpclient.h"
#include "websocketclient.h"
#include "httpserver.h"
#include "websocketserver.h"

PCH_REGISTER_COMPONENT(pch::HttpClient, "cpp.pch.httpclient")
PCH_REGISTER_COMPONENT(pch::WebSocketClient, "cpp.pch.websocketclient")
PCH_REGISTER_COMPONENT(pch::HttpServer, "cpp.pch.httpserver")
PCH_REGISTER_COMPONENT(pch::WebSocketServer, "cpp.pch.websocketserver")

PCH_PLUGIN_INFO_BEGIN("pchnetwork", "0.1.0", pchnetwork)
PCH_PLUGIN_INFO(desc, "PlugCompHub network plugin (Boost.Asio/Beast backend)")
PCH_PLUGIN_INFO_END()

PCH_COMPONENT_EXPORT_TABLE_BEGIN(pchnetwork)
PCH_COMPONENT_EXPORT_TABLE_ITEM(pch::HttpClient)
PCH_COMPONENT_EXPORT_TABLE_ITEM(pch::WebSocketClient)
PCH_COMPONENT_EXPORT_TABLE_ITEM(pch::HttpServer)
PCH_COMPONENT_EXPORT_TABLE_ITEM(pch::WebSocketServer)
PCH_COMPONENT_EXPORT_TABLE_END()
