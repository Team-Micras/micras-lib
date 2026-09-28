/**
 * @file
 *
 * @brief Fake STM32CubeMX spi.h of the reference projects: its handles and init functions.
 */

#ifndef MICRAS_LIB_TESTS_HOST_BOARD_SPI_H
#define MICRAS_LIB_TESTS_HOST_BOARD_SPI_H

#include "main.h"

/**
 * @brief Handle hspi3, a full duplex master in SPI mode 3.
 */
extern SPI_HandleTypeDef hspi3;

/**
 * @brief Configure what MX_SPI3_Init configures.
 */
void MX_SPI3_Init();

#endif  // MICRAS_LIB_TESTS_HOST_BOARD_SPI_H
