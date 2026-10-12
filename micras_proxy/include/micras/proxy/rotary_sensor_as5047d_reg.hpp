/**
 * @file
 */

#ifndef MICRAS_PROXY_ROTARY_SENSOR_AS5047D_REG_HPP
#define MICRAS_PROXY_ROTARY_SENSOR_AS5047D_REG_HPP

#include <cstdint>

/**
 * @brief Registers struct for the configuration of an AS5047D.
 *
 * @note The AS5047D has no DISABLE, SETTINGS3 nor ECC register, and its SETTINGS1 and SETTINGS2
 * are laid out differently from those of the AS5047U: the ABI resolution is ABIRES, in SETTINGS2,
 * together with ABIBIN, in SETTINGS1, which selects between the decimal and the binary resolutions.
 * Writing these registers over SPI changes only their volatile copies, which a reset reloads from
 * the OTP memory, and the OTP memory is never programmed.
 */
struct As5047dRegisters {
    /**
     * @brief Registers union types definition.
     */
    ///@{
    union Zposm {
        struct __attribute__((__packed__)) Fields {
            uint8_t ZPOSM : 8;
        };

        Fields  fields;
        uint8_t raw;
    };

    union Zposl {
        struct __attribute__((__packed__)) Fields {
            uint8_t ZPOSL           : 6;
            uint8_t comp_l_error_en : 1;
            uint8_t comp_h_error_en : 1;
        };

        Fields  fields;
        uint8_t raw;
    };

    /**
     * @note Factory_setting is a read only bit preprogrammed to 1, and na is preprogrammed to 0
     * and must not be overwritten, so they are written as 1 and 0.
     */
    union Settings1 {
        struct __attribute__((__packed__)) Fields {
            uint8_t Factory_setting : 1;
            uint8_t na              : 1;
            uint8_t DIR             : 1;
            uint8_t UVW_ABI         : 1;
            uint8_t DAECDIS         : 1;
            uint8_t ABIBIN          : 1;
            uint8_t Data_select     : 1;
            uint8_t PWMon           : 1;
        };

        Fields  fields;
        uint8_t raw;
    };

    union Settings2 {
        struct __attribute__((__packed__)) Fields {
            uint8_t UVWPP  : 3;
            uint8_t HYS    : 2;
            uint8_t ABIRES : 3;
        };

        Fields  fields;
        uint8_t raw;
    };

    ///@}

    /**
     * @brief Register addresses in the AS5047D memory.
     */
    ///@{
    static constexpr uint16_t errfl_addr{0x0001};
    static constexpr uint16_t zposm_addr{0x0016};
    static constexpr uint16_t zposl_addr{0x0017};
    static constexpr uint16_t settings1_addr{0x0018};
    static constexpr uint16_t settings2_addr{0x0019};
    ///@}

    /**
     * @brief Member variables to be configured in the AS5047D.
     */
    ///@{
    Zposm     zposm;
    Zposl     zposl;
    Settings1 settings1;
    Settings2 settings2;
    ///@}
};

#endif  // MICRAS_PROXY_ROTARY_SENSOR_AS5047D_REG_HPP
