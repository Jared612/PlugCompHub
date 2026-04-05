cmake_minimum_required(VERSION 3.10)

find_package(Threads REQUIRED)

file(GLOB THREADPOOL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB THREADPOOL_HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_library(mcpthreadpool SHARED ${THREADPOOL_SOURCES} ${THREADPOOL_HEADERS})

target_include_directories(mcpthreadpool
	PUBLIC
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(mcpthreadpool
	PUBLIC
		mcp
	PRIVATE
		Threads::Threads
		${CMAKE_DL_LIBS}
)

install(TARGETS mcpthreadpool
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)
install(FILES
	include/imcpthreadpool.h
	DESTINATION include
)
