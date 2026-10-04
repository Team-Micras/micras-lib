/**
 * @file
 */

#include <cstdint>

#include <adc.h>
#include <crc.h>
#include <dma.h>
#include <fmac.h>
#include <gpio.h>
#include <main.h>
#include <spi.h>
#include <tim.h>
#include <usart.h>

#include "micras/hal/host/board.hpp"

namespace {
constexpr uint32_t timer_clock{200000000};

constexpr uint32_t spi3_kernel_clock{125000000};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
TIM_TypeDef tim2_registers{};
TIM_TypeDef tim5_registers{};
TIM_TypeDef tim8_registers{};
TIM_TypeDef tim15_registers{};
SPI_TypeDef spi3_registers{};
}  // namespace

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

static void init_timer(
    TIM_HandleTypeDef& handle, TIM_TypeDef& registers, const char* name, uint32_t prescaler, uint32_t counter_mode,
    uint32_t period
) {
    handle.Instance = &registers;
    handle.Init = {.Prescaler = prescaler, .CounterMode = counter_mode, .Period = period};
    registers = {.CR1 = counter_mode, .CCER = 0, .PSC = prescaler, .ARR = period, .kernel_clock = timer_clock};
    handle.State = HAL_TIM_STATE_READY;
    micras::hal::host::Board::name_handle(&handle, name);
}

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
uint32_t SystemCoreClock{400000000};

GPIO_TypeDef GPIOA_instance{'A'};
GPIO_TypeDef GPIOB_instance{'B'};
GPIO_TypeDef GPIOC_instance{'C'};
GPIO_TypeDef GPIOD_instance{'D'};

ADC_HandleTypeDef  hadc3{};
CRC_HandleTypeDef  hcrc{};
FMAC_HandleTypeDef hfmac{};
SPI_HandleTypeDef  hspi3{};
TIM_HandleTypeDef  htim2{};
TIM_HandleTypeDef  htim5{};
TIM_HandleTypeDef  htim8{};
TIM_HandleTypeDef  htim15{};
UART_HandleTypeDef huart4{};

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

extern "C" {
void SystemClock_Config() { }
}

void MX_GPIO_Init() {
    using micras::hal::host::Board;
    Board::name_gpio(Encoder_A_GPIO_Port, Encoder_A_Pin, "Encoder_A");
    Board::name_gpio(Encoder_B_GPIO_Port, Encoder_B_Pin, "Encoder_B");
    Board::name_gpio(PWM_GPIO_Port, PWM_Pin, "PWM");
    Board::name_gpio(Button_GPIO_Port, Button_Pin, "Button");
    Board::name_gpio(Status_LED_GPIO_Port, Status_LED_Pin, "Status_LED");
    Board::name_gpio(PWM_DMA_GPIO_Port, PWM_DMA_Pin, "PWM_DMA");
    Board::name_gpio(SPI_CSn_GPIO_Port, SPI_CSn_Pin, "SPI_CSn");
}

void MX_DMA_Init() { }

void MX_ADC3_Init() {
    hadc3.Init.NbrOfConversion = 2;
    hadc3.State = HAL_ADC_STATE_READY;
    micras::hal::host::Board::name_handle(&hadc3, "hadc3");
}

void MX_CRC_Init() {
    hcrc.Init = {
        .DefaultPolynomialUse = DEFAULT_POLYNOMIAL_DISABLE,
        .DefaultInitValueUse = DEFAULT_INIT_VALUE_DISABLE,
        .GeneratingPolynomial = 0x1D,
        .CRCLength = CRC_POLYLENGTH_8B,
        .InitValue = 0xC4,
        .InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE,
        .OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE,
    };
    hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;
    micras::hal::host::Board::name_handle(&hcrc, "hcrc");
}

void MX_FMAC_Init() {
    hfmac.State = HAL_FMAC_STATE_READY;
    micras::hal::host::Board::name_handle(&hfmac, "hfmac");
}

void MX_SPI3_Init() {
    spi3_registers.kernel_clock = spi3_kernel_clock;
    hspi3.Instance = &spi3_registers;
    hspi3.Init = {
        .CLKPolarity = SPI_POLARITY_HIGH,
        .CLKPhase = SPI_PHASE_2EDGE,
        .BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32,
    };
    hspi3.State = HAL_SPI_STATE_READY;
    micras::hal::host::Board::name_handle(&hspi3, "hspi3");
}

void MX_TIM2_Init() {
    init_timer(htim2, tim2_registers, "htim2", 0, TIM_COUNTERMODE_UP, 0xFFFFFFFF);
}

void MX_TIM5_Init() {
    init_timer(htim5, tim5_registers, "htim5", 0, TIM_COUNTERMODE_UP, 0xFFFFFFFF);
}

void MX_TIM8_Init() {
    init_timer(htim8, tim8_registers, "htim8", 0, TIM_COUNTERMODE_UP, 249);
}

void MX_TIM15_Init() {
    init_timer(htim15, tim15_registers, "htim15", 199, TIM_COUNTERMODE_UP, 999);
}

void MX_UART4_Init() {
    huart4.Init.BaudRate = 115200;
    huart4.gState = HAL_UART_STATE_READY;
    huart4.RxState = HAL_UART_STATE_READY;
    micras::hal::host::Board::name_handle(&huart4, "huart4");
}
