#pragma once

#include <sys/cdefs.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys_clock.h>

#include <cstdint>

class Audio {
   private:
    static const struct device* codec_dev;
    static const struct device* i2s_dev;
    static int16_t* current_block;

   public:
    // Double buffered.
    static constexpr unsigned int BLOCK_COUNT = 2;

    static constexpr unsigned int CHANNEL_COUNT      = 2;
    static constexpr unsigned int SAMPLING_FREQUENCY = 44100;

    // Milliseconds of data a block must contain.
    static constexpr unsigned int BLOCK_DURATION_MS = 50;

    static constexpr unsigned int SAMPLES_PER_BLOCK =
        SAMPLING_FREQUENCY * BLOCK_DURATION_MS * CHANNEL_COUNT / 1000;

    // Disallow creating an instance of this class.
    Audio() = delete;

    /// @brief Audio initialization function
    /// Call this function before you call any other function from this library
    /// @return 0 on success, -ERRNO otherwise
    static int init(const struct device* codec_dev, const struct device* i2s_dev);

    /// @brief Set the output volume/amplitude
    /// @param volume volume value, from 0 to 255
    /// @return 0 on success, -ERRNO otherwise
    static int set_volume(uint8_t volume);

    /// @brief Get an audio block to write data to
    /// @param timeout timeout for allocation
    /// @return buffer pointer on success, nullptr otherwise
    static int16_t* get_block(k_timeout_t timeout);

    /// @brief Clear the current block.
    static void clear_block(void);

    /// @brief Write the previously requested block.
    /// @return 0 on success, -ERRNO otherwise
    static int write_block(void);

    /// @brief Trigger start the queued writes.
    /// @return 0 on success, -ERRNO otherwise
    static int start_writes(void);

   private:
    static struct k_mem_slab mem_slab;
    static int16_t __aligned(4) mem_slab_buffer[BLOCK_COUNT][SAMPLES_PER_BLOCK];
};
