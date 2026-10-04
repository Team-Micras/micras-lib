/**
 * @file
 *
 * @brief Fake STM32CubeMX tim.h of the reference projects: its handles and init functions.
 */

#ifndef MICRAS_LIB_TESTS_HOST_BOARD_TIM_H
#define MICRAS_LIB_TESTS_HOST_BOARD_TIM_H

#include "main.h"

/**
 * @brief Handle htim2, a free-running 32-bit timer.
 */
extern TIM_HandleTypeDef htim2;

/**
 * @brief Handle htim5, a timer in encoder mode.
 */
extern TIM_HandleTypeDef htim5;

/**
 * @brief Handle htim8, whose first channel a DMA stream feeds.
 */
extern TIM_HandleTypeDef htim8;

/**
 * @brief Handle htim15, whose first channel is a PWM output.
 */
extern TIM_HandleTypeDef htim15;

/**
 * @brief Configure what MX_TIM2_Init configures.
 */
void MX_TIM2_Init();

/**
 * @brief Configure what MX_TIM5_Init configures.
 */
void MX_TIM5_Init();

/**
 * @brief Configure what MX_TIM8_Init configures.
 */
void MX_TIM8_Init();

/**
 * @brief Configure what MX_TIM15_Init configures.
 */
void MX_TIM15_Init();

#endif  // MICRAS_LIB_TESTS_HOST_BOARD_TIM_H
