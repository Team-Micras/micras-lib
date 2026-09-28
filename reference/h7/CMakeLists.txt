cmake_minimum_required(VERSION 3.28...4.4)

###############################################################################
## Reference STM32H725 project
###############################################################################

# The smallest CubeMX project that exercises every peripheral micras-lib uses, built and linked with
# its bench program the way a consumer of the stm32 backend builds its firmware. What every family's
# reference shares is in ../common.

set(MICRAS_REFERENCE_FAMILY h7)
include(${CMAKE_CURRENT_LIST_DIR}/../common/toolchain.cmake)

project(micras_lib_reference_h7 C CXX ASM)

include(${CMAKE_CURRENT_LIST_DIR}/../common/reference.cmake)
