#include "control.h"
#include "services.h"

void start_motor(void) {

    // Service_Motor_Align_Rotor(0.1, 200); // Example: Align rotor with 10% duty for 200 ms
 
    Service_Motor_OpenLoopRamp_Start(0.4, 0.5, 10.0, 500.0, 10000, true, RAMP_PROFILE_EXPONENTIAL, NULL, NULL); // Ramp from 10% to 50% duty over 5 seconds in CW direction
}