/**
 * @file service_manager.cpp
 * @brief 系统服务管理器实现
 * @author 云流操作系统开发团队
 * @date 2026-02-04
 * @version 1.0.0
 */

#include "service_manager.h"
#include <algorithm>
#include <fstream>
#include <thread>
#include <chrono>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <json/json.h>

namespace CloudFlow {
namespace System {

// 服务类实现
class Service {
public:
    Service(const ServiceConfig& config) 
        : config_(config)
        , status_{ServiceState::Stopped, -1, {}, {}, 0, "", 0, 0.0}
        , monitoring_thread_running_(false) {
    }
    
    ~Service() {
        stopMonitoring();
        if (status_.state == ServiceState::Running || status_.state == ServiceState::Starting) {
            stop();
        }
    }
    
    bool start() {
        if (status_.state == ServiceState::Running || status_.state == ServiceState::Starting) {
            return true; // 已经在运行或启动中
        }
        
        // 检查依赖服务
        if (!checkDependencies()) {
            status_.last_error = "依赖服务未就绪";
            status_.state = ServiceState::Failed;
            return false;
        }
        
        // 更新状态
        status_.state = ServiceState::Starting;
        status_.last_error.clear();
        
        // 创建子进程
        pid_t pid = fork();
        if (pid == -1) {
            status_.last_error = "创建进程失败";
            status_.state = ServiceState::Failed;
            return false;
        }
        
        if (pid == 0) { // 子进程
            // 设置工作目录
            if (!config_.working_directory.empty()) {
                if (chdir(config_.working_directory.c_str()) == -1) {
                    exit(EXIT_FAILURE);
                }
            }
            
            // 设置环境变量
            for (const auto& env : config_.environment) {
                setenv(env.first.c_str(), env.second.c_str(), 1);
            }
            
            // 准备参数
            std::vector<const char*> args;
            args.push_back(config_.executable_path.c_str());
            for (const auto& arg : config_.args) {
                args.push_back(arg.c_str());
            }
            args.push_back(nullptr);
            
            // 执行程序
            execvp(config_.executable_path.c_str(), const_cast<char* const*>(args.data()));
            
            // 如果执行失败
            exit(EXIT_FAILURE);
        } else { // 父进程
            status_.pid = pid;
            status_.start_time = std::chrono::system_clock::now();
            status_.last_activity = status_.start_time;
            status_.restart_count++;
            
            // 等待进程启动
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // 检查进程是否还在运行
            if (kill(pid, 0) == 0) {
                status_.state = ServiceState::Running;
                startMonitoring();
                return true;
            } else {
                status_.last_error = "进程启动后立即退出";
                status_.state = ServiceState::Failed;
                return false;
            }
        }
    }
    
    bool stop() {
        if (status_.state == ServiceState::Stopped || status_.state == ServiceState::Stopping) {
            return true; // 已经停止或正在停止
        }
        
        if (status_.pid == -1) {
            status_.state = ServiceState::Stopped;
            return true;
        }
        
        status_.state = ServiceState::Stopping;
        
        // 发送SIGTERM信号
        if (kill(status_.pid, SIGTERM) == 0) {
            // 等待进程退出
            for (int i = 0; i < 10; ++i) { // 最多等待5秒
                int status;
                pid_t result = waitpid(status_.pid, &status, WNOHANG);
                
                if (result == status_.pid) {
                    // 进程已退出
                    status_.pid = -1;
                    status_.state = ServiceState::Stopped;
                    stopMonitoring();
                    return true;
                } else if (result == 0) {
                    // 进程还在运行，继续等待
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                } else {
                    // 错误
                    break;
                }
            }
            
            // 如果进程没有正常退出，强制终止
            kill(status_.pid, SIGKILL);
            waitpid(status_.pid, nullptr, 0);
        }
        
        status_.pid = -1;
        status_.state = ServiceState::Stopped;
        stopMonitoring();
        return true;
    }
    
    bool restart() {
        if (!stop()) {
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.restart_delay));
        
        return start();
    }
    
    ServiceState getState() const {
        return status_.state;
    }
    
    ServiceStatus getStatus() const {
        return status_;
    }
    
    ServiceConfig getConfig() const {
        return config_;
    }
    
