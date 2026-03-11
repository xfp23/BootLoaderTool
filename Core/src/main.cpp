#include "main.h"
#include "global.h"

int main()
{
    // printf("hello world");
    Global_Init();

    bl.LoadFirmware(IronBoot::FirmwareType_t::HEX, const_cast<char *>("STM32F407VET6_Bootloader.hex"));
    bl.StartBl();

    while (bl.IsRunning());
    //while (true);
    exit(0);
    return 0;
}