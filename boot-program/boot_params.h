/**
 * @file boot_params.h
 * @brief 启动参数定义
 * @author 云流操作系统开发团队
 * @date 2026-02-04
 * @version 1.0.0
 * 
 * 定义系统启动过程中使用的参数和配置结构
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace CloudFlow::Boot {

/**
 * @enum BootMode
 * @brief 启动模式
 */
enum class BootMode {
    Normal,         ///< 正常启动
    Recovery,       ///< 恢复模式
    Safe,           ///< 安全模式
    Debug,          ///< 调试模式
    Rescue,         ///< 救援模式
    Network         ///< 网络启动
};

/**
 * @enum KernelImageType
 * @brief 内核镜像类型
 */
enum class KernelImageType {
    Unknown,        ///< 未知类型
    ELF,            ///< ELF格式
    PE,             ///< PE格式
    Raw,            ///< 原始二进制
    Compressed,     ///< 压缩镜像
    Multiboot       ///< 多重启动兼容
};

/**
 * @enum MemoryMapType
 * @brief 内存映射类型
 */
enum class MemoryMapType {
    Available,      ///< 可用内存
    Reserved,       ///< 保留内存
    ACPI_Reclaim,   ///< ACPI可回收内存
    ACPI_NVS,       ///< ACPI NVS内存
    Bad             ///< 坏内存区域
};

/**
 * @struct MemoryRegion
 * @brief 内存区域描述
 */
struct MemoryRegion {
    uint64_t base_address;   ///< 基地址
    uint64_t length;         ///< 区域长度
    MemoryMapType type;      ///< 内存类型
    uint32_t attributes;      ///< 内存属性
};

/**
 * @struct BootDeviceInfo
 * @brief 启动设备信息
 */
struct BootDeviceInfo {
    std::string device_path;  ///< 设备路径
    uint32_t device_type;    ///< 设备类型
    uint64_t sector_size;    ///< 扇区大小
    uint64_t total_sectors;  ///< 总扇区数
    std::string vendor;      ///< 设备厂商
    std::string model;       ///< 设备型号
};

/**
 * @struct KernelParameters
 * @brief 内核参数
 */
struct KernelParameters {
    std::string kernel_path;          ///< 内核镜像路径
    std::string initrd_path;          ///< 初始RAM磁盘路径
    std::string root_device;          ///< 根设备
    std::string root_fstype;          ///< 根文件系统类型
    std::string console;              ///< 控制台设备
    std::string language;            ///< 系统语言
    std::string timezone;             ///< 时区设置
    
    // 启动选项
    bool quiet;                       ///< 安静启动
    bool verbose;                    ///< 详细输出
    bool debug;                      ///< 调试模式
    bool single_user;                ///< 单用户模式
    bool network;                    ///< 网络启动
    
    // 内存设置
    uint64_t mem_limit;              ///< 内存限制
    uint64_t mem_offset;             ///< 内存偏移
    
    // 其他参数
    std::vector<std::string> modules; ///< 加载模块列表
    std::unordered_map<std::string, std::string> custom_params; ///< 自定义参数
};

/**
 * @struct BootInfo
 * @brief 启动信息
 */
struct BootInfo {
    BootMode boot_mode;               ///< 启动模式
    KernelImageType kernel_type;      ///< 内核镜像类型
    BootDeviceInfo boot_device;       ///< 启动设备信息
    
    // 内存信息
    std::vector<MemoryRegion> memory_map; ///< 内存映射
    uint64_t total_memory;           ///< 总内存大小
    uint64_t available_memory;       ///< 可用内存大小
    
    // 内核信息
    uint64_t kernel_base;            ///< 内核基地址
    uint64_t kernel_size;            ///< 内核大小
    uint64_t initrd_base;            ///< 初始RAM磁盘基地址
    uint64_t initrd_size;            ///< 初始RAM磁盘大小
    
    // 启动参数
    KernelParameters params;         ///< 内核参数
    
    // 系统信息
    std::string architecture;        ///< 系统架构
    std::string platform;            ///< 硬件平台
    uint32_t boot_flags;             ///< 启动标志
    
    // 时间和版本信息
    uint64_t boot_time;              ///< 启动时间戳
    std::string bootloader_version;  ///< 引导程序版本
    std::string kernel_version;      ///< 内核版本
};

/**
 * @struct MultibootInfo
 * @brief 多重启动信息（兼容Multiboot规范）
 */
