cmake_minimum_required(VERSION 3.10)

add_executable(example_threadpool
	threadpool_example.cpp
)
target_include_directories(example_threadpool PRIVATE
	"${CMAKE_SOURCE_DIR}/include/pch"
)
target_link_libraries(example_threadpool PRIVATE pch)
set_property(TARGET example_threadpool PROPERTY FOLDER "example")

add_executable(example_application
	application_example.cpp
)
target_include_directories(example_application PRIVATE
	"${CMAKE_SOURCE_DIR}/include/pch"
)
target_link_libraries(example_application PRIVATE pch)
set_property(TARGET example_application PROPERTY FOLDER "example")

add_executable(example_net
	net_example.cpp
)
target_include_directories(example_net PRIVATE
	"${CMAKE_SOURCE_DIR}/include/pch"
)
target_link_libraries(example_net PRIVATE pch)
set_property(TARGET example_net PROPERTY FOLDER "example")

add_executable(example_sqlite
	sqlite_example.cpp
)
target_include_directories(example_sqlite PRIVATE
	"${CMAKE_SOURCE_DIR}/include/pch"
)
target_link_libraries(example_sqlite PRIVATE pch)
set_property(TARGET example_sqlite PROPERTY FOLDER "example")

add_executable(example_sqlite_orm
	sqlite_orm_example.cpp
)
target_include_directories(example_sqlite_orm PRIVATE
	"${CMAKE_SOURCE_DIR}/include/pch"
)
target_link_libraries(example_sqlite_orm PRIVATE pch)
set_property(TARGET example_sqlite_orm PROPERTY FOLDER "example")

add_executable(example_logger
	logger_example.cpp
)
target_include_directories(example_logger PRIVATE
	"${CMAKE_SOURCE_DIR}/include/pch"
)
target_link_libraries(example_logger PRIVATE pch)
set_property(TARGET example_logger PROPERTY FOLDER "example")

if(WIN32)
	foreach(_tgt IN ITEMS example_threadpool example_application example_net example_sqlite example_sqlite_orm example_logger)
		add_custom_command(TARGET ${_tgt} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				"$<TARGET_FILE:pch>"
				"$<TARGET_FILE:pchthreadpool>"
				"$<TARGET_FILE:pchapplication>"
				"$<TARGET_FILE:pchnetwork>"
				"$<TARGET_FILE:pchsqlite>"
				"$<TARGET_FILE:pchlogger>"
				"$<TARGET_FILE_DIR:${_tgt}>"
		)
	endforeach()
endif()
