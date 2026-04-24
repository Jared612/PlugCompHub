# 线程池扩展：仅标准库 + pcx，无第三方头路径
cmake_minimum_required(VERSION 3.10)

find_package(Threads REQUIRED)

file(GLOB THREADPOOL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB THREADPOOL_HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_library(pcxthreadpool SHARED ${THREADPOOL_SOURCES} ${THREADPOOL_HEADERS})
# 未单独 project()：目标名固定为 pcxthreadpool，由顶层 add_subdirectory 引入

target_include_directories(pcxthreadpool
	PUBLIC
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(pcxthreadpool
	PUBLIC
		pcx
	PRIVATE
		Threads::Threads
		${CMAKE_DL_LIBS}
)

install(TARGETS pcxthreadpool
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)
install(FILES
	include/ipcxthreadpool.h
	DESTINATION include
)
