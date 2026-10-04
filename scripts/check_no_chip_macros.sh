#!/usr/bin/env bash
# Fails when the code every STM32 family shares tests a chip in the preprocessor.
#
# Usage: scripts/check_no_chip_macros.sh [<root>]
#
# The chip-specific code of the stm32 backend lives in micras_hal/stm32/<family>/, and CMake compiles
# exactly one family. Everything else is shared, so a conditional directive there (#if, #ifdef,
# #ifndef, #elif, #elifdef, #elifndef) that names a chip, a family, a core or a peripheral that only
# some parts have is an error: that code belongs in a family function or a family header. The include
# guards (#ifndef MICRAS_..._HPP) name none of them.

set -euo pipefail

root=${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}

shared_paths=(
    micras_hal/include
    micras_hal/stm32/src
    micras_proxy
    micras_core
    micras_nav
    micras_comm
)

directive='^[[:space:]]*#[[:space:]]*(if|ifdef|ifndef|elif|elifdef|elifndef)\b'
chip_names='\b(STM32\w*|IWDG\w*|FLASH_\w+|RCC_\w+|DBGMCU\w*|DWT\w*|SCB_\w+|ITM_\w+|__CORTEX_M|__ARM_ARCH\w*|__FPU_\w+|__[ID]CACHE_PRESENT|__MPU_PRESENT|CORE_CM\w*|ADC_CALIB\w*|\w+_TypeDef)\b'

cd "$root"

for path in "${shared_paths[@]}"; do
    if [ ! -d "$path" ]; then
        echo "check_no_chip_macros: $path does not exist under $root" >&2
        exit 2
    fi
done

matches=$(grep -rnE --include='*.hpp' --include='*.tpp' --include='*.cpp' --include='*.h' --include='*.c' \
    "$directive.*($chip_names)" "${shared_paths[@]}" || true)

if [ -n "$matches" ]; then
    echo "Chip macros in code every family shares; move them into micras_hal/stm32/<family>/:" >&2
    echo "$matches" >&2
    exit 1
fi

echo "No chip macro in ${shared_paths[*]}"
