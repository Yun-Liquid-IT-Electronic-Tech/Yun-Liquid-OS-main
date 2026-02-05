/**
 * @file boot_config.h
 * @brief 启动配置管理
 * @author 云流操作系统开发团队
 * @date 2026-02-04
 * @version 1.0.0
 * 
 * 启动配置管理器，负责管理多重启动配置和启动菜单
 */

#pragma once

#include "boot_params.h"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace CloudFlow::Boot {

/**
 * @enum BootEntryType
 * @brief 启动条目类型
 */
enum class BootEntryType {
    Kernel,         ///< 内核启动
    ChainLoader,    ///< 链式加载器
    Rescue,         ///< 救援模式
    Firmware,       ///< 固件设置
    Custom          ///< 自定义命令
};

/**
 * @enum BootMenuStyle
 * @brief 启动菜单样式
 */
enum class BootMenuStyle {
    Text,           ///< 文本菜单
    Graphical,      ///< 图形菜单
    Minimal,        ///< 最小化菜单
    Hidden          ///< 隐藏菜单
};

/**
 * @struct BootEntry
 * @brief 启动条目
 */
struct BootEntry {
    std::string name;           ///< 条目名称
    std::string description;    ///< 条目描述
    BootEntryType type;         ///< 条目类型
    
    // 内核启动参数
    std::string kernel_path;    ///< 内核镜像路径
    std::string initrd_path;    ///< 初始RAM磁盘路径
    std::string cmdline;        ///< 命令行参数
    
    // 链式加载参数
    std::string chain_device;   ///< 链式加载设备
    std::string chain_path;     ///< 链式加载路径
    
    // 自定义命令
    std::string custom_command; ///< 自定义命令
    
    // 显示设置
    bool visible;               ///< 是否可见
    int priority;              ///< 优先级
    
    // 安全设置
    bool secure_boot;          ///< 安全启动
    bool signature_required;   ///< 需要签名验证
};

/**
 * @struct BootMenuConfig
 * @brief 启动菜单配置
 */
struct BootMenuConfig {
    BootMenuStyle style;           ///< 菜单样式
    uint32_t timeout;              ///< 超时时间（秒）
    std::string default_entry;     ///< 默认条目
    bool show_countdown;           ///< 显示倒计时
    bool allow_edit;               ///< 允许编辑参数
    bool password_protected;       ///< 密码保护
    std::string password_hash;     ///< 密码哈希
    
    // 显示设置
    uint32_t screen_width;        ///< 屏幕宽度
    uint32_t screen_height;       ///< 屏幕高度
    uint32_t text_color;          ///< 文本颜色
    uint32_t background_color;    ///< 背景颜色
    uint32_t highlight_color;     ///< 高亮颜色
    
    // 主题设置
    std::string theme_name;       ///< 主题名称
    std::string background_image;  ///< 背景图片
    std::string font_name;        ///< 字体名称
    uint32_t font_size;           ///< 字体大小
};

/**
 * @struct BootConfig
 * @brief 启动配置
 */
struct BootConfig {
    // 全局设置
    std::string version;          ///< 配置版本
    uint64_t timestamp;           ///< 时间戳
    std::string architecture;     ///< 系统架构
    
    // 启动条目
    std::vector<BootEntry> entries; ///< 启动条目列表
    
    // 菜单配置
    BootMenuConfig menu_config;   ///< 菜单配置
    
    // 安全设置
    bool secure_boot_enabled;     ///< 安全启动启用
    bool signature_verification;   ///< 签名验证
    std::string trusted_keys;     ///< 可信密钥
    
    // 调试设置
    bool debug_mode;              ///< 调试模式
    bool verbose_logging;        ///< 详细日志
    std::string log_level;        ///< 日志级别
};

/**
 * @class IBootConfigStorage
 * @brief 启动配置存储接口
 * 
 * 抽象启动配置的存储方式（文件系统、NVRAM、网络等）
 */
