# 内核配置模块构建配置

# 设置源文件
set(KERNEL_CONFIG_SOURCES
    kernel_config.cpp
)

# 设置头文件
set(KERNEL_CONFIG_HEADERS
    kernel_config.h
)

# 创建静态库
add_library(kernel_config STATIC
    ${KERNEL_CONFIG_SOURCES}
    ${KERNEL_CONFIG_HEADERS}
)

# 设置目标属性
target_include_directories(kernel_config PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接依赖库
target_link_libraries(kernel_config
    jsoncpp
)

# 安装配置
install(TARGETS kernel_config
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${KERNEL_CONFIG_HEADERS}
    DESTINATION include/cloudflow/kernel
)

# 设置编译器选项
target_compile_options(kernel_config PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# 设置C++标准
target_compile_features(kernel_config PRIVATE cxx_std_17)