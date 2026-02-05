/**
 * @file boot_params.cpp
 * @brief 启动参数管理器实现
 * @author 云流操作系统开发团队
 * @date 2026-02-04
 * @version 1.0.0
 * 
 * 启动参数管理器的具体实现
 */

#include "boot_params.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <unordered_map>

namespace CloudFlow::Boot {

// 启动参数管理器实现类
class BootParams::Impl {
public:
    Impl() {
        resetToDefaults();
    }
    
    ~Impl() = default;
    
    bool parseCommandLine(const std::string& cmdline) {
        try {
            // 清空当前参数
            kernel_params_.custom_params.clear();
            
            // 使用正则表达式解析命令行参数
            std::regex param_regex(R"((\S+)=([^\s]*)|(\S+))");
            std::sregex_iterator it(cmdline.begin(), cmdline.end(), param_regex);
            std::sregex_iterator end;
            
            while (it != end) {
                std::smatch match = *it;
                
                if (match[1].matched && match[2].matched) {
                    // 键值对参数
                    std::string key = match[1].str();
                    std::string value = match[2].str();
                    
                    // 处理特殊参数
                    if (key == "root") {
                        kernel_params_.root_device = value;
                    } else if (key == "rootfstype") {
                        kernel_params_.root_fstype = value;
                    } else if (key == "console") {
                        kernel_params_.console = value;
                    } else if (key == "language") {
                        kernel_params_.language = value;
                    } else if (key == "timezone") {
                        kernel_params_.timezone = value;
                    } else if (key == "mem") {
                        kernel_params_.mem_limit = parseMemorySize(value);
                    } else if (key == "mem_offset") {
                        kernel_params_.mem_offset = parseMemorySize(value);
                    } else {
                        // 添加到自定义参数
                        kernel_params_.custom_params[key] = value;
                    }
                } else if (match[3].matched) {
                    // 标志参数
                    std::string flag = match[3].str();
                    
                    if (flag == "quiet") {
                        kernel_params_.quiet = true;
                    } else if (flag == "verbose") {
                        kernel_params_.verbose = true;
                    } else if (flag == "debug") {
                        kernel_params_.debug = true;
                    } else if (flag == "single") {
                        kernel_params_.single_user = true;
                    } else if (flag == "network") {
                        kernel_params_.network = true;
                    }
                }
                
                ++it;
            }
            
            return true;
        } catch (const std::exception& e) {
            last_error_ = std::string("命令行解析失败: ") + e.what();
            return false;
        }
    }
    
    bool loadFromConfig(const std::string& config_file) {
        try {
            std::ifstream file(config_file);
            if (!file.is_open()) {
                last_error_ = "无法打开配置文件: " + config_file;
                return false;
            }
            
            std::string line;
            while (std::getline(file, line)) {
                // 跳过注释和空行
                if (line.empty() || line[0] == '#') {
                    continue;
                }
                
                // 解析配置行
                size_t pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string key = line.substr(0, pos);
                    std::string value = line.substr(pos + 1);
                    
                    // 去除前后空格
                    key.erase(0, key.find_first_not_of(" \t"));
                    key.erase(key.find_last_not_of(" \t") + 1);
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \t") + 1);
                    
                    // 处理配置项
                    if (key == "kernel") {
                        kernel_params_.kernel_path = value;
                    } else if (key == "initrd") {
                        kernel_params_.initrd_path = value;
                    } else if (key == "root") {
                        kernel_params_.root_device = value;
                    } else if (key == "rootfstype") {
                        kernel_params_.root_fstype = value;
                    } else if (key == "console") {
                        kernel_params_.console = value;
                    } else if (key == "language") {
                        kernel_params_.language = value;
                    } else if (key == "timezone") {
                        kernel_params_.timezone = value;
                    } else if (key == "mem_limit") {
                        kernel_params_.mem_limit = std::stoull(value);
                    } else if (key == "mem_offset") {
                        kernel_params_.mem_offset = std::stoull(value);
                    } else if (key == "quiet") {
                        kernel_params_.quiet = (value == "true" || value == "1");
                    } else if (key == "verbose") {
                        kernel_params_.verbose = (value == "true" || value == "1");
                    } else if (key == "debug") {
                        kernel_params_.debug = (value == "true" || value == "1");
                    } else if (key == "single_user") {
                        kernel_params_.single_user = (value == "true" || value == "1");
                    } else if (key == "network") {
                        kernel_params_.network = (value == "true" || value == "1");
                    }
                }
            }
            
            return true;
        } catch (const std::exception& e) {
            last_error_ = std::string("配置文件加载失败: ") + e.what();
            return false;
        }
    }
    
