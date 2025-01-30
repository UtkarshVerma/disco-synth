#pragma once

#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/sys/ring_buffer.h>

#include <cstdint>

class USB {
   private:
    static const struct device* dev;

    static void write(const uint8_t* const buffer, const uint32_t size);

   public:
    // Disallow creating an instance of this class.
    USB() = delete;

    /// @brief USB initialization function.
    /// Call this function before printing to the serial port
    /// @return 0 on success, -ERRNO otherwise
    static int init(const struct device* dev);

    /// @brief Read data from the USB port
    /// @param data data pointer
    /// @param size size of the data pointer
    /// @return number of bytes read
    static uint32_t read(char* data, uint32_t size);

    /// @brief basic print function
    /// Does not support floating point, does not print a new line
    /// @param format C standard string format
    /// @param variables to parse into the string
    /// @return Number of bytes written
    static int print(const char* const format, ...);

    /// @brief basic println function
    /// Does not support floating point, does print a new line
    /// @param format C standard string format
    /// @param variables to parse into the string
    /// @return Number of bytes written
    static int println(const char* const format, ...);
};
