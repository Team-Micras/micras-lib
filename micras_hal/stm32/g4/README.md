# STM32G4 family

Code of the stm32 backend that only the STM32G4 family runs, which CMake compiles when the board
target defines an STM32G4 device (or `MICRAS_LIB_STM32_FAMILY` is `g4`):

- `src/family.cpp`: the functions of `micras/hal/family.hpp`. The ADC calibrates for single-ended
  conversions, there is no cache to enable (`HAL_Init` sets up the ART accelerator), the watchdog is
  `IWDG`, every core frequency the clock tree reaches is supported (the family has no boost option
  byte), a timer runs at twice its APB clock when the APB prescaler (`RCC_HCLK_DIV1`) is not 1, and
  the cycle counter starts without an unlock, since the Cortex-M4 has no lock access register.
- `src/flash.cpp`: the flash driver, pages of 2 KB programmed in 64-bit double words. A sector of the
  shared `Flash` class is a page.
- `include/micras/hal/family/flash.hpp`: the flash geometry the shared `flash.hpp` reads. The storage
  region is bank 2 of a 512 KB part, such as the STM32G474RET6 of the old Micras board, in the dual
  bank mode it leaves the factory in: 128 pages from 0x08040000. Every flash operation checks the
  `DBANK` option bit and the size of the part, and fails on any other layout rather than erase what
  may be code.

The code in `../src` is the same on every family and names no chip.
