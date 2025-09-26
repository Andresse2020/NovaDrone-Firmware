#ifndef I_LED_H
#define I_LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * @file i_led.h
 * @brief Abstract interface for LEDs or simple actuators.
 *
 * Provides a hardware-agnostic API to control LEDs without exposing GPIOs or channels.
 */

/**
 * @brief Logical LED identifiers.
 */
typedef enum {
    LED_STATUS,   /**< Status LED */
    LED_ERROR,    /**< Error LED */
    LED_POWER     /**< Power indicator LED */
} led_id_t;

/**
 * @brief LED interface.
 */
typedef struct
{
    /**
     * @brief Initialize all LEDs.
     * @return true if successful, false otherwise.
     */
    bool (*init)(void);

    /**
     * @brief Turn on a logical LED.
     * @param led LED identifier
     * @return true if successful, false otherwise
     */
    bool (*on)(led_id_t led);

    /**
     * @brief Turn off a logical LED.
     * @param led LED identifier
     * @return true if successful, false otherwise
     */
    bool (*off)(led_id_t led);

    /**
     * @brief Toggle a logical LED.
     * @param led LED identifier
     * @return true if successful, false otherwise
     */
    bool (*toggle)(led_id_t led);

    /**
     * @brief Turn off all LEDs (safety function)
     */
    void (*all_off)(void);

} i_led_t;

extern i_led_t* ILED;

#ifdef __cplusplus
}
#endif

#endif /* I_LED_H */
