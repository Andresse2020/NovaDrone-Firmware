#include "services.h"
#include "i_led.h"
#include "i_time.h"
#include "i_time_oneshot.h"

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



void LedToggle_Callback(void *ctx)
{
    (void)ctx; // unused
    ILED->toggle(LED_STATUS);        // Toggle the status LED

    // Re-arm the timer for another 1-second delay
    IOneShotTimer->start(100000, LedToggle_Callback, NULL);
}

/**
 * @brief Simple test of one-shot timer driver.
 */
void Service_Test_OneShotTimer(void)
{
    // Initialize the driver
    // IOneShotTimer->init();

    // Start the first one-shot delay (1 second)
    IOneShotTimer->start(100000, LedToggle_Callback, NULL);
}