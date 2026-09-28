# micras-lib

The libraries of the [Micras](https://github.com/Team-Micras/MicrasFirmware) micromouse, for any
robot that wants them: the core utilities, the navigation, the communication protocol, the hardware
abstraction layer and the proxies of the devices behind it. The HAL has one set of headers and two
backends, selected by CMake: **stm32**, for the robot, built against the files STM32CubeMX generates
for an STM32H7 or an STM32G4, and **host**, which runs the same code on a PC for tests and simulations.

| Library | Target | What it holds | Needs |
|---|---|---|---|
| `micras_core` | `micras::core` | math, filters, PID, COBS, the variable pool, the serializable interface | nothing |
| `micras_nav` | `micras::nav` | maze, planner, turn tables, route compiler, velocity planner, localizer, controller, mission | core |
| `micras_comm` | `micras::comm` | the framing and the session layer of the [communication protocol](micras_comm/README.md) | core |
| `micras_hal` | `micras::hal` | GPIO, PWM, DMA-fed PWM, encoder, ADC over DMA, SPI, UART over DMA, CRC, FMAC, flash, timer, MCU | the board target |
| `micras_proxy` | `micras::proxy` | IMU, rotary sensors, wall and torque sensors, motors, storage, watchdog and the rest | hal, core, ST's `lsm6dsv-pid` |
| `micras_proxy_models` | `micras::proxy_models` | LSM6DSV and AS5047U chip models (host backend only) | hal |

## Using it

A project adds micras-lib as a git submodule, under `external/` by convention, and adds it to its
build after defining its board target:

```bash
git submodule add -b refactor/restructure https://github.com/Team-Micras/micras-lib.git external/micras-lib
```

```cmake
include(cmake/cube.cmake)  # defines stm32cubemx, the board target
add_subdirectory(external/micras-lib EXCLUDE_FROM_ALL)
include(external/micras-lib/cmake/micras_warnings.cmake)

add_executable(robot src/main.cpp)
target_link_libraries(robot PRIVATE micras::nav micras::proxy cube_app STM32_Drivers)
micras_apply_warnings(robot WERROR ${ROBOT_WERROR})
```

- `EXCLUDE_FROM_ALL` builds only the libraries something links. What is fetched at configure time is
  chosen by the options below: a project that needs only the navigation sets `MICRAS_LIB_HAL=OFF`,
  links `micras::nav`, and builds core and nav with nothing downloaded.
- Adding micras-lib a second time, as a project that reaches it through two submodules does, is a
  no-op: the first add wins.
- As a subproject micras-lib sets no language standard and no flag: the consumer's reach its
  libraries by inheritance, and each library requires C++23 without extensions on its own.
- `cmake/micras_warnings.cmake` is the warning set of every Micras repository.
  `micras_apply_warnings(<target>... [WERROR <bool>])` applies it to the consumer's own targets; the
  libraries get it from `MICRAS_LIB_WARNINGS`.

| Option | Default | Effect |
|---|---|---|
| `MICRAS_LIB_NAV`, `MICRAS_LIB_COMM` | `ON` | Add the library; core is always added |
| `MICRAS_LIB_HAL` | `ON` | Add `micras_hal` |
| `MICRAS_LIB_PROXY` | `ON` | Add `micras_proxy`, which fetches `lsm6dsv-pid`; forced off without the HAL |
| `MICRAS_LIB_HAL_BACKEND` | `stm32` | `stm32` or `host` |
| `MICRAS_LIB_STM32_FAMILY` | empty | `h7` or `g4`; read from the board target's device macro when empty |
| `MICRAS_LIB_CUBE_TARGET` | `stm32cubemx` | The board target the HAL links, on every backend |
| `MICRAS_LIB_WARNINGS` | `ON` | Apply the project warnings to every library target |
| `MICRAS_LIB_WERROR` | `OFF` | Treat them as errors |
| `MICRAS_LIB_TESTS`, `MICRAS_LIB_DOCS` | on when top level | The tests (host backend, fetches doctest) and the `docs` target |

Third-party code is fetched with FetchContent and pinned by commit: `lsm6dsv-pid` (v5.1.1) by
`micras_proxy`, doctest (v2.5.3) by the tests and doxygen-awesome-css (v2.5.0) by the docs.

## The stm32 backend

A consumer of the stm32 backend:

1. Defines the board target (`stm32cubemx` by default) **before** adding micras-lib. It carries
   `main.h`, the HAL and CMSIS include directories and the device macro that selects the family
   (`STM32H725xx` or `STM32G474xx`, for instance). The vendor driver and application objects (`cube_app` and
   `STM32_Drivers` in the reference project) are linked into every executable: they hold the vector
   table, the interrupt handlers and the `MX_*_Init` functions.
2. Enables in `stm32xx_hal_conf.h` every module the headers name: ADC, CRC, FMAC, SPI, TIM, UART,
   FLASH, DMA, RCC and GPIO.
3. Keeps `USE_HAL_ADC_REGISTER_CALLBACKS` and `USE_HAL_SPI_REGISTER_CALLBACKS` at 0. micras-lib
   defines `HAL_ADC_ConvCpltCallback`, `HAL_ADC_ErrorCallback`, `HAL_SPI_TxRxCpltCallback` and
   `HAL_SPI_ErrorCallback`, and the consumer must not. They win over the vendor's weak defaults because
   each lives in the source of a constructor the program references (`AdcDma`, `Spi`).
4. Places every object that holds a DMA buffer in memory the DMA reaches, with the data cache off or
   the buffers non-cacheable.
5. Configures the CRC unit for the AS5047U (polynomial 0x1D, 8 bits, initial value 0xC4) when it uses
   `RotarySensor`.
6. Keeps code out of the flash `Storage` uses, which the family names (see the table below): the upper
   half of bank 1 on the STM32H7, bank 2 on the STM32G4. A sector of `Storage::Config` is the unit
   the family erases, so a G4 storage spans as many 2 KB pages as its pool needs.

### Families

| | `h7` | `g4` |
|---|---|---|
| Parts | STM32H7, the STM32H725 of Micras v1 and the upcoming projects | STM32G4, the STM32G474RET6 of the old board, Micras v0 |
| Selected by | `STM32H7` among the board target's definitions | `STM32G4` among the board target's definitions |
| Flash | 128 KB sectors programmed in 256-bit flash words | 2 KB pages programmed in 64-bit double words |
| `Storage` region | the upper half of bank 1: sectors 4 to 7, from 0x08080000, on a 1 MB STM32H725xG | bank 2 of a 512 KB part in its factory dual bank mode: 128 pages from 0x08040000; on any other layout (single bank, a 128 or 256 KB part) every flash operation fails |
| Erasing | around 2 s per sector, up to 4 s | around 20 ms per page |
| Caches | `Mcu::init` enables the instruction cache | nothing to enable, `HAL_Init` sets up the ART accelerator |
| Watchdog | `IWDG1`, frozen under the debugger | `IWDG`, frozen under the debugger |
| `cpu_frequency_boost` | reads the `CPUFREQ_BOOST` option byte of the STM32H72x/73x | always supported, the family has no such option byte |
| ADC calibration | offset, single ended | single ended |
| Timer clock | twice the APB clock when its prescaler (`RCC_APBx_DIV1`) is not 1 | twice the APB clock when its prescaler (`RCC_HCLK_DIV1`) is not 1 |
| Cycle counter | `DWT` unlocked through its lock access register, then started | `DWT` started, the Cortex-M4 has no lock |

The chip-independent code of the backend is in `micras_hal/stm32/src/`, and it names no chip. What
differs between families is in `micras_hal/stm32/<family>/`, which CMake compiles for the selected
family alone: the flash driver (`src/flash.cpp`), the flash geometry the shared `flash.hpp` reads
(`include/micras/hal/family/flash.hpp`: `flash_word_bits`, `sector_size`, `storage_first_sector`,
`storage_sectors`), and the steps of the shared classes that differ, the functions of the internal
header `micras/hal/family.hpp` (`calibrate_adc`, `enable_caches`, `was_reset_by_watchdog`,
`freeze_watchdog_in_debug`, `is_cpu_frequency_supported`, `watchdog`, `timer_clock`,
`enable_cycle_counter`) in `src/family.cpp`. `scripts/check_no_chip_macros.sh` fails when a
conditional directive in the shared code (`micras_hal/include`, `micras_hal/stm32/src`, the proxies,
core, nav and comm) names a chip, and the CI runs it.

### Reference projects and benches

`reference/h7/` and `reference/g4/` are the smallest STM32CubeMX projects that exercise everything the
HAL and the proxies use, and nothing of Micras: an ADC scanning the internal reference and the battery
channel into a circular DMA buffer, the CRC unit set up for the AS5047U, the FMAC, a full duplex SPI
master with DMA and a GPIO chip select, a PWM channel, a DMA-fed PWM channel, an encoder timer, a
free-running timer, a UART with DMA both ways, the independent watchdog, one input and one output pin,
and the internal oscillator through the PLL. CubeMX generates no `main()` and no callback
registration. Each `CMakeLists.txt` glues the generated tree in as a consumer does
(`reference/common/toolchain.cmake` and `reference/common/reference.cmake`), adds micras-lib with
`MICRAS_LIB_WERROR=ON`, and builds and links `micras_lib_bench_<family>`, which proves the link-time
contract above.

| | `reference/h7` | `reference/g4` |
|---|---|---|
| Part | STM32H725RGVx | STM32G474RETx |
| Core clock | 400 MHz: HSI 64 MHz / 4 x 50 / 2, VOS1 | 170 MHz: HSI 16 MHz / 4 x 85 / 2, range 1 boost |
| UART | UART4 | USART3 |
| Supply | the direct SMPS supply of Micras v1 | the internal regulator |

The Cube files are generated, never committed:

```bash
docker build --file .docker/Dockerfile --target generated-g4 --output type=local,dest=reference/g4 .
cmake --preset reference-g4
cmake --build --preset reference-g4
```

and the same with `h7`. The bench program is `reference/common/bench.cpp`; each family's `bench.cpp`
names the UART and converts the internal reference with its family's low-layer ADC header. It drives
no pin, so it can be flashed on any board with the part (for the H7, one whose supply is the direct
SMPS one the project configures, as on Micras v1; for the G4, the old Micras board). It writes each
result into a `volatile` global for the debugger or STM32CubeMonitor:

| Global | Check |
|---|---|
| `storage_passed` | a `Storage` saved before a watchdog reset is restored after it |
| `watchdog_passed` | a `Watchdog` left alone resets the chip, and the reset cause reads back |
| `timer_passed`, `timer_clock_error` | the free-running timer against the cycle counter, for the timer clock the family reports |
| `crc_passed`, `crc_value` | the CRC unit against the same CRC computed in software |
| `fmac_passed`, `fmac_error` | `FmacFilter` against the same filter computed in software |
| `adc_passed`, `vdda_mv` | the internal reference converted over DMA, against its factory calibration |
| `bench_done` | every check ran |

The pin-driving classes (`Gpio`, `Pwm`, `PwmDma`, `Encoder`, `Spi`, `UartDma`) are compiled and linked,
but started only when the debugger sets `start_pin_drivers` before the checks end.

## The host backend

A consumer of the host backend:

1. Defines its board target, a static library standing in for the Cube layer, before adding
   micras-lib, and sets `MICRAS_LIB_CUBE_TARGET` to it. It provides `main.h` (which includes
   `stm32_host.h`), the per-peripheral headers its configuration includes, `SystemCoreClock`, the
   flash geometry macros the host's `micras/hal/family/flash.hpp` reads
   (`FLASH_NB_32BITWORD_IN_FLASHWORD`, `FLASH_SECTOR_SIZE`, `FLASH_SECTOR_TOTAL`, with the values of
   the chip it stands for), and the handles and `MX_*_Init` functions that fill the registers the host
   `Pwm`, `Timer` and `Spi` read. It links `micras::hal`, and `micras_hal` links it back.
2. Keeps exceptions on if a thread of its own runs the program the HAL serves.
3. Resets `micras::hal::host::Board` and `micras::hal::host::Clock` between independent runs in one
   process.

`tests/support/host_board/` is the board of micras-lib's own tests: the peripherals of the reference
project under the same handle and pin names, with the values its CubeMX project generates.

### Design

The host backend implements every `micras_hal` class on a PC, private and static data members
included, and includes nothing but the HAL's headers and its own. It also implements the family
functions of `micras/hal/family.hpp` (`host/src/family.cpp`: nothing to calibrate, cache or unlock, a
timer's clock is its kernel clock). A change to a shared header lands with every implementation in one
commit.

- **Ports.** `Board` is a registry of ports (`GpioPort`, `PwmPort`, `PwmDmaPort`, `AdcPort`,
  `UartPort`, `EncoderPort`, `SpiPort`, `FmacPort`, `FlashPort`, `McuPort`) **keyed by the address of
  the Cube handle or GPIO port** that names them, so the consumer's own configuration is the key:
  `.handle = &htim15` there and `Board::pwm(&htim15, TIM_CHANNEL_1)` in a binding reach the same port.
  Nothing in the backend knows a physics engine; a consumer's bindings connect ports to whatever
  simulates the board. A port the program touched that nothing is bound to is listed by
  `Board::unbound()`, and `McuPort` counts watchdog expiries and emergency stops instead of resetting a
  process.
- **Time.** `Clock` is the only place time exists. **Every read of the timer costs one microsecond
  of host time**, since a read on the robot takes time too and a loop that polls the timer must see it
  advance. When a read crosses the end of a step, the clock calls its handover: whatever drives the
  world advances it by one step and the read returns after it. A busy wait therefore runs the world
  exactly as long as it waits, and the time inside one iteration of a loop is the number of timer reads
  times the quantum: deterministic, not a measurement of anything.
- **Registers.** `stm32_host.h` holds the handle types and HAL constants, with only the fields the
  backend reads or writes. The host `Pwm` and `Timer` compute frequencies and duty cycles from the
  prescaler, autoreload and counter mode the board's init functions write, with the timer's kernel
  clock in its `Instance`.
- **FMAC.** `Fmac` keeps the configured IIR filter in the accelerator's port and computes it in q1.15,
  with exact products and the output clipped to the q1.15 range.
- **Flash.** `FlashPort` holds the reserved region as bytes, erased to 0xFF and written once per
  erase, and its `write_budget` cuts writes off after a number of flash words, as a power loss would.

### The SPI device slot

A chip on an SPI bus is a `SpiDevice` (`micras/hal/host/spi_device.hpp`): `select()`,
`exchange(tx, rx)`, `deselect()` and the SPI mode it answers in. A binding attaches it with
`Board::spi_device(handle, cs_port, cs_pin, device)`, **keyed by the bus and the chip select**, since
one bus carries several chips. The host `Spi` selects the device in `select_device`, routes every
`transmit`, `receive` and `transmit_receive` to it, and deselects it in `unselect_device`, so a register
read the driver makes of two HAL calls is one transaction for the chip, as on the bus.

- **The mode is checked on every transfer.** A device whose mode differs from the one `select_device`
  wrote into the handle is not reached and the program reads all ones, as from a chip clocked on the
  wrong edge. So is a chip select with no device, which is also an unbound port.
- **`start_transfer` completes on the host clock.** The bytes are exchanged at the start, and the
  transfer ends when the host clock passes the time its bytes take at the bus's bit rate: the kernel
  clock the board writes into the handle's `Instance`, divided by the baud rate prescaler.
  `get_transfer`, and a `select_device` that finds the bus busy, end it through `on_transfer_end`,
  which raises the chip select and sets `COMPLETE`, as the DMA interrupt does.
- **Blocking transfers take no time.** Only timer reads cost time on the host.
- **Devices outlive the drivers that talk to them.** A driver ends its last transfer when it is
  destroyed; forgetting the ports with `Board::reset()` detaches the devices first.

### The chip models

`micras_proxy/models/` builds `micras_proxy_models` (namespace `micras::models`): SPI devices that
know the slot and nothing else, so the real `Imu` and `RotarySensor` proxies run on the host.

- **`Lsm6dsvModel`**: the 128 registers of the main page with the datasheet defaults (WHO_AM_I 0x70),
  `SW_POR` and `SW_RESET`, auto-increment under `IF_INC`, SPI mode 3. `push_sample(angular_velocity,
  linear_acceleration)`, in rad/s and m/s^2 at the configured rate, encodes a sample with the full
  scale in CTRL6/CTRL8, the one the driver wrote, and sets `GDA`/`XLDA`; reading a sensor's output
  high byte clears its bit. A sensor whose output data rate is off ignores samples.
- **`As5047uModel`**: 24-bit frames with the CRC-8 (polynomial 0x1D, initial value 0xC4, final XOR
  0xFF) checked on every frame, answers pipelined by one frame, the volatile registers (DISABLE,
  ZPOSM, ZPOSL, SETTINGS1 to 3, ECC), ERRFL with the CRC and framing error bits, SPI mode 1. The
  position is not modeled: it reaches the program through the timer encoder, as on the robot.

## Developing

| Preset | What |
|---|---|
| `host` | Debug, the host backend and the tests under the address and undefined behavior sanitizers |
| `host-release` | Release, the host backend and the tests |
| `reference-h7` | The reference STM32H725 project and its bench, with the ARM toolchain |
| `reference-g4` | The reference STM32G474 project and its bench, with the ARM toolchain |

```bash
cmake --preset host
cmake --build --preset host
ctest --preset host
cmake --build --preset host --target format_check lint docs
```

The host presets use GCC 15 (`gcc-15`, `g++-15`) and Ninja. The tests are doctest suites, one
executable per folder of `tests/` (`core`, `comm`, `hal`, `proxy`, `models`, `nav`), each case
registered in CTest. The nav suite plans on the ten contest mazes of `tests/data/mazes/` with golden
route times and step counts, which an intended change of the planner regenerates.

`format`, `format_check`, `lint` and `lint_fix` use clang 22, found as `clang-format-22`,
`clang-tidy-22`, `run-clang-tidy-22` and `clang-apply-replacements-22`; configuring the top level
fails without them. `lint` covers the host build; `cmake --build --preset reference-<family> --target
lint` covers the stm32 backend of that family and the bench against its ARM headers.
`scripts/check_no_chip_macros.sh` checks the shared code for chip macros. `docs` builds the Doxygen pages into
`build/<preset>/docs/`.

`.docker/Dockerfile` has the image every CI job runs in (the `host` stage, shared byte for byte with
micras-simulation and MicrasFirmware), the CubeMX generation of the reference projects
(`generated-h7`, `generated-g4`), and the ARM toolchain on top of the host stage (`stm32`).
`.github/workflows/ci.yaml` builds, tests, checks the format, lints, builds the docs and checks the
shared code for chip macros on the host, and builds and lints each reference and its bench.

The code style is the one of every Micras repository: `.clang-format`, `.clang-tidy` (with
`tests/.clang-tidy`), Doxygen on every declaration of a header, no comments in sources, American
English, and gitmoji commit messages with one imperative sentence.
