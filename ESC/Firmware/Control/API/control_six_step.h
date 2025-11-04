/**
 * @file control_motor.h
 * @brief Public API for BLDC motor control (sensorless six-step, synchronous handover).
 *
 * This module implements a robust sensorless BLDC controller with:
 *  - Smooth Open→Closed Loop transition (no torque gap)
 *  - µs-accurate commutation scheduling
 *  - Closed-loop speed regulation (PID)
 *  - Direction control (CW / CCW)
 *  - Speed ramping (linear acceleration / deceleration)
 *
 * Typical usage:
 *  @code
 *  Control_Motor_Init();
 *  Control_Motor_SetSpeed(+1500.0f);   // Run CW at 1500 RPM
 *  ...
 *  Control_Motor_SetSpeed(-1200.0f);   // Reverse direction (CCW)
 *  ...
 *  Control_Motor_Stop();               // Stop motor safely
 *  @endcode
 *
 * Author: Andresse Bassinga
 * License: MIT
 */

#ifndef CONTROL_MOTOR_H
#define CONTROL_MOTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 *  PUBLIC CONSTANTS & CONFIGURATION
 * ========================================================================== */

/**
 * @brief Default ramp slope [RPM per millisecond].
 *
 * Defines how fast the internal target can change each control cycle.
 * Example:
 *   20 RPM/ms → ~20,000 RPM/s acceleration.
 */
#define CONTROL_MOTOR_DEFAULT_RAMP_SLOPE_RPM_MS  10.0f

/**
 * @brief Number of motor pole pairs (mechanical↔electrical conversion).
 * Should match the actual motor used.
 */
#define CONTROL_MOTOR_POLE_PAIRS  6

/* ============================================================================
 *  PUBLIC ENUMERATIONS
 * ========================================================================== */

/**
 * @brief Motor operation modes (read-only).
 */
typedef enum
{
    CONTROL_MOTOR_MODE_STOPPED = 0,   ///< PWM disabled, no rotation
    CONTROL_MOTOR_MODE_OPEN_LOOP,     ///< Open-loop ramp during startup
    CONTROL_MOTOR_MODE_CLOSED_LOOP    ///< Closed-loop BEMF control active
} control_motor_mode_t;

/* ============================================================================
 *  PUBLIC FUNCTIONS (USER API)
 * ========================================================================== */

/**
 * @brief Initialize motor control module and register control loops.
 *
 * Call once during system startup before any other motor control calls.
 */
void Control_Motor_Init(void);

/**
 * @brief Command a new motor speed in RPM.
 *
 * - Positive value → CW rotation
 * - Negative value → CCW rotation
 * - 0 → decelerate to stop
 *
 * If the motor is stopped, it will automatically perform alignment,
 * then open-loop startup, and finally switch to closed-loop mode.
 *
 * @param rpm  Target speed in mechanical RPM (signed).
 */
void Control_Motor_SetSpeed_RPM(float rpm);

/**
 * @brief Stop the motor smoothly (soft stop).
 *
 * The function sets the commanded speed to zero,
 * lets the ramp decelerate, and disables PWM when safe.
 */
void Control_Motor_Stop(void);

/**
 * @brief Get the current commanded target speed (RPM).
 * @return Current speed command in RPM (positive value).
 */
float Control_Motor_GetTargetSpeed_RPM(void);

/**
 * @brief Set the maximum slope of the internal speed ramp (RPM per ms).
 *
 * Use this to tune acceleration / deceleration behavior.
 * Default: 25 RPM/ms (~25,000 RPM/s)
 *
 * @param rpm_per_ms  Ramp slope (1.0f → 500.0f typical range).
 */
void Control_Motor_SetRampSlope(float rpm_per_ms);

/**
 * @brief Print motor control debug statistics via logging interface.
 *
 * Prints:
 *  - Mode (Stopped / OL / CL)
 *  - Zero-crossing count
 *  - Commutation count
 *  - Current step
 *  - Last period (µs)
 */
void Control_Motor_PrintStats(void);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_MOTOR_H */
