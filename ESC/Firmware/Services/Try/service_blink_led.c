#include "services.h"
#include "i_led.h"
#include "utilities.h"

// Non-blocking status LED blink using ITime->getTick()
void service_blink_status_Led(uint32_t delay_ms) {
    static uint32_t last_toggle_tick = 0; // Keep track of the last toggle time
    uint32_t current_tick = ITime->getTick(); // Get current system tick in ms

    // Check if the delay has elapsed
    if ((current_tick - last_toggle_tick) >= delay_ms) {
        ILED->toggle(LED_STATUS);        // Toggle the status LED
        last_toggle_tick = current_tick; // Update last toggle time
    }
}