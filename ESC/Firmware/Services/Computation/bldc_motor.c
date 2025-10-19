#include "services.h"
#include "i_inverter.h"
#include "i_time_oneshot.h"
#include <math.h>

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


/* ========================================================================== */
/* === Open-Loop Ramp (Event-Driven) ====================================== */
/* ========================================================================== */

/**
 * @brief Context structure for open-loop six-step ramp (event-driven).
 * 
 * This structure holds both static configuration parameters and
 * dynamic state variables updated at each commutation event.
 */
typedef struct
{
    /* === Configuration parameters (fixed during ramp) === */
    float duty_start;        /**< Starting duty (0.0–1.0) */
    float duty_end;          /**< Final duty (0.0–1.0) */
    float freq_start_hz;     /**< Starting electrical frequency (Hz) */
    float freq_end_hz;       /**< Final electrical frequency (Hz) */
    uint32_t ramp_time_us;   /**< Total ramp duration (µs) */
    bool direction_cw;       /**< true = clockwise, false = CCW */
    motor_ramp_profile_t profile_type;   /**< Ramp progression type */

    /* === User callback === */
    motor_ramp_callback_t on_complete;   /**< Callback called at ramp end */
    void *user_context;                  /**< Optional user data passed to callback */

    /* === Dynamic state variables (updated each step) === */
    uint8_t step_index;      /**< Current six-step position (0–5) */
    uint64_t elapsed_us;     /**< Accumulated elapsed time (µs) */
    float current_duty;      /**< Current applied duty */
    float current_freq_hz;   /**< Current electrical frequency (Hz) */
    bool active;             /**< Ramp currently running flag */

} motor_ramp_context_t;


/* ========================================================================== */
/* === Static ramp context ================================================= */
/* ========================================================================== */
static motor_ramp_context_t s_ramp_ctx;  // Only one active ramp at a time

/* Forward declaration of callback */
static void Motor_Ramp_OnStepEvent(void *user_context);



/* ========================================================================== */
/* === Function: Start Open-Loop Ramp ====================================== */
/* ========================================================================== */
/**
 * @brief Start an open-loop six-step ramp (event-driven, non-blocking).
 *
 * This function initializes the ramp context, performs the first commutation,
 * and schedules the next commutation step using the one-shot timer. The ramp
 * progresses automatically without blocking the CPU. When the ramp completes,
 * the optional user callback is invoked.
 *
 * @param duty_start     Starting duty (0.0–1.0)
 * @param duty_end       Final duty (0.0–1.0)
 * @param freq_start_hz  Starting electrical frequency (Hz)
 * @param freq_end_hz    Final electrical frequency (Hz)
 * @param ramp_time_ms   Total ramp duration in milliseconds
 * @param cw             true = clockwise, false = counterclockwise
 * @param profile_type   Ramp progression profile
 * @param on_complete    Optional callback called when ramp finishes (can be NULL)
 * @param user_ctx       User context pointer passed to the callback (can be NULL)
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
    void *user_ctx)
{
    /* === 1. Cancel any previous ramp ================================= */
    if (IOneShotTimer->isActive())
        IOneShotTimer->cancel();

    /* === 2. Initialize context ======================================= */
    memset(&s_ramp_ctx, 0, sizeof(s_ramp_ctx));

    s_ramp_ctx.duty_start       = duty_start;
    s_ramp_ctx.duty_end         = duty_end;
    s_ramp_ctx.freq_start_hz    = freq_start_hz;
    s_ramp_ctx.freq_end_hz      = freq_end_hz;
    s_ramp_ctx.ramp_time_us     = ramp_time_ms * 1000U;
    s_ramp_ctx.direction_cw     = cw;
    s_ramp_ctx.profile_type     = profile_type;
    s_ramp_ctx.on_complete      = on_complete;
    s_ramp_ctx.user_context     = user_ctx;

    s_ramp_ctx.step_index       = 0;
    s_ramp_ctx.elapsed_us       = 0;
    s_ramp_ctx.current_duty     = duty_start;
    s_ramp_ctx.current_freq_hz  = freq_start_hz;
    s_ramp_ctx.active           = true;

    /* === 3. Apply first commutation step immediately ================= */
    Inverter_SixStepCommutate(s_ramp_ctx.step_index, s_ramp_ctx.current_duty, cw);

    /* === 4. Compute delay for next commutation ======================= */
    float step_delay_us_f = 1e6f / (6.0f * s_ramp_ctx.current_freq_hz);
    uint32_t step_delay_us = (uint32_t)(step_delay_us_f < 100.0f ? 100.0f : step_delay_us_f);

    /* === 5. Schedule next step ======================================= */
    IOneShotTimer->start(step_delay_us, Motor_Ramp_OnStepEvent, &s_ramp_ctx);
}


