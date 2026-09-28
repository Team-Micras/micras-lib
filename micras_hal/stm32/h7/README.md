# STM32H7 family

Code of the stm32 backend that only the STM32H7 family runs, which CMake compiles when the board
target defines an STM32H7 device (or `MICRAS_LIB_STM32_FAMILY` is `h7`):

- `src/family.cpp`: the functions of `micras/hal/family.hpp`. The ADC calibrates its offset for
  single-ended conversions, `Mcu::init` enables the instruction cache, the watchdog is `IWDG1`, the
  `CPUFREQ_BOOST` option byte of the STM32H72x/73x tells whether the core may run above its default
  maximum, a timer runs at twice its APB clock when the APB prescaler (`RCC_APBx_DIV1`) is not 1, and
  the cycle counter is unlocked through the lock access register of the Cortex-M7 before it starts.
- `src/flash.cpp`: the flash driver, sectors of 128 KB programmed in 256-bit flash words.
- `include/micras/hal/family/flash.hpp`: the flash geometry the shared `flash.hpp` reads. The storage
  region is the upper half of bank 1: sectors 4 to 7 of an STM32H725xG.

The code in `../src` is the same on every family and names no chip.
