# SQLite 扩展：将 amalgamation 的 sqlite3.c 直接编译进本库，避免单独管理第三方预编译库
cmake_minimum_required(VERSION 3.10)

find_package(Threads REQUIRED)

file(GLOB PCXSQLITE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB PCXSQLITE_HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_library(pcxsqlite SHARED
	${PCXSQLITE_SOURCES}
	${PCXSQLITE_HEADERS}
	"${CMAKE_SOURCE_DIR}/3rdparty/sqlite3/include/sqlite3.c"
)
# 单翻译单元 sqlite3.c 体量大，增量编译时仅 pcxsqlite 自身源文件频繁变更

target_include_directories(pcxsqlite
	PUBLIC
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
		"${CMAKE_SOURCE_DIR}/3rdparty/sqlite3/include"
)

# 与 pcxnet 相同：对外暴露 pcx 依赖，实现侧私有链接线程与动态加载器
target_link_libraries(pcxsqlite
	PUBLIC
		pcx
	PRIVATE
		Threads::Threads
		${CMAKE_DL_LIBS}
)

install(TARGETS pcxsqlite
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)

install(FILES
	include/ipcxsqlite.h
	include/ipcxsqliteorm.h
	DESTINATION include
)
