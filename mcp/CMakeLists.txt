cmake_minimum_required(VERSION 3.10)

project(mcp)
add_definitions(-D__MCP_EXPORTS)

file(GLOB_RECURSE SOURCE *.cpp)
file(GLOB_RECURSE HEADER *.h)

set(SOURCE "${SOURCE}")

add_library(${PROJECT_NAME} SHARED ${SOURCE} ${HEADER})

# 构建期可见：源码根目录 + include
# 安装后仅暴露 include 目录
# （internal.h 不会被 install(FILES ...) 安装）
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

find_package(Threads)
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})

# 仅安装对外公共头文件（不安装 internal.h）。始终生成 install 规则，VS 才有 INSTALL 目标。
install(TARGETS ${PROJECT_NAME}
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)

install(FILES
	include/mcp.h
	include/interface.h
	include/error.h
	include/mcpcomponent.h
	include/mcpplugin.h
	include/mcpexport.h
	DESTINATION include
)
