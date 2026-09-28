/**
 * @file
 *
 * @brief Fake STM32CubeMX fmac.h of the reference projects: its handles and init functions.
 */

#ifndef MICRAS_LIB_TESTS_HOST_BOARD_FMAC_H
#define MICRAS_LIB_TESTS_HOST_BOARD_FMAC_H

#include "main.h"

/**
 * @brief Handle hfmac.
 */
extern FMAC_HandleTypeDef hfmac;

/**
 * @brief Configure what MX_FMAC_Init configures.
 */
void MX_FMAC_Init();

#endif  // MICRAS_LIB_TESTS_HOST_BOARD_FMAC_H
