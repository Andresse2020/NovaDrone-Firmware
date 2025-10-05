#include "bsp_utils.h"
#include "clock_config.h"
#include "gpio.h"
#include "i_led.h"
#include "utilities.h"
#include "tim.h"

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();



    Error_Handler();
    return 0;
}