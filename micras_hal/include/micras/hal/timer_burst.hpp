/**
 * @file
 */

#ifndef MICRAS_HAL_TIMER_BURST_HPP
#define MICRAS_HAL_TIMER_BURST_HPP

#include <cstdint>
#include <span>

#include <main.h>

namespace micras::hal {
/**
 * @brief Class that has a timer load its compare registers from a table at every update event.
 *
 * @details The timer's update DMA request writes one row of the table, a value for each compare
 * register from the first on, into the DMA burst register at every update. The table is read
 * circularly, so the timer cycles through its rows on its own, with no interrupt and nothing for
 * the program to do. The compare registers are preloaded, and an update moves the preloaded values
 * in before the DMA request writes the next ones, so a row written at an update takes effect at the
 * one after: counting the updates since the start from zero, the first row is in force until
 * update 0, the second from update 0 to update 1, and row k of the table from update k + 1 on.
 *
 * @note The DMA stream is the one the peripheral configuration links to the update request of the
 * timer, which has to be memory to peripheral, of words and circular.
 */
class TimerBurst {
public:
    /**
     * @brief Configuration of the burst.
     */
    struct Config {
        void (*init_function)();
        TIM_HandleTypeDef* handle;
    };

    /**
     * @brief Construct a new TimerBurst object.
     *
     * @param config Configuration of the burst.
     */
    explicit TimerBurst(const Config& config);

    /**
     * @brief Stop the timer and get it ready to run the table from its start.
     *
     * @note The counter is stopped and reset, the first row is loaded at once and the second
     * preloaded, and the stream is started, so that the next start of the counter begins the cycle. The table is
     * borrowed, and the stream keeps reading it, so it has to outlive the burst; a change to it takes effect when the
     * stream reaches it.
     *
     * @param first Compare values in force until the first update.
     * @param second Compare values in force from the first update to the second.
     * @param table Rows of compare values, each with one value per register of first.
     * @return True if the stream started.
     */
    bool arm(std::span<const uint32_t> first, std::span<const uint32_t> second, std::span<const uint32_t> table);

    /**
     * @brief Start the counter of an armed timer.
     */
    void start();

    /**
     * @brief Stop the counter and the stream.
     */
    void stop();

    /**
     * @brief Check if the timer and its stream were found.
     *
     * @return True if the burst can run.
     */
    bool was_initialized() const;

private:
    /**
     * @brief Timer handle.
     */
    TIM_HandleTypeDef* handle;

    /**
     * @brief Flag to check if the burst can run.
     */
    bool initialized{};
};
}  // namespace micras::hal

#endif  // MICRAS_HAL_TIMER_BURST_HPP
