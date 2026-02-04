/**
 * @file filesystem.h
 * @brief 文件系统管理器
 * @author 云流操作系统开发团队
 * @date 2026-02-04
 * @version 1.0.0
 * 
 * 提供统一的文件系统接口，支持多种文件系统类型、挂载管理、权限控制等功能
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <chrono>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

namespace CloudFlow::FileSystem {

/**
 * @enum FileSystemType
 * @brief 文件系统类型
 */
enum class FileSystemType {
    Unknown,        ///< 未知文件系统
    Ext4,           ///< ext4文件系统
    XFS,            ///< XFS文件系统
    Btrfs,          ///< Btrfs文件系统
    NTFS,           ///< NTFS文件系统（只读支持）
    FAT32,          ///< FAT32文件系统
    ExFAT,          ///< ExFAT文件系统
    Virtual,        ///< 虚拟文件系统
    Network         ///< 网络文件系统
};

/**
 * @enum MountState
 * @brief 挂载状态
 */
enum class MountState {
    Unmounted,      ///< 未挂载
    Mounting,       ///< 挂载中
    Mounted,        ///< 已挂载
    Unmounting,     ///< 卸载中
    Error           ///< 错误状态
};

/**
 * @enum FileType
 * @brief 文件类型
 */
enum class FileType {
    Regular,        ///< 普通文件
    Directory,      ///< 目录
    SymbolicLink,   ///< 符号链接
    BlockDevice,    ///< 块设备
    CharacterDevice,///< 字符设备
    FIFO,           ///< FIFO管道
    Socket          ///< 套接字
};

/**
 * @struct FileInfo
 * @brief 文件信息
 */
struct FileInfo {
    std::string name;           ///< 文件名
    std::string path;           ///< 文件路径
    FileType type;              ///< 文件类型
    mode_t permissions;         ///< 文件权限
    uid_t owner;                ///< 文件所有者
    gid_t group;                ///< 文件所属组
    off_t size;                 ///< 文件大小
    std::chrono::system_clock::time_point created_time;    ///< 创建时间
    std::chrono::system_clock::time_point modified_time;   ///< 修改时间
    std::chrono::system_clock::time_point accessed_time;   ///< 访问时间
};

/**
 * @struct MountInfo
 * @brief 挂载信息
 */
struct MountInfo {
    std::string device;         ///< 设备路径
    std::string mount_point;    ///< 挂载点
    FileSystemType fs_type;     ///< 文件系统类型
    std::string options;        ///< 挂载选项
    MountState state;           ///< 挂载状态
    uint64_t total_size;        ///< 总空间大小
    uint64_t free_size;         ///< 可用空间大小
    uint64_t used_size;         ///< 已用空间大小
};

/**
 * @struct FileSystemStats
 * @brief 文件系统统计信息
 */
struct FileSystemStats {
    uint64_t total_blocks;      ///< 总块数
    uint64_t free_blocks;       ///< 空闲块数
    uint64_t available_blocks;  ///< 可用块数
    uint64_t total_inodes;      ///< 总inode数
    uint64_t free_inodes;       ///< 空闲inode数
    uint32_t block_size;        ///< 块大小
    std::string fs_name;        ///< 文件系统名称
};

/**
 * @class IFileSystem
 * @brief 文件系统接口
 * 
 * 所有文件系统实现必须实现的接口
 */
class IFileSystem {
public:
    virtual ~IFileSystem() = default;
    
    /**
     * @brief 挂载文件系统
     * @param device 设备路径
     * @param mount_point 挂载点
     * @param options 挂载选项
     * @return 挂载是否成功
     */
    virtual bool mount(const std::string& device, const std::string& mount_point, const std::string& options = "") = 0;
    
    /**
     * @brief 卸载文件系统
     * @param mount_point 挂载点
     * @return 卸载是否成功
     */
    virtual bool unmount(const std::string& mount_point) = 0;
    
    /**
     * @brief 检查文件系统
     * @param device 设备路径
     * @return 检查是否成功
     */
    virtual bool check(const std::string& device) = 0;
    
    /**
     * @brief 格式化文件系统
     * @param device 设备路径
     * @param options 格式化选项
     * @return 格式化是否成功
     */
    virtual bool format(const std::string& device, const std::string& options = "") = 0;
    
    /**
     * @brief 获取文件系统统计信息
     * @param path 路径
     * @return 统计信息
     */
    virtual FileSystemStats getStats(const std::string& path) = 0;
    
    /**
     * @brief 获取挂载信息
     * @param mount_point 挂载点
     * @return 挂载信息
     */
    virtual MountInfo getMountInfo(const std::string& mount_point) = 0;
    
    /**
     * @brief 获取支持的文件系统类型
     * @return 支持的文件系统类型列表
     */
    virtual std::vector<FileSystemType> getSupportedTypes() const = 0;
    
    /**
     * @brief 获取文件系统特性
     * @return 特性列表
     */
    virtual std::vector<std::string> getFeatures() const = 0;
    
    /**
     * @brief 检查文件系统是否支持特定特性
     * @param feature 特性名称
     * @return 是否支持
     */
    virtual bool supportsFeature(const std::string& feature) const = 0;
};

/**
 * @class FileSystemManager
 * @brief 文件系统管理器
 * 
 * 负责管理所有文件系统的挂载、卸载和状态监控
 */
class FileSystemManager {
public:
    /**
     * @brief 构造函数
     */
    FileSystemManager();
    
    /**
     * @brief 析构函数
     */
    ~FileSystemManager();
    
    /**
     * @brief 初始化文件系统管理器
     * @return 初始化是否成功
     */
    bool initialize();
    
