// /**
//  * @file control_motor.c
//  * @brief Sensorless BLDC six-step control (open-loop ramp + closed-loop)
//  *
//  * FR :
//  *   - Avance de commutation corrigée : 30° (T/12)
//  *   - Duty minimum garanti à la transition
//  *   - Pas de formatage float direct (%s utilisé)
//  *   - Protection contre double armement du timer
//  */

// #include "control.h"
// #include "service_generic.h"
// #include "service_bemf_monitor.h"
// #include "service_bldc_motor.h"
// #include "service_loop.h"

// #include <stdbool.h>
// #include <stdint.h>

// /* ============================================================================
//  * Configuration constants
//  * ========================================================================== */

// #define COMM_DELAY_MIN_US           80.0f
// #define COMM_DELAY_MAX_US           30000.0f
// #define COMM_LEAD_FACTOR            0.5f   // ~30° electrical advance
// #define CL_MIN_VALID_ZC             6
// #define CL_MIN_DUTY_TRANSITION      0.35f

// /* ============================================================================
//  * Motor mode state machine
//  * ========================================================================== */

// typedef enum {
//     MOTOR_MODE_STOPPED = 0,
//     MOTOR_MODE_OPEN_LOOP,
//     MOTOR_MODE_CLOSED_LOOP
// } motor_mode_t;

// static motor_mode_t s_motor_mode = MOTOR_MODE_STOPPED;
// static uint8_t s_valid_zc_count = 0;

// /* ============================================================================
//  * Local variables
//  * ========================================================================== */

// static s_motor_phase_t s_floating_phase = S_MOTOR_PHASE_A;
// static bemf_status_t s_bemf_status;

// static char buffer_period[16];
// static char buffer_delay[16];
// static char buffer_duty[16];

// /* ============================================================================
//  * Closed-loop context
//  * ========================================================================== */

// static struct {
//     uint8_t step;
//     bool direction_cw;
//     float duty;
//     bool comm_armed;
// } s_closedloop_ctx = {
//     .step = 0,
//     .direction_cw = true,
//     .duty = 0.40f,
//     .comm_armed = false
// };

// /* ============================================================================
//  * Floating phase lookup
//  * ========================================================================== */
// /**
//  * @brief Returns the floating (HIZ) phase for the given commutation step.
//  * Must match the inverter six-step table exactly.
//  */
// static s_motor_phase_t Motor_GetFloatingPhase(uint8_t step, bool direction_cw)
// {
//     static const s_motor_phase_t floating_phase_cw[6] = {
//         S_MOTOR_PHASE_C,  // Step 0: A→B active, C floating
//         S_MOTOR_PHASE_B,  // Step 1: A→C active, B floating
//         S_MOTOR_PHASE_A,  // Step 2: B→C active, A floating
//         S_MOTOR_PHASE_C,  // Step 3: B→A active, C floating
//         S_MOTOR_PHASE_B,  // Step 4: C→A active, B floating
//         S_MOTOR_PHASE_A   // Step 5: C→B active, A floating
//     };

//     static const s_motor_phase_t floating_phase_ccw[6] = {
//         S_MOTOR_PHASE_B,  // Step 0
//         S_MOTOR_PHASE_C,  // Step 1
//         S_MOTOR_PHASE_A,  // Step 2
//         S_MOTOR_PHASE_B,  // Step 3
//         S_MOTOR_PHASE_C,  // Step 4
//         S_MOTOR_PHASE_A   // Step 5
//     };

//     return direction_cw ? floating_phase_cw[step % 6]
//                         : floating_phase_ccw[step % 6];
// }

// /* ============================================================================
//  * Closed-loop commutation callback
//  * ========================================================================== */
// /**
//  * @brief Executes one commutation step during closed-loop operation.
//  */
// static void Motor_ClosedLoop_Commutate(void *user_ctx)
// {
//     (void)user_ctx;
//     s_closedloop_ctx.comm_armed = false;

//     s_closedloop_ctx.step = (s_closedloop_ctx.step + 1) % 6;

//     Inverter_SixStepCommutate(
//         s_closedloop_ctx.step,
//         s_closedloop_ctx.duty,
//         s_closedloop_ctx.direction_cw
//     );

//     s_floating_phase = Motor_GetFloatingPhase(
//         s_closedloop_ctx.step,
//         s_closedloop_ctx.direction_cw
//     );

//     Service_FloatToString(s_closedloop_ctx.duty, buffer_duty, 3);
//     LOG_INFO("CL Commutation -> step=%d | floating=%d | duty=%s",
//              s_closedloop_ctx.step, s_floating_phase, buffer_duty);
// }

// /* ============================================================================
//  * Motor fast loop (24 kHz)
//  * ========================================================================== */
// /**
//  * @brief Called at 24 kHz to monitor BEMF and manage commutation events.
//  */
// void Motor_FastLoop(void)
// {
//     SBemfMonitor->process(s_floating_phase);
//     SBemfMonitor->get_status(&s_bemf_status);

