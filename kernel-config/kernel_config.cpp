/**
 * @file kernel_config.cpp
 * @brief 内核配置管理器实现
 * @author 云流操作系统开发团队
 * @date 2026-02-04
 * @version 1.0.0
 */

#include "kernel_config.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#include <signal.h>
#include <json/json.h>

namespace CloudFlow::Kernel {

class KernelConfigManager::Impl {
public:
    Impl() : config_loaded_(false), requires_reboot_(false) {
        // 初始化默认配置
        initializeDefaultConfig();
    }
    
    ~Impl() {
        // 清理资源
    }
    
    bool initialize() {
        // 检测系统信息
        if (!detectSystemInfo()) {
            return false;
        }
        
        // 加载当前内核参数
        if (!loadCurrentParameters()) {
            return false;
        }
        
        // 加载当前模块信息
        if (!loadCurrentModules()) {
            return false;
        }
        
        return true;
    }
    
    bool loadConfig(const std::string& config_file) {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            return false;
        }
        
        Json::Value root;
        Json::CharReaderBuilder reader;
        std::string errors;
        
        if (!Json::parseFromStream(reader, file, &root, &errors)) {
            return false;
        }
        
        return parseConfig(root);
    }
    
    bool saveConfig(const std::string& config_file) const {
        Json::Value root;
        
        // 保存基本配置
        root["version"] = config_.version;
        root["arch"] = config_.arch;
        
        // 保存参数
        Json::Value params(Json::arrayValue);
        for (const auto& param : config_.parameters) {
            Json::Value param_obj;
            param_obj["name"] = param.name;
            param_obj["description"] = param.description;
            param_obj["type"] = static_cast<int>(param.type);
            param_obj["value"] = param.value;
            param_obj["default_value"] = param.default_value;
            param_obj["is_runtime"] = param.is_runtime;
            param_obj["is_required"] = param.is_required;
            
            if (!param.min_value.empty()) {
                param_obj["min_value"] = param.min_value;
            }
            if (!param.max_value.empty()) {
                param_obj["max_value"] = param.max_value;
            }
            
            params.append(param_obj);
        }
        root["parameters"] = params;
        
        // 保存模块
        Json::Value modules(Json::arrayValue);
        for (const auto& module : config_.modules) {
            Json::Value module_obj;
            module_obj["name"] = module.name;
            module_obj["description"] = module.description;
            module_obj["auto_load"] = module.auto_load;
            module_obj["is_builtin"] = module.is_builtin;
            
            if (!module.file_path.empty()) {
                module_obj["file_path"] = module.file_path;
            }
            
            modules.append(module_obj);
        }
        root["modules"] = modules;
        
        // 保存sysctl设置
        Json::Value sysctl_obj;
        for (const auto& setting : config_.sysctl_settings) {
            sysctl_obj[setting.first] = setting.second;
        }
        root["sysctl"] = sysctl_obj;
        
        // 写入文件
        std::ofstream file(config_file);
        if (!file.is_open()) {
            return false;
        }
        
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "    ";
        std::unique_ptr<Json::StreamWriter> json_writer(writer.newStreamWriter());
        json_writer->write(root, &file);
        
        return true;
    }
    
    bool applyConfig() {
        bool success = true;
        
        // 应用参数设置
        for (const auto& param : config_.parameters) {
            if (!applyParameter(param)) {
                success = false;
                // 记录错误但不停止应用其他参数
            }
        }
        
        // 应用模块加载
        for (const auto& module : config_.modules) {
            if (module.auto_load && !module.is_builtin) {
                if (!loadModuleInternal(module.name)) {
                    success = false;
                }
            }
        }
        
        // 应用sysctl设置
        if (!applySysctlSettings()) {
            success = false;
        }
        
        return success;
    }
    
