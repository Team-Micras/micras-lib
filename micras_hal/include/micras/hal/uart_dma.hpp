/**
 * @file
 */

#ifndef MICRAS_HAL_UART_DMA_HPP
#define MICRAS_HAL_UART_DMA_HPP

#include <cstddef>
#include <cstdint>
#include <main.h>
#include <span>

namespace micras::hal {
/**
 * @brief Class to handle a UART peripheral on STM32 microcontrollers using DMA.
 *
 * @note Reception is a circular DMA that only an error stops, and is polled by comparing the transfer
 * counter against what was already taken. Nothing is done in an interrupt, there is no window
 * between stopping and restarting in which bytes are lost, and the latency is one control loop
 * iteration, which is far below anything the radio adds. A transfer error disables the receive
 * DMA stream, which read finds whether or not the stream's interrupt is enabled, so that interrupt
 * is not needed.
 *
 * Transmission is polled the same way, and needs no interrupt either. The HAL only returns the
 * peripheral to ready from the transmission complete interrupt of the UART, which is left disabled
 * so that a reception error never aborts the circular transfer, and its DMA to ready from the
 * interrupt of the transmit stream, which may be disabled too. A transfer is over once its DMA
 * moved the last byte, which the stream or channel tells, and the next one closes what the HAL left
 * open: aborting the transmission returns the DMA to ready and unlocks it, which starting a
 * transfer needs, and leaves the byte the peripheral is still sending alone. Aborting also empties
 * the transmit FIFO of the peripheral when it is enabled, so a transfer is only over once that
 * FIFO is empty.
 */
class UartDma {
public:
    /**
     * @brief Configuration struct for the UART.
     */
    struct Config {
        void (*init_function)();
        UART_HandleTypeDef* handle;
    };

    /**
     * @brief Construct a new UartDma object.
     *
     * @param config UART configuration struct.
     */
    explicit UartDma(const Config& config);

    /**
     * @brief Special member functions deleted.
     *
     * @note The object owns a transfer in flight that points into its own buffers, so it cannot be
     * copied or moved.
     */
    ///@{
    UartDma(const UartDma&) = delete;
    UartDma(UartDma&&) = delete;
    UartDma& operator=(const UartDma&) = delete;
    UartDma& operator=(UartDma&&) = delete;
    ~UartDma() = default;
    ///@}

    /**
     * @brief Start receiving into a circular buffer.
     *
     * @note The buffer is borrowed and has to outlive the reception. It should live in a memory
     * the DMA controller can reach without going through the bus matrix of another domain.
     *
     * @param buffer Buffer the DMA writes into, wrapping around.
     * @return True if the reception was started, false otherwise.
     */
    bool start_rx(std::span<uint8_t> buffer);

    /**
     * @brief Take the bytes that arrived since the last call.
     *
     * @note Bytes are lost silently if this is called more rarely than the buffer takes to fill,
     * which the framing recovers from by resynchronizing on the next delimiter. A reception that the
     * vendor HAL ended or whose DMA stream was disabled by a transfer error is aborted and started
     * again from the beginning of the buffer, and the bytes it held are dropped.
     *
     * @param into Buffer to copy the received bytes into.
     * @return Number of bytes copied.
     */
    std::size_t read(std::span<uint8_t> into);

    /**
     * @brief Start sending a buffer.
     *
     * @note The buffer is borrowed and has to stay valid until the transfer completes.
     *
     * @param from Data to send.
     * @return True if the transfer was started, false if one is still running or it failed.
     */
    bool start_tx(std::span<const uint8_t> from);

    /**
     * @brief Check if a transfer is still running.
     *
     * @note The last byte may still be leaving the shift register, which does not stop the next
     * transfer from starting.
     *
     * @return True if the DMA is still feeding the peripheral or its transmit FIFO holds bytes,
     * false otherwise.
     */
    bool is_transmitting() const;

    /**
     * @brief Check if the peripheral was initialized and is receiving.
     *
     * @return True if the initialization was successful, false otherwise.
     */
    bool was_initialized() const;

private:
    /**
     * @brief Get how many bytes the DMA has written into the buffer so far.
     *
     * @return Index the DMA will write the next byte at.
     */
    std::size_t rx_head() const;

    /**
     * @brief UART handle.
     */
    UART_HandleTypeDef* handle;

    /**
     * @brief Destination buffer of the reception, written around by the DMA.
     */
    std::span<uint8_t> rx_buffer;

    /**
     * @brief Index of the first byte that has not been taken yet.
     */
    std::size_t rx_tail{};

    /**
     * @brief Whether the reception is running.
     */
    bool initialized{};
};
}  // namespace micras::hal

#endif  // MICRAS_HAL_UART_DMA_HPP
