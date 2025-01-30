/*
 * Copyright (c) 2024 Titouan Christophe
 * Copyright (c) 2025 Utkarsh Verma
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/audio/codec.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(cirrus_cs43l22);

#define DT_DRV_COMPAT cirrus_cs43l22

/*
 * See datasheet: https://statics.cirrus.com/pubs/proDatasheet/CS43L22_F2.pdf
 */

/* (datasheet) 6. REGISTER QUICK REFERENCE */
#define REG_ID                       0x01
#define REG_POWER_CTL_1              0x02
#define REG_POWER_CTL_2              0x04
#define REG_CLOCKING_CTL             0x05
#define REG_INTERFACE_CTL_1          0x06
#define REG_INTERFACE_CTL_2          0x07
#define REG_PASSTHROUGH_A            0x08
#define REG_PASSTHROUGH_B            0x09
#define REG_ANALOG_ZC_AND_SR         0x0a
#define REG_PASSTHROUGH_GANG_CONTROL 0x0c
#define REG_PLAYBACK_CTL_1           0x0d
#define REG_MISC_CTL                 0x0e
#define REG_PLAYBACK_CTL_2           0x0f
#define REG_PASSTHROUGH_A_VOL        0x14
#define REG_PASSTHROUGH_B_VOL        0x15
#define REG_PCMA_VOL                 0x1a
#define REG_PCMB_VOL                 0x1b
#define REG_BEEP_FREQ                0x1c
#define REG_BEEP_VOL                 0x1d
#define REG_BEEP_TONE                0x1e
#define REG_TONE_CTL                 0x1f
#define REG_MASTER_A_VOL             0x20
#define REG_MASTER_B_VOL             0x21
#define REG_HEADPHONES_A_VOL         0x22
#define REG_HEADPHONES_B_VOL         0x23
#define REG_SPEAKER_A_VOL            0x22
#define REG_SPEAKER_B_VOL            0x23
#define REG_STATUS                   0x2e
#define REG_SPEAKER_STATUS           0x31

/* (datasheet) 7.5.4 DAC Interface Format */
#define DAC_IF_FORMAT_LEFT_JUSTIFIED  0
#define DAC_IF_FORMAT_I2S             1
#define DAC_IF_FORMAT_RIGHT_JUSTIFIED 2

/* (datasheet) 7.5.5 Audio Word Length */
#define WORDLEN_32       0
#define WORDLEN_24       1
#define WORDLEN_20       2
#define WORDLEN_16       3
#define WORDLEN_RIGHT_24 0
#define WORDLEN_RIGHT_20 1
#define WORDLEN_RIGHT_18 2
#define WORDLEN_RIGHT_16 3

/* (datasheet) 7.12 Playback Control 2 */
#define HEADPHONES_B_MUTE (1 << 7)
#define HEADPHONES_A_MUTE (1 << 6)
#define SPEAKER_B_MUTE    (1 << 5)
#define SPEAKER_A_MUTE    (1 << 4)

#define cs43l22_write(_i2c, _reg, _value) cs43l22_write_masked(_i2c, _reg, _value, 0xff)
static inline int cs43l22_write_masked(const struct i2c_dt_spec *i2c, uint8_t reg,
                                       uint8_t value, uint8_t mask) {
    int ret;
    uint8_t actual_value = 0;
    if (mask != 0xff) {
        ret = i2c_burst_read_dt(i2c, reg, &actual_value, 1);
        if (ret) {
            LOG_ERR("Unable to get actual register value [%02X]", reg);
            return ret;
        }
    }
    actual_value = (actual_value & ~mask) | (value & mask);
    return i2c_burst_write_dt(i2c, reg, &actual_value, 1);
}

#define cs43l22_power_down(_i2c) cs43l22_write(_i2c, REG_POWER_CTL_1, 0x01)
#define cs43l22_power_up(_i2c)   cs43l22_write(_i2c, REG_POWER_CTL_1, 0x9e)

struct cs43l22_config {
    struct i2c_dt_spec i2c;
    struct gpio_dt_spec reset_gpio;
};

static int cs43l22_configure(const struct device *dev, struct audio_codec_cfg *audiocfg) {
    uint8_t format, wordlen;
    const struct cs43l22_config *cfg = dev->config;

    switch (audiocfg->dai_type) {
        case AUDIO_DAI_TYPE_I2S:
            format = DAC_IF_FORMAT_I2S;
            break;
        default:
            return -ENOTSUP;
    }

    if (format == DAC_IF_FORMAT_RIGHT_JUSTIFIED) {
        switch (audiocfg->dai_cfg.i2s.word_size) {
            case 16:
                wordlen = WORDLEN_RIGHT_16;
                break;
            case 18:
                wordlen = WORDLEN_RIGHT_18;
                break;
            case 20:
                wordlen = WORDLEN_RIGHT_20;
                break;
            case 24:
                wordlen = WORDLEN_RIGHT_24;
                break;
            default:
                return -ENOTSUP;
        }
    } else {
        switch (audiocfg->dai_cfg.i2s.word_size) {
            case 16:
                wordlen = WORDLEN_16;
                break;
            case 20:
                wordlen = WORDLEN_20;
                break;
            case 24:
                wordlen = WORDLEN_24;
                break;
            case 32:
                wordlen = WORDLEN_32;
                break;
            default:
                return -ENOTSUP;
        }
    }

    cs43l22_power_down(&cfg->i2c);
    /* Headphones always on, speaker always off */
    cs43l22_write(&cfg->i2c, REG_POWER_CTL_2, 0xaf);
    /* Automatic clock detection */
    cs43l22_write(&cfg->i2c, REG_CLOCKING_CTL, 0x80);
    /* Slave mode, do not invert SCLK, disable DSP, requested frame format */
    cs43l22_write_masked(&cfg->i2c, REG_INTERFACE_CTL_1, (format << 2) | wordlen, 0xdf);
    /* Enable soft ramp for volume changes */
    cs43l22_write(&cfg->i2c, REG_MISC_CTL, 0x02);

    cs43l22_power_up(&cfg->i2c);
    return 0;
}