    bool validateConfig() const {
        // 检查参数依赖关系
        for (const auto& param : config_.parameters) {
            for (const auto& dep : param.dependencies) {
                auto dep_param = std::find_if(config_.parameters.begin(), 
                                             config_.parameters.end(),
                                             [&](const KernelParameter& p) { 
                                                 return p.name == dep; 
                                             });
                
                if (dep_param == config_.parameters.end()) {
                    return false; // 依赖参数不存在
                }
                
                if (dep_param->value.empty() && dep_param->is_required) {
                    return false; // 必需依赖参数未设置
                }
            }
            
            // 检查冲突
            for (const auto& conflict : param.conflicts) {
                auto conflict_param = std::find_if(config_.parameters.begin(), 
                                                  config_.parameters.end(),
                                                  [&](const KernelParameter& p) { 
                                                      return p.name == conflict; 
                                                  });
                
                if (conflict_param != config_.parameters.end() && 
                    !conflict_param->value.empty()) {
                    return false; // 冲突参数已设置
                }
            }
        }
        
        return true;
    }
    
    std::string getParameterValue(const std::string& param_name) const {
        auto it = std::find_if(config_.parameters.begin(), 
                              config_.parameters.end(),
                              [&](const KernelParameter& p) { 
                                  return p.name == param_name; 
                              });
        
        if (it != config_.parameters.end()) {
            return it->value;
        }
        
        return "";
    }
    
    bool setParameterValue(const std::string& param_name, const std::string& value) {
        auto it = std::find_if(config_.parameters.begin(), 
                              config_.parameters.end(),
                              [&](const KernelParameter& p) { 
                                  return p.name == param_name; 
                              });
        
        if (it == config_.parameters.end()) {
            return false;
        }
        
        // 验证参数值
        if (!validateParameterValue(*it, value)) {
            return false;
        }
        
        it->value = value;
        
        // 通知监听器
        for (const auto& listener : param_change_listeners_) {
            listener(param_name, value);
        }
        
        return true;
    }
    
    bool loadModule(const std::string& module_name) {
        return loadModuleInternal(module_name);
    }
    
    bool unloadModule(const std::string& module_name) {
        return unloadModuleInternal(module_name);
    }
    
    std::vector<std::string> getLoadedModules() const {
        std::vector<std::string> loaded_modules;
        
        // 读取/proc/modules获取已加载模块
        std::ifstream modules_file("/proc/modules");
        if (!modules_file.is_open()) {
            return loaded_modules;
        }
        
        std::string line;
        while (std::getline(modules_file, line)) {
            std::istringstream iss(line);
            std::string module_name;
            iss >> module_name;
            loaded_modules.push_back(module_name);
        }
        
        return loaded_modules;
    }
    
    bool setSysctl(const std::string& key, const std::string& value) {
        config_.sysctl_settings[key] = value;
        return true;
    }
    
    std::string getSysctl(const std::string& key) const {
        auto it = config_.sysctl_settings.find(key);
        if (it != config_.sysctl_settings.end()) {
            return it->second;
        }
        return "";
    }
    
    bool applySysctlSettings() {
        bool success = true;
        
        for (const auto& setting : config_.sysctl_settings) {
            std::string command = "sysctl -w " + setting.first + "=" + setting.second;
            
            if (system(command.c_str()) != 0) {
                success = false;
            }
        }
        
        return success;
    }
    
    std::string getKernelVersion() const {
        return config_.version;
    }
    
    std::string getArchitecture() const {
        return config_.arch;
    }
    
    std::string generateReport() const {
        std::ostringstream report;
        
        report << "=== 内核配置报告 ===\n";
        report << "内核版本: " << config_.version << "\n";
        report << "系统架构: " << config_.arch << "\n\n";
        
        report << "参数配置:\n";
        for (const auto& param : config_.parameters) {
            report << "  " << param.name << ": " << param.value 
                   << " (默认: " << param.default_value << ")\n";
        }
        
        report << "\n模块配置:\n";
        for (const auto& module : config_.modules) {
            report << "  " << module.name 
                   << " [" << (module.auto_load ? "自动加载" : "手动加载") << "]\n";
        }
        
        report << "\nsysctl设置:\n";
        for (const auto& setting : config_.sysctl_settings) {
            report << "  " << setting.first << " = " << setting.second << "\n";
        }
        
        return report.str();
    }
    
    void resetToDefaults() {
        for (auto& param : config_.parameters) {
            param.value = param.default_value;
        }
        
        config_.sysctl_settings.clear();
    }
    
    bool requiresReboot() const {
        return requires_reboot_;
    }
    