struct MultibootInfo {
    uint32_t flags;                  ///< 标志位
    uint32_t mem_lower;              ///< 低端内存大小（KB）
    uint32_t mem_upper;              ///< 高端内存大小（KB）
    uint32_t boot_device;            ///< 启动设备
    uint32_t cmdline;                ///< 命令行参数地址
    uint32_t mods_count;             ///< 模块数量
    uint32_t mods_addr;              ///< 模块信息地址
    
    // 符号表信息
    union {
        struct {
            uint32_t tabsize;        ///< 符号表大小
            uint32_t strsize;        ///< 字符串表大小
            uint32_t addr;           ///< 地址
            uint32_t reserved;       ///< 保留
        } aout_sym;                  ///< a.out符号表
        
        struct {
            uint32_t num;            ///< ELF节数量
            uint32_t size;          ///< 节头大小
            uint32_t addr;          ///< 节头地址
            uint32_t shndx;         ///< 节头字符串表索引
        } elf_sec;                  ///< ELF节信息
    } u;
    
    uint32_t mmap_length;           ///< 内存映射长度
    uint32_t mmap_addr;             ///< 内存映射地址
    uint32_t drives_length;         ///< 驱动器信息长度
    uint32_t drives_addr;          ///< 驱动器信息地址
    uint32_t config_table;         ///< 配置表地址
    uint32_t boot_loader_name;     ///< 引导程序名称地址
    uint32_t apm_table;            ///< APM表地址
    uint32_t vbe_control_info;     ///< VBE控制信息地址
    uint32_t vbe_mode_info;        ///< VBE模式信息地址
    uint32_t vbe_mode;             ///< VBE模式
    uint32_t vbe_interface_seg;    ///< VBE接口段
    uint32_t vbe_interface_off;    ///< VBE接口偏移
    uint32_t vbe_interface_len;    ///< VBE接口长度
};

/**
 * @class BootParams
 * @brief 启动参数管理器
 * 
 * 负责管理启动参数的解析、验证和应用
 */
class BootParams {
public:
    /**
     * @brief 构造函数
     */
    BootParams();
    
    /**
     * @brief 析构函数
     */
    ~BootParams();
    
    /**
     * @brief 从命令行解析启动参数
     * @param cmdline 命令行字符串
     * @return 解析是否成功
     */
    bool parseCommandLine(const std::string& cmdline);
    
    /**
     * @brief 从配置文件加载启动参数
     * @param config_file 配置文件路径
     * @return 加载是否成功
     */
    bool loadFromConfig(const std::string& config_file);
    
    /**
     * @brief 保存启动参数到配置文件
     * @param config_file 配置文件路径
     * @return 保存是否成功
     */
    bool saveToConfig(const std::string& config_file) const;
    
    /**
     * @brief 验证启动参数的有效性
     * @return 参数是否有效
     */
    bool validate() const;
    
    /**
     * @brief 获取启动信息
     * @return 启动信息结构体
     */
    const BootInfo& getBootInfo() const;
    
    /**
     * @brief 设置启动信息
     * @param info 启动信息
     */
    void setBootInfo(const BootInfo& info);
    
    /**
     * @brief 获取内核参数
     * @return 内核参数结构体
     */
    const KernelParameters& getKernelParams() const;
    
    /**
     * @brief 设置内核参数
     * @param params 内核参数
     */
    void setKernelParams(const KernelParameters& params);
    
    /**
     * @brief 获取多重启动信息
     * @return 多重启动信息结构体
     */
    const MultibootInfo& getMultibootInfo() const;
    
    /**
     * @brief 设置多重启动信息
     * @param info 多重启动信息
     */
    void setMultibootInfo(const MultibootInfo& info);
    
    /**
     * @brief 生成内核命令行字符串
     * @return 命令行字符串
     */
    std::string generateCommandLine() const;
    
    /**
     * @brief 添加自定义参数
     * @param key 参数键
     * @param value 参数值
     */
    void addCustomParam(const std::string& key, const std::string& value);
    
    /**
     * @brief 获取自定义参数
     * @param key 参数键
     * @return 参数值，如果不存在则返回空字符串
     */
    std::string getCustomParam(const std::string& key) const;
    
    /**
     * @brief 检查参数是否包含特定键
     * @param key 参数键
     * @return 是否包含
     */
    bool hasParam(const std::string& key) const;
    
    /**
     * @brief 重置所有参数为默认值
     */
    void resetToDefaults();
    
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