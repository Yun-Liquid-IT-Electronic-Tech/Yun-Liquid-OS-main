/**
 * @file service_manager.h
 * @brief 系统服务管理器
 * @author 云流操作系统开发团队
 * @date 2026-02-04
 * @version 1.0.0
 * 
 * 负责管理系统服务的启动、停止、状态监控和依赖关系管理
 */

#ifndef CLOUDFLOW_SERVICE_MANAGER_H
#define CLOUDFLOW_SERVICE_MANAGER_H

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>

namespace CloudFlow {
namespace System {

// 前置声明
class Service;

/**
 * @brief 服务状态枚举
 */
enum class ServiceState {
    Stopped,        ///< 已停止
    Starting,       ///< 启动中
    Running,        ///< 运行中
    Stopping,       ///< 停止中
    Failed,         ///< 启动失败
    Unknown         ///< 未知状态
};

/**
 * @brief 服务类型枚举
 */
enum class ServiceType {
    System,         ///< 系统核心服务
    Network,        ///< 网络服务
    Storage,        ///< 存储服务
    User,           ///< 用户服务
    Application     ///< 应用服务
};

/**
 * @brief 服务启动优先级
 */
enum class ServicePriority {
    Critical = 0,   ///< 关键服务（最先启动）
    High = 1,       ///< 高优先级
    Normal = 2,     ///< 普通优先级
    Low = 3,        ///< 低优先级
    Idle = 4        ///< 空闲优先级（最后启动）
};

/**
 * @brief 服务配置信息
 */
struct ServiceConfig {
    std::string name;               ///< 服务名称
    std::string description;        ///< 服务描述
    ServiceType type;               ///< 服务类型
    ServicePriority priority;       ///< 启动优先级
    std::string executable_path;    ///< 可执行文件路径
    std::vector<std::string> args;  ///< 启动参数
    std::vector<std::string> dependencies; ///< 依赖服务
    bool auto_start;                ///< 是否自动启动
    int restart_delay;              ///< 重启延迟（毫秒）
    int max_restart_attempts;       ///< 最大重启