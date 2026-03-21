# 获取当前目录下的所有源文件
file(GLOB EXAMPLE_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

# 创建 testserver 可执行文件（排除 stress_client.cpp）
list(REMOVE_ITEM EXAMPLE_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/stress_client.cpp)
add_executable(testserver ${EXAMPLE_SRCS})
target_link_libraries(testserver muduo_core ${LIBS})
target_compile_options(testserver PRIVATE -std=c++11 -Wall)
set_target_properties(testserver PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# 新增：创建 stress_client 可执行文件
add_executable(stress_client ${CMAKE_CURRENT_SOURCE_DIR}/stress_client.cpp)
target_compile_options(stress_client PRIVATE -std=c++11 -Wall -pthread)
set_target_properties(stress_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)