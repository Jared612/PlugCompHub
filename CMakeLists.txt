cmake_minimum_required(VERSION 3.10)

project(mcpCore)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi  /MT")
	set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
endif()

if(MSVC)
	if(MSVC_VERSION GREATER 1800)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
	endif()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
endif()

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
if(WIN32)
	set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
else()
	set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)
endif()

add_subdirectory(mcp)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/threadpool/CMakeLists.txt")
	add_subdirectory(threadpool)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/mcpthreadpool/CMakeLists.txt")
	add_subdirectory(mcpthreadpool)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/mcpAppliction/CMakeLists.txt")
	add_subdirectory(mcpAppliction)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/demo_mcplib/CMakeLists.txt")
	add_subdirectory(demo_mcplib)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