    const std::vector<KernelParameter>& getParameters() const {
        return config_.parameters;
    }
    
    const std::vector<KernelModule>& getModules() const {
        return config_.modules;
    }
    
    void addParameterChangeListener(std::function<void(const std::string&, const std::string&)> callback) {
        param_change_listeners_.push_back(callback);
    }
    
    void addModuleStatusChangeListener(std::function<void(const std::string&, bool)> callback) {
        module_status_listeners_.push_back(callback);
    }

private:
    KernelConfig config_;
    bool config_loaded_;
    bool requires_reboot_;
    std::vector<std::function<void(const std::string&, const std::string&)>> param_change_listeners_;
    std::vector<std::function<void(const std::string&, bool)>> module_status_listeners_;
    
    void initializeDefaultConfig() {
        // 设置默认参数
        KernelParameter vm_swappiness = {
            "vm.swappiness", "内存交换倾向性", KernelParameterType::Integer, 
            "60", "60", "0", "100", false, true, {}, {}
        };
        
        KernelParameter net_ipv4_tcp_timestamps = {
            "net.ipv4.tcp_timestamps", "TCP时间戳", KernelParameterType::Boolean, 
            "1", "1", "", "", false, true, {}, {}
        };
        
        config_.parameters = {vm_swappiness, net_ipv4_tcp_timestamps};
    }
    
    bool detectSystemInfo() {
        struct utsname sys_info;
        if (uname(&sys_info) != 0) {
            return false;
        }
        
        config_.version = sys_info.release;
        config_.arch = sys_info.machine;
        
        return true;
    }
    
    bool loadCurrentParameters() {
        // 实现从/proc/sys读取当前参数值
        return true;
    }
    
    bool loadCurrentModules() {
        // 实现从/proc/modules读取当前模块信息
        return true;
    }
    
    bool parseConfig(const Json::Value& root) {
        try {
            // 解析基本配置
            if (root.isMember("version")) {
                config_.version = root["version"].asString();
            }
            
            if (root.isMember("arch")) {
                config_.arch = root["arch"].asString();
            }
            
            // 解析参数
            if (root.isMember("parameters")) {
                config_.parameters.clear();
                for (const auto& param_obj : root["parameters"]) {
                    KernelParameter param;
                    param.name = param_obj["name"].asString();
                    param.description = param_obj["description"].asString();
                    param.type = static_cast<KernelParameterType>(param_obj["type"].asInt());
                    param.value = param_obj["value"].asString();
                    param.default_value = param_obj["default_value"].asString();
                    param.is_runtime = param_obj["is_runtime"].asBool();
                    param.is_required = param_obj["is_required"].asBool();
                    
                    if (param_obj.isMember("min_value")) {
                        param.min_value = param_obj["min_value"].asString();
                    }
                    
                    if (param_obj.isMember("max_value")) {
                        param.max_value = param_obj["max_value"].asString();
                    }
                    
                    config_.parameters.push_back(param);
                }
            }
            
            // 解析模块
            if (root.isMember("modules")) {
                config_.modules.clear();
                for (const auto& module_obj : root["modules"]) {
                    KernelModule module;
                    module.name = module_obj["name"].asString();
                    module.description = module_obj["description"].asString();
                    module.auto_load = module_obj["auto_load"].asBool();
                    module.is_builtin = module_obj["is_builtin"].asBool();
                    
                    if (module_obj.isMember("file_path")) {
                        module.file_path = module_obj["file_path"].asString();
                    }
                    
                    config_.modules.push_back(module);
                }
            }
            
            // 解析sysctl设置
            if (root.isMember("sysctl")) {
                config_.sysctl_settings.clear();
                for (const auto& key : root["sysctl"].getMemberNames()) {
                    config_.sysctl_settings[key] = root["sysctl"][key].asString();
                }
            }
            
            config_loaded_ = true;
            return true;
            
        } catch (const std::exception& e) {
            return false;
        }
    }
    
    bool applyParameter(const KernelParameter& param) {
        if (!param.is_runtime) {
            requires_reboot_ = true;
            return true; // 非运行时参数需要重启，但应用成功
        }
        
        // 构建参数路径
        std::string param_path = "/proc/sys/" + param.name;
        std::replace(param_path.begin(), param_path.end(), '.', '/');
        
        // 写入参数值
        std::ofstream param_file(param_path);
        if (!param_file.is_open()) {
            return false;
        }
        
        param_file << param.value;
        return param_file.good();
    }
    
