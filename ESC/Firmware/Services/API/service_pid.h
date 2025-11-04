/**
 * @file service_pid.h
 * @brief Generic PID controller service (reusable for speed, current, voltage, etc.)
 *
 * This module provides a clean and reusable implementation of a classical PID controller.
 * It is independent of the control context and can be used by any higher layer
 * (e.g., motor speed control, temperature regulation, current loop, etc.).
 *
 * Key features:
 *  - Supports proportional, integral, and derivative terms
 *  - Includes integrator clamping (anti-windup)
 *  - Supports output saturation limits
 *  - Simple API for initialization, update, and reset
 */

#ifndef SERVICE_PID_H
#define SERVICE_PID_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @struct pid_t
 * @brief PID controller internal state and parameters.
 */
typedef struct {
    /* === Configuration parameters === */
    float kp;   /**< Proportional gain */
    float ki;   /**< Integral gain */
    float kd;   /**< Derivative gain */
    float dt;   /**< Sampling period in seconds */

    /* === Internal state === */
    float integrator;   /**< Integrator memory (accumulated error) */
    float prev_error;   /**< Previous error (for derivative term) */
    float output;       /**< Last computed output value */

    /* === Limits === */
    float out_min;      /**< Minimum output limit */
    float out_max;      /**< Maximum output limit */
    float integrator_limit; /**< Maximum allowed integrator value (anti-windup) */
} pid_t;

/**
 * @brief Initialize a PID controller instance.
 * 
 * @param pid Pointer to PID structure
 * @param kp Proportional gain
 * @param ki Integral gain
 * @param kd Derivative gain
 * @param dt Sampling period [s]
 */
void Service_PID_Init(pid_t *pid, float kp, float ki, float kd, float dt);

/**
 * @brief Compute the next PID output value.
 * 
 * @param pid Pointer to PID structure
 * @param setpoint Target value
 * @param measurement Measured value (feedback)
 * @return Computed output (after saturation)
 */
float Service_PID_Update(pid_t *pid, float setpoint, float measurement);

/**
 * @brief Reset the PID controller (clear integrator and previous error).
 * 
 * @param pid Pointer to PID structure
 */
void Service_PID_Reset(pid_t *pid);

#endif /* SERVICE_PID_H */
