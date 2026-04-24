# 顶层工程：聚合 pcx 核心与多个可选扩展子目录；不直接定义库目标。
cmake_minimum_required(VERSION 3.10)

# 解决方案/工程显示名；实际核心库目标名在 pcxcore 子目录中定义为 `pcx`
project(pcxCore)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows Release：生成 PDB 便于排障；/MT 静态 CRT 减少宿主对 VC 运行时的部署依赖
if(WIN32)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi  /MT")
	set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
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

# 必选：PCX 核心动态库
add_subdirectory(pcxcore)

# 以下为可选组件：目录或 CMakeLists 不存在时跳过，避免最小检出无法配置
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/threadpool/CMakeLists.txt")
	add_subdirectory(threadpool)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pcxthreadpool/CMakeLists.txt")
	add_subdirectory(pcxthreadpool)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pcxnet/CMakeLists.txt")
	add_subdirectory(pcxnet)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pcxsqlite/CMakeLists.txt")
	add_subdirectory(pcxsqlite)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pcxapplication/CMakeLists.txt")
	add_subdirectory(pcxapplication)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/demo_pcxlib/CMakeLists.txt")
	add_subdirectory(demo_pcxlib)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/example/CMakeLists.txt")
	add_subdirectory(example)
endif()

# IDE 中按文件夹组织目标（主要影响 Visual Studio 生成器）
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
