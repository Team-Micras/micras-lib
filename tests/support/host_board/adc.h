/**
 * @file
 *
 * @brief Fake STM32CubeMX adc.h of the reference projects: its handles and init functions.
 */

#ifndef MICRAS_LIB_TESTS_HOST_BOARD_ADC_H
#define MICRAS_LIB_TESTS_HOST_BOARD_ADC_H

#include "main.h"

/**
 * @brief Handle hadc3, which converts the internal reference and the temperature sensor.
 */
extern ADC_HandleTypeDef hadc3;

/**
 * @brief Configure what MX_ADC3_Init configures.
 */
void MX_ADC3_Init();

#endif  // MICRAS_LIB_TESTS_HOST_BOARD_ADC_H
