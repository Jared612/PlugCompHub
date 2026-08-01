# 线程池扩展：仅标准库 + pch，无第三方头路径
cmake_minimum_required(VERSION 3.10)

find_package(Threads REQUIRED)

file(GLOB THREADPOOL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB THREADPOOL_HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_library(pchthreadpool SHARED ${THREADPOOL_SOURCES} ${THREADPOOL_HEADERS})
# 未单独 project()：目标名固定为 pchthreadpool，由顶层 add_subdirectory 引入

target_include_directories(pchthreadpool
	PUBLIC
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(pchthreadpool
	PUBLIC
		pch
	PRIVATE
		Threads::Threads
		${CMAKE_DL_LIBS}
)

set_target_properties(pchthreadpool PROPERTIES FOLDER "threadpool")

install(TARGETS pchthreadpool
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)
install(FILES
	include/ithreadpool.h
	DESTINATION include
)
