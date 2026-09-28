# STM32H7 family

Code of the stm32 backend that only the STM32H7 family runs, which CMake compiles when the board
target defines an STM32H7 device (or `MICRAS_LIB_STM32_FAMILY` is `h7`): `src/` holds the family's
sources and `include/micras/hal/family/` the facts its shared headers read. The code in `../src` is
the same on every family and names none.