class IBootConfigStorage {
public:
    virtual ~IBootConfigStorage() = default;
    
    /**
     * @brief 初始化存储
     * @return 初始化是否成功
     */
    virtual bool initialize() = 0;
    
    /**
     * @brief 加载启动配置
     * @param config 配置结构体
     * @return 加载是否成功
     */
    virtual bool loadConfig(BootConfig& config) = 0;
    
    /**
     * @brief 保存启动配置
     * @param config 配置结构体
     * @return 保存是否成功
     */
    virtual bool saveConfig(const BootConfig& config) = 0;
    
    /**
     * @brief 检查配置是否存在
     * @return 配置是否存在
     */
    virtual bool configExists() = 0;
    
    /**
     * @brief 备份当前配置
     * @param backup_path 备份路径
     * @return 备份是否成功
     */
    virtual bool backupConfig(const std::string& backup_path) = 0;
    
    /**
     * @brief 恢复配置备份
     * @param backup_path 备份路径
     * @return 恢复是否成功
     */
    virtual bool restoreConfig(const std::string& backup_path) = 0;
    
    /**
     * @brief 获取错误信息
     * @return 错误描述
     */
    virtual std::string getLastError() const = 0;
};

/**
 * @class IBootMenuRenderer
 * @brief 启动菜单渲染器接口
 * 
 * 负责渲染不同样式的启动菜单
 */
class IBootMenuRenderer {
public:
    virtual ~IBootMenuRenderer() = default;
    
    /**
     * @brief 初始化渲染器
     * @param config 菜单配置
     * @return 初始化是否成功
     */
    virtual bool initialize(const BootMenuConfig& config) = 0;
    
    /**
     * @brief 渲染菜单
     * @param entries 启动条目列表
     * @param selected_index 选中索引
     * @param timeout_remaining 剩余超时时间
     * @return 渲染是否成功
     */
    virtual bool renderMenu(const std::vector<BootEntry>& entries, 
                           int selected_index, 
                           uint32_t timeout_remaining) = 0;
    
    /**
     * @brief 处理用户输入
     * @return 用户选择的条目索引，-1表示超时或取消
     */
    virtual int handleInput() = 0;
    
    /**
     * @brief 清理渲染资源
     */
    virtual void cleanup() = 0;
    
    /**
     * @brief 获取错误信息
     * @return 错误描述
     */
    virtual std::string getLastError() const = 0;
};

/**
 * @class BootConfigManager
 * @brief 启动配置管理器
 * 
 * 负责管理多重启动配置和启动菜单
 */
class BootConfigManager {
public:
    /**
     * @brief 构造函数
     */
    BootConfigManager();
    
    /**
     * @brief 析构函数
     */
    ~BootConfigManager();
    
    /**
     * @brief 初始化配置管理器
     * @param storage 配置存储
     * @return 初始化是否成功
     */
    bool initialize(std::shared_ptr<IBootConfigStorage> storage);
    
    /**
     * @brief 加载启动配置
     * @return 加载是否成功
     */
    bool loadConfig();
    
    /**
     * @brief 保存启动配置
     * @return 保存是否成功
     */
    bool saveConfig();
    
    /**
     * @brief 显示启动菜单
     * @return 用户选择的启动条目，空字符串表示取消或超时
     */
    std::string showBootMenu();
    
    /**
     * @brief 添加启动条目
     * @param entry 启动条目
     * @return 添加是否成功
     */
    bool addBootEntry(const BootEntry& entry);
    
    /**
     * @brief 删除启动条目
     * @param entry_name 条目名称
     * @return 删除是否成功
     */
    bool removeBootEntry(const std::string& entry_name);
    
    /**
     * @brief 修改启动条目
     * @param entry_name 条目名称
     * @param new_entry 新条目
     * @return 修改是否成功
     */
    bool modifyBootEntry(const std::string& entry_name, const BootEntry& new_entry);
    
