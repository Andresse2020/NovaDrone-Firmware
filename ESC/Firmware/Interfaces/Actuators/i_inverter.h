#ifndef I_INVERTER_H
#define I_INVERTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @file i_inverter.h
 * @brief Interface for controlling the power stage (inverter) of a motor.
 *
 * This interface provides abstraction for applying motor control commands
 * (e.g., PWM duty cycles, FOC voltage vectors, brake commands).
 * It hides the hardware details of the gate driver and PWM generation.
 */

/**
 * @brief Inverter operating modes.
 */
typedef enum
{
    INVERTER_MODE_DISABLED = 0,   /**< Outputs disabled, all phases floating */
    INVERTER_MODE_PWM,            /**< Classic 6-step or PWM control */
    INVERTER_MODE_FOC,            /**< Field Oriented Control voltage vector mode */
    INVERTER_MODE_BRAKE           /**< Brake or short-circuit mode */
} inverter_mode_t;

/**
 * @brief Structure for duty cycle commands in PWM mode.
 */
typedef struct
{
    float duty_a;   /**< Duty cycle phase A (0.0f - 1.0f) */
    float duty_b;   /**< Duty cycle phase B (0.0f - 1.0f) */
    float duty_c;   /**< Duty cycle phase C (0.0f - 1.0f) */
} inverter_pwm_cmd_t;

/**
 * @brief Structure for voltage vector commands in FOC mode.
 */
typedef struct
{
    float v_alpha;  /**< Alpha axis voltage (normalized) */
    float v_beta;   /**< Beta axis voltage (normalized) */
} inverter_foc_cmd_t;

/**
 * @brief Inverter interface.
 */
typedef struct
{
    /**
     * @brief Initialize the inverter hardware.
     * @return true if successful, false otherwise.
     */
    bool (*init)(void);

    /**
     * @brief Set inverter mode (PWM, FOC, disabled, brake).
     * @param mode Selected inverter mode.
     * @return true if successful, false otherwise.
     */
    bool (*set_mode)(inverter_mode_t mode);

    /**
     * @brief Apply duty cycles to the three phases (PWM mode).
     * @param cmd Duty cycles for phases A/B/C.
     * @return true if successful, false otherwise.
     */
    bool (*apply_pwm)(const inverter_pwm_cmd_t* cmd);

    /**
     * @brief Apply FOC voltage vector (FOC mode).
     * @param cmd Voltage vector (α/β frame).
     * @return true if successful, false otherwise.
     */
    bool (*apply_foc)(const inverter_foc_cmd_t* cmd);

    /**
     * @brief Immediately disable all outputs (safety function).
     */
    void (*disable)(void);
} i_inverter_t;


extern i_inverter_t* inverter; // Global interface instance


#ifdef __cplusplus
}
#endif

#endif /* I_INVERTER_H */
