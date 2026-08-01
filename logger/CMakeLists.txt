# spdlog-based file logger plugin
cmake_minimum_required(VERSION 3.10)

find_package(Threads REQUIRED)

file(GLOB LOGGER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB LOGGER_HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_library(pchlogger SHARED ${LOGGER_SOURCES} ${LOGGER_HEADERS})

target_include_directories(pchlogger
	PUBLIC
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
		"${CMAKE_SOURCE_DIR}/3rdparty/spdlog/include"
)

target_link_libraries(pchlogger
	PUBLIC
		pch
	PRIVATE
		Threads::Threads
		${CMAKE_DL_LIBS}
)

set_target_properties(pchlogger PROPERTIES FOLDER "logger")

install(TARGETS pchlogger
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)
install(FILES
	include/ifilelogger.h
	DESTINATION include
)
