#include "bsp_utils.h"  // Basic types and macros
#include "i_led.h"           // Include LED interface
#include "gpio.h"          // GPIO initialization

/**
 * @brief Hardware configuration of a logical LED.
 */
typedef struct {
    GPIO_TypeDef *port;  /**< GPIO port corresponding to the LED */
    uint16_t      pin;   /**< GPIO pin number corresponding to the LED */
} led_hw_t;

/**
 * @brief Maps a logical LED identifier to its hardware configuration.
 *
 * @param led Logical LED identifier
 * @return led_hw_t Structure containing GPIO port and pin for the LED
 *
 * This function allows the upper-level LED API to remain hardware-agnostic.
 * Each logical LED (LED_STATUS, LED_ERROR, LED_POWER) is mapped to the
 * specific STM32G4 GPIO port and pin it is physically connected to.
 */
static led_hw_t get_led_hw(led_id_t led)
{
    led_hw_t led_hw;

    switch(led)
    {
        case LED_STATUS:
            led_hw.port = LED_GPIO_Port;        // Status LED connected to GPIOB
            led_hw.pin  = LED_Pin;   // Pin 0
            break;

        case LED_ERROR:
            led_hw.port = NULL;        // Error LED connected to GPIOB
            led_hw.pin  = 0;   // Pin 1
            break;

        case LED_POWER:
            led_hw.port = NULL;        // Power LED connected to GPIOC
            led_hw.pin  = 0;  // Pin 13
            break;

        default:
            // Invalid LED ID: return safe default (no port)
            led_hw.port = NULL;
            led_hw.pin  = 0;
            break;
    }

    return led_hw;
}


/**
 * @brief Turns on a logical LED.
 *
 * @param led Logical LED identifier
 * @return true if the operation was successful, false otherwise
 *
 * This function uses the HAL GPIO API to set the pin corresponding
 * to the logical LED. It ensures the LED ID is valid and hardware
 * mapping exists before performing any operation.
 */
static bool led_on(led_id_t led)
{
    // Get hardware configuration for the requested LED
    led_hw_t hw = get_led_hw(led);

    // Check if the LED mapping is valid
    if(hw.port == NULL)
    {
        // Invalid LED ID
        return false;
    }

    // Set the GPIO pin to HIGH to turn on the LED
    HAL_GPIO_WritePin(hw.port, hw.pin, GPIO_PIN_SET);

    return true;
}

/**
 * @brief Turns off a logical LED.
 *
 * @param led Logical LED identifier
 * @return true if the operation was successful, false otherwise
 */
static bool led_off(led_id_t led)
{
    // Get hardware configuration
    led_hw_t hw = get_led_hw(led);

    // Check if the LED mapping is valid
    if(hw.port == NULL)
    {
        return false;
    }

    // Set the GPIO pin to LOW to turn off the LED
    HAL_GPIO_WritePin(hw.port, hw.pin, GPIO_PIN_RESET);

    return true;
}

/**
 * @brief Toggles a logical LED.
 *
 * @param led Logical LED identifier
 * @return true if the operation was successful, false otherwise
 */
static bool led_toggle(led_id_t led)
{
    // Get hardware configuration
    led_hw_t hw = get_led_hw(led);

    // Check if the LED mapping is valid
    if(hw.port == NULL)
    {
        return false;
    }

    // Toggle the GPIO pin state
    HAL_GPIO_TogglePin(hw.port, hw.pin);

    return true;
}

/**
 * @brief Turns off all LEDs (safety function)
 */
static void led_all_off(void)
{
    // Iterate through all defined LEDs
    for(led_id_t led = LED_STATUS; led <= LED_POWER; led++)
    {
        led_off(led);  // Use existing off function
    }
}

/**
 * @brief LED interface instance
 */
i_led_t driver_LED = {
    .init    = NULL,       // Will implement init next
    .on      = led_on,
    .off     = led_off,
    .toggle  = led_toggle,
    .all_off = led_all_off
};

i_led_t *ILED = &driver_LED;  // Global pointer to the LED interface