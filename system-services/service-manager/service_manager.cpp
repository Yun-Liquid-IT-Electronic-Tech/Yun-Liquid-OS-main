/**
 * @file service_manager.cpp
 * @brief 系统服务管理器实现
 * @author 云流操作系统开发团队
 * @date 2026-02-04
 * @version 1.0.0
 */

#include "service_manager.h"
#include "../../platform_compat.h"
#include <algorithm>
#include <fstream>
#include <thread>
#include <chrono>
// 简单的配置解析函数
#include <sstream>
#include <map>

// 简单的键值对配置解析
std::map<std::string, std::string> parseSimpleConfig(const std::string& content) {
    std::map<std::string, std::string> config;
    std::istringstream stream(content);
    std::string line;
    
    while (std::getline(stream, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            config[key] = value;
        }
    }
    
    return config;
}

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
        
        // 设置服务状态为启动中
        status_.state = ServiceState::Starting;
        status_.start_time = std::chrono::system_clock::now();
        status_.last_error.clear();
        
        // Windows平台使用CreateProcess启动服务
        #ifdef _WIN32
            STARTUPINFO si;
            PROCESS_INFORMATION pi;
            
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));
            
            // 构建命令行
            std::string cmd_line = config_.executable_path;
            for (const auto& arg : config_.args) {
                cmd_line += " ";
                cmd_line += arg;
            }
            
            char* cmd_line_str = const_cast<char*>(cmd_line.c_str());
            
            // 启动进程
            if (!CreateProcess(NULL, cmd_line_str, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                status_.last_error = "创建进程失败";
                status_.state = ServiceState::Failed;
                return false;
            }
            
            status_.pid = pi.dwProcessId;
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            
        #else
            // Linux平台使用fork和exec
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
            }
        #endif
        
        status_.last_activity = status_.start_time;
        status_.restart_count++;
        
        // 等待进程启动
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 检查进程是否还在运行
        #ifdef _WIN32
            HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, status_.pid);
            if (process != NULL) {
                DWORD exit_code;
                if (GetExitCodeProcess(process, &exit_code) && exit_code == STILL_ACTIVE) {
                    status_.state = ServiceState::Running;
                    startMonitoring();
                    CloseHandle(process);
                    return true;
                }
                CloseHandle(process);
            }
        #else
            if (kill(status_.pid, 0) == 0) {
                status_.state = ServiceState::Running;
                startMonitoring();
                return true;
            }
        #endif
        
        status_.last_error = "进程启动后立即退出";
        status_.state = ServiceState::Failed;
        return false;
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
        
        #ifdef _WIN32
            // Windows平台终止进程
            HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, status_.pid);
            if (process == NULL) {
                status_.last_error = "打开进程句柄失败";
                status_.state = ServiceState::Failed;
                return false;
            }
            
            if (!TerminateProcess(process, 0)) {
                status_.last_error = "终止进程失败";
                status_.state = ServiceState::Failed;
                CloseHandle(process);
                return false;
            }
            
            WaitForSingleObject(process, config_.shutdown_timeout);
            CloseHandle(process);
            
        #else
            // Linux平台发送停止信号
            if (kill(status_.pid, SIGTERM) == -1) {
                status_.last_error = "发送停止信号失败";
                status_.state = ServiceState::Failed;
                return false;
            }
            
            // 等待进程退出
            int wait_status;
            pid_t result = waitpid(status_.pid, &wait_status, WNOHANG);
            
            if (result == -1) {
                status_.last_error = "等待进程退出失败";
                status_.state = ServiceState::Failed;
                return false;
            }
            
            if (result == 0) { // 进程还在运行，强制终止
                std::this_thread::sleep_for(std::chrono::milliseconds(config_.shutdown_timeout));
                
                if (kill(status_.pid, SIGKILL) == -1) {
                    status_.last_error = "强制终止进程失败";
                    status_.state = ServiceState::Failed;
                    return false;
                }
                
                waitpid(status_.pid, &wait_status, 0);
            }
        #endif
        
        status_.state = ServiceState::Stopped;
        status_.pid = -1;
        status_.uptime = std::chrono::seconds(0);
        
        // 停止监控线程
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
                            #ifdef _WIN32
                                HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, status_.pid);
                                if (process != NULL) {
                                    DWORD exit_code;
                                    if (GetExitCodeProcess(process, &exit_code) && exit_code == STILL_ACTIVE) {
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
                                        
                                        CloseHandle(process);
                                        break;
                                    }
                                    CloseHandle(process);
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
                            #else
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
                            #endif
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
            std::ofstream file(filename);
            if (!file.is_open()) {
                return false;
            }
            
            // 简单的文本格式保存
            for (const auto& pair : services_) {
                const auto& service = pair.second;
                ServiceConfig config = service->getConfig();
                ServiceStatus status = service->getStatus();
                
                file << "[service_state]" << std::endl;
                file << "name=" << config.name << std::endl;
                file << "state=" << static_cast<int>(status.state) << std::endl;
                file << "pid=" << status.pid << std::endl;
                file << "restart_count=" << status.restart_count << std::endl;
                file << "auto_start=" << (config.auto_start ? "true" : "false") << std::endl;
                file << std::endl;
            }
            
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
            
            // 读取文件内容
            std::string content((std::istreambuf_iterator<char>(file)), 
                               std::istreambuf_iterator<char>());
            
            // 简单的配置解析
            auto config_map = parseSimpleConfig(content);
            
            // 简化实现：启动所有自动启动的服务
            for (const auto& pair : services_) {
                ServiceConfig config = pair.second->getConfig();
                if (config.auto_start) {
                    pair.second->start();
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
        const std::string config_file = "/etc/cloudflow/services.conf";
        
        try {
            std::ifstream file(config_file);
            if (!file.is_open()) {
                // 配置文件不存在，使用默认配置
                return createDefaultServices();
            }
            
            // 读取文件内容
            std::string content((std::istreambuf_iterator<char>(file)), 
                               std::istreambuf_iterator<char>());
            
            // 简单的配置解析
            auto config_map = parseSimpleConfig(content);
            
            // 简化配置解析：直接创建默认服务
            return createDefaultServices();
        } catch (const std::exception& e) {
            return createDefaultServices();
        }
    }
    
    bool saveConfig() {
        // 简化实现：保存到固定文件
        const std::string config_file = "/etc/cloudflow/services.conf";
        
        try {
            std::ofstream file(config_file);
            if (!file.is_open()) {
                return false;
            }
            
            // 简单的文本格式保存
            for (const auto& pair : services_) {
                const auto& service = pair.second;
                ServiceConfig config = service->getConfig();
                
                file << "[service]" << std::endl;
                file << "name=" << config.name << std::endl;
                file << "description=" << config.description << std::endl;
                file << "executable_path=" << config.executable_path << std::endl;
                file << "auto_start=" << (config.auto_start ? "true" : "false") << std::endl;
                file << "restart_delay=" << config.restart_delay << std::endl;
                file << "max_restart_attempts=" << config.max_restart_attempts << std::endl;
                file << "working_directory=" << config.working_directory << std::endl;
                file << std::endl;
            }
            
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
ServiceManager::ServiceManager() : impl_(new Impl()) {}

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