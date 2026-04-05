#include "mcpthreadpool.h"
#include "internal.h"
#include "mcp.h"
#include "mcpcomponent.h"

MCP_REGISTER_COMPONENT(mcp::McpThreadPool, "cpp.mcp.mcpthreadpool")

MCP_PLUGIN_INFO_BEGIN("mcpthreadpool", "0.1.0", mcpthreadpool)
MCP_PLUGIN_INFO(desc, "MCP thread pool plugin")
MCP_PLUGIN_INFO_END()

MCP_COMPONENT_EXPORT_TABLE_BEGIN(mcpthreadpool)
MCP_COMPONENT_EXPORT_TABLE_ITEM(mcp::McpThreadPool)
MCP_COMPONENT_EXPORT_TABLE_END()
