#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>

class SerialPort
{
public:

    using DataCallback = std::function<void(const uint8_t*, size_t)>;

    SerialPort();
    ~SerialPort();

    bool Open(const std::string& port, uint32_t baud, DataCallback cb);
    void Close();

    bool Write(const uint8_t* data, size_t len);

    bool IsOpen() const;

    static std::vector<std::string> ScanPorts();

private:

    void ReadThread();

private:

    HANDLE m_handle = INVALID_HANDLE_VALUE;

    OVERLAPPED m_ovRead{};
    HANDLE m_readEvent = NULL;

    std::thread m_thread;
    std::atomic<bool> m_running{ false };

    DataCallback m_callback;
};