    bool saveToConfig(const std::string& config_file) const {
        try {
            std::ofstream file(config_file);
            if (!file.is_open()) {
                last_error_ = "无法创建配置文件: " + config_file;
                return false;
            }
            
            // 写入配置头
            file << "# 云流操作系统启动配置\n";
            file << "# 生成时间: " << boot_info_.boot_time << "\n\n";
            
            // 写入内核参数
            if (!kernel_params_.kernel_path.empty()) {
                file << "kernel=" << kernel_params_.kernel_path << "\n";
            }
            if (!kernel_params_.initrd_path.empty()) {
                file << "initrd=" << kernel_params_.initrd_path << "\n";
            }
            if (!kernel_params_.root_device.empty()) {
                file << "root=" << kernel_params_.root_device << "\n";
            }
            if (!kernel_params_.root_fstype.empty()) {
                file << "rootfstype=" << kernel_params_.root_fstype << "\n";
            }
            if (!kernel_params_.console.empty()) {
                file << "console=" << kernel_params_.console << "\n";
            }
            if (!kernel_params_.language.empty()) {
                file << "language=" << kernel_params_.language << "\n";
            }
            if (!kernel_params_.timezone.empty()) {
                file << "timezone=" << kernel_params_.timezone << "\n";
            }
            
            // 写入内存设置
            if (kernel_params_.mem_limit > 0) {
                file << "mem_limit=" << kernel_params_.mem_limit << "\n";
            }
            if (kernel_params_.mem_offset > 0) {
                file << "mem_offset=" << kernel_params_.mem_offset << "\n";
            }
            
            // 写入标志参数
            file << "quiet=" << (kernel_params_.quiet ? "true" : "false") << "\n";
            file << "verbose=" << (kernel_params_.verbose ? "true" : "false") << "\n";
            file << "debug=" << (kernel_params_.debug ? "true" : "false") << "\n";
            file << "single_user=" << (kernel_params_.single_user ? "true" : "false") << "\n";
            file << "network=" << (kernel_params_.network ? "true" : "false") << "\n";
            
            return true;
        } catch (const std::exception& e) {
            last_error_ = std::string("配置文件保存失败: ") + e.what();
            return false;
        }
    }
    
    bool validate() const {
        // 验证内核路径
        if (kernel_params_.kernel_path.empty()) {
            last_error_ = "内核路径不能为空";
            return false;
        }
        
        // 验证根设备
        if (kernel_params_.root_device.empty()) {
            last_error_ = "根设备不能为空";
            return false;
        }
        
        // 验证内存设置
        if (kernel_params_.mem_limit > 0 && kernel_params_.mem_limit < 1024 * 1024) {
            last_error_ = "内存限制太小";
            return false;
        }
        
        return true;
    }
    
    const BootInfo& getBootInfo() const {
        return boot_info_;
    }
    
    void setBootInfo(const BootInfo& info) {
        boot_info_ = info;
    }
    
    const KernelParameters& getKernelParams() const {
        return kernel_params_;
    }
    
    void setKernelParams(const KernelParameters& params) {
        kernel_params_ = params;
    }
    
    const MultibootInfo& getMultibootInfo() const {
        return multiboot_info_;
    }
    
    void setMultibootInfo(const MultibootInfo& info) {
        multiboot_info_ = info;
    }
    
    std::string generateCommandLine() const {
        std::ostringstream cmdline;
        
        // 添加根设备参数
        if (!kernel_params_.root_device.empty()) {
            cmdline << "root=" << kernel_params_.root_device << " ";
        }
        
        // 添加根文件系统类型
        if (!kernel_params_.root_fstype.empty()) {
            cmdline << "rootfstype=" << kernel_params_.root_fstype << " ";
        }
        
        // 添加控制台参数
        if (!kernel_params_.console.empty()) {
            cmdline << "console=" << kernel_params_.console << " ";
        }
        
        // 添加语言参数
        if (!kernel_params_.language.empty()) {
            cmdline << "language=" << kernel_params_.language << " ";
        }
        
        // 添加时区参数
        if (!kernel_params_.timezone.empty()) {
            cmdline << "timezone=" << kernel_params_.timezone << " ";
        }
        
        // 添加内存参数
        if (kernel_params_.mem_limit > 0) {
            cmdline << "mem=" << kernel_params_.mem_limit << " ";
        }
        
        if (kernel_params_.mem_offset > 0) {
            cmdline << "mem_offset=" << kernel_params_.mem_offset << " ";
        }
        
        // 添加标志参数
        if (kernel_params_.quiet) {
            cmdline << "quiet ";
        }
        
        if (kernel_params_.verbose) {
            cmdline << "verbose ";
        }
        
        if (kernel_params_.debug) {
            cmdline << "debug ";
        }
        
        if (kernel_params_.single_user) {
            cmdline << "single ";
        }
        
        if (kernel_params_.network) {
            cmdline << "network ";
        }
        
        // 添加自定义参数
        for (const auto& param : kernel_params_.custom_params) {
            cmdline << param.first << "=" << param.second << " ";
        }
        
        std::string result = cmdline.str();
        if (!result.empty() && result.back() == ' ') {
            result.pop_back();
        }
        
        return result;
    }
    
