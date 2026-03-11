#include "global.h"


SerialPort sp; // 串口对象
IronBoot bl; // bootloader对象
Timer timer;


void Global_Init()
{
    //auto ports = SerialPort::ScanPorts();

    //std::cout << "Available COM ports:\n";

    //for (size_t i = 0; i < ports.size(); ++i)
    //{
    //    std::cout << i << ": " << ports[i] << std::endl;
    //}

    //int portIndex = 0;

    //printf("Select port index: ");
    //scanf_s("%d", &portIndex);

    //if (portIndex < 0 || portIndex >= ports.size())
    //{
    //    printf("Invalid index\n");
    //    return;
    //}

    sp.Open(
        //ports[portIndex],
        "COM50",
        115200,
        [](const uint8_t* data, size_t len)
        {
            printf("RX %d bytes\n", (int)len);
            bl.ReceiveFrameDataCallback(const_cast<uint8_t*>(data), len);
        });

    bl.RegisterTransmitCallback(
        [](uint8_t* data, size_t len) -> bool
        {
            return sp.Write(data, len);
        }
    );

    timer.Register(1, [](void* userdata) { bl.Timer1msHandler(); }, nullptr);


}
