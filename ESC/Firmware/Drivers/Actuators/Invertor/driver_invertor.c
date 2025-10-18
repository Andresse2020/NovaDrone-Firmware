/**
 * @file inverter_stm32g4.c
 * @brief 3-phase inverter driver using TIM1 (CH1/CH1N, CH2/CH2N, CH3/CH3N)
 *
 * Implements i_inverter_t interface.
 * PWM frequency, deadtime, polarity, and break input are configured in BSP.
 * This driver only handles arming, enabling/disabling PWM, duty updates,
 * emergency stop, and fault management.
 */

#include "i_inverter.h"
#include "bsp_utils.h"
#include "tim.h"

/* === External handles from CubeMX === */
extern TIM_HandleTypeDef htim1;  // TIM used for PWM generation

/* === Timer and channel mapping ======================================= */
// Pointer to timer used for PWM
static TIM_HandleTypeDef* inverter_tim = &htim1;

// Phase to TIM channel mapping (same identifier used for CHx and CHxN)
static const uint32_t inverter_channels[PHASE_COUNT] = {
    TIM_CHANNEL_1,  // Phase A
    TIM_CHANNEL_2,  // Phase B
    TIM_CHANNEL_3   // Phase C
};

/* === Internal state =================================================== */
static inverter_status_t inverter_status = {0};  // Tracks armed/enabled/running/faults
static inverter_duty_t inverter_duties = {0};   // Cached duty cycles (0.0..1.0)

/* === Forward declarations ============================================ */
static bool Driver_Init(void);
static bool Driver_Arm(void);
static bool Driver_Enable(void);
static bool Driver_Disable(void);
static void Driver_EmergencyStop(bool latch_fault);
static bool Driver_SetPhaseDuty(inverter_phase_t phase, float duty);
static bool Driver_SetAllDuties(const inverter_duty_t* duties);
static bool Driver_GetDuties(inverter_duty_t* out);
static void Driver_GetStatus(inverter_status_t* out);
static bool Driver_ClearFaults(void);
static void Driver_NotifyFault(inverter_fault_t fault);
static bool Driver_SetOutputState(inverter_phase_t phase, phase_output_state_t state);

/* === Global interface instance ======================================= */
i_inverter_t stm32g4_inverter_driver = {
    .init             = Driver_Init,
    .arm              = Driver_Arm,
    .enable           = Driver_Enable,
    .disable          = Driver_Disable,
    .emergency_stop   = Driver_EmergencyStop,
    .set_phase_duty   = Driver_SetPhaseDuty,
    .set_all_duties   = Driver_SetAllDuties,
    .get_duties       = Driver_GetDuties,
    .get_status       = Driver_GetStatus,
    .clear_faults     = Driver_ClearFaults,
    .notify_fault     = Driver_NotifyFault,
    .set_output_state = Driver_SetOutputState
};

i_inverter_t* IInverter = &stm32g4_inverter_driver;

/* ========================================================= */
/* === Implementation ====================================== */
/* ========================================================= */

/**
 * @brief Initialize the inverter driver.
 * Clears all internal status and cached duty cycles.
 * Hardware (TIM1 and GPIOs) is assumed configured in BSP.
 */
static bool Driver_Init(void)
{
    inverter_status.enabled = false;
    inverter_status.armed   = false;
    inverter_status.running = false;
    inverter_status.fault   = INVERTER_FAULT_NONE;

    for (int i = 0; i < PHASE_COUNT; i++)
        inverter_duties.phase_duty[i] = 0.0f;

    return true;
}

/**
 * @brief Arm the inverter.
 * Prepares hardware for enabling PWM outputs.
 * Does not start PWM yet.
 */
static bool Driver_Arm(void)
{
    if (inverter_status.fault != INVERTER_FAULT_NONE)
        return false;  // Cannot arm if fault exists

    inverter_status.armed = true;
    return true;
}

/**
 * @brief Enable PWM outputs on all 3 phases.
 * Requires that inverter is armed and no fault is active.
 */
static bool Driver_Enable(void)
{
    if (!inverter_status.armed || inverter_status.fault != INVERTER_FAULT_NONE)
        return false;

    for (int i = 0; i < PHASE_COUNT; i++)
    {
        // Start normal PWM channel
        HAL_TIM_PWM_Start(inverter_tim, inverter_channels[i]);
        // Start complementary channel
        HAL_TIMEx_PWMN_Start(inverter_tim, inverter_channels[i]);
    }

    inverter_status.enabled = true;
    inverter_status.running = true;

    return true;
}

/**
 * @brief Disable PWM outputs on all phases.
 * Keeps hardware configuration intact for fast re-enable.
 */
static bool Driver_Disable(void)
{
    for (int i = 0; i < PHASE_COUNT; i++)
    {
        HAL_TIM_PWM_Stop(inverter_tim, inverter_channels[i]);
        HAL_TIMEx_PWMN_Stop(inverter_tim, inverter_channels[i]);
    }

    inverter_status.enabled = false;
    inverter_status.running = false;

    return true;
}

