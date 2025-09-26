#ifndef CLOCK_CONFIG_H
#define CLOCK_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_utils.h"

/**
 * @brief Configure the system clock.
 * 
 * Cette fonction doit être appelée au démarrage pour configurer
 * la fréquence du CPU, les PLL, et les horloges périphériques.
 */
void SystemClock_Config(void);

#ifdef __cplusplus
}
#endif

#endif /* CLOCK_CONFIG_H */

