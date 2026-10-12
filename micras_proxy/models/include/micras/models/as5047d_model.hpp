/**
 * @file
 *
 * @brief The AS5047D magnetic position sensor, as its SPI interface shows it.
 */

#ifndef MICRAS_MODELS_AS5047D_MODEL_HPP
#define MICRAS_MODELS_AS5047D_MODEL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "micras/hal/host/spi_device.hpp"

namespace micras::models {
/**
 * @brief The registers of an AS5047D behind an SPI chip select.
 *
 * @details Every frame is two bytes. A command frame carries an even parity
 *          bit, the read flag and a 14-bit address; a data frame carries an
 *          even parity bit, the error flag and 14 bits of data. The parity bit
 *          makes the number of ones in the 16 bits even. Answers are pipelined
 *          by one frame: what the sensor sends during a frame answers the frame
 *          before it.
 *
 * @details A read command is answered with the register's content. A write
 *          command is answered with the register's old content and is followed
 *          by a data frame holding the new one, which is answered with the new
 *          content.
 *
 * @note A frame with a wrong parity is ignored, and a write whose data frame it
 *       was is dropped; it sets the parity error bit of ERRFL. A command to an
 *       address the chip does not have sets the invalid command bit. A chip
 *       select raised in the middle of a frame drops it and sets the framing
 *       error bit. The error flag of an answer is set when the frame it answers
 *       had an error, and reading ERRFL clears it.
 *
 * @note ZPOSM, ZPOSL, SETTINGS1 and SETTINGS2 are held as volatile registers,
 *       powering on as the datasheet's defaults: bit 0 of SETTINGS1 is a read
 *       only factory bit set to 1. The angle, the magnitude, the diagnostics and
 *       the OTP programming are not modeled and read zero.
 */
class As5047dModel : public hal::host::SpiDevice {
public:
    /**
     * @brief Bytes in a frame.
     */
    static constexpr std::size_t frame_size{2};

    /**
     * @brief Addresses of the registers the chip has.
     */
    ///@{
    static constexpr uint16_t nop_address{0x0000};
    static constexpr uint16_t errfl_address{0x0001};
    static constexpr uint16_t prog_address{0x0003};
    static constexpr uint16_t zposm_address{0x0016};
    static constexpr uint16_t zposl_address{0x0017};
    static constexpr uint16_t settings1_address{0x0018};
    static constexpr uint16_t settings2_address{0x0019};
    static constexpr uint16_t diaagc_address{0x3FFC};
    static constexpr uint16_t mag_address{0x3FFD};
    static constexpr uint16_t angleunc_address{0x3FFE};
    static constexpr uint16_t anglecom_address{0x3FFF};
    ///@}

    /**
     * @brief Bits of ERRFL.
     */
    ///@{
    static constexpr uint16_t framing_error{1U << 0U};
    static constexpr uint16_t invalid_command{1U << 1U};
    static constexpr uint16_t parity_error{1U << 2U};
    ///@}

    /**
     * @brief Power the sensor on, in SPI mode 1.
     */
    As5047dModel();

    /**
     * @brief Start a transaction at the start of a frame.
     */
    void select() override;

    /**
     * @brief Shift the pending answer out and the frame in, acting on every frame completed.
     *
     * @param transmitted Bytes the controller sends.
     * @param received Bytes of the answer to the previous frame.
     */
    void exchange(std::span<const uint8_t> transmitted, std::span<uint8_t> received) override;

    /**
     * @brief End the transaction, dropping an incomplete frame.
     */
    void deselect() override;

    /**
     * @brief Get a register without the side effects of reading it over SPI.
     *
     * @param address Register address.
     * @return The register's content, zero for one not modeled.
     */
    uint16_t peek(uint16_t address) const;

    /**
     * @brief Set the parity bit of a frame.
     *
     * @param frame The frame, its parity bit ignored.
     * @return The frame with an even number of ones.
     */
    static uint16_t with_parity(uint16_t frame);

private:
    /**
     * @brief Act on one complete frame and prepare the answer to it.
     *
     * @param frame The frame, most significant byte first.
     */
    void process(const std::array<uint8_t, frame_size>& frame);

    /**
     * @brief Build a data frame answering with a value.
     *
     * @param data The value.
     * @param error Whether the frame answered had an error.
     * @return The frame.
     */
    static uint16_t answer_with(uint16_t data, bool error);

    /**
     * @brief Check if the chip has a register at an address.
     *
     * @param address Register address.
     * @return True if the address is valid.
     */
    static bool valid_address(uint16_t address);

    /**
     * @brief Get the register a write to an address changes.
     *
     * @param address Register address.
     * @return The register, or null when the address is not writable.
     */
    uint8_t* writable(uint16_t address);

    /**
     * @brief Volatile copies of the non-volatile registers.
     */
    ///@{
    uint8_t zposm{0};
    uint8_t zposl{0};
    uint8_t settings1{0x01};
    uint8_t settings2{0};
    ///@}

    /**
     * @brief Error flags, in ERRFL's layout.
     */
    uint16_t errfl{0};

    /**
     * @brief Frame shifted out next: the answer to the last complete frame.
     */
    uint16_t answer;

    /**
     * @brief Bytes of the frame being received.
     */
    std::array<uint8_t, frame_size> frame{};

    /**
     * @brief Number of bytes of the frame received so far.
     */
    std::size_t received_bytes{0};

    /**
     * @brief Address whose write command was received, so the next frame holds its data.
     */
    std::optional<uint16_t> pending_write;
};
}  // namespace micras::models

#endif  // MICRAS_MODELS_AS5047D_MODEL_HPP
