#include "bsp_utils.h"
#include "clock_config.h"
#include "gpio.h"
#include "i_led.h"
#include "i_time.h"
#include "tim.h"
#include "usart.h"
#include "i_system.h"


// Non-blocking status LED blink using ITime->getTick()
void blink_status_Led(uint32_t delay_ms) {
    static uint32_t last_toggle_tick = 0; // Keep track of the last toggle time
    uint32_t current_tick = ITime->getTick(); // Get current system tick in ms

    // Check if the delay has elapsed
    if ((current_tick - last_toggle_tick) >= delay_ms) {
        ILED->toggle(LED_STATUS);        // Toggle the status LED
        last_toggle_tick = current_tick; // Update last toggle time
    }
}


int main(void)
{
    DSystem_Init();
    Driver_Init();

    if (HAL_UART_Init(&huart2) != HAL_OK)
        return false;


    while (1)
    {
        blink_status_Led(100);
        Debug_Printf(&huart2, "hello world!!!");
    }
    
    return 0;
}