static void cs43l22_start_output(const struct device *dev) {}

static void cs43l22_stop_output(const struct device *dev) {}

static int cs43l22_apply_properties(const struct device *dev) {
    return 0;
}

static int cs43l22_set_property(const struct device *dev, audio_property_t property,
                                audio_channel_t channel, audio_property_value_t val) {
    const struct cs43l22_config *cfg = dev->config;

    if (property == AUDIO_PROPERTY_OUTPUT_MUTE) {
        uint8_t dac_channel_mute = 0;
        switch (channel) {
            case AUDIO_CHANNEL_ALL:
                dac_channel_mute =
                    HEADPHONES_A_MUTE | HEADPHONES_B_MUTE | SPEAKER_A_MUTE | SPEAKER_B_MUTE;
                break;
            case AUDIO_CHANNEL_SIDE_LEFT:
                dac_channel_mute = HEADPHONES_A_MUTE;
                break;
            case AUDIO_CHANNEL_SIDE_RIGHT:
                dac_channel_mute = HEADPHONES_B_MUTE;
                break;
            case AUDIO_CHANNEL_FRONT_LEFT:
                dac_channel_mute = SPEAKER_A_MUTE;
                break;
            case AUDIO_CHANNEL_FRONT_RIGHT:
                dac_channel_mute = SPEAKER_B_MUTE;
                break;
            default:
                return -ENOTSUP;
        }
        return cs43l22_write_masked(&cfg->i2c, REG_PLAYBACK_CTL_2,
                                    val.mute ? dac_channel_mute : 0, dac_channel_mute);
    } else if (property == AUDIO_PROPERTY_OUTPUT_VOLUME) {
        uint8_t reg, volume_scaled;
        switch (channel) {
            case AUDIO_CHANNEL_SIDE_LEFT:
                reg = REG_HEADPHONES_A_VOL;
                break;
            case AUDIO_CHANNEL_SIDE_RIGHT:
                reg = REG_HEADPHONES_B_VOL;
                break;
            case AUDIO_CHANNEL_FRONT_LEFT:
                reg = REG_SPEAKER_A_VOL;
                break;
            case AUDIO_CHANNEL_FRONT_RIGHT:
                reg = REG_SPEAKER_B_VOL;
                break;
            default:
                return -ENOTSUP;
        }

        if ((uint8_t)val.vol < 0xff) {
            volume_scaled = (uint8_t)val.vol + 1;
        } else {
            volume_scaled = 0x0;
        }
        return cs43l22_write(&cfg->i2c, reg, volume_scaled);
    }

    return -ENOTSUP;
}

static const struct audio_codec_api cs43l22_api = {
    .configure        = cs43l22_configure,
    .start_output     = cs43l22_start_output,
    .stop_output      = cs43l22_stop_output,
    .set_property     = cs43l22_set_property,
    .apply_properties = cs43l22_apply_properties,
};

static int cs43l22_init(const struct device *dev) {
    uint8_t regval;
    const struct cs43l22_config *cfg = dev->config;

    int ret = gpio_pin_configure_dt(&cfg->reset_gpio, GPIO_OUTPUT_ACTIVE);
    if (ret) {
        return -EIO;
    }

    ret = i2c_burst_read_dt(&cfg->i2c, REG_ID, &regval, 1);
    if (ret) {
        LOG_ERR("Unable to read device ID");
        return -ENODEV;
    }

    if ((regval >> 3) != 0x1C) {
        LOG_ERR("Wrong Chip ID");
        return -ENODEV;
    }

    LOG_DBG("Found CS43L22 (chip=%02X, rev=%c%d)", regval >> 3, 'A' + ((regval >> 1) & 3),
            regval & 1);
    return 0;
}

#define CS43L22_INIT(inst)                                                        \
    static const struct cs43l22_config cs43l22_config_##inst = {                  \
        .i2c        = I2C_DT_SPEC_INST_GET(inst),                                 \
        .reset_gpio = GPIO_DT_SPEC_INST_GET(inst, reset_gpios),                   \
    };                                                                            \
                                                                                  \
    DEVICE_DT_INST_DEFINE(inst, cs43l22_init, NULL, NULL, &cs43l22_config_##inst, \
                          POST_KERNEL, CONFIG_AUDIO_CODEC_INIT_PRIORITY, &cs43l22_api);

DT_INST_FOREACH_STATUS_OKAY(CS43L22_INIT)
