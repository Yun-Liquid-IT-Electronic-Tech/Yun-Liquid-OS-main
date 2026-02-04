/**
 * @file filesystem.cpp
 * @brief 文件系统管理器实现
 * @author 云流操作系统开发团队
 * @date 2026-02-04
 * @version 1.0.0
 * 
 * 文件系统管理器的具体实现，包括挂载管理、文件操作、目录遍历等功能
 */

#include "filesystem.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <mutex>
#include <thread>
#include <system_error>
#include <fcntl.h>
#include <unistd.h>

namespace CloudFlow::FileSystem {

// 文件系统管理器实现类
class FileSystemManager::Impl {
public:
    Impl() : initialized_(false) {}
    
    ~Impl() {
        cleanup();
    }
    
    bool initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (initialized_) {
            return true;
        }
        
        try {
            // 初始化默认文件系统驱动程序
            initializeDefaultFileSystems();
            
            // 扫描已挂载的文件系统
            scanMounts();
            
            initialized_ = true;
            return true;
        } catch (const std::exception& e) {
            last_error_ = std::string("初始化失败: ") + e.what();
            return false;
        }
    }
    
    bool registerFileSystem(std::shared_ptr<IFileSystem> fs, FileSystemType fs_type) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!fs) {
            last_error_ = "文件系统驱动程序为空";
            return false;
        }
        
        file_systems_[fs_type] = fs;
        return true;
    }
    
    bool unregisterFileSystem(FileSystemType fs_type) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = file_systems_.find(fs_type);
        if (it != file_systems_.end()) {
            file_systems_.erase(it);
            return true;
        }
        
        last_error_ = "未找到指定的文件系统类型";
        return false;
    }
    
    bool mount(const std::string& device, const std::string& mount_point, 
               FileSystemType fs_type, const std::string& options) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!initialized_) {
            last_error_ = "文件系统管理器未初始化";
            return false;
        }
        
        auto it = file_systems_.find(fs_type);
        if (it == file_systems_.end()) {
            last_error_ = "不支持的文件系统类型";
            return false;
        }
        
        // 检查挂载点是否已存在
        if (mount_info_.find(mount_point) != mount_info_.end()) {
            last_error_ = "挂载点已被占用";
            return false;
        }
        
        // 创建挂载点目录
        if (!createDirectory(mount_point)) {
            last_error_ = "无法创建挂载点目录";
            return false;
        }
        
        // 调用文件系统驱动程序进行挂载
        if (it->second->mount(device, mount_point, options)) {
            MountInfo info;
            info.device = device;
            info.mount_point = mount_point;
            info.fs_type = fs_type;
            info.options = options;
            info.state = MountState::Mounted;
            
            // 获取文件系统统计信息
            auto stats = it->second->getStats(mount_point);
            info.total_size = stats.total_blocks * stats.block_size;
            info.free_size = stats.free_blocks * stats.block_size;
            info.used_size = info.total_size - info.free_size;
            
            mount_info_[mount_point] = info;
            
            // 通知挂载状态变化
            notifyMountStateChange(mount_point, MountState::Unmounted, MountState::Mounted);
            
            return true;
        } else {
            last_error_ = "挂载失败: " + it->second->getLastError();
            return false;
        }
    }
    
    bool unmount(const std::string& mount_point) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto mount_it = mount_info_.find(mount_point);
        if (mount_it == mount_info_.end()) {
            last_error_ = "未找到挂载点";
            return false;
        }
        
        auto fs_it = file_systems_.find(mount_it->second.fs_type);
        if (fs_it == file_systems_.end()) {
            last_error_ = "未找到对应的文件系统驱动程序";
            return false;
        }
        
        // 通知挂载状态变化
        notifyMountStateChange(mount_point, MountState::Mounted, MountState::Unmounting);
        
        if (fs_it->second->unmount(mount_point)) {
            mount_info_.erase(mount_it);
            
            // 通知挂载状态变化
            notifyMountStateChange(mount_point, MountState::Unmounting, MountState::Unmounted);
            
            return true;
        } else {
            mount_it->second.state = MountState::Error;
            last_error_ = "卸载失败: " + fs_it->second->getLastError();
            
            // 通知挂载状态变化
            notifyMountStateChange(mount_point, MountState::Unmounting, MountState::Error);
            
            return false;
        }
    }
    
    bool remount(const std::string& mount_point, const std::string& options) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto mount_it = mount_info_.find(mount_point);
        if (mount_it == mount_info_.end()) {
            last_error_ = "未找到挂载点";
            return false;
        }
        
        // 先卸载再挂载
        std::string device = mount_it->second.device;
        FileSystemType fs_type = mount_it->second.fs_type;
        
        if (!unmount(mount_point)) {
            return false;
        }
        
        return mount(device, mount_point, fs_type, options);
    }
    
    bool check(const std::string& device, FileSystemType fs_type) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = file_systems_.find(fs_type);
        if (it == file_systems_.end()) {
            last_error_ = "不支持的文件系统类型";
            return false;
        }
        
        return it->second->check(device);
    }
    
    bool format(const std::string& device, FileSystemType fs_type, const std::string& options) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = file_systems_.find(fs_type);
        if (it == file_systems_.end()) {
            last_error_ = "不支持的文件系统类型";
            return false;
        }
        
        return it->second->format(device, options);
    }
    
    std::vector<std::string> getMountPoints() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::string> mount_points;
        for (const auto& pair : mount_info_) {
            mount_points.push_back(pair.first);
        }
        
        return mount_points;
    }
    
    MountInfo getMountInfo(const std::string& mount_point) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = mount_info_.find(mount_point);
        if (it != mount_info_.end()) {
            return it->second;
        }
        
        MountInfo empty_info;
        empty_info.state = MountState::Unmounted;
        return empty_info;
    }
    
    FileSystemStats getStats(const std::string& path) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 查找路径对应的挂载点
        std::string mount_point = findMountPoint(path);
        if (mount_point.empty()) {
            last_error_ = "未找到对应的挂载点";
            return FileSystemStats{};
        }
        
        auto mount_it = mount_info_.find(mount_point);
        if (mount_it == mount_info_.end()) {
            last_error_ = "挂载点信息不存在";
            return FileSystemStats{};
        }
        
        auto fs_it = file_systems_.find(mount_it->second.fs_type);
        if (fs_it == file_systems_.end()) {
            last_error_ = "未找到对应的文件系统驱动程序";
            return FileSystemStats{};
        }
        
        return fs_it->second->getStats(path);
    }
    
    size_t scanFileSystems() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t count = 0;
        
        // 扫描系统挂载表
        std::ifstream mounts_file("/proc/mounts");
        if (mounts_file.is_open()) {
            std::string line;
            while (std::getline(mounts_file, line)) {
                std::istringstream iss(line);
                std::string device, mount_point, fs_type, options;
                
                if (iss >> device >> mount_point >> fs_type >> options) {
                    // 解析文件系统类型
                    FileSystemType type = parseFsType(fs_type);
                    if (type != FileSystemType::Unknown) {
                        // 检查是否已存在
                        if (mount_info_.find(mount_point) == mount_info_.end()) {
                            MountInfo info;
                            info.device = device;
                            info.mount_point = mount_point;
                            info.fs_type = type;
                            info.options = options;
                            info.state = MountState::Mounted;
                            
                            mount_info_[mount_point] = info;
                            count++;
                        }
                    }
                }
            }
        }
        
        return count;
    }
    
    std::vector<FileSystemType> getSupportedFileSystems() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<FileSystemType> supported_types;
        for (const auto& pair : file_systems_) {
            supported_types.push_back(pair.first);
        }
        
        return supported_types;
    }
    
    void addMountStateChangeListener(std::function<void(const std::string&