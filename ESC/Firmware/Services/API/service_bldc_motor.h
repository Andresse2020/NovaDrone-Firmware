/**
 * @file bldc_motor.h
 * @brief General API for BLDC motor control and ramp management.
 *
 * This module defines a generic interface for controlling a Brushless DC (BLDC)
 * motor. It provides open-loop control features such as six-step commutation,
 * duty and frequency ramp generation, and direction handling. The design
 * allows future extensions such as closed-loop (FOC) or sensor-based control.
 *
 * === Main Features =======================================================
 * - Unified BLDC motor control interface
 * - Open-loop six-step (trapezoidal) commutation
 * - Non-blocking duty/frequency ramp generation
 * - Multiple ramp profiles (linear, exponential, quadratic, logarithmic)
 * - CW/CCW rotation control
 * - Safe stop and state reset
 * - Optional user callback for ramp completion
 *
 * === Example Usage =======================================================
 * ```c
 * #include "bldc_motor.h"
 *
 * static void onRampComplete(void *ctx)
 * {
 *     // Ramp completed callback
 * }
 *
 * void app_init(void)
 * {
 *     // Start an open-loop ramp
 *     Service_Motor_OpenLoopRamp_Start(
 *         0.1f,          // duty_start
 *         0.8f,          // duty_end
 *         10.0f,         // freq_start_hz
 *         100.0f,        // freq_end_hz
 *         2000,          // ramp_time_ms
 *         true,          // CW direction
 *         RAMP_PROFILE_LINEAR,
 *         onRampComplete,
 *         NULL);
 * }
 * ```
 *
 * @date    October 22, 2025
 * @version 1.1
 * @license Internal / project-specific license
 */

#ifndef BLDC_MOTOR_H
#define BLDC_MOTOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/* === Public Types ======================================================== */
/* ========================================================================= */

/**
 * @brief Ramp profile selection.
 */
typedef enum
{
    RAMP_PROFILE_LINEAR = 0,      /**< Linear frequency progression */
    RAMP_PROFILE_EXPONENTIAL,     /**< Exponential frequency progression */
    RAMP_PROFILE_QUADRATIC,       /**< Quadratic progression (smooth acceleration) */
    RAMP_PROFILE_LOGARITHMIC      /**< Logarithmic progression (fast start, smooth end) */
} motor_ramp_profile_t;

/**
 * @brief User callback type for ramp completion notification.
 *
 * @param user_context Pointer provided during ramp start (optional).
 */
typedef void (*motor_ramp_callback_t)(void *user_context);


/**
 * @brief Commutation event callback type.
 *
 * @param user_ctx Optional user context (can be NULL).
 */
typedef void (*commutation_callback_t)(void *user_ctx);


/* ========================================================================= */
/* === Public API ========================================================== */
/* ========================================================================= */

/**
 * @brief Start an open-loop motor ramp (non-blocking).
 *
 * Initializes the ramp context, performs the first commutation step,
 * and schedules the next steps using an event-driven timer. The ramp
 * progresses automatically until completion or stop request.
 *
 * @param duty_start     Starting PWM duty (0.0 – 1.0)
 * @param duty_end       Final PWM duty (0.0 – 1.0)
 * @param freq_start_hz  Starting electrical frequency (Hz)
 * @param freq_end_hz    Final electrical frequency (Hz)
 * @param ramp_time_ms   Total ramp duration in milliseconds
 * @param cw             true = clockwise, false = counterclockwise
 * @param profile_type   Ramp progression profile
 * @param on_complete    Optional callback invoked at ramp completion (can be NULL)
 * @param user_ctx       Optional user data passed to the callback (can be NULL)
 */
void Service_Motor_OpenLoopRamp_Start(
    float duty_start,
    float duty_end,
    float freq_start_hz,
    float freq_end_hz,
    uint32_t ramp_time_ms,
    bool cw,
    motor_ramp_profile_t profile_type,
    motor_ramp_callback_t on_complete,
    void *user_ctx);


/**
 * @brief Start non-blocking rotor alignment.
 *
 * @param duty              PWM duty cycle [0.0–1.0].
 * @param duration_ms       Duration of alignment (milliseconds).
 * @param on_alignment_done Callback called when alignment completes (nullable).
 *
 * @details
 * Electrical configuration:
 *   - Phase A: PWM high (current source)
 *   - Phase B: Low (0% duty → low-side ON)
 *   - Phase C: Floating (Hi-Z)
 *
 * After the alignment duration, the inverter is disabled and the callback
 * is invoked if provided.
 */
void Service_Motor_Align_Rotor(float duty, uint32_t duration_ms, void (*on_alignment_done)(void));


/**
 * @brief Stop any ongoing open-loop ramp.
 *
 * Cancels pending events, disables the inverter safely,
 * and resets the internal ramp context.
 */
void Service_Motor_OpenLoopRamp_Stop(void);

/**
 * @brief Stop the open-loop ramp WITHOUT disabling the inverter.
 *
 * Used when transitioning to closed-loop control.
 */
void Service_Motor_OpenLoopRamp_StopSoft(void);

/**
 * @brief Apply a commutation pattern for the active control mode.
 *
 * In open-loop mode, this performs a six-step commutation.
 * In future modes (e.g., FOC or sensor-based), this function
 * can be reimplemented to apply the appropriate control method.
 *
 * @param step  Step index (0–5 for six-step mode)
 * @param duty  Normalized PWM duty (0.0 – 1.0)
 * @param cw    true = clockwise, false = counterclockwise
 */
void Inverter_SixStepCommutate(uint8_t step, float duty, bool cw);

/**
 * @brief Schedule a six-step commutation event after a specified delay.
 *
 * This helper service uses the one-shot timer to schedule a callback after
 * a given microsecond delay. It abstracts away the hardware timer interface
 * from the Control layer.
 *
 * @param delay_us   Delay before triggering commutation (µs)
 * @param callback   Function to call when the timer expires
 * @param user_ctx   Optional user context (can be NULL)
 */
void Service_ScheduleCommutation(float delay_us, commutation_callback_t callback, void *user_ctx);

/**
 * @brief Get the current ramp state (for transition to closed-loop).
 *
 * Allows the Control layer to read the latest ramp parameters without
 * exposing internal variables like s_ramp_ctx.
 *
 * @param step_index     Pointer to receive the current commutation step
 * @param duty           Pointer to receive the current duty cycle
 * @param direction_cw   Pointer to receive the rotation direction
 */
void Service_Motor_OpenLoopRamp_GetState(uint8_t *step_index, float *duty, bool *direction_cw);

/**
 * @brief Stop the motor by disabling the inverter and cancelling timers.
 */
void Service_Motor_Stop(void);


/* ========================================================================= */
/* === End of Public Interface ============================================ */
/* ========================================================================= */

#ifdef __cplusplus
}
#endif

#endif /* BLDC_MOTOR_H */
