/**
 * @file
 *
 * @brief Fake STM32CubeMX crc.h of the reference projects: its handles and init functions.
 */

#ifndef MICRAS_LIB_TESTS_HOST_BOARD_CRC_H
#define MICRAS_LIB_TESTS_HOST_BOARD_CRC_H

#include "main.h"

/**
 * @brief Handle hcrc, set up for the AS5047U frames: 8 bits, polynomial 0x1D, initial value 0xC4.
 */
extern CRC_HandleTypeDef hcrc;

/**
 * @brief Configure what MX_CRC_Init configures.
 */
void MX_CRC_Init();

#endif  // MICRAS_LIB_TESTS_HOST_BOARD_CRC_H
