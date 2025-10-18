#include "control.h"
#include "services.h"

void start_motor(void) {

    Service_Motor_Align_Rotor(0.1, 200); // Example: Align rotor with 10% duty for 200 ms
 
    Service_Motor_OpenLoopRamp(0.4, 0.5, 10.0, 500.0, 1000, true); // Ramp from 10% to 50% duty over 5 seconds in CW direction
}