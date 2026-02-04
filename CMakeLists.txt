# 系统核心文件构建配置

# 添加系统服务模块
add_subdirectory(system-services)

# 添加内核配置模块
add_subdirectory(kernel-config)

# 添加设备驱动模块
add_subdirectory(device-drivers)

# 添加文件系统模块
add_subdirectory(filesystem)