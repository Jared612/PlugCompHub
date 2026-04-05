#include "mcpapplication.h"
#include "mcpcomponent.h"
#include "mcpplugin.h"

MCP_REGISTER_COMPONENT(mcp::McpApplication, "cpp.mcp.application")

MCP_PLUGIN_INFO_BEGIN("mcpapplication", "1.0.0", mcpapplication)
MCP_PLUGIN_INFO(desc, "MCP application framework plugin")
MCP_PLUGIN_INFO_END()

MCP_COMPONENT_EXPORT_TABLE_BEGIN(mcpapplication)
MCP_COMPONENT_EXPORT_TABLE_ITEM(mcp::McpApplication)
MCP_COMPONENT_EXPORT_TABLE_END()
