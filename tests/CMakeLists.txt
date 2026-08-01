# 冒烟测试：只依赖公共头（core/include + 各模块 include），验证接口边界
cmake_minimum_required(VERSION 3.10)

add_executable(pch_core_smoke core_smoke_test.cpp)
target_include_directories(pch_core_smoke PRIVATE
	"${CMAKE_SOURCE_DIR}/include/pch"
	"${CMAKE_SOURCE_DIR}/examples"
)
target_link_libraries(pch_core_smoke PRIVATE pch)

add_executable(pch_plugin_smoke plugin_smoke_test.cpp)
target_include_directories(pch_plugin_smoke PRIVATE
	"${CMAKE_SOURCE_DIR}/include/pch"
	"${CMAKE_SOURCE_DIR}/examples"
)
target_link_libraries(pch_plugin_smoke PRIVATE pch)

# 将运行所需的动态库复制到测试可执行文件目录
foreach(_tgt IN ITEMS pch_core_smoke pch_plugin_smoke)
	add_custom_command(TARGET ${_tgt} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"$<TARGET_FILE:pch>"
			"$<TARGET_FILE_DIR:${_tgt}>"
	)
endforeach()
add_custom_command(TARGET pch_plugin_smoke POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"$<TARGET_FILE:pchthreadpool>"
		"$<TARGET_FILE_DIR:pch_plugin_smoke>"
)

add_test(NAME core_smoke COMMAND pch_core_smoke)
add_test(NAME plugin_smoke COMMAND pch_plugin_smoke)
