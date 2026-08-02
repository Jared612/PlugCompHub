# Network plugin: HTTP/WebSocket client and server using Boost.Asio + Boost.Beast
cmake_minimum_required(VERSION 3.10)

project(pchnetwork)

find_package(Threads REQUIRED)

set(PCHNETWORK_SOURCES
	src/asiocontext.cpp
	src/httpclient.cpp
	src/websocketclient.cpp
	src/httpserver.cpp
	src/websocketserver.cpp
	src/plugin.cpp
)
set(PCHNETWORK_HEADERS
	src/asiocontext.h
	src/httpclient.h
	src/websocketclient.h
	src/httpserver.h
	src/websocketserver.h
	${CMAKE_SOURCE_DIR}/include/pch/network/ihttpclient.h
	${CMAKE_SOURCE_DIR}/include/pch/network/iwebsocketclient.h
	${CMAKE_SOURCE_DIR}/include/pch/network/ihttpserver.h
	${CMAKE_SOURCE_DIR}/include/pch/network/iwebsocketserver.h
)

add_library(pchnetwork SHARED ${PCHNETWORK_SOURCES} ${PCHNETWORK_HEADERS})

target_include_directories(pchnetwork
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/pch>
		$<INSTALL_INTERFACE:include>
	PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
	SYSTEM PRIVATE
		"${CMAKE_SOURCE_DIR}/3rdparty/boost"
)

# Boost.Asio 在 Windows 上需要 Winsock2；_WIN32_WINNT 保证使用新版 WinSock API
if(WIN32)
	target_compile_definitions(pchnetwork PRIVATE _WIN32_WINNT=0x0A00)
endif()

target_link_libraries(pchnetwork
	PUBLIC
		pch
	PRIVATE
		Threads::Threads
		ws2_32
		mswsock
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
	${CMAKE_SOURCE_DIR}/include/pch/network/ihttpserver.h
	${CMAKE_SOURCE_DIR}/include/pch/network/iwebsocketserver.h
	DESTINATION include/network
)
