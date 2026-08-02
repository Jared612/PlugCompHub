# 顶层工程：聚合 pch 核心与多个可选扩展子目录；不直接定义库目标。
cmake_minimum_required(VERSION 3.10)

# 解决方案/工程显示名
project(PlugCompHub VERSION 0.2.1)

# Installation output directory: cmake --install build → dist/bin/ dist/lib/ dist/include/
set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/dist" CACHE PATH "Install root directory")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows Release：生成 PDB 便于排障
if(WIN32)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
	set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
endif()

# CRT 契约：默认所有配置统一使用静态 CRT（/MT /MTd），保证 core、插件、宿主
# 跨 DLL 传递内存（std::string / std::function / 虚表对象）时运行库一致。
# 需要动态 CRT 时：cmake -DPCH_USE_STATIC_CRT=OFF
option(PCH_USE_STATIC_CRT "Use static C runtime (/MT) for all configurations on Windows" ON)
if(WIN32 AND PCH_USE_STATIC_CRT)
	foreach(_cfg DEBUG RELEASE MINSIZEREL RELWITHDEBINFO)
		string(REPLACE "/MDd" "/MTd" CMAKE_CXX_FLAGS_${_cfg} "${CMAKE_CXX_FLAGS_${_cfg}}")
		string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_${_cfg} "${CMAKE_CXX_FLAGS_${_cfg}}")
	endforeach()
endif()

# MSVC：UTF-8 源文件编码；/W4 提高可维护性
if(MSVC)
	if(MSVC_VERSION GREATER 1800)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
	endif()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
endif()

# 统一产物目录，便于运行时 PATH 指向单一 bin
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
if(WIN32)
	set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
else()
	set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)
endif()

# 必选：pch 核心动态库
add_subdirectory(core)

# 可选插件：统一放在 plugins/ 下，目录或 CMakeLists 不存在时跳过，避免最小检出无法配置
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/plugins/CMakeLists.txt")
	add_subdirectory(plugins)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt")
	add_subdirectory(examples)
endif()

# ---- 安装包导出：支持消费方 find_package(PCH) ----
include(CMakePackageConfigHelpers)
set(PCH_INSTALL_CMAKEDIR lib/cmake/PCH)

install(EXPORT PCHTargets
	FILE PCHTargets.cmake
	NAMESPACE PCH::
	DESTINATION ${PCH_INSTALL_CMAKEDIR}
)

configure_package_config_file(
	cmake/PCHConfig.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/PCHConfig.cmake
	INSTALL_DESTINATION ${PCH_INSTALL_CMAKEDIR}
)

write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/PCHConfigVersion.cmake
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY SameMajorVersion
)

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/PCHConfig.cmake
	${CMAKE_CURRENT_BINARY_DIR}/PCHConfigVersion.cmake
	DESTINATION ${PCH_INSTALL_CMAKEDIR}
)

# ---- 测试 ----
include(CTest)
if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
	add_subdirectory(tests)
endif()

# IDE 中按文件夹组织目标（主要影响 Visual Studio 生成器）
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
