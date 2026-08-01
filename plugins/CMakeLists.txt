# 可选插件聚合目录：目录或 CMakeLists 不存在时跳过
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/threadpool/CMakeLists.txt")
	add_subdirectory(threadpool)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/network/CMakeLists.txt")
	add_subdirectory(network)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/logger/CMakeLists.txt")
	add_subdirectory(logger)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sqlite/CMakeLists.txt")
	add_subdirectory(sqlite)
endif()
