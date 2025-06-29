#include <filesystem>
#include <iostream>

#include <windows.h>

bool StartProcessNoWindow(const std::string& exePath, const std::string& workDir = "")
{
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_NORMAL;

    // 可选：传递命令行参数时要注意格式
    // 这里只启动程序本身
    std::string cmdLine = "\"" + exePath + "\"";

    // 指定工作目录，如果为空则用当前进程的工作目录
    const char* workingDir = workDir.empty() ? NULL : workDir.c_str();

    BOOL result = CreateProcessA(
        NULL, // 应用程序名称
        &cmdLine[0], // 命令行参数，注意要可写
        NULL, // 进程安全属性
        NULL, // 线程安全属性
        FALSE, // 继承句柄
        NULL, // 创建窗口
        NULL, // 环境变量
        workingDir, // 当前目录
        &si, // 启动信息
        &pi // 进程信息
    );

    if (result) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    } else {
        std::cerr << "CreateProcess failed: " << GetLastError() << std::endl;
        return false;
    }
}

std::string get_exe_directory()
{
    char exePath[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::filesystem::path p = exePath;
    return p.parent_path().string();
}

void check_and_prompt(const std::string& exePath)
{
    if (!std::filesystem::exists(exePath)) {
        std::string message = "文件不存在: " + exePath;
        MessageBoxA(NULL, message.c_str(), "错误", MB_OK | MB_ICONERROR);
    }
}
#pragma comment(lib, "user32.lib")
#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
int main()
{
    auto&& p = get_exe_directory();
    std::string exePath = p + "/APILTM/APILTM.exe";
    // 如果文件不存在弹窗提示
    check_and_prompt(exePath);
    std::string workDir = p + "/APILTM/";
    StartProcessNoWindow(exePath, workDir);
    return 0;
}