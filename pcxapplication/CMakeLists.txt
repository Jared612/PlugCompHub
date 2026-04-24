# 应用封装层：依赖已构建的 `pcx` 核心，提供 TOML 驱动启停
cmake_minimum_required(VERSION 3.10)

project(pcxapplication)

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

# 显式列出源文件，避免 GLOB 遗漏新文件或 IDE 索引不一致
set(_pcx_app_sources
	src/configtoml.cpp
	src/pcxapplication.cpp
	src/applicationpcx.cpp
	src/plugin.cpp
)

add_library(pcxapplication SHARED ${_pcx_app_sources})

# 与 ipcxapplication.h 中 PCX_APPLICATION_EXPORTS 一致，导出工厂符号
target_compile_definitions(pcxapplication PRIVATE PCX_APPLICATION_EXPORTS)

# PUBLIC：安装后消费者只需 link pcxapplication 并包含 include；PRIVATE：实现细节与 cpptoml 仅编译期可见
target_include_directories(pcxapplication
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
		${CMAKE_SOURCE_DIR}/3rdparty/cpptoml
)

target_link_libraries(pcxapplication PUBLIC pcx)

# 插件路径解析等可能隐式依赖平台线程与动态加载器
find_package(Threads REQUIRED)
target_link_libraries(pcxapplication PRIVATE Threads::Threads ${CMAKE_DL_LIBS})

# 输出名与安装头文件前缀对齐
set_target_properties(pcxapplication PROPERTIES OUTPUT_NAME "pcxapplication")

install(TARGETS pcxapplication
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)
install(FILES
	include/ipcxapplication.h
	DESTINATION include
)
