/**
 * @file service_dc_motor.c
 * @brief Service layer - DC motor control using inverter interface (2-phase pairs).
 *
 * This module provides simple bidirectional DC control on each possible phase pair:
 *  - Phase A ↔ Phase B
 *  - Phase B ↔ Phase C
 *  - Phase C ↔ Phase A
 *
 * Each pair can be used to test motor wiring or pre-FOC hardware validation.
 */

#include "services.h"
#include "i_inverter.h"
#include <math.h>

/* -------------------------------------------------------------------------- */
/*                              Config constants                              */
/* -------------------------------------------------------------------------- */

#define DC_MOTOR_DEAD_DUTY  0.0f     /**< PWM duty for inactive side */
#define DC_MOTOR_MAX_DUTY   0.95f    /**< Limit to prevent 100% duty */

/* -------------------------------------------------------------------------- */
/*                        Internal helper function                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief Generic DC control between two inverter phases.
 */
static void apply_dc_pair(inverter_phase_t high_side, inverter_phase_t low_side, float duty)
{
    // Clamp
    if (duty > 1.0f) duty = 1.0f;
    else if (duty < -1.0f) duty = -1.0f;

    float abs_duty = fabsf(duty);
    if (abs_duty > DC_MOTOR_MAX_DUTY)
        abs_duty = DC_MOTOR_MAX_DUTY;

    if (duty > 0.0f)
    {
        // Forward: low → high
        IInverter->set_phase_duty(high_side, abs_duty);
        IInverter->set_phase_duty(low_side, DC_MOTOR_DEAD_DUTY);
    }
    else if (duty < 0.0f)
    {
        // Reverse: high → low
        IInverter->set_phase_duty(high_side, DC_MOTOR_DEAD_DUTY);
        IInverter->set_phase_duty(low_side, abs_duty);
    }
    else
    {
        // Stop (floating)
        IInverter->set_phase_duty(high_side, DC_MOTOR_DEAD_DUTY);
        IInverter->set_phase_duty(low_side, DC_MOTOR_DEAD_DUTY);
    }
}

/* -------------------------------------------------------------------------- */
/*                        Public service functions                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief Command a DC motor between PHASE_A and PHASE_B.
 */
void Service_DC_Command_AB(float duty)
{
    apply_dc_pair(PHASE_B, PHASE_A, duty);
}

/**
 * @brief Command a DC motor between PHASE_B and PHASE_C.
 */
void Service_DC_Command_BC(float duty)
{
    apply_dc_pair(PHASE_C, PHASE_B, duty);
}

/**
 * @brief Command a DC motor between PHASE_C and PHASE_A.
 */
void Service_DC_Command_CA(float duty)
{
    apply_dc_pair(PHASE_A, PHASE_C, duty);
}

/**
 * @brief Stop all DC motor outputs (float all phases).
 */
void Service_DC_StopAll(void)
{
    IInverter->set_all_duties(&(inverter_duty_t){ .phase_duty = {0.0f, 0.0f, 0.0f} });
}
