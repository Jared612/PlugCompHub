# pch core: pch shared library, exports Initialize/Terminate etc. symbols for host and plugin convention loading
cmake_minimum_required(VERSION 3.10)

project(pch)

# Explicit source lists: new files must be registered here
set(CORE_SOURCES
	src/coreapi.cpp
	src/coreinit.cpp
	src/environment.cpp
	src/loggerManager.cpp
	src/messagecenter.cpp
	src/component/componentManager.cpp
	src/component/objectManager.cpp
	src/plugin/plugin.cpp
	src/plugin/pluginManager.cpp
)
set(CORE_HEADERS
	src/environment.h
	src/loggerManager.h
	src/messagecenter.h
	src/component/component.h
	src/component/componentManager.h
	src/component/objectManager.h
	src/plugin/plugin.h
	src/plugin/pluginManager.h
	include/core.h
	include/interface.h
	include/error.h
	include/componentinfo.h
	include/plugininfo.h
	include/coreexport.h
)

add_library(${PROJECT_NAME} SHARED ${CORE_SOURCES} ${CORE_HEADERS})
# Works with coreexport.h __PCH_EXPORTS branch, produces dllexport instead of dllimport
target_compile_definitions(${PROJECT_NAME} PRIVATE __PCH_EXPORTS)
# Target filename: pch.dll on Windows, libpch.so / libpch.dylib on Unix

# Build-time visibility: only include/ is PUBLIC; src/ stays PRIVATE so
# consumers cannot depend on internal implementations.
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin
        ${CMAKE_CURRENT_SOURCE_DIR}/src/component
)

# MessageCenter etc. use std::thread; plugin loading needs dlopen on non-Windows
find_package(Threads)
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})

# Only install public-facing headers; implementation headers stay in src/.
set_target_properties(${PROJECT_NAME} PROPERTIES
	FOLDER "core"
	EXPORT_NAME pch
)

install(TARGETS ${PROJECT_NAME} EXPORT PCHTargets
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
