#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include "stm32g4xx_ll_adc.h"   // LL helper to enable internal paths


// Common utility functions and definitions

void Error_Handler(void); // Error handler function


#ifdef __cplusplus
}
#endif

#endif /* COMMON_H */