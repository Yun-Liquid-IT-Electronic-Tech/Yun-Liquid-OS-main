/**
 * @file device_driver.h
 * @brief 设备驱动程序框架
 * @author 云流操作系统开发团队
 * @date 2026-02-04
 * @version 1.0.0
 * 
 * 提供统一的设备驱动程序接口，支持热插拔、电源管理、错误处理等功能
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace CloudFlow::Device {

/**
 * @enum DeviceType
 * @brief 设备类型
 */
enum class DeviceType {
    Unknown,        ///< 未知设备
    Block,          ///< 块设备（硬盘、SSD等）
    Character,      ///< 字符设备（终端、串口等）
    Network,        ///< 网络设备
    Graphics,       ///< 图形设备
    Audio,          ///< 音频设备
    Input,          ///< 输入设备
    Storage,        ///< 存储设备
    USB,            ///< USB设备
    PCI,            ///< PCI设备
    Virtual         ///< 虚拟设备
};

/**
 * @enum DeviceState
 * @brief 设备状态
 */
enum class DeviceState {
    Unknown,        ///< 未知状态
    Attached,       ///< 设备已连接
    Detached,       ///< 设备已断开
    Initializing,   ///< 初始化中
    Ready,          ///< 准备就绪
    Running,        ///< 运行中
    Suspended,      ///< 挂起
    Error,          ///< 错误状态
    Removed         ///< 设备已移除
};

/**
 * @enum PowerState
 * @brief 电源状态
 */
enum class PowerState {
    Unknown,        ///< 未知电源状态
    FullOn,         ///< 全功率运行
    LowPower,       ///< 低功耗模式
    Standby,        ///< 待机模式
    Sleep,          ///< 睡眠模式
    Off             ///< 关闭
};

/**
 * @struct DeviceInfo
 * @brief 设备信息
 */
struct DeviceInfo {
    std::string device_id;          ///< 设备唯一标识
    std::string name;               ///< 设备名称
    std::string description;        ///< 设备描述
    DeviceType type;                ///< 设备类型
    std::string vendor;             ///< 供应商
    std::string model;              ///< 型号
    std::string version;            ///< 版本
    std::string bus_info;           ///< 总线信息
    std::vector<std::string> capabilities;  ///< 设备能力
    std::unordered_map<std::string, std::string> properties;  ///< 设备属性
};

/**
 * @struct DeviceOperation
 * @brief 设备操作
 */
struct DeviceOperation {
    std::string name;               ///< 操作名称
    std::function<bool()> handler;  ///< 操作处理函数
    std::string description;        ///< 操作描述
};

/**
 * @class IDeviceDriver
 * @brief 设备驱动程序接口
 * 
 * 所有设备驱动程序必须实现的接口
 */
class IDeviceDriver {
public:
    virtual ~IDeviceDriver() = default;
    
    /**
     * @brief 初始化设备驱动程序
     * @return 初始化是否成功
     */
    virtual bool initialize() = 0;
    
    /**
     * @brief 启动设备
     * @return 启动是否成功
     */
    virtual bool start() = 0;
    
    /**
     * @brief 停止设备
     * @return 停止是否成功
     */
    virtual bool stop() = 0;
    
    /**
     * @brief 挂起设备（进入低功耗模式）
     * @return 挂起是否成功
     */
    virtual bool suspend() = 0;
    
    /**
     * @brief 恢复设备（从低功耗模式唤醒）
     * @return 恢复是否成功
     */
    virtual bool resume() = 0;
    
    /**
     * @brief 获取设备信息
     * @return 设备信息
     */
    virtual DeviceInfo getDeviceInfo() const = 0;
    
    /**
     * @brief 获取设备状态
     * @return 设备状态
     */
    virtual DeviceState getDeviceState() const = 0;
    
    /**
     * @brief 获取电源状态
     * @return 电源状态
     */
    virtual PowerState getPowerState() const = 0;
    
    /**
     * @brief 设置电源状态
     * @param state 目标电源状态
     * @return 设置是否成功
     */
    virtual bool setPowerState(PowerState state) = 0;
    
    /**
     * @brief 读取设备数据
     * @param buffer 数据缓冲区
     * @param size 要读取的数据大小
     * @param offset 偏移量
     * @return 实际读取的字节数，-1表示错误
     */
    virtual ssize_t read(void* buffer, size_t size, off_t offset = 0) = 0;
    
