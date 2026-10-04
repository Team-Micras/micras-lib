/**
 * @file
 *
 * @brief Fake STM32CubeMX usart.h of the reference projects: its handles and init functions.
 */

#ifndef MICRAS_LIB_TESTS_HOST_BOARD_USART_H
#define MICRAS_LIB_TESTS_HOST_BOARD_USART_H

#include "main.h"

/**
 * @brief Handle huart4, with DMA on receive and transmit.
 */
extern UART_HandleTypeDef huart4;

/**
 * @brief Configure what MX_UART4_Init configures.
 */
void MX_UART4_Init();

#endif  // MICRAS_LIB_TESTS_HOST_BOARD_USART_H