    bool validateParameterValue(const KernelParameter& param, const std::string& value) {
        switch (param.type) {
            case KernelParameterType::Integer: {
                try {
                    int int_value = std::stoi(value);
                    if (!param.min_value.empty()) {
                        int min_val = std::stoi(param.min_value);
                        if (int_value < min_val) return false;
                    }
                    if (!param.max_value.empty()) {
                        int max_val = std::stoi(param.max_value);
                        if (int_value > max_val) return false;
                    }
                } catch (...) {
                    return false;
                }
                break;
            }
            case KernelParameterType::Boolean:
                if (value != "0" && value != "1") {
                    return false;
                }
                break;
            default:
                break;
        }
        
        return true;
    }
    
    bool loadModuleInternal(const std::string& module_name) {
        std::string command = "modprobe " + module_name;
        
        if (system(command.c_str()) == 0) {
            // 通知监听器
            for (const auto& listener : module_status_listeners_) {
                listener(module_name, true);
            }
            return true;
        }
        
        return false;
    }
    
    bool unloadModuleInternal(const std::string& module_name) {
        std::string command = "modprobe -r " + module_name;
        
        if (system(command.c_str()) == 0) {
            // 通知监听器
            for (const auto& listener : module_status_listeners_) {
                listener(module_name, false);
            }
            return true;
        }
        
        return false;
    }
};

// KernelConfigManager 公共接口实现
KernelConfigManager::KernelConfigManager() : impl_(std::make_unique<Impl>()) {}

KernelConfigManager::~KernelConfigManager() = default;

bool KernelConfigManager::initialize() { return impl_->initialize(); }
bool KernelConfigManager::loadConfig(const std::string& config_file) { return impl_->loadConfig(config_file); }
bool KernelConfigManager::saveConfig(const std::string& config_file) const { return impl_->saveConfig(config_file); }
bool KernelConfigManager::applyConfig() { return impl_->applyConfig(); }
bool KernelConfigManager::validateConfig() const { return impl_->validateConfig(); }
std::string KernelConfigManager::getParameterValue(const std::string& param_name) const { return impl_->getParameterValue(param_name); }
bool KernelConfigManager::setParameterValue(const std::string& param_name, const std::string& value) { return impl_->setParameterValue(param_name, value); }
bool KernelConfigManager::loadModule(const std::string& module_name) { return impl_->loadModule(module_name); }
bool KernelConfigManager::unloadModule(const std::string& module_name) { return impl_->unloadModule(module_name); }
std::vector<std::string> KernelConfigManager::getLoadedModules() const { return impl_->getLoadedModules(); }
bool KernelConfigManager::setSysctl(const std::string& key, const std::string& value) { return impl_->setSysctl(key, value); }
std::string KernelConfigManager::getSysctl(const std::string& key) const { return impl_->getSysctl(key); }
bool KernelConfigManager::applySysctlSettings() { return impl_->applySysctlSettings(); }
std::string KernelConfigManager::getKernelVersion() const { return impl_->getKernelVersion(); }
std::string KernelConfigManager::getArchitecture() const { return impl_->getArchitecture(); }
std::string KernelConfigManager::generateReport() const { return impl_->generateReport(); }
void KernelConfigManager::resetToDefaults() { impl_->resetToDefaults(); }
bool KernelConfigManager::requiresReboot() const { return impl_->requiresReboot(); }
const std::vector<KernelParameter>& KernelConfigManager::getParameters() const { return impl_->getParameters(); }
const std::vector<KernelModule>& KernelConfigManager::getModules() const { return impl_->getModules(); }
void KernelConfigManager::addParameterChangeListener(std::function<void(const std::string&, const std::string&)> callback) { impl_->addParameterChangeListener(callback); }
void KernelConfigManager::addModuleStatusChangeListener(std::function<void(const std::string&, bool)> callback) { impl_->addModuleStatusChangeListener(callback); }

} // namespace CloudFlow::Kernel