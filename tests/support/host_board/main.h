/**
 * @file
 *
 * @brief Fake STM32CubeMX main.h of the reference projects, for the host tests.
 *
 * @note The host counterpart of reference/h7: the same peripherals under the
 *       same handle and pin names, one instance each, with the values its
 *       CubeMX project generates. The independent watchdog has no handle here,
 *       since the HAL drives its registers directly. The configurations the
 *       tests build from them compile unchanged against the generated tree of
 *       the reference.
 */

#ifndef MICRAS_LIB_TESTS_HOST_BOARD_MAIN_H
#define MICRAS_LIB_TESTS_HOST_BOARD_MAIN_H

#include <cstdint>

#include "stm32_host.h"

/*****************************************
 * Microcontroller
 *****************************************/

/**
 * @brief Core clock after SystemClock_Config: HSI 64 MHz / PLLM 4 * PLLN 50 / PLLP 2.
 */
extern uint32_t SystemCoreClock;

/**
 * @brief Flash geometry of the STM32H725xG: one bank of eight 128 KB sectors, 256-bit flash words.
 */
///@{
#define FLASH_NB_32BITWORD_IN_FLASHWORD 8U
inline constexpr uint32_t FLASH_SECTOR_SIZE{0x00020000U};
inline constexpr uint32_t FLASH_SECTOR_TOTAL{8U};
///@}

/*****************************************
 * GPIO ports and pins
 *****************************************/

/**
 * @brief The GPIO ports of the reference, one instance each.
 */
///@{
extern GPIO_TypeDef GPIOA_instance;
extern GPIO_TypeDef GPIOB_instance;
extern GPIO_TypeDef GPIOC_instance;
extern GPIO_TypeDef GPIOD_instance;
///@}

/**
 * @brief The CMSIS names of the GPIO ports, the addresses of their instances.
 */
///@{
#define GPIOA (&GPIOA_instance)
#define GPIOB (&GPIOB_instance)
#define GPIOC (&GPIOC_instance)
#define GPIOD (&GPIOD_instance)
///@}

/**
 * @brief The pin labels of the reference's CubeMX project: each pin and its port.
 */
///@{
#define Encoder_A_Pin GPIO_PIN_0
#define Encoder_A_GPIO_Port GPIOA
#define Encoder_B_Pin GPIO_PIN_1
#define Encoder_B_GPIO_Port GPIOA
#define PWM_Pin GPIO_PIN_2
#define PWM_GPIO_Port GPIOA
#define Button_Pin GPIO_PIN_7
#define Button_GPIO_Port GPIOA
#define Status_LED_Pin GPIO_PIN_12
#define Status_LED_GPIO_Port GPIOB
#define PWM_DMA_Pin GPIO_PIN_6
#define PWM_DMA_GPIO_Port GPIOC
#define SPI_CSn_Pin GPIO_PIN_2
#define SPI_CSn_GPIO_Port GPIOD
///@}

#endif  // MICRAS_LIB_TESTS_HOST_BOARD_MAIN_H
