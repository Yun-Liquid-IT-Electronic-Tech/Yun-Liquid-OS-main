/**
 * @file boot_loader.h
 * @brief 引导加载器
 * @author 云流操作系统开发团队
 * @date 2026-02-04
 * @version 1.0.0
 * 
 * 引导加载器负责加载内核镜像到内存并初始化系统环境
 */

#pragma once

#include "boot_params.h"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace CloudFlow::Boot {

/**
 * @enum BootLoaderStage
 * @brief 引导加载器阶段
 */
enum class BootLoaderStage {
    Initializing,    ///< 初始化阶段
    LoadingKernel,   ///< 加载内核阶段
    SettingUpMemory, ///< 设置内存阶段
    PreparingEnv,    ///< 准备环境阶段
    HandingOver,     ///< 交接控制权阶段
    Complete         ///< 完成阶段
};

/**
 * @enum BootErrorCode
 * @brief 引导错误代码
 */
enum class BootErrorCode {
    Success,                ///< 成功
    InvalidKernel,          ///< 无效的内核镜像
    MemoryAllocationFailed, ///< 内存分配失败
    DeviceReadError,        ///< 设备读取错误
    InvalidBootParams,      ///< 无效的启动参数
    HardwareFailure,        ///< 硬件故障
    FileSystemError,        ///< 文件系统错误
    UnknownError           ///< 未知错误
};

/**
 * @struct BootProgress
 * @brief 引导进度信息
 */
struct BootProgress {
    BootLoaderStage current_stage;   ///< 当前阶段
    uint32_t progress_percent;       ///< 进度百分比
    std::string stage_description;   ///< 阶段描述
    uint64_t bytes_loaded;           ///< 已加载字节数
    uint64_t total_bytes;            ///< 总字节数
};

/**
 * @class IBootDevice
 * @brief 引导设备接口
 * 
 * 抽象引导设备操作，支持不同设备类型（硬盘、网络、USB等）
 */
class IBootDevice {
public:
    virtual ~IBootDevice() = default;
    
    /**
     * @brief 初始化引导设备
     * @return 初始化是否成功
     */
    virtual bool initialize() = 0;
    
    /**
     * @brief 读取设备数据
     * @param buffer 数据缓冲区
     * @param size 要读取的大小
     * @param offset 偏移量
     * @return 实际读取的字节数，-1表示错误
     */
    virtual ssize_t read(void* buffer, size_t size, off_t offset) = 0;
    
    /**
     * @brief 获取设备信息
     * @return 设备信息
     */
    virtual BootDeviceInfo getDeviceInfo() const = 0;
    
    /**
     * @brief 检查设备是否就绪
     * @return 设备是否就绪
     */
    virtual bool isReady() const = 0;
    
    /**
     * @brief 获取错误信息
     * @return 错误描述
     */
    virtual std::string getLastError() const = 0;
};

/**
 * @class IKernelLoader
 * @brief 内核加载器接口
 * 
 * 负责解析和加载不同格式的内核镜像
 */
class IKernelLoader {
public:
    virtual ~IKernelLoader() = default;
    
    /**
     * @brief 检查内核镜像格式是否支持
     * @param data 镜像数据
     * @param size 数据大小
     * @return 是否支持
     */
    virtual bool supportsFormat(const void* data, size_t size) = 0;
    
    /**
     * @brief 加载内核镜像
     * @param device 引导设备
     * @param kernel_path 内核镜像路径
     * @param boot_info 启动信息
     * @return 加载是否成功
     */
    virtual bool loadKernel(std::shared_ptr<IBootDevice> device, 
                           const std::string& kernel_path, 
                           BootInfo& boot_info) = 0;
    
    /**
     * @brief 获取内核镜像类型
     * @return 内核镜像类型
     */
    virtual KernelImageType getKernelType() const = 0;
    
    /**
     * @brief 验证内核完整性
     * @param data 镜像数据
     * @param size 数据大小
     * @return 验证是否通过
     */
    virtual bool verifyIntegrity(const void* data, size_t size) = 0;
    
    /**
     * @brief 获取错误信息
     * @return 错误描述
     */
    virtual std::string getLastError() const = 0;
};

/**
 * @class BootLoader
 * @brief 引导加载器主类
 * 
 * 负责协调整个引导过程，包括设备初始化、内核加载、环境设置等
 */
class BootLoader {
public:
    /**
     * @brief 构造函数
     */
    BootLoader();
    
    /**
     * @brief 析构函数
     */
    ~BootLoader();
    
    /**
     * @brief 初始化引导加载器
     * @param boot_params 启动参数
     * @return 初始化是否成功
     */
    bool initialize(std::shared_ptr<BootParams> boot_params);
    
    /**
     * @brief 执行引导过程
     * @return 引导是否成功
     */
    bool boot();
    