    /**
     * @brief 获取启动条目
     * @param entry_name 条目名称
     * @return 启动条目，如果不存在则返回空对象
     */
    BootEntry getBootEntry(const std::string& entry_name) const;
    
    /**
     * @brief 获取所有启动条目
     * @return 启动条目列表
     */
    std::vector<BootEntry> getAllBootEntries() const;
    
    /**
     * @brief 设置默认启动条目
     * @param entry_name 条目名称
     * @return 设置是否成功
     */
    bool setDefaultEntry(const std::string& entry_name);
    
    /**
     * @brief 获取默认启动条目
     * @return 默认启动条目名称
     */
    std::string getDefaultEntry() const;
    
    /**
     * @brief 验证启动配置
     * @return 配置是否有效
     */
    bool validateConfig() const;
    
    /**
     * @brief 生成启动参数
     * @param entry_name 条目名称
     * @return 启动参数
     */
    std::shared_ptr<BootParams> generateBootParams(const std::string& entry_name);
    
    /**
     * @brief 注册菜单渲染器
     * @param renderer 菜单渲染器
     * @param style 支持的菜单样式
     * @return 注册是否成功
     */
    bool registerMenuRenderer(std::shared_ptr<IBootMenuRenderer> renderer, BootMenuStyle style);
    
    /**
     * @brief 设置菜单样式
     * @param style 菜单样式
     * @return 设置是否成功
     */
    bool setMenuStyle(BootMenuStyle style);
    
    /**
     * @brief 获取当前配置
     * @return 当前配置
     */
    const BootConfig& getConfig() const;
    
    /**
     * @brief 设置配置
     * @param config 新配置
     */
    void setConfig(const BootConfig& config);
    
    /**
     * @brief 备份当前配置
     * @param backup_path 备份路径
     * @return 备份是否成功
     */
    bool backupConfig(const std::string& backup_path);
    
    /**
     * @brief 恢复配置备份
     * @param backup_path 备份路径
     * @return 恢复是否成功
     */
    bool restoreConfig(const std::string& backup_path);
    
    /**
     * @brief 获取错误信息
     * @return 错误描述
     */
    std::string getLastError() const;
    
    /**
     * @brief 添加配置变更监听器
     * @param callback 回调函数
     */
    void addConfigChangeListener(std::function<void(const BootConfig&)> callback);
    
    /**
     * @brief 生成配置报告
     * @return 配置报告字符串
     */
    std::string generateReport() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @class BootEntryValidator
 * @brief 启动条目验证器
 * 
 * 负责验证启动条目的有效性和安全性
 */
class BootEntryValidator {
public:
    /**
     * @brief 构造函数
     */
    BootEntryValidator();
    
    /**
     * @brief 析构函数
     */
    ~BootEntryValidator();
    
    /**
     * @brief 验证启动条目
     * @param entry 启动条目
     * @return 验证是否通过
     */
    bool validateEntry(const BootEntry& entry);
    
    /**
     * @brief 验证内核镜像
     * @param kernel_path 内核镜像路径
     * @return 验证是否通过
     */
    bool validateKernel(const std::string& kernel_path);
    
    /**
     * @brief 验证初始RAM磁盘
     * @param initrd_path 初始RAM磁盘路径
     * @return 验证是否通过
     */
    bool validateInitrd(const std::string& initrd_path);
    
    /**
     * @brief 验证命令行参数
     * @param cmdline 命令行参数
     * @return 验证是否通过
     */
    bool validateCmdline(const std::string& cmdline);
    
    /**
     * @brief 验证签名
     * @param file_path 文件路径
     * @param signature 签名数据
     * @return 验证是否通过
     */
    bool verifySignature(const std::string& file_path, const std::string& signature);
    
    /**
     * @brief 设置可信密钥
     * @param keys 可信密钥列表
     */
    void setTrustedKeys(const std::vector<std::string>& keys);
    
    /**
     * @brief 获取错误信息
     * @return 错误描述
     */
    std::string getLastError() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace CloudFlow::Boot