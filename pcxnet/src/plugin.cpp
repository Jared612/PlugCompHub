#include "pcxcomponent.h"
#include "pcx.h"
#include "pcxplugin.h"

#include "pcxasiocontext.h"
#include "pcxhttpclient.h"
#include "pcxhttpserver.h"
#include "pcxwebsocketclient.h"
#include "pcxwebsocketserver.h"

PCX_REGISTER_COMPONENT(pcx::PcxAsioContext, "cpp.pcx.asiocontext")
PCX_REGISTER_COMPONENT(pcx::PcxHttpClient, "cpp.pcx.httpclient")
PCX_REGISTER_COMPONENT(pcx::PcxHttpServer, "cpp.pcx.httpserver")
PCX_REGISTER_COMPONENT(pcx::PcxWebSocketClient, "cpp.pcx.websocketclient")
PCX_REGISTER_COMPONENT(pcx::PcxWebSocketServer, "cpp.pcx.websocketserver")

PCX_PLUGIN_INFO_BEGIN("pcxnet", "0.1.0", pcxnet)
PCX_PLUGIN_INFO(desc, "Plucomx Plugin Component Platform network plugin")
PCX_PLUGIN_INFO_END()

PCX_COMPONENT_EXPORT_TABLE_BEGIN(pcxnet)
PCX_COMPONENT_EXPORT_TABLE_ITEM(pcx::PcxAsioContext)
PCX_COMPONENT_EXPORT_TABLE_ITEM(pcx::PcxHttpClient)
PCX_COMPONENT_EXPORT_TABLE_ITEM(pcx::PcxHttpServer)
PCX_COMPONENT_EXPORT_TABLE_ITEM(pcx::PcxWebSocketClient)
PCX_COMPONENT_EXPORT_TABLE_ITEM(pcx::PcxWebSocketServer)
PCX_COMPONENT_EXPORT_TABLE_END()
