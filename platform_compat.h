/**
 * @file platform_compat.h
 * @brief 平台兼容性头文件
 * @author 云流操作系统开发团队
 * @date 2026-02-06
 * @version 1.0.0
 */

#ifndef CLOUDFLOW_PLATFORM_COMPAT_H
#define CLOUDFLATFORM_COMPAT_H

#ifdef _WIN32
    // Windows平台定义
    #include <windows.h>
    #include <process.h>
    #include <io.h>
    
    // 进程相关
    typedef DWORD pid_t;
    #define getpid() GetCurrentProcessId()
    
    // 文件描述符相关
    #define STDIN_FILENO 0
    #define STDOUT_FILENO 1
    #define STDERR_FILENO 2
    
    // 信号相关（Windows不支持Unix信号，使用模拟）
    #define SIGTERM 15
    #define SIGKILL 9
    #define SIGCHLD 17
    
    // 等待状态宏
    #define WIFEXITED(status) ((status) != STILL_ACTIVE)
    #define WEXITSTATUS(status) (status)
    #define WIFSIGNALED(status) (0)
    
    // 模拟waitpid函数
    static int waitpid(pid_t pid, int* status, int options) {
        DWORD exit_code;
        HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (process == NULL) return -1;
        
        WaitForSingleObject(process, INFINITE);
        GetExitCodeProcess(process, &exit_code);
        CloseHandle(process);
        
        if (status) *status = exit_code;
        return pid;
    }
    
    // 模拟kill函数
    static int kill(pid_t pid, int sig) {
        HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (process == NULL) return -1;
        
        BOOL result = TerminateProcess(process, sig);
        CloseHandle(process);
        return result ? 0 : -1;
    }
    
    // 模拟fork函数（Windows不支持fork，返回错误）
    static pid_t fork() {
        return -1; // Windows不支持fork
    }
    
    // 模拟exec函数族
    static int execvp(const char* file, char* const argv[]) {
        // Windows下使用CreateProcess替代
        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        
        // 构建命令行
        std::string cmd_line = file;
        for (int i = 1; argv[i] != nullptr; i++) {
            cmd_line += " ";
            cmd_line += argv[i];
        }
        
        char* cmd_line_str = const_cast<char*>(cmd_line.c_str());
        
        if (!CreateProcess(NULL, cmd_line_str, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            return -1;
        }
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 0;
    }
    
    // 文件权限相关
    #define S_IRWXU 0700
    #define S_IRUSR 0400
    #define S_IWUSR 0200
    #define S_IXUSR 0100
    
    static int chmod(const char* path, int mode) {
        // Windows下文件权限设置简化处理
        return 0;
    }
    
#else
    // Linux/Unix平台
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
    #include <signal.h>
    #include <sys/stat.h>
#endif

// 通用类型定义
typedef enum {
    PLATFORM_WINDOWS,
    PLATFORM_LINUX,
    PLATFORM_MACOS,
    PLATFORM_UNKNOWN
} PlatformType;

// 平台检测函数
static PlatformType get_platform() {
#ifdef _WIN32
    return PLATFORM_WINDOWS;
#elif defined(__linux__)
    return PLATFORM_LINUX;
#elif defined(__APPLE__)
    return PLATFORM_MACOS;
#else
    return PLATFORM_UNKNOWN;
#endif
}

// 跨平台路径分隔符
#ifdef _WIN32
    #define PATH_SEPARATOR '\\'
    #define PATH_SEPARATOR_STR "\\"
#else
    #define PATH_SEPARATOR '/'
    #define PATH_SEPARATOR_STR "/"
#endif

// 跨平台文件操作
static bool create_directory(const std::string& path) {
#ifdef _WIN32
    return CreateDirectoryA(path.c_str(), NULL) != 0;
#else
    return mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0;
#endif
}

// 跨平台进程管理
static bool is_process_running(pid_t pid) {
#ifdef _WIN32
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (process == NULL) return false;
    
    DWORD exit_code;
    BOOL result = GetExitCodeProcess(process, &exit_code);
    CloseHandle(process);
    
    return result && exit_code == STILL_ACTIVE;
#else
    return kill(pid, 0) == 0;
#endif
}

#endif // CLOUDFLOW_PLATFORM_COMPAT_H