    /**
     * @brief 写入设备数据
     * @param buffer 数据缓冲区
     * @param size 要写入的数据大小
     * @param offset 偏移量
     * @return 实际写入的字节数，-1表示错误
     */
    virtual ssize_t write(const void* buffer, size_t size, off_t offset = 0) = 0;
    
    /**
     * @brief 设备控制操作（ioctl类似）
     * @param request 控制请求
     * @param arg 参数
     * @return 操作结果
     */
    virtual int ioctl(unsigned long request, void* arg) = 0;
    
    /**
     * @brief 获取设备支持的操作列表
     * @return 操作列表
     */
    virtual std::vector<DeviceOperation> getSupportedOperations() const = 0;
    
    /**
     * @brief 执行设备操作
     * @param operation_name 操作名称
     * @return 操作是否成功
     */
    virtual bool performOperation(const std::string& operation_name) = 0;
    
    /**
     * @brief 获取错误信息
     * @return 错误描述
     */
    virtual std::string getLastError() const = 0;
    
    /**
     * @brief 清除错误状态
     */
    virtual void clearError() = 0;
    
    /**
     * @brief 检查设备是否就绪
     * @return 设备是否就绪
     */
    virtual bool isReady() const = 0;
};

/**
 * @class DeviceManager
 * @brief 设备管理器
 * 
 * 负责管理所有设备驱动程序的加载、卸载和状态监控
 */
class DeviceManager {
public:
    /**
     * @brief 构造函数
     */
    DeviceManager();
    
    /**
     * @brief 析构函数
     */
    ~DeviceManager();
    
    /**
     * @brief 初始化设备管理器
     * @return 初始化是否成功
     */
    bool initialize();
    
    /**
     * @brief 注册设备驱动程序
     * @param driver 设备驱动程序
     * @param device_id 设备ID
     * @return 注册是否成功
     */
    bool registerDriver(std::shared_ptr<IDeviceDriver> driver, const std::string& device_id);
    
    /**
     * @brief 注销设备驱动程序
     * @param device_id 设备ID
     * @return 注销是否成功
     */
    bool unregisterDriver(const std::string& device_id);
    
    /**
     * @brief 获取设备驱动程序
     * @param device_id 设备ID
     * @return 设备驱动程序，如果不存在则返回nullptr
     */
    std::shared_ptr<IDeviceDriver> getDriver(const std::string& device_id) const;
    
    /**
     * @brief 获取所有设备ID
     * @return 设备ID列表
     */
    std::vector<std::string> getDeviceIds() const;
    
    /**
     * @brief 启动设备
     * @param device_id 设备ID
     * @return 启动是否成功
     */
    bool startDevice(const std::string& device_id);
    
    /**
     * @brief 停止设备
     * @param device_id 设备ID
     * @return 停止是否成功
     */
    bool stopDevice(const std::string& device_id);
    
    /**
     * @brief 挂起设备
     * @param device_id 设备ID
     * @return 挂起是否成功
     */
    bool suspendDevice(const std::string& device_id);
    
    /**
     * @brief 恢复设备
     * @param device_id 设备ID
     * @return 恢复是否成功
     */
    bool resumeDevice(const std::string& device_id);
    
    /**
     * @brief 启动所有设备
     * @return 启动是否成功
     */
    bool startAllDevices();
    
    /**
     * @brief 停止所有设备
     * @return 停止是否成功
     */
    bool stopAllDevices();
    
    /**
     * @brief 扫描并加载可用设备驱动程序
     * @return 扫描到的设备数量
     */
    size_t scanForDevices();
    
    /**
     * @brief 获取设备信息
     * @param device_id 设备ID
     * @return 设备信息
     */
    DeviceInfo getDeviceInfo(const std::string& device_id) const;
    
    /**
     * @brief 获取设备状态
     * @param device_id 设备ID
     * @return 设备状态
     */
    DeviceState getDeviceState(const std::string& device_id) const;
    
    /**
     * @brief 设置设备电源状态
     * @param device_id 设备ID
     * @param state 目标电源状态
     * @return 设置是否成功
     */
    bool setDevicePowerState(const std::string& device_id, PowerState state);
    
    /**
     * @brief 添加设备状态变化监听器
     * @param callback 回调函数
     */
    void addDeviceStateChangeListener(std::function<void(const std::string&, DeviceState, DeviceState)> callback);
    
    /**
     * @brief 添加设备错误监听器
     * @param callback 回调函数
     */
    void addDeviceErrorListener(std::function<void(const std::string&, const std::string&)> callback);
    