    /**
     * @brief 注册引导设备
     * @param device 引导设备
     * @param priority 优先级（数字越小优先级越高）
     * @return 注册是否成功
     */
    bool registerBootDevice(std::shared_ptr<IBootDevice> device, int priority = 0);
    
    /**
     * @brief 注册内核加载器
     * @param loader 内核加载器
     * @return 注册是否成功
     */
    bool registerKernelLoader(std::shared_ptr<IKernelLoader> loader);
    
    /**
     * @brief 获取引导进度信息
     * @return 引导进度
     */
    BootProgress getProgress() const;
    
    /**
     * @brief 获取启动信息
     * @return 启动信息
     */
    const BootInfo& getBootInfo() const;
    
    /**
     * @brief 获取错误代码
     * @return 错误代码
     */
    BootErrorCode getErrorCode() const;
    
    /**
     * @brief 获取错误信息
     * @return 错误描述
     */
    std::string getLastError() const;
    
    /**
     * @brief 添加引导进度监听器
     * @param callback 回调函数
     */
    void addProgressListener(std::function<void(const BootProgress&)> callback);
    
    /**
     * @brief 添加引导错误监听器
     * @param callback 回调函数
     */
    void addErrorListener(std::function<void(BootErrorCode, const std::string&)> callback);
    
    /**
     * @brief 设置调试模式
     * @param debug 是否启用调试模式
     */
    void setDebugMode(bool debug);
    
    /**
     * @brief 生成引导报告
     * @return 引导报告字符串
     */
    std::string generateReport() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @class MemoryManager
 * @brief 内存管理器
 * 
 * 负责引导阶段的内存管理和分配
 */
class MemoryManager {
public:
    /**
     * @brief 构造函数
     */
    MemoryManager();
    
    /**
     * @brief 析构函数
     */
    ~MemoryManager();
    
    /**
     * @brief 初始化内存管理器
     * @return 初始化是否成功
     */
    bool initialize();
    
    /**
     * @brief 探测系统内存
     * @return 探测到的内存区域数量
     */
    size_t detectMemory();
    
    /**
     * @brief 分配内存块
     * @param size 请求的大小
     * @param alignment 对齐要求
     * @return 分配的内存地址，0表示失败
     */
    uint64_t allocate(size_t size, size_t alignment = 1);
    
    /**
     * @brief 释放内存块
     * @param address 内存地址
     * @param size 内存大小
     */
    void free(uint64_t address, size_t size);
    
    /**
     * @brief 获取内存映射
     * @return 内存映射列表
     */
    std::vector<MemoryRegion> getMemoryMap() const;
    
    /**
     * @brief 获取总内存大小
     * @return 总内存大小（字节）
     */
    uint64_t getTotalMemory() const;
    
    /**
     * @brief 获取可用内存大小
     * @return 可用内存大小（字节）
     */
    uint64_t getAvailableMemory() const;
    
    /**
     * @brief 设置内存保护
     * @param address 内存地址
     * @param size 内存大小
     * @param read_only 是否只读
     * @return 设置是否成功
     */
    bool setMemoryProtection(uint64_t address, size_t size, bool read_only);
    
    /**
     * @brief 获取错误信息
     * @return 错误描述
     */
    std::string getLastError() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @class EnvironmentSetup
 * @brief 环境设置器
 * 
 * 负责设置引导阶段的环境参数
 */
class EnvironmentSetup {
public:
    /**
     * @brief 构造函数
     */
    EnvironmentSetup();
    
    /**
     * @brief 析构函数
     */
    ~EnvironmentSetup();
    
    /**
     * @brief 初始化环境设置器
     * @return 初始化是否成功
     */
    bool initialize();
    
    /**
     * @brief 设置视频模式
     * @param width 宽度
     * @param height 高度
     * @param bpp 每像素位数
     * @return 设置是否成功
     */
    bool setVideoMode(uint32_t width, uint32_t height, uint32_t bpp);
    
    /**
     * @brief 设置控制台
     * @param console_type 控制台类型
     * @return 设置是否成功
     */
    bool setConsole(const std::string& console_type);
    
    /**
     * @brief 设置中断向量表
     * @return 设置是否成功
     */
    bool setupInterrupts();
    
    /**
     * @brief 设置时钟
     * @return 设置是否成功
     */
    bool setupClock();
    
    /**
     * @brief 准备内核环境
     * @param boot_info 启动信息
     * @return 准备是否成功
     */
    bool prepareKernelEnvironment(const BootInfo& boot_info);
    
    /**
     * @brief 交接控制权给内核
     * @param kernel_entry 内核入口点
     * @param boot_info 启动信息
     */
    void handoverToKernel(uint64_t kernel_entry, const BootInfo& boot_info);
    
    /**
     * @brief 获取错误信息
     * @return 错误描述
     */
    std::string getLastError() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace CloudFlow::Boot