    void setConfig(const ServiceConfig& config) {
        config_ = config;
    }
    
    void updateStatus(const ServiceStatus& status) {
        ServiceState old_state = status_.state;
        status_ = status;
        
        if (old_state != status_.state && status_change_callback_) {
            status_change_callback_(config_.name, old_state, status_.state);
        }
    }
    
    void setStatusChangeCallback(std::function<void(ServiceState, ServiceState)> callback) {
        status_change_callback_ = std::move(callback);
    }
    
    void setErrorCallback(std::function<void(const std::string&)> callback) {
        error_callback_ = std::move(callback);
    }

private:
    bool checkDependencies() {
        // 这里应该检查依赖服务是否运行
        // 简化实现：假设所有依赖都满足
        return true;
    }
    
    void startMonitoring() {
        if (monitoring_thread_running_) {
            return;
        }
        
        monitoring_thread_running_ = true;
        monitoring_thread_ = std::thread([this]() {
            while (monitoring_thread_running_) {
                // 检查进程状态
                if (status_.pid != -1) {
                    if (kill(status_.pid, 0) == 0) {
                        // 进程还在运行
                        status_.last_activity = std::chrono::system_clock::now();
                        
                        // 更新资源使用情况（简化实现）
                        updateResourceUsage();
                    } else {
                        // 进程已退出
                        status_.state = ServiceState::Failed;
                        status_.last_error = "进程意外退出";
                        
                        if (error_callback_) {
                            error_callback_(status_.last_error);
                        }
                        
                        // 尝试自动重启
                        if (status_.restart_count < config_.max_restart_attempts) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(config_.restart_delay));
                            start();
                        }
                        
                        break;
                    }
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        });
    }
    
    void stopMonitoring() {
        monitoring_thread_running_ = false;
        if (monitoring_thread_.joinable()) {
            monitoring_thread_.join();
        }
    }
    
    void updateResourceUsage() {
        // 简化实现：模拟资源使用数据
        // 实际实现应该读取/proc文件系统或使用系统调用
        status_.memory_usage = 1024 + (rand() % 4096); // 1-5MB
        status_.cpu_usage = (rand() % 100) / 100.0;   // 0-100%
    }
    
    ServiceConfig config_;
    ServiceStatus status_;
    std::function<void(ServiceState, ServiceState)> status_change_callback_;
    std::function<void(const std::string&)> error_callback_;
    std::thread monitoring_thread_;
    bool monitoring_thread_running_;
};

// 服务管理器实现类
class ServiceManager::Impl {
public:
    Impl() : monitoring_interval_(1000), monitoring_running_(false) {
    }
    
    ~Impl() {
        stopMonitoring();
        stopAllServices();
    }
    
    bool initialize() {
        // 加载配置文件
        return loadConfig();
    }
    
    bool registerService(const ServiceConfig& config) {
        if (services_.find(config.name) != services_.end()) {
            return false; // 服务已存在
        }
        
        auto service = std::make_unique<Service>(config);
        service->setStatusChangeCallback([this, name = config.name](ServiceState old_state, ServiceState new_state) {
            if (status_change_callback_) {
                status_change_callback_(name, old_state, new_state);
            }
        });
        
        service->setErrorCallback([this, name = config.name](const std::string& error) {
            if (error_callback_) {
                error_callback_(name, error);
            }
        });
        
        services_[config.name] = std::move(service);
        
        // 保存配置
        return saveConfig();
    }
    
    bool unregisterService(const std::string& service_name) {
        auto it = services_.find(service_name);
        if (it == services_.end()) {
            return false;
        }
        
        // 停止服务
        it->second->stop();
        
        services_.erase(it);
        
        // 保存配置
        return saveConfig();
    }
    
    bool startService(const std::string& service_name) {
        auto it = services_.find(service_name);
        if (it == services_.end()) {
            return false;
        }
        
        return it->second->start();
    }
    
    bool stopService(const std::string& service_name) {
        auto it = services_.find(service_name);
        if (it == services_.end()) {
            return false;
        }
        
        return it->second->stop();
    }
    
    bool restartService(const std::string& service_name) {
        auto it = services_.find(service_name);
        if (it == services_.end()) {
            return false;
        }
        
        return it->second->restart();
    }
    
