#include "Timer.h"
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

Timer::Timer() : m_timerId(0), m_userFunc(nullptr), m_userData(nullptr) {
}

Timer::~Timer() {
    Stop();
}

bool Timer::Register(uint32_t interval_ms, TimerCallback callback, void* userdata) {
    Stop(); // 如果已经在运行，先停止

    m_userFunc = callback;
    m_userData = userdata;

    // 1. 提升系统时钟分辨率
    timeBeginPeriod(1);

    // 2. 开启定时器
    // 将 this 指针作为 dwUser 传入，这样在静态回调里能找回当前对象
    m_timerId = timeSetEvent(
        interval_ms,
        1,
        Win32Callback,
        (DWORD_PTR)this,
        TIME_PERIODIC | TIME_KILL_SYNCHRONOUS
    );

    return m_timerId != 0;
}

void Timer::Stop() {
    if (m_timerId != 0) {
        timeKillEvent(m_timerId);
        timeEndPeriod(1);
        m_timerId = 0;
    }
}

uint32_t Timer::GetSystemTick() {
    // 获取 Windows 系统启动以来的毫秒数
    return (uint32_t)timeGetTime();
}

// 静态中转函数
void CALLBACK Timer::Win32Callback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2) {
    // 将 dwUser 转回 Timer 实例指针
    Timer* pThis = (Timer*)dwUser;

    // 执行用户注册的回调
    if (pThis && pThis->m_userFunc) {
        pThis->m_userFunc(pThis->m_userData);
    }
}