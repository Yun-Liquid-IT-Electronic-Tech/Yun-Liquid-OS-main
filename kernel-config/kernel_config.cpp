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
    
    std::string getParameterValue(const std::string& param_name