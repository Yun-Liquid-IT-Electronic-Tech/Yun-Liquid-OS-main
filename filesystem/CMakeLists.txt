# 文件系统模块构建配置

# 设置模块名称
set(MODULE_NAME filesystem)

# 添加源文件
set(SOURCES
    filesystem.cpp
)

# 添加头文件目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# 创建静态库
add_library(${MODULE_NAME} STATIC ${SOURCES})

# 设置编译属性
set_target_properties(${MODULE_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# 添加依赖库
target_link_libraries(${MODULE_NAME} PRIVATE
    pthread
)

# 安装配置
install(TARGETS ${MODULE_NAME}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES filesystem.h
    DESTINATION include/CloudFlow/FileSystem
)