# spdlog-based file logger plugin
cmake_minimum_required(VERSION 3.10)

project(pchlogger)

find_package(Threads REQUIRED)

set(LOGGER_SOURCES
	src/filelogger.cpp
	src/plugin.cpp
)
set(LOGGER_HEADERS
	src/filelogger.h
	${CMAKE_SOURCE_DIR}/include/pch/logger/ifilelogger.h
)

add_library(pchlogger SHARED ${LOGGER_SOURCES} ${LOGGER_HEADERS})

target_include_directories(pchlogger
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/pch>
		$<INSTALL_INTERFACE:include>
	PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
	SYSTEM PRIVATE
		"${CMAKE_SOURCE_DIR}/3rdparty/spdlog/include"
)

target_link_libraries(pchlogger
	PUBLIC
		pch
	PRIVATE
		Threads::Threads
		${CMAKE_DL_LIBS}
)

set_target_properties(pchlogger PROPERTIES
	FOLDER "logger"
	EXPORT_NAME logger
)

install(TARGETS pchlogger EXPORT PCHTargets
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)
install(FILES
	${CMAKE_SOURCE_DIR}/include/pch/logger/ifilelogger.h
	DESTINATION include/logger
)
