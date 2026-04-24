cmake_minimum_required(VERSION 3.10)

add_executable(example_threadpool
	threadpool_example.cpp
)
target_include_directories(example_threadpool PRIVATE
	"${CMAKE_SOURCE_DIR}/pcxcore/include"
	"${CMAKE_SOURCE_DIR}/pcxcore/src"
	"${CMAKE_SOURCE_DIR}/pcxthreadpool/include"
)
target_link_libraries(example_threadpool PRIVATE pcx)
set_property(TARGET example_threadpool PROPERTY FOLDER "example")

add_executable(example_application
	application_example.cpp
)
target_include_directories(example_application PRIVATE
	"${CMAKE_SOURCE_DIR}/pcxcore/include"
	"${CMAKE_SOURCE_DIR}/pcxcore/src"
	"${CMAKE_SOURCE_DIR}/pcxapplication/include"
)
target_link_libraries(example_application PRIVATE pcx)
set_property(TARGET example_application PROPERTY FOLDER "example")

add_executable(example_net
	net_example.cpp
)
target_include_directories(example_net PRIVATE
	"${CMAKE_SOURCE_DIR}/pcxcore/include"
	"${CMAKE_SOURCE_DIR}/pcxcore/src"
	"${CMAKE_SOURCE_DIR}/pcxnet/include"
)
target_link_libraries(example_net PRIVATE pcx)
set_property(TARGET example_net PROPERTY FOLDER "example")

add_executable(example_sqlite
	sqlite_example.cpp
)
target_include_directories(example_sqlite PRIVATE
	"${CMAKE_SOURCE_DIR}/pcxcore/include"
	"${CMAKE_SOURCE_DIR}/pcxcore/src"
	"${CMAKE_SOURCE_DIR}/pcxsqlite/include"
)
target_link_libraries(example_sqlite PRIVATE pcx)
set_property(TARGET example_sqlite PROPERTY FOLDER "example")

add_executable(example_sqlite_orm
	sqlite_orm_example.cpp
)
target_include_directories(example_sqlite_orm PRIVATE
	"${CMAKE_SOURCE_DIR}/pcxcore/include"
	"${CMAKE_SOURCE_DIR}/pcxcore/src"
	"${CMAKE_SOURCE_DIR}/pcxsqlite/include"
)
target_link_libraries(example_sqlite_orm PRIVATE pcx)
set_property(TARGET example_sqlite_orm PROPERTY FOLDER "example")

if(WIN32)
	foreach(_tgt IN ITEMS example_threadpool example_application example_net example_sqlite example_sqlite_orm)
		add_custom_command(TARGET ${_tgt} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				"$<TARGET_FILE:pcx>"
				"$<TARGET_FILE:pcxthreadpool>"
				"$<TARGET_FILE:pcxapplication>"
				"$<TARGET_FILE:pcxnet>"
				"$<TARGET_FILE:pcxsqlite>"
				"$<TARGET_FILE_DIR:${_tgt}>"
		)
	endforeach()
endif()
