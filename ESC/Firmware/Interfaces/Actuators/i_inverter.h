/**
 * @file i_inverter.h
 * @brief Abstract interface for a 3-phase PWM inverter controller.
 *
 * This interface defines a hardware-agnostic API for controlling a 3-phase inverter.
 * Each phase is controlled by complementary PWM outputs. Faults, emergency stops,
 * and duty management are included. All hardware configuration (frequency, deadtime,
 * polarity) is handled in the BSP.
 *
 * Target MCU: STM32G4 series (STM32G473CCTX) but the interface is portable.
 */

#ifndef I_INVERTER_H
#define I_INVERTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* === Phase identifiers ================================================= */

/** 
 * @brief Phases of the 3-phase inverter. 
 * PHASE_COUNT is convenient for loops and arrays.
 */
typedef enum {
    PHASE_A = 0,
    PHASE_B = 1,
    PHASE_C = 2,
    PHASE_COUNT = 3
} inverter_phase_t;

/* === Fault definitions ================================================= */

/**
 * @brief Fault reasons for inverter hardware or safety events.
 * Used for status reporting and fault handling.
 */
typedef enum {
    INVERTER_FAULT_NONE = 0,        /**< No fault detected */
    INVERTER_FAULT_OVERCURRENT,     /**< Overcurrent protection triggered */
    INVERTER_FAULT_OVERTEMP,        /**< Overtemperature detected */
    INVERTER_FAULT_UNDERVOLT,       /**< Supply voltage too low */
    INVERTER_FAULT_BREAK_INPUT,     /**< External break input triggered */
    INVERTER_FAULT_HW,              /**< Hardware fault (driver / gate issue) */
    INVERTER_FAULT_UNKNOWN          /**< Unknown / unclassified fault */
} inverter_fault_t;

/* === Inverter status ================================================== */

/**
 * @brief Basic inverter status flags.
 * Provides a quick view of the current state and last fault.
 */
typedef struct {
    bool enabled;            /**< PWM outputs currently active */
    bool armed;              /**< Inverter armed (precharge checks passed) */
    bool running;            /**< PWM actively switching */
    inverter_fault_t fault;  /**< Last fault detected */
} inverter_status_t;

/* === Duty cycle structure ============================================= */

/**
 * @brief Duty values for 3 phases.
 * Each duty is normalized (0.0 .. 1.0). Negative values are invalid.
 * This is used both for setting and reading cached duty cycles.
 */
typedef struct {
    float phase_duty[PHASE_COUNT];
} inverter_duty_t;

/* === Interface function typedefs ====================================== */

/**
 * @brief Initialize the inverter driver and configure hardware peripherals.
 * This includes TIMs, complementary outputs, GPIOs, and break input.
 * All hardware configuration (frequency, deadtime, polarity) is handled in BSP.
 * @return true if initialization succeeds
 */
typedef bool (*inverter_init_t)(void);

/**
 * @brief Arm the inverter.
 * Performs precharge, gate driver enable checks, or any necessary safety checks.
 * Does not start PWM outputs — prepares hardware for enable.
 * @return true if arming succeeds
 */
typedef bool (*inverter_arm_t)(void);

/**
 * @brief Enable PWM outputs after the inverter is armed.
 * Starts switching of all 3 phases.
 * @return true if outputs are successfully enabled
 */
typedef bool (*inverter_enable_t)(void);

/**
 * @brief Disable PWM outputs immediately.
 * Keeps peripheral configuration intact for fast re-enable.
 * @return true if outputs were successfully disabled
 */
typedef bool (*inverter_disable_t)(void);

/**
 * @brief Emergency stop.
 * Immediately disables outputs and optionally latches a fault state.
 * @param latch_fault If true, fault state is latched and must be cleared manually.
 */
typedef void (*inverter_emergency_stop_t)(bool latch_fault);

/**
 * @brief Set duty cycle for a single phase.
 * Duty is clamped or rejected if outside safety limits.
 * @param phase Phase identifier (PHASE_A/B/C)
 * @param duty Normalized duty (0.0 .. 1.0)
 * @return true if duty accepted and applied
 */
typedef bool (*inverter_set_phase_duty_t)(inverter_phase_t phase, float duty);

/**
 * @brief Set duty cycles for all 3 phases at once (atomic update).
 * Prevents intermediate states during simultaneous phase updates.
 * @param duties Pointer to caller-owned inverter_duty_t structure
 * @return true if duties accepted and applied
 */
typedef bool (*inverter_set_all_duties_t)(const inverter_duty_t* duties);

/**
 * @brief Retrieve the current cached duty cycles.
 * Non-blocking; returns last set values.
 * @param out Pointer to inverter_duty_t to fill
 * @return true if cached values are valid
 */
typedef bool (*inverter_get_duties_t)(inverter_duty_t* out);

/**
 * @brief Get current inverter status.
 * @param out Pointer to inverter_status_t structure to fill
 */
typedef void (*inverter_get_status_t)(inverter_status_t* out);

/**
 * @brief Clear latched faults (if supported by driver).
 * Resets fault state after diagnostics or recovery.
 * @return true if faults successfully cleared
 */
typedef bool (*inverter_clear_faults_t)(void);

/**
 * @brief Notify driver of low-level fault detected by ISR or hardware.
 * Called from ADC, comparator, or gate driver ISR.
 * @param fault Detected fault reason
 */
typedef void (*inverter_notify_fault_t)(inverter_fault_t fault);

/* === Interface struct ================================================= */

typedef struct {
    inverter_init_t            init;             /**< Initialize hardware */
    inverter_arm_t             arm;              /**< Arm inverter (precharge) */
    inverter_enable_t          enable;           /**< Enable PWM outputs */
    inverter_disable_t         disable;          /**< Disable PWM outputs */
    inverter_emergency_stop_t  emergency_stop;   /**< Emergency stop + latch fault */
    inverter_set_phase_duty_t  set_phase_duty;   /**< Set duty of single phase */
    inverter_set_all_duties_t  set_all_duties;   /**< Set duties for all phases atomically */
    inverter_get_duties_t      get_duties;       /**< Read cached duties */
    inverter_get_status_t      get_status;       /**< Read inverter status */
    inverter_clear_faults_t    clear_faults;     /**< Clear latched faults */
    inverter_notify_fault_t    notify_fault;     /**< Notify of low-level fault from ISR */
} i_inverter_t;

/* === Global instance ================================================== */
extern i_inverter_t* IInverter;

#ifdef __cplusplus
}
#endif

#endif /* I_INVERTER_H */
