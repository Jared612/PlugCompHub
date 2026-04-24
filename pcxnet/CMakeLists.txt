# 网络扩展：Beast/Asio 头文件来自仓库内 Boost 源码树，链接核心 pcx
cmake_minimum_required(VERSION 3.10)

project(pcxnet)

find_package(Threads REQUIRED)

file(GLOB PCXNET_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB PCXNET_HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_library(pcxnet SHARED ${PCXNET_SOURCES} ${PCXNET_HEADERS})

# 声明最低支持 Win7+ API 集，避免 Asio 使用较新 Winsock 特性时出现宏不一致
if(WIN32)
	target_compile_definitions(pcxnet PRIVATE _WIN32_WINNT=0x0601)
endif()

target_include_directories(pcxnet
	PUBLIC
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
		"${CMAKE_SOURCE_DIR}/3rdparty/boost_1_90_0"
)

# PUBLIC pcx：消费者链接 pcxnet 时自动获得核心依赖；PRIVATE：线程与 dl 仅为本库实现需要
target_link_libraries(pcxnet
	PUBLIC
		pcx
	PRIVATE
		Threads::Threads
		${CMAKE_DL_LIBS}
)

install(TARGETS pcxnet
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)

install(FILES
	include/ipcxhttpclient.h
	include/ipcxhttpserver.h
	include/ipcxwebsocketclient.h
	include/ipcxwebsocketserver.h
	include/ipcxasiocontext.h
	DESTINATION include
)
