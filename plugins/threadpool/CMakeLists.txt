# 线程池扩展：仅标准库 + pch，无第三方头路径
cmake_minimum_required(VERSION 3.10)

project(pchthreadpool)

find_package(Threads REQUIRED)

set(THREADPOOL_SOURCES
	src/threadpool.cpp
	src/plugin.cpp
)
set(THREADPOOL_HEADERS
	src/threadpool.h
	${CMAKE_SOURCE_DIR}/include/pch/threadpool/ithreadpool.h
)

add_library(pchthreadpool SHARED ${THREADPOOL_SOURCES} ${THREADPOOL_HEADERS})

target_include_directories(pchthreadpool
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/pch>
		$<INSTALL_INTERFACE:include>
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

set_target_properties(pchthreadpool PROPERTIES
	FOLDER "threadpool"
	EXPORT_NAME threadpool
)

install(TARGETS pchthreadpool EXPORT PCHTargets
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)
install(FILES
	${CMAKE_SOURCE_DIR}/include/pch/threadpool/ithreadpool.h
	DESTINATION include/threadpool
)
