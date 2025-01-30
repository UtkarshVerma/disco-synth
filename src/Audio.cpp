#include "Audio.hpp"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stm32_ll_spi.h>
#include <string.h>
#include <sys/cdefs.h>
#include <zephyr/audio/codec.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_core.h>
#include <zephyr/sys/time_units.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys_clock.h>

#include <cstdint>

LOG_MODULE_REGISTER(audio, LOG_LEVEL_INF);

int16_t *Audio::current_block;
const struct device *Audio::codec_dev;
const struct device *Audio::i2s_dev;
struct k_mem_slab Audio::mem_slab;
int16_t Audio::mem_slab_buffer[][SAMPLES_PER_BLOCK];

int Audio::init(const struct device *const codec_dev, const struct device *const i2s_dev) {
    int ret;

    if (codec_dev == nullptr || i2s_dev == nullptr) {
        LOG_ERR("Got null device pointer");
        return -EINVAL;
    }

    ret = k_mem_slab_init(&mem_slab, mem_slab_buffer, sizeof(mem_slab_buffer[0]), BLOCK_COUNT);
    if (ret < 0) {
        LOG_ERR("Failed to initialize memory slab: %d", -ret);
        return ret;
    }

    if (!device_is_ready(i2s_dev)) {
        LOG_ERR("I2S bus not ready");
        return -ENODEV;
    }

    if (!device_is_ready(codec_dev)) {
        LOG_ERR("Codec bus not ready");
        return -ENODEV;
    }

    Audio::codec_dev = codec_dev;
    Audio::i2s_dev   = i2s_dev;

    const struct i2s_config i2s_config = {
        .word_size      = 16,  // NOTE: Each sample is int16_t
        .channels       = CHANNEL_COUNT,
        .format         = I2S_FMT_DATA_FORMAT_I2S,
        .options        = I2S_OPT_FRAME_CLK_MASTER | I2S_OPT_BIT_CLK_MASTER,
        .frame_clk_freq = SAMPLING_FREQUENCY,
        .mem_slab       = &mem_slab,
        .block_size     = sizeof(mem_slab_buffer[0]),
        .timeout        = SYS_FOREVER_MS,
    };
    ret = i2s_configure(Audio::i2s_dev, I2S_DIR_TX, &i2s_config);
    if (ret < 0) {
        LOG_ERR("Failed to configure I2S: %d", -ret);
        return ret;
    }

    struct audio_codec_cfg codec_config;
    codec_config.dai_type    = AUDIO_DAI_TYPE_I2S;
    codec_config.dai_cfg.i2s = i2s_config;
    ret                      = audio_codec_configure(Audio::codec_dev, &codec_config);
    if (ret < 0) {
        LOG_ERR("Failed to configure audio codec: %d", -ret);
        return ret;
    }

    return 0;
}

int Audio::set_volume(const uint8_t volume) {
    int ret;

    audio_property_value_t value = {.vol = volume};

    ret = audio_codec_set_property(codec_dev, AUDIO_PROPERTY_OUTPUT_VOLUME,
                                   AUDIO_CHANNEL_SIDE_LEFT, value);
    if (ret < 0) {
        LOG_ERR("Failed to set right channel volume: %d", -ret);
        return ret;
    }

    ret = audio_codec_set_property(codec_dev, AUDIO_PROPERTY_OUTPUT_VOLUME,
                                   AUDIO_CHANNEL_SIDE_RIGHT, value);
    if (ret < 0) {
        LOG_ERR("Failed to set right channel volume: %d", -ret);
        return ret;
    }

    return 0;
}

int16_t *Audio::get_block(const k_timeout_t timeout) {
    // NOTE:
    // `i2s_write()` frees the allocated block once the DMA is done so no free
    // is required.
    int ret = k_mem_slab_alloc(&mem_slab, (void **)&current_block, timeout);
    if (ret < 0) {
        LOG_ERR("Failed to allocate TX block: %d", -ret);
        return nullptr;
    }

    return current_block;
}

void Audio::clear_block(void) {
    (void)memset(current_block, 0, sizeof(mem_slab_buffer[0]));
}

int Audio::write_block(void) {
    int ret;

    ret = i2s_write(i2s_dev, current_block, sizeof(mem_slab_buffer[0]));
    if (ret < 0) {
        if (ret != -EIO) {
            LOG_ERR("Failed to write data: %d", -ret);
            return ret;
        }

        LOG_DBG("I2S queue emptied, attempting to prepare it");
        ret = i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_PREPARE);
        if (ret < 0) {
            LOG_ERR("Failed to prepare I2S: %d", -ret);
            return ret;
        }
        Audio::write_block();

        return 0;
    }

    return 0;
}

int Audio::start_writes(void) {
    int ret;

    ret = i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START);
    if (ret < 0) {
        LOG_ERR("Failed to trigger I2S: %d", -ret);
        return ret;
    }

    return 0;
}
