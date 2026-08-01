# SQLite 扩展：将 amalgamation 的 sqlite3.c 直接编译进本库，避免单独管理第三方预编译库
cmake_minimum_required(VERSION 3.10)

project(pchsqlite)

find_package(Threads REQUIRED)

set(PCHSQLITE_SOURCES
	src/sqlite.cpp
	src/sqliteorm.cpp
	src/plugin.cpp
)
set(PCHSQLITE_HEADERS
	src/sqlite.h
	src/sqliteorm.h
	src/kvtabledefine.h
	include/isqlite.h
	include/isqliteorm.h
)

add_library(pchsqlite SHARED
	${PCHSQLITE_SOURCES}
	${PCHSQLITE_HEADERS}
	"${CMAKE_SOURCE_DIR}/3rdparty/sqlite3/include/sqlite3.c"
)
# 单翻译单元 sqlite3.c 体量大，增量编译时仅 pchsqlite 自身源文件频繁变更

target_include_directories(pchsqlite
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
		"${CMAKE_SOURCE_DIR}/3rdparty/sqlite3/include"
)

# 与 pchnet 相同：对外暴露 pch 依赖，实现侧私有链接线程与动态加载器
target_link_libraries(pchsqlite
	PUBLIC
		pch
	PRIVATE
		Threads::Threads
		${CMAKE_DL_LIBS}
)

set_target_properties(pchsqlite PROPERTIES
	FOLDER "sqlite"
	EXPORT_NAME sqlite
)

install(TARGETS pchsqlite EXPORT PCHTargets
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)

install(FILES
	include/isqlite.h
	include/isqliteorm.h
	DESTINATION include
)