    ServiceStatus getServiceStatus(const std::string& service_name) const {
        auto it = services_.find(service_name);
        if (it == services_.end()) {
            return ServiceStatus{ServiceState::Unknown, -1, {}, {}, 0, "服务不存在", 0, 0.0};
        }
        
        return it->second->getStatus();
    }
    
    bool isServiceRunning(const std::string& service_name) const {
        auto it = services_.find(service_name);
        if (it == services_.end()) {
            return false;
        }
        
        ServiceState state = it->second->getState();
        return state == ServiceState::Running || state == ServiceState::Starting;
    }
    
    std::vector<std::string> getServiceNames() const {
        std::vector<std::string> names;
        names.reserve(services_.size());
        
        for (const auto& pair : services_) {
            names.push_back(pair.first);
        }
        
        return names;
    }
    
    ServiceConfig getServiceConfig(const std::string& service_name) const {
        auto it = services_.find(service_name);
        if (it == services_.end()) {
            return ServiceConfig{};
        }
        
        return it->second->getConfig();
    }
    
    bool setServiceConfig(const std::string& service_name, const ServiceConfig& config) {
        auto it = services_.find(service_name);
        if (it == services_.end()) {
            return false;
        }
        
        it->second->setConfig(config);
        return saveConfig();
    }
    
    bool enableService(const std::string& service_name) {
        auto it = services_.find(service_name);
        if (it == services_.end()) {
            return false;
        }
        
        ServiceConfig config = it->second->getConfig();
        config.auto_start = true;
        it->second->setConfig(config);
        
        return saveConfig();
    }
    
    bool disableService(const std::string& service_name) {
        auto it = services_.find(service_name);
        if (it == services_.end()) {
            return false;
        }
        
        ServiceConfig config = it->second->getConfig();
        config.auto_start = false;
        it->second->setConfig(config);
        
        return saveConfig();
    }
    
    bool startAllServices() {
        // 按优先级排序服务
        std::vector<std::string> service_names = getServiceNames();
        std::sort(service_names.begin(), service_names.end(), [this](const std::string& a, const std::string& b) {
            ServiceConfig config_a = getServiceConfig(a);
            ServiceConfig config_b = getServiceConfig(b);
            return static_cast<int>(config_a.priority) < static_cast<int>(config_b.priority);
        });
        
        bool all_started = true;
        for (const auto& name : service_names) {
            ServiceConfig config = getServiceConfig(name);
            if (config.auto_start) {
                if (!startService(name)) {
                    all_started = false;
                }
            }
        }
        
        return all_started;
    }
    
    bool stopAllServices() {
        bool all_stopped = true;
        for (const auto& pair : services_) {
            if (!pair.second->stop()) {
                all_stopped = false;
            }
        }
        
        return all_stopped;
    }
    
    bool reloadConfig() {
        return loadConfig();
    }
    
    void setStatusChangeCallback(std::function<void(const std::string&, ServiceState, ServiceState)> callback) {
        status_change_callback_ = std::move(callback);
    }
    
    void setErrorCallback(std::function<void(const std::string&, const std::string&)> callback) {
        error_callback_ = std::move(callback);
    }
    
    void startMonitoring(int interval = 1000) {
        monitoring_interval_ = interval;
        
        if (monitoring_running_) {
            return;
        }
        
        monitoring_running_ = true;
        monitoring_thread_ = std::thread([this]() {
            while (monitoring_running_) {
                // 监控所有服务状态
                for (const auto& pair : services_) {
                    // 这里可以添加更详细的状态检查
                    // 当前实现中服务已经自行监控
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(monitoring_interval_));
            }
        });
    }
    
    void stopMonitoring() {
        monitoring_running_ = false;
        if (monitoring_thread_.joinable()) {
            monitoring_thread_.join();
        }
    }
    
