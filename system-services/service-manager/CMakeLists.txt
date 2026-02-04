# 服务管理器模块构建配置

# 设置源文件
set(SERVICE_MANAGER_SOURCES
    service_manager.cpp
)

# 设置头文件
set(SERVICE_MANAGER_HEADERS
    service_manager.h
)

# 创建静态库
add_library(service_manager STATIC
    ${SERVICE_MANAGER_SOURCES}
    ${SERVICE_MANAGER_HEADERS}
)

# 设置目标属性
target_include_directories(service_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接依赖库
target_link_libraries(service_manager
    Qt6::Core
)

# 安装配置
install(TARGETS service_manager
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${SERVICE_MANAGER_HEADERS}
    DESTINATION include/cloudflow/system
)

# 设置编译器选项
target_compile_options(service_manager PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# 设置C++标准
target_compile_features(service_manager PRIVATE cxx_std_17)