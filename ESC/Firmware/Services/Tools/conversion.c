#include "services.h"

/**
 * @brief Convert a float number to string for debug output.
 *
 * Works without printf/%f support. Handles negative numbers and
 * configurable decimal precision.
 *
 * @param value     Float value to convert
 * @param str       Destination buffer (must be large enough)
 * @param precision Number of digits after decimal point (e.g., 2 -> 1.23)
 */
void Service_FloatToString(float value, char* str, uint8_t precision)
{
    if (!str) return;

    char* ptr = str;

    // Handle negative numbers
    if (value < 0.0f) {
        *ptr++ = '-';
        value = -value;
    }

    // Extract integer part
    uint32_t int_part = (uint32_t)value;
    float frac_part = value - (float)int_part;

    // Convert integer part to string (reverse first)
    char int_str[12]; // enough for 32-bit integer
    int idx = 0;
    if (int_part == 0) {
        int_str[idx++] = '0';
    } else {
        while (int_part > 0) {
            int_str[idx++] = '0' + (int_part % 10);
            int_part /= 10;
        }
    }
    // Reverse integer part
    for (int i = idx - 1; i >= 0; i--) {
        *ptr++ = int_str[i];
    }

    // Add decimal point
    if (precision > 0) {
        *ptr++ = '.';

        // Convert fractional part
        for (uint8_t i = 0; i < precision; i++) {
            frac_part *= 10.0f;
            uint8_t digit = (uint8_t)frac_part;
            *ptr++ = '0' + digit;
            frac_part -= digit;
        }
    }

    // Null-terminate string
    *ptr = '\0';
}