    void addCustomParam(const std::string& key, const std::string& value) {
        kernel_params_.custom_params[key] = value;
    }
    
    std::string getCustomParam(const std::string& key) const {
        auto it = kernel_params_.custom_params.find(key);
        if (it != kernel_params_.custom_params.end()) {
            return it->second;
        }
        return "";
    }
    
    bool hasParam(const std::string& key) const {
        return kernel_params_.custom_params.find(key) != kernel_params_.custom_params.end();
    }
    
    void resetToDefaults() {
        // 重置启动信息
        boot_info_ = BootInfo{};
        boot_info_.boot_mode = BootMode::Normal;
        boot_info_.kernel_type = KernelImageType::Unknown;
        boot_info_.architecture = "x86_64";
        boot_info_.platform = "PC";
        boot_info_.boot_time = 0;
        boot_info_.bootloader_version = "1.0.0";
        
        // 重置内核参数
        kernel_params_ = KernelParameters{};
        kernel_params_.kernel_path = "/boot/vmlinuz";
        kernel_params_.initrd_path = "/boot/initrd.img";
        kernel_params_.root_device = "/dev/sda1";
        kernel_params_.root_fstype = "ext4";
        kernel_params_.console = "tty0";
        kernel_params_.language = "zh_CN.UTF-8";
        kernel_params_.timezone = "Asia/Shanghai";
        kernel_params_.quiet = false;
        kernel_params_.verbose = false;
        kernel_params_.debug = false;
        kernel_params_.single_user = false;
        kernel_params_.network = false;
        
        // 重置多重启动信息
        multiboot_info_ = MultibootInfo{};
    }
    
    std::string getLastError() const {
        return last_error_;
    }

private:
    uint64_t parseMemorySize(const std::string& size_str) {
        if (size_str.empty()) {
            return 0;
        }
        
        uint64_t multiplier = 1;
        std::string number_str = size_str;
        
        // 检查单位
        char last_char = size_str.back();
        if (last_char == 'K' || last_char == 'k') {
            multiplier = 1024;
            number_str = size_str.substr(0, size_str.length() - 1);
        } else if (last_char == 'M' || last_char == 'm') {
            multiplier = 1024 * 1024;
            number_str = size_str.substr(0, size_str.length() - 1);
        } else if (last_char == 'G' || last_char == 'g') {
            multiplier = 1024 * 1024 * 1024;
            number_str = size_str.substr(0, size_str.length() - 1);
        }
        
        try {
            uint64_t value = std::stoull(number_str);
            return value * multiplier;
        } catch (const std::exception&) {
            return 0;
        }
    }

private:
    BootInfo boot_info_;
    KernelParameters kernel_params_;
    MultibootInfo multiboot_info_;
    mutable std::string last_error_;
};

// BootParams 公共接口实现
BootParams::BootParams() : impl_(std::make_unique<Impl>()) {}

BootParams::~BootParams() = default;

bool BootParams::parseCommandLine(const std::string& cmdline) {
    return impl_->parseCommandLine(cmdline);
}

bool BootParams::loadFromConfig(const std::string& config_file) {
    return impl_->loadFromConfig(config_file);
}

bool BootParams::saveToConfig(const std::string& config_file) const {
    return impl_->saveToConfig(config_file);
}

bool BootParams::validate() const {
    return impl_->validate();
}

const BootInfo& BootParams::getBootInfo() const {
    return impl_->getBootInfo();
}

void BootParams::setBootInfo(const BootInfo& info) {
    impl_->setBootInfo(info);
}

const KernelParameters& BootParams::getKernelParams() const {
    return impl_->getKernelParams();
}

void BootParams::setKernelParams(const KernelParameters& params) {
    impl_->setKernelParams(params);
}

const MultibootInfo& BootParams::getMultibootInfo() const {
    return impl_->getMultibootInfo();
}

void BootParams::setMultibootInfo(const MultibootInfo& info) {
    impl_->setMultibootInfo(info);
}

std::string BootParams::generateCommandLine() const {
    return impl_->generateCommandLine();
}

void BootParams::addCustomParam(const std::string& key, const std::string& value) {
    impl_->addCustomParam(key, value);
}

std::string BootParams::getCustomParam(const std::string& key) const {
    return impl_->getCustomParam(key);
}

bool BootParams::hasParam(const std::string& key) const {
    return impl_->hasParam(key);
}

void BootParams::resetToDefaults() {
    impl_->resetToDefaults();
}

std::string BootParams::getLastError() const {
    return impl_->getLastError();
}

} // namespace CloudFlow::Boot