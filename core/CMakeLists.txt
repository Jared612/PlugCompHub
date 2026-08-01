# pch core: pch shared library, exports Initialize/Terminate etc. symbols for host and plugin convention loading
cmake_minimum_required(VERSION 3.10)

project(pch)
# Works with coreexport.h __PCH_EXPORTS branch, produces dllexport instead of dllimport
add_definitions(-D__PCH_EXPORTS)

# Collect implementation and header files; new src files auto-participate in build
file(GLOB_RECURSE CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE CORE_HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)
file(GLOB CORE_PUBLIC_HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)
set(CORE_HEADERS ${CORE_HEADERS} ${CORE_PUBLIC_HEADERS})

add_library(${PROJECT_NAME} SHARED ${CORE_SOURCES} ${CORE_HEADERS})
# Target filename: pch.dll on Windows, libpch.so / libpch.dylib on Unix

# Build-time visibility: source root + include
# After install, only expose include dir
# (internal.h is not installed via install(FILES ...))
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/plugin>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/component>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# MessageCenter etc. use std::thread; plugin loading needs dlopen on non-Windows
find_package(Threads)
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})

# Only install public-facing headers (not internal.h). Always generate install rules so VS has INSTALL target.
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "core")

install(TARGETS ${PROJECT_NAME}
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)

install(FILES
	include/core.h
	include/interface.h
	include/error.h
	include/componentinfo.h
	include/plugininfo.h
	include/coreexport.h
	DESTINATION include
)
