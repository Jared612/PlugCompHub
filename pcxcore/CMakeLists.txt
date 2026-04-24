# PCX 核心：`pcx` 共享库，导出 Initialize/Terminate 等符号供宿主与插件约定加载
cmake_minimum_required(VERSION 3.10)

project(pcx)
# 与 pcxexport.h 中 __PCX_EXPORTS 分支配合，生成 dllexport 而非 dllimport
add_definitions(-D__PCX_EXPORTS)

# 收集实现与头文件；新增 src 下源文件会自动参与构建
file(GLOB CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB CORE_HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

add_library(${PROJECT_NAME} SHARED ${CORE_SOURCES} ${CORE_HEADERS})
# 目标文件名：Windows 为 pcx.dll，Unix 为 libpcx.so / libpcx.dylib

# 构建期可见：源码根目录 + include
# 安装后仅暴露 include 目录
# （internal.h 不会被 install(FILES ...) 安装）
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# MessageCenter 等使用 std::thread；插件加载在非 Windows 上依赖 dlopen
find_package(Threads)
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})

# 仅安装对外公共头文件（不安装 internal.h）。始终生成 install 规则，VS 才有 INSTALL 目标。
install(TARGETS ${PROJECT_NAME}
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)

install(FILES
	include/pcx.h
	include/interface.h
	include/error.h
	include/pcxcomponent.h
	include/pcxplugin.h
	include/pcxexport.h
	DESTINATION include
)