/* ========================================================================== */
/* === Function: Ramp Callback (Timer Event) =============================== */
/* ========================================================================== */
/**
 * @brief One-shot timer callback for open-loop ramp progression.
 *
 * This callback is invoked automatically when the one-shot timer expires.
 * It performs one commutation step, updates the ramp progression,
 * computes new frequency/duty according to the selected profile,
 * and re-arms the timer until the ramp is complete.
 *
 * @param user_context Pointer to current ramp context (motor_ramp_context_t)
 */
static void Motor_Ramp_OnStepEvent(void *user_context)
{
    motor_ramp_context_t *ctx = (motor_ramp_context_t *)user_context;
    if (ctx == NULL || !ctx->active)
        return;

    /* === 1. Update elapsed time ===================================== */
    float step_period_us_f = 1e6f / (6.0f * ctx->current_freq_hz);
    uint32_t step_period_us = (uint32_t)(step_period_us_f < 100.0f ? 100.0f : step_period_us_f);
    ctx->elapsed_us += step_period_us;

    /* === 2. Check for ramp completion ================================ */
    if (ctx->elapsed_us >= ctx->ramp_time_us)
    {
        ctx->active = false;
        IInverter->disable();   // Stop PWM safely

        // Notify application if callback is set
        if (ctx->on_complete)
            ctx->on_complete(ctx->user_context);

        return;
    }

    /* === 3. Compute progress ratio (0.0 → 1.0) ======================= */
    float ratio = (float)ctx->elapsed_us / (float)ctx->ramp_time_us;
    if (ratio > 1.0f)
        ratio = 1.0f;

    /* === 4. Compute frequency based on profile ======================= */
    switch (ctx->profile_type)
    {
        default:
        case RAMP_PROFILE_LINEAR:
            ctx->current_freq_hz =
                ctx->freq_start_hz +
                ratio * (ctx->freq_end_hz - ctx->freq_start_hz);
            break;

        case RAMP_PROFILE_EXPONENTIAL:
            ctx->current_freq_hz =
                ctx->freq_start_hz *
                powf((ctx->freq_end_hz / ctx->freq_start_hz), ratio);
            break;

        case RAMP_PROFILE_QUADRATIC:
            ctx->current_freq_hz =
                ctx->freq_start_hz +
                powf(ratio, 2.0f) * (ctx->freq_end_hz - ctx->freq_start_hz);
            break;

        case RAMP_PROFILE_LOGARITHMIC:
            ctx->current_freq_hz =
                ctx->freq_end_hz -
                (ctx->freq_end_hz - ctx->freq_start_hz) * expf(-4.0f * ratio);
            break;
    }

    /* === 5. Compute duty (smooth progression) ======================== */
    ctx->current_duty =
        ctx->duty_start +
        powf(ratio, 1.5f) * (ctx->duty_end - ctx->duty_start);

    /* === 6. Perform next commutation step ============================ */
    ctx->step_index = (ctx->step_index + 1) % 6;
    Inverter_SixStepCommutate(ctx->step_index, ctx->current_duty, ctx->direction_cw);

    /* === 7. Schedule next commutation ================================ */
    float next_step_us_f = 1e6f / (6.0f * ctx->current_freq_hz);
    uint32_t next_step_us = (uint32_t)(next_step_us_f < 100.0f ? 100.0f : next_step_us_f);

    IOneShotTimer->start(next_step_us, Motor_Ramp_OnStepEvent, ctx);
}


/* ========================================================================== */
/* === Function: Stop Open-Loop Ramp ====================================== */
/* ========================================================================== */

/**
 * @brief Stop the ongoing open-loop six-step ramp.
 *
 * This function cancels any pending timer event, disables the inverter outputs,
 * and clears the internal ramp context. It can be called at any time, even if
 * no ramp is currently active.
 */
void Service_Motor_OpenLoopRamp_Stop(void)
{
    // 1. Cancel pending timer event
    if (IOneShotTimer->isActive())
    {
        IOneShotTimer->cancel();
    }

    // 2. Disable inverter for safety
    IInverter->disable();

    // 3. Clear ramp context
    memset(&s_ramp_ctx, 0, sizeof(s_ramp_ctx));
    s_ramp_ctx.active = false;
}
