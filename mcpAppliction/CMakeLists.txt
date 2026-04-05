cmake_minimum_required(VERSION 3.10)

project(mcpAppliction)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
	if(MSVC_VERSION GREATER 1800)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
	endif()
endif()

if(WIN32)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi /MT")
	set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
endif()

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
if(WIN32)
	set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
else()
	set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)
endif()

set(_mcp_app_sources
	mcpappliction/configtoml.cpp
	mcpappliction/mcpapplication.cpp
	mcpappliction/applicationmcp.cpp
	mcpappliction/plugin.cpp
)

add_library(mcpAppliction SHARED ${_mcp_app_sources})

target_compile_definitions(mcpAppliction PRIVATE MCP_APPLICATION_EXPORTS)

target_include_directories(mcpAppliction
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/mcpappliction
		${CMAKE_SOURCE_DIR}/3rdparty/cpptoml
)

target_link_libraries(mcpAppliction PUBLIC mcp)

find_package(Threads REQUIRED)
target_link_libraries(mcpAppliction PRIVATE Threads::Threads ${CMAKE_DL_LIBS})

set_target_properties(mcpAppliction PROPERTIES OUTPUT_NAME "mcpAppliction")

install(TARGETS mcpAppliction
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)
install(FILES
	include/imcpapplication.h
	DESTINATION include
)
