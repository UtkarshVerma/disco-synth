#include "USB.hpp"

#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_core.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/util.h>
#include <zephyr/toolchain.h>
#include <zephyr/usb/usb_device.h>

#include <cstdint>

LOG_MODULE_REGISTER(usb, LOG_LEVEL_INF);

static uint8_t format_buffer[1024];
static uint8_t rx_buffer[10];  // NOTE: Improbable to get faster input from a keyboard.
static struct ring_buf rx_ringbuf;
static const uint8_t* tx_buffer;
static size_t tx_remaining_bytes;

const struct device* USB::dev;

static void irq_handler(const struct device* const dev, void* const user_data) {
    if (uart_irq_update(dev) == 0) {
        return;
    }

    if (uart_irq_rx_ready(dev)) {
        uint8_t buffer[64];
        unsigned int received_bytes;
        do {
            // NOTE: This can not error since the same errors have been checked during init.
            received_bytes = uart_fifo_read(dev, buffer, sizeof(buffer));
            LOG_DBG("Received %u bytes", received_bytes);

            const uint32_t buffered_bytes = ring_buf_put(&rx_ringbuf, buffer, received_bytes);
            if (buffered_bytes < received_bytes) {
                LOG_ERR("Dropped %u bytes", received_bytes - buffered_bytes);
            }
        } while (received_bytes == sizeof(buffer));
    }

    if (uart_irq_tx_ready(dev)) {
        // NOTE: This can not error since the same errors have been checked during init.
        const unsigned int bytes_sent = uart_fifo_fill(dev, tx_buffer, tx_remaining_bytes);
        tx_buffer += bytes_sent;
        tx_remaining_bytes -= bytes_sent;

        if (tx_remaining_bytes == 0) {
            LOG_DBG("Everything sent, disable TX IRQ");
            uart_irq_tx_disable(dev);
        }
    }
}

void USB::write(const uint8_t* const buffer, const uint32_t size) {
    tx_buffer          = buffer;
    tx_remaining_bytes = size;

    uart_irq_tx_enable(USB::dev);
}

int USB::init(const struct device* const dev) {
    int ret;

    if (dev == nullptr) {
        LOG_ERR("Device pointer is null");
        return -EINVAL;
    }

    if (!device_is_ready(dev)) {
        LOG_ERR("CDC ACM device not found");
        return -1;
    }

    USB::dev = dev;

    ret = usb_enable(nullptr);
    if (ret < 0) {
        LOG_ERR("Failed to enable USB: %d", -ret);
        return ret;
    }

    LOG_INF("Wait for DTR");
    uint32_t dtr = 0;
    while (dtr == 0) {
        (void)uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);

        // Give CPU resources to low priority threads
        (void)k_msleep(100);
    }
    LOG_INF("DTR set");

    ring_buf_init((struct ring_buf*)&rx_ringbuf, sizeof(rx_buffer), rx_buffer);

    ret = uart_irq_callback_user_data_set(dev, irq_handler, nullptr);
    if (ret < 0) {
        switch (ret) {
            case -ENOTSUP:
                LOG_ERR("Interrupt-driven UART API support not enabled");
                break;
            case -ENOSYS:
                LOG_ERR("UART device does not support Interrupt-driven API");
                break;
            default:
                LOG_ERR("Failed to set UART callback: %d", -ret);
        }

        return ret;
    }

    uart_irq_rx_enable(dev);

    return 0;
}

uint32_t USB::read(char* const data, const uint32_t size) {
    return ring_buf_get((struct ring_buf*)&rx_ringbuf, (uint8_t*)data, size);
}

int USB::print(const char* format, ...) {
    va_list args;
    va_start(args, format);

    const int count = vsnprintf((char*)format_buffer, sizeof(format_buffer), format, args);
    write(format_buffer, count);

    va_end(args);

    return count;
}

int USB::println(const char* format, ...) {
    va_list args;
    va_start(args, format);

    int count = vsnprintf((char*)format_buffer, sizeof(format_buffer), format, args);

    constexpr char newline[]        = "\r\n";
    constexpr size_t newline_length = sizeof(newline) - 1;
    const size_t suffix_length      = MIN(sizeof(format_buffer) - count, newline_length);
    (void)memcpy(&format_buffer[count], newline, suffix_length);
    count += suffix_length;

    write(format_buffer, count);

    va_end(args);

    return count;
}
