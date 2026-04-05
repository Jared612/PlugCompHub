cmake_minimum_required(VERSION 3.10)

add_executable(demo_mcplib main.cpp)

target_include_directories(demo_mcplib PRIVATE
	"${CMAKE_SOURCE_DIR}/mcpAppliction/mcpappliction"
)

target_compile_features(demo_mcplib PRIVATE cxx_std_14)

if(MSVC)
	target_compile_options(demo_mcplib PRIVATE /utf-8 /W4)
endif()

# mcp / imcpthreadpool 头由依赖目标传递；勿再叠加 mcplib 与源码 mcp/include，否则会重复包含。
target_link_libraries(demo_mcplib PRIVATE mcpAppliction mcpthreadpool)

add_custom_command(TARGET demo_mcplib POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${CMAKE_CURRENT_SOURCE_DIR}/demo.toml"
		"$<TARGET_FILE_DIR:demo_mcplib>/demo.toml"
	COMMENT "Copying demo.toml next to demo_mcplib.exe"
)

if(WIN32)
	add_custom_command(TARGET demo_mcplib POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"$<TARGET_FILE:mcp>"
			"$<TARGET_FILE:mcpthreadpool>"
			"$<TARGET_FILE:mcpAppliction>"
			"$<TARGET_FILE_DIR:demo_mcplib>"
		COMMENT "Copying MCP DLLs next to demo_mcplib.exe"
	)
endif()
