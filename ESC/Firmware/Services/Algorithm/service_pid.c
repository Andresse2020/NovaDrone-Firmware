/**
 * @file service_pid.c
 * @brief Implementation of a generic PID controller.
 */

#include "service_pid.h"

/**
 * @brief Initialize a PID controller with given parameters.
 */
void Service_PID_Init(pid_t *pid, float kp, float ki, float kd, float dt)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->dt = dt;

    pid->integrator = 0.0f;
    pid->prev_error = 0.0f;
    pid->output = 0.0f;

    /* Default limits (can be customized after init) */
    pid->out_min = 0.0f;
    pid->out_max = 1.0f;
    pid->integrator_limit = 1.0f;
}

/**
 * @brief Compute the next PID output based on setpoint and measurement.
 *
 * The PID algorithm is implemented as:
 *
 *     error = setpoint - measurement
 *     integrator += Ki * error * dt
 *     derivative = (error - prev_error) / dt
 *     output = Kp * error + integrator + Kd * derivative
 *
 * With saturation and anti-windup protection.
 */
float Service_PID_Update(pid_t *pid, float setpoint, float measurement)
{
    /* --- Compute instantaneous error --- */
    float error = setpoint - measurement;

    /* --- Integrator with anti-windup --- */
    pid->integrator += pid->ki * error * pid->dt;

    if (pid->integrator > pid->integrator_limit)
        pid->integrator = pid->integrator_limit;
    else if (pid->integrator < -pid->integrator_limit)
        pid->integrator = -pid->integrator_limit;

    /* --- Derivative term (based on error delta) --- */
    float derivative = (error - pid->prev_error) / pid->dt;
    pid->prev_error = error;

    /* --- Compute raw output --- */
    float output = pid->kp * error + pid->integrator + pid->kd * derivative;

    /* --- Output saturation --- */
    if (output > pid->out_max)
        output = pid->out_max;
    else if (output < pid->out_min)
        output = pid->out_min;

    pid->output = output;
    return output;
}

/**
 * @brief Reset the internal state of the PID controller.
 */
void Service_PID_Reset(pid_t *pid)
{
    pid->integrator = 0.0f;
    pid->prev_error = 0.0f;
    pid->output = 0.0f;
}