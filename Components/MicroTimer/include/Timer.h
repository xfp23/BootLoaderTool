#pragma once
#include <windows.h>
#include <stdint.h>

class Timer {
public:
    // 定义回调函数指针：void func(void* userdata)
    typedef void (*TimerCallback)(void* userdata);

    Timer();
    ~Timer();

    // 注册并启动定时器
    // interval_ms: 周期（毫秒）
    // callback: 回调函数
    // userdata: 透传的用户指针
    bool Register(uint32_t interval_ms, TimerCallback callback, void* userdata);

    // 停止定时器
    void Stop();

    // 获取自系统启动以来的 Tick（全局静态方法，类似 HAL_GetTick）
    static uint32_t GetSystemTick();

private:
    // Windows 多媒体定时器回调（必须是静态，否则无法作为指针传递）
    static void CALLBACK Win32Callback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);

    UINT m_timerId;
    TimerCallback m_userFunc;
    void* m_userData;
};