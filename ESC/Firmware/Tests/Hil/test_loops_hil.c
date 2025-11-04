#include "i_periodic_loop.h"
#include "i_time.h"
#include "i_system.h"
#include "i_led.h"

static uint32_t counter = 0;

void Motor_FastLoop(void)
{
    counter++;
    if (counter >= 1000) // every 1s
    {
        ILED->toggle(LED_STATUS);        // Toggle the status LED
        counter = 0;
    }
}

int main(void)
{
    DSystem_Init();
    Driver_Init();

    ILowLoop->init();
    ILowLoop->register_callback(Motor_FastLoop);
    ILowLoop->start();

    while (1)
    {
        // main loop remains free
    }

    return 0;
}
