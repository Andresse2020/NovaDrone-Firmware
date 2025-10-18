#include "services.h"
#include "i_inverter.h"
#include "utilities.h"


/* === Step pattern structure ========================================== */
typedef struct {
    phase_output_state_t state[PHASE_COUNT];
} sixstep_pattern_t;


/* === Commutation table (CW direction) ================================ */
static const sixstep_pattern_t sixstep_table_cw[6] = {
    {{STATE_PWM_HIGH, STATE_PWM_LOW,  STATE_HIZ}},  // Step 0
    {{STATE_PWM_HIGH, STATE_HIZ,      STATE_PWM_LOW}},
    {{STATE_HIZ,      STATE_PWM_HIGH, STATE_PWM_LOW}},
    {{STATE_PWM_LOW,  STATE_PWM_HIGH, STATE_HIZ}},
    {{STATE_PWM_LOW,  STATE_HIZ,      STATE_PWM_HIGH}},
    {{STATE_HIZ,      STATE_PWM_LOW,  STATE_PWM_HIGH}}
};

/* === Commutation table (CCW direction) =============================== */
static const sixstep_pattern_t sixstep_table_ccw[6] = {
    {{STATE_HIZ,      STATE_PWM_LOW,  STATE_PWM_HIGH}}, // Step 0
    {{STATE_PWM_LOW,  STATE_HIZ,      STATE_PWM_HIGH}},
    {{STATE_PWM_LOW,  STATE_PWM_HIGH, STATE_HIZ}},
    {{STATE_HIZ,      STATE_PWM_HIGH, STATE_PWM_LOW}},
    {{STATE_PWM_HIGH, STATE_HIZ,      STATE_PWM_LOW}},
    {{STATE_PWM_HIGH, STATE_PWM_LOW,  STATE_HIZ}}
};


/**
 * @brief Align the rotor to a known position (CORRECT VERSION)
 * @details Uses complementary PWM with asymmetric duties for alignment
 */
void Service_Motor_Align_Rotor(float duty, uint32_t duration_ms)
{
    // Safety clamp
    if (duty < 0.0f) duty = 0.0f;
    if (duty > 1.0f) duty = 1.0f;

    /* === CONFIGURATION 1: Phase A high, Phase B low, Phase C floating === */
    inverter_duty_t duties = {
        .phase_duty = {
            duty,   // Phase A: PWM à duty% (current source)
            0.0f,   // Phase B: 0% duty = low-side toujours ON (current sink)
            0.0f    // Phase C: Don't care (sera en Hi-Z)
        }
    };

    // Configure output states BEFORE setting duties
    IInverter->set_output_state(PHASE_A, STATE_PWM_ACTIVE);  // PWM complémentaire
    IInverter->set_output_state(PHASE_B, STATE_PWM_ACTIVE);  // PWM complémentaire (0% = low ON)
    IInverter->set_output_state(PHASE_C, STATE_HIZ);         // Floating

    // Apply duties
    IInverter->set_all_duties(&duties);

    // Maintain alignment field
    ITime->delay_ms(duration_ms);

    // Return to Hi-Z for safety
    IInverter->disable();
}


/* === Main commutation function ======================================= */
/**
 * @brief Apply a six-step commutation pattern.
 * @param step Step index [0..5]
 * @param duty Normalized PWM duty (0.0 .. 1.0)
 * @param cw   Rotation direction (true = CW, false = CCW)
 */
void Inverter_SixStepCommutate(uint8_t step, float duty, bool cw)
{
    if (step >= 6) return;

    const sixstep_pattern_t *pattern = cw ? &sixstep_table_cw[step] : &sixstep_table_ccw[step];

    inverter_duty_t duties = {0};

    // Apply duty only on active phases
    for (uint8_t ph = 0; ph < PHASE_COUNT; ph++)
        duties.phase_duty[ph] = (pattern->state[ph] != STATE_HIZ) ? duty : 0.0f;

    // Apply duty atomically
    IInverter->set_all_duties(&duties);

    // Configure output states for each phase
    for (uint8_t ph = 0; ph < PHASE_COUNT; ph++)
        IInverter->set_output_state((inverter_phase_t)ph, pattern->state[ph]);
}


/**
 * @brief Open-loop ramp-up for six-step BLDC startup.
 * @param duty_start     Starting PWM duty (0.0–1.0)
 * @param duty_end       Final PWM duty (0.0–1.0)
 * @param freq_start_hz  Starting electrical frequency (Hz)
 * @param freq_end_hz    Final electrical frequency (Hz)
 * @param ramp_time_ms   Total ramp duration in milliseconds
 * @param cw             true = clockwise, false = counterclockwise
 *
 * This function performs a linear open-loop acceleration of the motor by
 * increasing both the electrical frequency and PWM duty simultaneously.
 * The rotor is driven through the six-step commutation sequence with
 * progressively shorter step delays.
 */
void Service_Motor_OpenLoopRamp(
    float duty_start,
    float duty_end,
    float freq_start_hz,
    float freq_end_hz,
    uint32_t ramp_time_ms,
    bool cw)
{
    const uint8_t step_count = 6;
    uint8_t step = 0;
    uint64_t elapsed_us = 0;  // microsecond precision

    uint64_t ramp_time_us = (uint64_t)ramp_time_ms * 1000ULL;

    while (elapsed_us < ramp_time_us)
    {
        /* Compute normalized ramp progress (0.0 → 1.0) */
        float ratio = (float)elapsed_us / (float)ramp_time_us;

        /* Interpolate current frequency */
        float freq = freq_start_hz + ratio * (freq_end_hz - freq_start_hz);

        /* Interpolate PWM duty */
        float duty = duty_start + ratio * (duty_end - duty_start);

        /* Compute delay per commutation step (µs) */
        float step_delay_us_f = 1e6f / (6.0f * freq);
        uint32_t step_delay_us = (uint32_t)(step_delay_us_f < 100.0f ? 100.0f : step_delay_us_f);

        /* Perform one commutation step */
        Inverter_SixStepCommutate(step, duty, cw);

        /* Wait for this step duration */
        ITime->delay_us(step_delay_us);

        /* Advance to next step */
        step = (step + 1) % step_count;

        /* Accumulate elapsed time in µs */
        elapsed_us += step_delay_us;
    }

    /* Disable inverter at the end for safety */
    IInverter->disable();
}