    bool saveServiceState(const std::string& filename) const {
        try {
            Json::Value root;
            Json::Value services_array(Json::arrayValue);
            
            for (const auto& pair : services_) {
                const auto& service = pair.second;
                Json::Value service_obj;
                
                ServiceConfig config = service->getConfig();
                ServiceStatus status = service->getStatus();
                
                service_obj["name"] = config.name;
                service_obj["state"] = static_cast<int>(status.state);
                service_obj["pid"] = status.pid;
                service_obj["restart_count"] = status.restart_count;
                service_obj["auto_start"] = config.auto_start;
                
                services_array.append(service_obj);
            }
            
            root["services"] = services_array;
            
            std::ofstream file(filename);
            if (!file.is_open()) {
                return false;
            }
            
            Json::StreamWriterBuilder builder;
            builder["indentation"] = "  ";
            std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
            writer->write(root, &file);
            
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }
    
    bool restoreServiceState(const std::string& filename) {
        try {
            std::ifstream file(filename);
            if (!file.is_open()) {
                return false;
            }
            
            Json::Value root;
            file >> root;
            
            const auto& services_array = root["services"];
            for (const auto& service_obj : services_array) {
                std::string name = service_obj["name"].asString();
                bool auto_start = service_obj["auto_start"].asBool();
                
                auto it = services_.find(name);
                if (it != services_.end() && auto_start) {
                    it->second->start();
                }
            }
            
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

private:
    bool loadConfig() {
        // 简化实现：从固定文件加载配置
        const std::string config_file = "/etc/cloudflow/services.json";
        
        try {
            std::ifstream file(config_file);
            if (!file.is_open()) {
                // 配置文件不存在，使用默认配置
                return createDefaultServices();
            }
            
            Json::Value root;
            file >> root;
            
            const auto& services_array = root["services"];
            for (const auto& service_obj : services_array) {
                ServiceConfig config;
                
                config.name = service_obj["name"].asString();
                config.description = service_obj["description"].asString();
                config.type = static_cast<ServiceType>(service_obj["type"].asInt());
                config.priority = static_cast<ServicePriority>(service_obj["priority"].asInt());
                config.executable_path = service_obj["executable_path"].asString();
                config.auto_start = service_obj["auto_start"].asBool();
                config.restart_delay = service_obj["restart_delay"].asInt();
                config.max_restart_attempts = service_obj["max_restart_attempts"].asInt();
                config.working_directory = service_obj["working_directory"].asString();
                
                // 解析参数
                const auto& args_array = service_obj["args"];
                for (const auto& arg : args_array) {
                    config.args.push_back(arg.asString());
                }
                
                // 解析依赖
                const auto& deps_array = service_obj["dependencies"];
                for (const auto& dep : deps_array) {
                    config.dependencies.push_back(dep.asString());
                }
                
                // 解析环境变量
                const auto& env_obj = service_obj["environment"];
                for (const auto& key : env_obj.getMemberNames()) {
                    config.environment[key] = env_obj[key].asString();
                }
                
                registerService(config);
            }
            
            return true;
        } catch (const std::exception& e) {
            return createDefaultServices();
        }
    }
    
    bool saveConfig() {
        // 简化实现：保存到固定文件
        const std::string config_file = "/etc/cloudflow/services.json";
        
        try {
            Json::Value root;
            Json::Value services_array(Json::arrayValue);
            
            for (const auto& pair : services_) {
                const auto& service = pair.second;
                ServiceConfig config = service->getConfig();
                
                Json::Value service_obj;
                
                service_obj["name"] = config.name;
                service_obj["description"] = config.description;
                service_obj["type"] = static_cast<int>(config.type);
                service_obj["priority"] = static_cast<int>(config.priority);
                service_obj["executable_path"] = config.executable_path;
                service_obj["auto_start"] = config.auto_start;
                service_obj["restart_delay"] = config.restart_delay;
                service_obj["max_restart_attempts"] = config.max_restart_attempts;
                service_obj["working_directory"] = config.working_directory;
                
                // 保存参数
                Json::Value args_array(Json::arrayValue);
                for (const auto& arg : config.args) {
                    args_array.append(arg);
                }
                service_obj["args"] = args_array;
                
                // 保存依赖
                Json::Value deps_array(Json::arrayValue);
                for (const auto& dep : config.dependencies) {
                    deps_array.append(dep);
                }
                service_obj["dependencies"] = deps_array;
                
                // 保存环境变量
                Json::Value env_obj;
                for (const auto& env : config.environment) {
                    env_obj[env.first] = env.second;
                }
                service_obj["environment"] = env_obj;
                
                services_array.append(service_obj);
            }
            
            root["services"] = services_array;
            
            std::ofstream file(config_file);
            if (!file.is_open()) {
                return false;
            }
            
            Json::StreamWriterBuilder builder;
            builder["indentation"] = "  ";
            std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
            writer->write(root, &file);
            
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }
    
    bool createDefaultServices() {
        // 创建一些默认的系统服务
        std::vector<ServiceConfig> default_services = {
            {
                "network", "网络服务", ServiceType::Network, ServicePriority::Critical,
                "/usr/sbin/networkd", {}, {}, true, 1000, 3, "/var/run", {}
            },
            {
                "storage", "存储服务", ServiceType::Storage, ServicePriority::High,
                "/usr/sbin/storaged", {}, {"network"}, true, 2000, 3, "/var/run", {}
            },
            {
                "desktop", "桌面环境", ServiceType::System, ServicePriority::Normal,
                "/usr/bin/desktop", {}, {"network", "storage"}, true, 3000, 5, "/home/user", {}
            }
        };
        
        for (const auto& config : default_services) {
            if (!registerService(config)) {
                return false;
            }
        }
        
        return true;
    }
    
    std::unordered_map<std::string, std::unique_ptr<Service>> services_;
    std::function<void(const std::string&, ServiceState, ServiceState)> status_change_callback_;
    std::function<void(const std::string&, const std::string&)> error_callback_;
    std::thread monitoring_thread_;
    int monitoring_interval_;
    bool monitoring_running_;
};

// ServiceManager 公共接口实现
ServiceManager::ServiceManager() : impl_(std::make_unique<Impl>()) {}

ServiceManager::~ServiceManager() = default;

bool ServiceManager::initialize() { return impl_->initialize(); }
bool ServiceManager::registerService(const ServiceConfig& config) { return impl_->registerService(config); }
bool ServiceManager::unregisterService(const std::string& service_name) { return impl_->unregisterService(service_name); }
bool ServiceManager::startService(const std::string& service_name) { return impl_->startService(service_name); }
bool ServiceManager::stopService(const std::string& service_name) { return impl_->stopService(service_name); }
bool ServiceManager::restartService(const std::string& service_name) { return impl_->restartService(service_name); }
ServiceStatus ServiceManager::getServiceStatus(const std::string& service_name) const { return impl_->getServiceStatus(service_name); }
bool ServiceManager::isServiceRunning(const std::string& service_name) const { return impl_->isServiceRunning(service_name); }
std::vector<std::string> ServiceManager::getServiceNames() const { return impl_->getServiceNames(); }
ServiceConfig ServiceManager::getServiceConfig(const std::string& service_name) const { return impl_->getServiceConfig(service_name); }
bool ServiceManager::setServiceConfig(const std::string& service_name, const ServiceConfig& config) { return impl_->setServiceConfig(service_name, config); }
bool ServiceManager::enableService(const std::string& service_name) { return impl_->enableService(service_name); }
bool ServiceManager::disableService(const std::string& service_name) { return impl_->disableService(service_name); }
bool ServiceManager::startAllServices() { return impl_->startAllServices(); }
bool ServiceManager::stopAllServices() { return impl_->stopAllServices(); }
bool ServiceManager::reloadConfig() { return impl_->reloadConfig(); }
void ServiceManager::setStatusChangeCallback(std::function<void(const std::string&, ServiceState, ServiceState)> callback) { impl_->setStatusChangeCallback(std::move(callback)); }
void ServiceManager::setErrorCallback(std::function<void(const std::string&, const std::string&)> callback) { impl_->setErrorCallback(std::move(callback)); }
void ServiceManager::startMonitoring(int interval) { impl_->startMonitoring(interval); }
void ServiceManager::stopMonitoring() { impl_->stopMonitoring(); }
bool ServiceManager::saveServiceState(const std::string& filename) const { return impl_->saveServiceState(filename); }
bool ServiceManager::restoreServiceState(const std::string& filename) { return impl_->restoreServiceState(filename); }

} // namespace System
} // namespace CloudFlow