/**
 * @brief Emergency stop.
 * Immediately disables PWM outputs and optionally latches a fault.
 */
static void Driver_EmergencyStop(bool latch_fault)
{
    Driver_Disable();

    if (latch_fault)
        inverter_status.fault = INVERTER_FAULT_HW;  // Generic hardware fault

    inverter_status.armed = false;
}

/**
 * @brief Set duty cycle for a single phase.
 * Duty is normalized 0..1. Clamped if necessary.
 */
static bool Driver_SetPhaseDuty(inverter_phase_t phase, float duty)
{
    if (phase >= PHASE_COUNT || duty < 0.0f || duty > 1.0f)
        return false;

    inverter_duties.phase_duty[phase] = duty;

    // Convert normalized duty to timer CCR value
    uint32_t pulse = (uint32_t)(duty * (__HAL_TIM_GET_AUTORELOAD(inverter_tim) + 1));
    __HAL_TIM_SET_COMPARE(inverter_tim, inverter_channels[phase], pulse);

    return true;
}

/**
 * @brief Set duty cycles for all phases atomically.
 */
static bool Driver_SetAllDuties(const inverter_duty_t* duties)
{
    if (!duties) return false;

    for (int i = 0; i < PHASE_COUNT; i++)
    {
        if (duties->phase_duty[i] < 0.0f || duties->phase_duty[i] > 1.0f)
            return false;  // Reject invalid value
    }

    inverter_duties = *duties;

    for (int i = 0; i < PHASE_COUNT; i++)
    {
        uint32_t pulse = (uint32_t)(duties->phase_duty[i] * (__HAL_TIM_GET_AUTORELOAD(inverter_tim) + 1));
        __HAL_TIM_SET_COMPARE(inverter_tim, inverter_channels[i], pulse);
    }

    return true;
}

/**
 * @brief Retrieve cached duty cycles.
 */
static bool Driver_GetDuties(inverter_duty_t* out)
{
    if (!out) return false;

    *out = inverter_duties;
    return true;
}

/**
 * @brief Get current inverter status.
 */
static void Driver_GetStatus(inverter_status_t* out)
{
    if (!out) return;

    *out = inverter_status;
}

/**
 * @brief Clear latched faults.
 */
static bool Driver_ClearFaults(void)
{
    inverter_status.fault = INVERTER_FAULT_NONE;
    return true;
}

/**
 * @brief Notify driver of a low-level fault detected by ISR or hardware.
 */
static void Driver_NotifyFault(inverter_fault_t fault)
{
    inverter_status.fault = fault;
    Driver_Disable();  // Immediately disable outputs on fault
}

/**
 * @brief Set output state for a single phase
 * @param phase Phase identifier
 * @param state Desired output state
 * @details Controls high-side and low-side outputs independently:
 * - Hi-Z: Both MOSFETs OFF
 * - PWM: Normal complementary operation
 * - PWM_HIGH: High-side PWM, low-side always OFF
 * - PWM_LOW: High-side always OFF, low-side PWM
 */
static bool Driver_SetOutputState(inverter_phase_t phase, phase_output_state_t state)
{
    if (phase >= PHASE_COUNT) return false;
    
    uint32_t channel = inverter_channels[phase];
    
    switch (state)
    {
        case STATE_HIZ:
            /* Both outputs Hi-Z (stop both channels) */
            HAL_TIM_PWM_Stop(inverter_tim, channel);
            HAL_TIMEx_PWMN_Stop(inverter_tim, channel);
            break;
            
        case STATE_PWM_ACTIVE:
            /* Normal complementary PWM (both channels active) */
            HAL_TIM_PWM_Start(inverter_tim, channel);
            HAL_TIMEx_PWMN_Start(inverter_tim, channel);
            break;
            
        case STATE_PWM_HIGH:
            /* High-side PWM, Low-side forced OFF */
            HAL_TIM_PWM_Start(inverter_tim, channel);
            HAL_TIMEx_PWMN_Stop(inverter_tim, channel);
            break;
            
        case STATE_PWM_LOW:
            /* High-side forced OFF, Low-side PWM */
            HAL_TIM_PWM_Stop(inverter_tim, channel);
            HAL_TIMEx_PWMN_Start(inverter_tim, channel);
            break;
            
        case STATE_FORCE_HIGH:
            /* Force 100% duty (high-side ON) */
            __HAL_TIM_SET_COMPARE(inverter_tim, channel, 
                                 __HAL_TIM_GET_AUTORELOAD(inverter_tim) + 1);
            HAL_TIM_PWM_Start(inverter_tim, channel);
            HAL_TIMEx_PWMN_Stop(inverter_tim, channel);
            break;
            
        case STATE_FORCE_LOW:
            /* Force 0% duty (low-side ON) */
            __HAL_TIM_SET_COMPARE(inverter_tim, channel, 0);
            HAL_TIM_PWM_Stop(inverter_tim, channel);
            HAL_TIMEx_PWMN_Start(inverter_tim, channel);
            break;
    }

    return true;
}