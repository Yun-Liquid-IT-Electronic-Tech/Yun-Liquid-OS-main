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
    
    void addMountStateChangeListener(std::function<void(const std::string&, MountState, MountState)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        mount_state_listeners_.push_back(callback);
    }
    
    void addFileSystemErrorListener(std::function<void(const std::string&, const std::string&)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        error_listeners_.push_back(callback);
    }
    
    std::string generateReport() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::ostringstream report;
        report << "文件系统管理器报告\n";
        report << "==================\n\n";
        
        report << "初始化状态: " << (initialized_ ? "已初始化" : "未初始化") << "\n";
        report << "支持的文件系统类型数量: " << file_systems_.size() << "\n";
        report << "当前挂载点数量: " << mount_info_.size() << "\n\n";
        
        report << "挂载点详情:\n";
        for (const auto& pair : mount_info_) {
            const MountInfo& info = pair.second;
            report << "  挂载点: " << info.mount_point << "\n";
            report << "    设备: " << info.device << "\n";
            report << "    文件系统: " << fsTypeToString(info.fs_type) << "\n";
            report << "    状态: " << mountStateToString(info.state) << "\n";
            report << "    总空间: " << info.total_size / (1024 * 1024) << " MB\n";
            report << "    可用空间: " << info.free_size / (1024 * 1024) << " MB\n";
            report << "    已用空间: " << info.used_size / (1024 * 1024) << " MB\n\n";
        }
        
        return report.str();
    }
    
    bool saveMountConfig(const std::string& file_path) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::ofstream config_file(file_path);
        if (!config_file.is_open()) {
            last_error_ = "无法打开配置文件";
            return false;
        }
        
        for (const auto& pair : mount_info_) {
            const MountInfo& info = pair.second;
            config_file << info.device << " " << info.mount_point << " " 
                       << fsTypeToString(info.fs_type) << " " << info.options << "\n";
        }
        
        return true;
    }
    
    bool loadMountConfig(const std::string& file_path) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::ifstream config_file(file_path);
        if (!config_file.is_open()) {
            last_error_ = "无法打开配置文件";
            return false;
        }
        
        std::string line;
        while (std::getline(config_file, line)) {
            std::istringstream iss(line);
            std::string device, mount_point, fs_type_str, options;
            
            if (iss >> device >> mount_point >> fs_type_str >> options) {
                FileSystemType fs_type = parseFsType(fs_type_str);
                if (fs_type != FileSystemType::Unknown) {
                    mount(device, mount_point, fs_type, options);
                }
            }
        }
        
        return true;
    }
    
    bool autoMountAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 扫描并自动挂载所有配置的文件系统
        size_t mounted_count = 0;
        
        // 这里可以添加自动挂载逻辑，比如根据/etc/fstab配置
        // 目前简化实现，只挂载根文件系统
        
        return mounted_count > 0;
    }
    
    std::string getLastError() const {
        return last_error_;
    }

private:
    void initializeDefaultFileSystems() {
        // 这里可以添加默认的文件系统驱动程序
        // 目前为空实现，实际项目中需要根据具体需求添加
    }
    
    void scanMounts() {
        // 扫描当前系统的挂载信息
        scanFileSystems();
    }
    
    bool createDirectory(const std::string& path) {
        // 简化实现，实际项目中需要更完善的目录创建逻辑
        return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
    }
    
    std::string findMountPoint(const std::string& path) const {
        // 查找路径对应的挂载点
        for (const auto& pair : mount_info_) {
            if (path.find(pair.first) == 0) {
                return pair.first;
            }
        }
        return "";
    }
    
    FileSystemType parseFsType(const std::string& fs_type_str) const {
        if (fs_type_str == "ext4") return FileSystemType::Ext4;
        if (fs_type_str == "xfs") return FileSystemType::XFS;
        if (fs_type_str == "btrfs") return FileSystemType::Btrfs;
        if (fs_type_str == "ntfs") return FileSystemType::NTFS;
        if (fs_type_str == "vfat") return FileSystemType::FAT32;
        if (fs_type_str == "exfat") return FileSystemType::ExFAT;
        return FileSystemType::Unknown;
    }
    
    std::string fsTypeToString(FileSystemType type) const {
        switch (type) {
            case FileSystemType::Ext4: return "ext4";
            case FileSystemType::XFS: return "xfs";
            case FileSystemType::Btrfs: return "btrfs";
            case FileSystemType::NTFS: return "ntfs";
            case FileSystemType::FAT32: return "vfat";
            case FileSystemType::ExFAT: return "exfat";
            default: return "unknown";
        }
    }
    
    std::string mountStateToString(MountState state) const {
        switch (state) {
            case MountState::Unmounted: return "未挂载";
            case MountState::Mounting: return "挂载中";
            case MountState::Mounted: return "已挂载";
            case MountState::Unmounting: return "卸载中";
            case MountState::Error: return "错误";
            default: return "未知";
        }
    }
    
    void notifyMountStateChange(const std::string& mount_point, MountState old_state, MountState new_state) {
        for (const auto& listener : mount_state_listeners_) {
            listener(mount_point, old_state, new_state);
        }
    }
    
    void notifyFileSystemError(const std::string& mount_point, const std::string& error) {
        for (const auto& listener : error_listeners_) {
            listener(mount_point, error);
        }
    }
    
    void cleanup() {
        // 卸载所有挂载的文件系统
        for (const auto& mount_point : getMountPoints()) {
            unmount(mount_point);
        }
        
        file_systems_.clear();
        mount_info_.clear();
        initialized_ = false;
    }

private:
    mutable std::mutex mutex_;
    bool initialized_;
    std::unordered_map<FileSystemType, std::shared_ptr<IFileSystem>> file_systems_;
    std::unordered_map<std::string, MountInfo> mount_info_;
    std::vector<std::function<void(const std::string&, MountState, MountState)>> mount_state_listeners_;
    std::vector<std::function<void(const std::string&, const std::string&)>> error_listeners_;
    mutable std::string last_error_;
