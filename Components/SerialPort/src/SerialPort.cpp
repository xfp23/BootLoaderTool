#include "SerialPort.h"
#include <iostream>

SerialPort::SerialPort()
{
}

SerialPort::~SerialPort()
{
    Close();
}

bool SerialPort::IsOpen() const
{
    return m_handle != INVALID_HANDLE_VALUE;
}

std::vector<std::string> SerialPort::ScanPorts()
{
    std::vector<std::string> ports;

    for (int i = 1; i <= 255; ++i)
    {
        char name[16];
        sprintf_s(name, "COM%d", i);

        std::string path = "\\\\.\\" + std::string(name);

        HANDLE h = CreateFileA(
            path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

        if (h != INVALID_HANDLE_VALUE)
        {
            ports.push_back(name);
            CloseHandle(h);
        }
    }

    return ports;
}

bool SerialPort::Open(const std::string& port, uint32_t baud, DataCallback cb)
{
    Close();

    std::string path = "\\\\.\\" + port;

    m_handle = CreateFileA(
        path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        NULL);

    if (m_handle == INVALID_HANDLE_VALUE)
        return false;

    DCB dcb{};
    dcb.DCBlength = sizeof(DCB);

    if (!GetCommState(m_handle, &dcb))
        return false;

    dcb.BaudRate = baud;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;

    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    if (!SetCommState(m_handle, &dcb))
        return false;

    COMMTIMEOUTS t{};
    t.ReadIntervalTimeout = 50;
    t.ReadTotalTimeoutConstant = 50;
    t.ReadTotalTimeoutMultiplier = 10;

    SetCommTimeouts(m_handle, &t);

    m_readEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    m_ovRead = {};
    m_ovRead.hEvent = m_readEvent;

    m_callback = cb;

    m_running = true;
    m_thread = std::thread(&SerialPort::ReadThread, this);

    return true;
}

void SerialPort::ReadThread()
{
    uint8_t buf[2048];

    while (m_running)
    {
        DWORD bytes = 0;

        BOOL ok = ReadFile(
            m_handle,
            buf,
            sizeof(buf),
            NULL,
            &m_ovRead);

        if (!ok)
        {
            if (GetLastError() == ERROR_IO_PENDING)
            {
                if (WaitForSingleObject(m_readEvent, 100) == WAIT_OBJECT_0)
                {
                    GetOverlappedResult(
                        m_handle,
                        &m_ovRead,
                        &bytes,
                        FALSE);

                    ResetEvent(m_readEvent);
                }
            }
        }

        if (bytes > 0 && m_callback)
        {
            m_callback(buf, bytes);
        }
    }
}

bool SerialPort::Write(const uint8_t* data, size_t len)
{
    if (!IsOpen())
        return false;

    OVERLAPPED ov{};
    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    DWORD written = 0;

    BOOL ok = WriteFile(
        m_handle,
        data,
        (DWORD)len,
        NULL,
        &ov);

    if (!ok)
    {
        if (GetLastError() == ERROR_IO_PENDING)
        {
            WaitForSingleObject(ov.hEvent, INFINITE);

            GetOverlappedResult(
                m_handle,
                &ov,
                &written,
                FALSE);
        }
    }

    CloseHandle(ov.hEvent);

    return written == len;
}

void SerialPort::Close()
{
    m_running = false;

    if (m_handle != INVALID_HANDLE_VALUE)
    {
        CancelIoEx(m_handle, &m_ovRead);
    }

    if (m_thread.joinable())
    {
        m_thread.join();
    }

    if (m_readEvent)
    {
        CloseHandle(m_readEvent);
        m_readEvent = NULL;
    }

    if (m_handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
}