    /**
     * @brief 注册文件系统驱动程序
     * @param fs 文件系统驱动程序
     * @param fs_type 文件系统类型
     * @return 注册是否成功
     */
    bool registerFileSystem(std::shared_ptr<IFileSystem> fs, FileSystemType fs_type);
    
    /**
     * @brief 注销文件系统驱动程序
     * @param fs_type 文件系统类型
     * @return 注销是否成功
     */
    bool unregisterFileSystem(FileSystemType fs_type);
    
    /**
     * @brief 挂载文件系统
     * @param device 设备路径
     * @param mount_point 挂载点
     * @param fs_type 文件系统类型
     * @param options 挂载选项
     * @return 挂载是否成功
     */
    bool mount(const std::string& device, const std::string& mount_point, 
               FileSystemType fs_type, const std::string& options = "");
    
    /**
     * @brief 卸载文件系统
     * @param mount_point 挂载点
     * @return 卸载是否成功
     */
    bool unmount(const std::string& mount_point);
    
    /**
     * @brief 重新挂载文件系统
     * @param mount_point 挂载点
     * @param options 新的挂载选项
     * @return 重新挂载是否成功
     */
    bool remount(const std::string& mount_point, const std::string& options);
    
    /**
     * @brief 检查文件系统
     * @param device 设备路径
     * @param fs_type 文件系统类型
     * @return 检查是否成功
     */
    bool check(const std::string& device, FileSystemType fs_type);
    
    /**
     * @brief 格式化文件系统
     * @param device 设备路径
     * @param fs_type 文件系统类型
     * @param options 格式化选项
     * @return 格式化是否成功
     */
    bool format(const std::string& device, FileSystemType fs_type, const std::string& options = "");
    
    /**
     * @brief 获取所有挂载点
     * @return 挂载点列表
     */
    std::vector<std::string> getMountPoints() const;
    
    /**
     * @brief 获取挂载信息
     * @param mount_point 挂载点
     * @return 挂载信息
     */
    MountInfo getMountInfo(const std::string& mount_point) const;
    
    /**
     * @brief 获取文件系统统计信息
     * @param path 路径
     * @return 统计信息
     */
    FileSystemStats getStats(const std::string& path) const;
    
    /**
     * @brief 扫描并加载可用文件系统
     * @return 扫描到的文件系统数量
     */
    size_t scanFileSystems();
    
    /**
     * @brief 获取支持的文件系统类型
     * @return 支持的文件系统类型列表
     */
    std::vector<FileSystemType> getSupportedFileSystems() const;
    
    /**
     * @brief 添加挂载状态变化监听器
     * @param callback 回调函数
     */
    void addMountStateChangeListener(std::function<void(const std::string&, MountState, MountState)> callback);
    
    /**
     * @brief 添加文件系统错误监听器
     * @param callback 回调函数
     */
    void addFileSystemErrorListener(std::function<void(const std::string&, const std::string&)> callback);
    
    /**
     * @brief 生成文件系统报告
     * @return 文件系统报告字符串
     */
    std::string generateReport() const;
    
    /**
     * @brief 保存挂载配置
     * @param file_path 文件路径
     * @return 保存是否成功
     */
    bool saveMountConfig(const std::string& file_path) const;
    
    /**
     * @brief 加载挂载配置
     * @param file_path 文件路径
     * @return 加载是否成功
     */
    bool loadMountConfig(const std::string& file_path);
    
    /**
     * @brief 自动挂载所有配置的文件系统
     * @return 挂载是否成功
     */
    bool autoMountAll();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @class File
 * @brief 文件操作类
 * 
 * 提供统一的文件读写接口
 */
class File {
public:
    /**
     * @brief 构造函数
     */
    File();
    
    /**
     * @brief 析构函数
     */
    ~File();
    
    /**
     * @brief 打开文件
     * @param path 文件路径
     * @param mode 打开模式
     * @return 打开是否成功
     */
    bool open(const std::string& path, const std::string& mode);
    
    /**
     * @brief 关闭文件
     */
    void close();
    
    /**
     * @brief 读取文件数据
     * @param buffer 数据缓冲区
     * @param size 要读取的数据大小
     * @return 实际读取的字节数
     */
    ssize_t read(void* buffer, size_t size);
    
    /**
     * @brief 写入文件数据
     * @param buffer 数据缓冲区
     * @param size 要写入的数据大小
     * @return 实际写入的字节数
     */
    ssize_t write(const void* buffer, size_t size);
    
    /**
     * @brief 设置文件指针位置
     * @param offset 偏移量
     * @param whence 起始位置（SEEK_SET, SEEK_CUR, SEEK_END）
     * @return 新的文件指针位置
     */
    off_t seek(off_t offset, int whence);
    
    /**
     * @brief 获取文件大小
     * @return 文件大小
     */
    off_t size() const;
    
    /**
     * @brief 检查文件是否打开
     * @return 文件是否打开
     */
    bool isOpen() const;
    
    /**
     * @brief 获取文件信息
     * @return 文件信息
     */
    FileInfo getInfo() const;
    
    /**
     * @brief 刷新文件缓冲区
     * @return 刷新是否成功
     */
    bool flush();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @class Directory
 * @brief 目录操作类
 * 
 * 提供统一的目录遍历接口
 */
class Directory {
public:
    /**
     * @brief 构造函数
     */
    Directory();
    
    /**
     * @brief 析构函数
     */
    ~Directory();
    
    /**
     * @brief 打开目录
     * @param path 目录路径
     * @return 打开是否成功
     */
    bool open(const std::string& path);
    
    /**
     * @brief