//     if (s_bemf_status.zero_cross_detected)
//     {
//         Service_FloatToString(s_bemf_status.period_us, buffer_period, 2);
//         LOG_INFO("BEMF ZC: phase=%d | period=%s us | valid=%d | mode=%d",
//                  s_bemf_status.floating_phase,
//                  buffer_period,
//                  s_bemf_status.valid,
//                  s_motor_mode);

//         /* === Closed-loop mode === */
//         if (s_motor_mode == MOTOR_MODE_CLOSED_LOOP && s_bemf_status.valid)
//         {
//             if (!s_closedloop_ctx.comm_armed)
//             {
//                 float delay_us = s_bemf_status.period_us * COMM_LEAD_FACTOR;
//                 if (delay_us < COMM_DELAY_MIN_US) delay_us = COMM_DELAY_MIN_US;
//                 if (delay_us > COMM_DELAY_MAX_US) delay_us = COMM_DELAY_MAX_US;

//                 Service_ScheduleCommutation(delay_us, Motor_ClosedLoop_Commutate, NULL);
//                 s_closedloop_ctx.comm_armed = true;

//                 Service_FloatToString(delay_us, buffer_delay, 2);
//                 LOG_INFO("Scheduled CL commutation in %s us | step=%d | float=%d",
//                          buffer_delay, s_closedloop_ctx.step, s_floating_phase);
//             }
//         }

//         SBemfMonitor->clear_flag();
//     }

//     /* === Transition: Open-loop → Closed-loop === */
//     if (s_motor_mode == MOTOR_MODE_OPEN_LOOP && s_bemf_status.valid)
//     {
//         if (++s_valid_zc_count >= CL_MIN_VALID_ZC)
//         {
//             s_motor_mode = MOTOR_MODE_CLOSED_LOOP;
//             LOG_INFO("Switching to CLOSED_LOOP mode");

//             Service_Motor_OpenLoopRamp_GetState(
//                 &s_closedloop_ctx.step,
//                 &s_closedloop_ctx.duty,
//                 &s_closedloop_ctx.direction_cw
//             );

//             if (s_closedloop_ctx.duty < CL_MIN_DUTY_TRANSITION)
//                 s_closedloop_ctx.duty = CL_MIN_DUTY_TRANSITION;

//             s_floating_phase = Motor_GetFloatingPhase(
//                 s_closedloop_ctx.step,
//                 s_closedloop_ctx.direction_cw
//             );

//             Service_Motor_OpenLoopRamp_StopSoft();

//             /* Schedule initial commutation after 30° */
//             if (!s_closedloop_ctx.comm_armed && s_bemf_status.period_us > 0.0f)
//             {
//                 float delay_us = s_bemf_status.period_us * COMM_LEAD_FACTOR;
//                 if (delay_us < COMM_DELAY_MIN_US) delay_us = COMM_DELAY_MIN_US;
//                 if (delay_us > COMM_DELAY_MAX_US) delay_us = COMM_DELAY_MAX_US;

//                 Service_ScheduleCommutation(delay_us, Motor_ClosedLoop_Commutate, NULL);
//                 s_closedloop_ctx.comm_armed = true;

//                 Service_FloatToString(delay_us, buffer_delay, 2);
//                 LOG_INFO("Initial CL commutation scheduled in %s us | step=%d | float=%d",
//                          buffer_delay, s_closedloop_ctx.step, s_floating_phase);
//             }
//             else
//             {
//                 LOG_INFO("Cannot arm initial CL commutation: invalid period");
//             }
//         }
//     }
// }

// /* ============================================================================
//  * Initialization & ramp start
//  * ========================================================================== */

// void Control_Motor_StartRamp(void)
// {
//     SBemfMonitor->reset();
//     s_motor_mode = MOTOR_MODE_OPEN_LOOP;
//     s_valid_zc_count = 0;

//     LOG_INFO("Motor mode: OPEN_LOOP (ramp start)");

//     Service_Motor_OpenLoopRamp_Start(
//         0.3f,  // Starting duty
//         0.50f,  // Final duty
//         10.0f,   // Start freq (Hz)
//         500.0f, // End freq (Hz)
//         2000,   // Duration (ms)
//         true,   // CW
//         RAMP_PROFILE_LINEAR,
//         NULL,
//         NULL
//     );

//     LOG_INFO("Open-loop ramp started");
// }

// void Control_Motor_Init(void)
// {
//     SBemfMonitor->init();
//     SFastLoop->init();
//     SFastLoop->register_callback(Motor_FastLoop);
//     SFastLoop->start();

//     Service_Motor_Align_Rotor(0.3f, 500, Control_Motor_StartRamp);

//     LOG_INFO("BEMF Monitor test started (24 kHz fast loop)");
// }