    /**
     * @brief 添加热插拔监听器
     * @param callback 回调函数
     */
    void addHotplugListener(std::function<void(const std::string&, bool)> callback);
    
    /**
     * @brief 生成设备报告
     * @return 设备报告字符串
     */
    std::string generateDeviceReport() const;
    
    /**
     * @brief 保存设备状态
     * @param file_path 文件路径
     * @return 保存是否成功
     */
    bool saveDeviceState(const std::string& file_path) const;
    
    /**
     * @brief 加载设备状态
     * @param file_path 文件路径
     * @return 加载是否成功
     */
    bool loadDeviceState(const std::string& file_path);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @class BaseDeviceDriver
 * @brief 设备驱动程序基类
 * 
 * 提供设备驱动程序的通用实现，简化具体驱动程序的开发
 */
class BaseDeviceDriver : public IDeviceDriver {
public:
    BaseDeviceDriver();
    virtual ~BaseDeviceDriver();
    
    // IDeviceDriver 接口实现
    bool initialize() override;
    bool start() override;
    bool stop() override;
    bool suspend() override;
    bool resume() override;
    DeviceInfo getDeviceInfo() const override;
    DeviceState getDeviceState() const override;
    PowerState getPowerState() const override;
    bool setPowerState(PowerState state) override;
    ssize_t read(void* buffer, size_t size, off_t offset = 0) override;
    ssize_t write(const void* buffer, size_t size, off_t offset = 0) override;
    int ioctl(unsigned long request, void* arg) override;
    std::vector<DeviceOperation> getSupportedOperations() const override;
    bool performOperation(const std::string& operation_name) override;
    std::string getLastError() const override;
    void clearError() override;
    bool isReady() const override;

protected:
    /**
     * @brief 设置设备信息
     * @param info 设备信息
     */
    void setDeviceInfo(const DeviceInfo& info);
    
    /**
     * @brief 设置设备状态
     * @param state 设备状态
     */
    void setDeviceState(DeviceState state);
    
    /**
     * @brief 设置电源状态
     * @param state 电源状态
     */
    void setPowerStateInternal(PowerState state);
    
    /**
     * @brief 设置错误信息
     * @param error 错误信息
     */
    void setLastError(const std::string& error);
    
    /**
     * @brief 添加支持的操作
     * @param operation 设备操作
     */
    void addSupportedOperation(const DeviceOperation& operation);
    
    /**
     * @brief 设备特定的初始化逻辑（由子类实现）
     * @return 初始化是否成功
     */
    virtual bool deviceSpecificInitialize() = 0;
    
    /**
     * @brief 设备特定的启动逻辑（由子类实现）
     * @return 启动是否成功
     */
    virtual bool deviceSpecificStart() = 0;
    
    /**
     * @brief 设备特定的停止逻辑（由子类实现）
     * @return 停止是否成功
     */
    virtual bool deviceSpecificStop() = 0;
    
    /**
     * @brief 设备特定的挂起逻辑（由子类实现）
     * @return 挂起是否成功
     */
    virtual bool deviceSpecificSuspend() = 0;
    
    /**
     * @brief 设备特定的恢复逻辑（由子类实现）
     * @return 恢复是否成功
     */
    virtual bool deviceSpecificResume() = 0;
    
    /**
     * @brief 设备特定的读取逻辑（由子类实现）
     * @param buffer 数据缓冲区
     * @param size 要读取的数据大小
     * @param offset 偏移量
     * @return 实际读取的字节数
     */
    virtual ssize_t deviceSpecificRead(void* buffer, size_t size, off_t offset) = 0;
    
    /**
     * @brief 设备特定的写入逻辑（由子类实现）
     * @param buffer 数据缓冲区
     * @param size 要写入的数据大小
     * @param offset 偏移量
     * @return 实际写入的字节数
     */
    virtual ssize_t deviceSpecificWrite(const void* buffer, size_t size, off_t offset) = 0;
    
    /**
     * @brief 设备特定的控制操作（由子类实现）
     * @param request 控制请求
     * @param arg 参数
     * @return 操作结果
     */
    virtual int deviceSpecificIoctl(unsigned long request, void* arg) = 0;

private:
    DeviceInfo device_info_;
    DeviceState device_state_;
    PowerState power_state_;
    std::string last_error_;
    std::vector<DeviceOperation> supported_operations_;
    std::mutex state_mutex_;
};

} // namespace CloudFlow::Device