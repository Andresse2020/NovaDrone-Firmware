#include "bsp_utils.h"
#include "clock_config.h"
#include "gpio.h"
#include "i_led.h"
#include "utilities.h"
#include "tim.h"

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
  HAL_Init();
  SystemClock_Config();   // Configure HCLK = 150 MHz
  MX_GPIO_Init();
  MX_TIM1_Init();

  /* Start PWM Channel 1 */
  if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  /* Start complementary output Channel 1N */
  if (HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  /* Start PWM Channel 2 */
  if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }

  /* Start complementary output Channel 1N */
  if (HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }

  /* Start PWM Channel 3 */
  if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }

  /* Start complementary output Channel 1N */
  if (HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }


    // Exemple : 25 % duty cycle
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 400);

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, 0);

  while (1)
  {
    blink_status_Led(500);
    // Application loop
  }
}
