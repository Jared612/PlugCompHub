# Network plugin: HTTP client and WebSocket client using cpp-httplib
cmake_minimum_required(VERSION 3.10)

project(pchnetwork)

find_package(Threads REQUIRED)

file(GLOB PCHNETWORK_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB PCHNETWORK_HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_library(pchnetwork SHARED ${PCHNETWORK_SOURCES} ${PCHNETWORK_HEADERS})

target_include_directories(pchnetwork
	PUBLIC
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
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

set_target_properties(pchnetwork PROPERTIES FOLDER "network")

install(TARGETS pchnetwork
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)

install(FILES
	include/ihttpclient.h
	include/iwebsocketclient.h
	DESTINATION include
)
