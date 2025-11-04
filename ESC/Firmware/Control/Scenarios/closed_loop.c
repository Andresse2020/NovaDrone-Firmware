// /**
//  * @file closed_loop.c
//  * @brief Closed-loop sensorless control using BEMF zero-crossing detection
//  */

// #include "control.h"
// #include "service_generic.h"
// #include "service_bemf_monitor.h"

// /* Private variables */
// static bool s_closed_loop_active = false;
// static float s_current_speed = 0.0f;
// static uint8_t s_current_step = 0;
// static float s_current_duty = 0.0f;

// /**
//  * @brief Callback when zero-crossing is detected
//  */
// static void OnZeroCross(void)
// {
//     if (!s_closed_loop_active) return;

//     // Get timing information
//     bemf_status_t bemf_status;
//     SBemfMonitor->get_status(&bemf_status);

//     if (bemf_status.valid)
//     {
//         // Calculate next commutation timing (30° after ZC)
//         float delay_us = bemf_status.period_us / 6.0f;
        
//         // Schedule next commutation
//         Service_ScheduleCommutation(delay_us, NULL, NULL);
        
//         // Update step
//         s_current_step = (s_current_step + 1) % 6;
        
//         // Apply commutation
//         Inverter_SixStepCommutate(s_current_step, s_current_duty, true);
//     }
// }

// void Control_Motor_StartClosedLoop(void)
// {
//     // Get current state from open loop
//     Service_Motor_OpenLoopRamp_GetState(&s_current_step, &s_current_duty, NULL);
    
//     // Stop open loop WITHOUT disabling inverter
//     Service_Motor_OpenLoopRamp_StopSoft();
    
//     // Enable closed loop control
//     s_closed_loop_active = true;
    
//     // Register zero-cross callback
//     SBemfMonitor->register_callback(OnZeroCross);
// }