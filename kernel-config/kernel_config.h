/**
 * @file kernel_config.h
 * @brief 内核配置管理器
 * @author 云流操作系统开发团队
 * @date 2026-02-04
 * @version 1.0.0
 * 
 * 负责管理Linux内核的配置参数、模块加载和系统调优
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <fstream>
#include <json/json.h>

namespace CloudFlow::Kernel {

/**
 * @enum KernelParameterType
 * @brief 内核参数类型
 */
enum class KernelParameterType {
    Integer,    ///< 整型参数
    Boolean,    ///< 布尔型参数
    String,     ///< 字符串参数
    Module,     ///< 内核模块
    Sysctl      ///< 系统控制参数
};

/**
 * @struct KernelParameter
 * @brief 内核参数配置
 */
struct KernelParameter {
    std::string name;           ///< 参数名称
    std::string description;    ///< 参数描述
    KernelParameterType type;   ///< 参数类型
    std::string value;          ///< 参数值
    std::string default_value;  ///< 默认值
    std::string min_value;      ///< 最小值（仅对整型有效）
    std::string max_value;      ///< 最大值（仅对整型有效）
    bool is_required;           ///< 是否必需
    bool is_runtime;            ///< 是否运行时可修改
    std::vector<std::string> dependencies;  ///< 依赖参数
    std::vector<std::string> conflicts;     ///< 冲突参数
};

/**
 * @struct KernelModule
 * @brief 内核模块配置
 */
struct KernelModule {
    std::string name;           ///< 模块名称
    std::string description;    ///< 模块描述
    std::string file_path;      ///< 模块文件路径
    std::vector<std::string> parameters;    ///< 模块参数
    std::vector<std::string> dependencies;  ///< 依赖模块
    bool auto_load;             ///< 是否自动加载
    bool is_builtin;            ///< 是否为内置模块
};

/**
 * @struct KernelConfig
 * @brief 内核配置集合
 */
struct KernelConfig {
    std::string version;                        ///< 内核版本
    std::string arch;                           ///< 架构
    std::vector<KernelParameter> parameters;    ///< 参数列表
    std::vector<KernelModule> modules;          ///< 模块列表
    std::unordered_map<std::string, std::string> sysctl_settings;  ///< sysctl设置
};

/**
 * @class KernelConfigManager
 * @brief 内核配置管理器
 * 
 * 负责管理内核配置参数的加载、验证、应用和持久化
 */
class KernelConfigManager {
public:
    /**
     * @brief 构造函数
     */
    KernelConfigManager();
    
    /**
     * @brief 析构函数
     */
    ~KernelConfigManager();
    
    /**
     * @brief 初始化配置管理器
     * @return 初始化是否成功
     */
    bool initialize();
    
    /**
     * @brief 从JSON文件加载配置
     * @param config_file 配置文件路径
     * @return 加载是否成功
     */
    bool loadConfig(const std::string& config_file);
    
    /**
     * @brief 保存配置到JSON文件
     * @param config_file 配置文件路径
     * @return 保存是否成功
     */
    bool saveConfig(const std::string& config_file) const;
    
    /**
     * @brief 应用内核配置
     * @return 应用是否成功
     */
    bool applyConfig();
    
    /**
     * @brief 验证配置的有效性
     * @return 配置是否有效
     */
    bool validateConfig() const;
    
    /**
     * @brief 获取当前内核参数值
     * @param param_name 参数名称
     * @return 参数值，如果参数不存在则返回空字符串
     */
    std::string getParameterValue(const std::string& param_name) const;
    
    /**
     * @brief 设置内核参数值
     * @param param_name 参数名称
     * @param value 参数值
     * @return 设置是否成功
     */
    bool setParameterValue(const std::string& param_name, const std::string& value);
    
    /**
     * @brief 加载内核模块
     * @param module_name 模块名称
     * @return 加载是否成功
     */
    bool loadModule(const std::string& module_name);
    
    /**
     * @brief 卸载内核模块
     * @param module_name 模块名称
     * @return 卸载是否成功
     */
    bool unloadModule(const std::string& module_name);
    
    /**
     * @brief 获取已加载的模块列表
     * @return 模块名称列表
     */
    std::vector<std::string> getLoadedModules() const;
    
    /**
     * @brief 设置sysctl参数
     * @param key 参数键
     * @param value 参数值
     * @return 设置是否成功
     */
    bool setSysctl(const std::string& key, const std::string& value);
    
    /**
     * @brief 获取sysctl参数值
     * @param key 参数键
     * @return 参数值
     */
    std::string getSysctl(const std::string& key) const;
    
    /**
     * @brief 应用所有sysctl设置
     * @return 应用是否成功
     */
    bool applySysctlSettings();
    
    /**
     * @brief 获取内核版本信息
     * @return 内核版本字符串
     */
    std::string getKernelVersion() const;
    
    /**
     * @brief 获取系统架构信息
     * @return 架构字符串
     */
    std::string getArchitecture() const;
    
    /**
     * @brief 生成内核配置报告
     * @return 配置报告字符串
     */
    std::string generateReport() const;
    
    /**
     * @brief 重置所有参数为默认值
     */
    void resetToDefaults();
    
    /**
     * @brief 检查配置是否需要重启
     * @return 是否需要重启
     */
    bool requiresReboot() const;
    
    /**
     * @brief 获取所有配置参数
     * @return 参数列表
     */
    const std::vector<KernelParameter>& getParameters() const;
    
    /**
     * @brief 获取所有内核模块
     * @return 模块列表
     */
    const std::vector<KernelModule>& getModules() const;
    
    /**
     * @brief 添加参数配置变更监听器
     * @param callback 变更回调函数
     */
    void addParameterChangeListener(std::function<void(const std::string&, const std::string&)> callback);
    
    /**
     * @brief 添加模块状态变更监听器
     * @param callback 变更回调函数
     */
    void addModuleStatusChangeListener(std::function<void(const std::string&, bool)> callback);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace CloudFlow::Kernel