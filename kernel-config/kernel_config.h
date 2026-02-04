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
 * 负责管理内核配置参数的加载、验证、应用和持久