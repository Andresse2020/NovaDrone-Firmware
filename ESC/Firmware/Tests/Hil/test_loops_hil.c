#include "i_fastloop.h"
#include "i_time.h"
#include "i_system.h"
#include "i_led.h"

static uint32_t counter = 0;

void Motor_FastLoop(void)
{
    counter++;
    if (counter >= 24000) // every 1s
    {
        ILED->toggle(LED_STATUS);        // Toggle the status LED
        counter = 0;
    }
}

int main(void)
{
    DSystem_Init();
    Driver_Init();

    IFastLoop->init();
    IFastLoop->register_callback(Motor_FastLoop);
    IFastLoop->start();

    while (1)
    {
        // main loop remains free
    }

    return 0;
}
