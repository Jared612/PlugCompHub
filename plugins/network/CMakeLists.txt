# Network plugin: HTTP client and WebSocket client using cpp-httplib
cmake_minimum_required(VERSION 3.10)

project(pchnetwork)

find_package(Threads REQUIRED)

set(PCHNETWORK_SOURCES
	src/httpclient.cpp
	src/websocketclient.cpp
	src/plugin.cpp
)
set(PCHNETWORK_HEADERS
	src/httpclient.h
	src/websocketclient.h
	${CMAKE_SOURCE_DIR}/include/pch/network/ihttpclient.h
	${CMAKE_SOURCE_DIR}/include/pch/network/iwebsocketclient.h
)

add_library(pchnetwork SHARED ${PCHNETWORK_SOURCES} ${PCHNETWORK_HEADERS})

target_include_directories(pchnetwork
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/pch>
		$<INSTALL_INTERFACE:include>
	PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
		"${CMAKE_SOURCE_DIR}/3rdparty/httplib"
)

# httplib requires _WIN32_WINNT >= 0x0A00 for Windows 10+
if(WIN32)
	target_compile_definitions(pchnetwork PRIVATE _WIN32_WINNT=0x0A00)
endif()

target_link_libraries(pchnetwork
	PUBLIC
		pch
	PRIVATE
		Threads::Threads
		ws2_32
)

set_target_properties(pchnetwork PROPERTIES
	FOLDER "network"
	EXPORT_NAME network
)

install(TARGETS pchnetwork EXPORT PCHTargets
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)

install(FILES
	${CMAKE_SOURCE_DIR}/include/pch/network/ihttpclient.h
	${CMAKE_SOURCE_DIR}/include/pch/network/iwebsocketclient.h
	DESTINATION include/network
)
