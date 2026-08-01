# Application wrapper layer: depends on built pch core, provides TOML-driven start/stop
cmake_minimum_required(VERSION 3.10)

project(pchapplication)

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

# Explicit source list, avoids GLOB missing new files or IDE index inconsistency
set(_pch_app_sources
	src/configtoml.cpp
	src/application.cpp
	src/appskeleton.cpp
	src/plugin.cpp
)

add_library(pchapplication SHARED ${_pch_app_sources})

# Consistent with iapplication.h APPLICATION_EXPORTS, export factory symbols
target_compile_definitions(pchapplication PRIVATE APPLICATION_EXPORTS)

# PUBLIC: after install consumer just links pchapplication and includes include; PRIVATE: impl details and cpptoml compile-time only
target_include_directories(pchapplication
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/pch>
		$<INSTALL_INTERFACE:include>
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
		${CMAKE_SOURCE_DIR}/3rdparty/cpptoml
)

target_link_libraries(pchapplication PUBLIC pch)

# Plugin path resolution etc. may implicitly depend on platform threads and dynamic loader
find_package(Threads REQUIRED)
target_link_libraries(pchapplication PRIVATE Threads::Threads ${CMAKE_DL_LIBS})

# Output name aligned with installed header prefix
set_target_properties(pchapplication PROPERTIES OUTPUT_NAME "pchapplication")

set_target_properties(pchapplication PROPERTIES
	FOLDER "application"
	EXPORT_NAME application
)

install(TARGETS pchapplication EXPORT PCHTargets
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)
install(FILES
	${CMAKE_SOURCE_DIR}/include/pch/application/iapplication.h
	DESTINATION include/application
)
