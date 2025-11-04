/**
 * @file dc_motor.h
 * @brief General API for DC motor control using inverter phase pairs.
 *
 * This module provides a simple, event-free (direct) control interface
 * for driving a DC motor using any two inverter phases (A, B, C).
 * It enables bidirectional control and can be used for hardware validation,
 * wiring tests, or basic motor spin-up before implementing advanced control
 * (e.g., FOC or sensor feedback).
 *
 * === Main Features =======================================================
 * - Control DC motor direction and speed using inverter phase pairs
 * - Bidirectional control (positive and negative duty)
 * - Support for all possible phase pairs:
 *     * Phase A ↔ Phase B
 *     * Phase B ↔ Phase C
 *     * Phase C ↔ Phase A
 * - Safe stop function (sets all phases to Hi-Z or 0 duty)
 * - Duty clamping and maximum duty protection
 *
 * === Example Usage =======================================================
 * ```c
 * #include "dc_motor.h"
 *
 * void app_test_motor(void)
 * {
 *     // Drive motor between Phase A and B forward at 50% duty
 *     Service_DC_Command_AB(0.5f);
 *
 *     // Reverse direction
 *     Service_DC_Command_AB(-0.5f);
 *
 *     // Stop motor
 *     Service_DC_StopAll();
 * }
 * ```
 *
 * @date    October 22, 2025
 * @version 1.0
 * @license Internal / project-specific license
 */

#ifndef DC_MOTOR_H
#define DC_MOTOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/* === Public API ========================================================== */
/* ========================================================================= */

/**
 * @brief Command a DC motor connected between PHASE_A and PHASE_B.
 *
 * @param duty Normalized PWM duty (-1.0 to +1.0):
 *             - Positive values → forward direction (A → B)
 *             - Negative values → reverse direction (B → A)
 *             - 0.0 → stop (float both sides)
 */
void Service_DC_Command_AB(float duty);

/**
 * @brief Command a DC motor connected between PHASE_B and PHASE_C.
 *
 * @param duty Normalized PWM duty (-1.0 to +1.0):
 *             - Positive values → forward direction (B → C)
 *             - Negative values → reverse direction (C → B)
 *             - 0.0 → stop (float both sides)
 */
void Service_DC_Command_BC(float duty);

/**
 * @brief Command a DC motor connected between PHASE_C and PHASE_A.
 *
 * @param duty Normalized PWM duty (-1.0 to +1.0):
 *             - Positive values → forward direction (C → A)
 *             - Negative values → reverse direction (A → C)
 *             - 0.0 → stop (float both sides)
 */
void Service_DC_Command_CA(float duty);

/**
 * @brief Stop all DC motor outputs (float all inverter phases).
 *
 * Sets all phase PWM duties to 0.0 (or high-impedance depending on inverter driver).
 */
void Service_DC_StopAll(void);


/* ========================================================================= */
/* === End of Public Interface ============================================ */
/* ========================================================================= */

#ifdef __cplusplus
}
#endif

#endif /* DC_MOTOR_H */
