#include <linux/module.h>
#include <linux/i2c.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#define ES9039Q2M_MODE_REG           0x00
#define ES9039Q2M_SOFT_RAMP_REG      0x82
#define ES9039Q2M_CH1_VOLUME_REG     0x4A
#define ES9039Q2M_CH2_VOLUME_REG     0x4B
#define ES9039Q2M_VOLUME_HOLD_REG    0x59
#define ES9039Q2M_FILTER_SHAPE_REG   0x58
#define ES9039Q2M_AUTOMUTE_REG       0x7B
#define ES9039Q2M_NUM_FILTER_SHAPES  8
#define ES9039Q2M_I2C_DELAY_US       1000   // 1ms delay after I2C operations

static const char * const es9039q2m_filter_shape_texts[] = {
    "Minimum Phase",
    "Linear Phase Fast Roll Off Apodizing",
    "Linear Phase Fast Roll Off",
    "Linear Phase Fast Roll Off Low Ripple",
    "Linear Phase Slow Roll Off",
    "Minimum Phase Fast Roll Off",
    "Minimum Phase Slow Roll Off",
    "Minimum Phase Slow Roll Off Low Dispersion"
};

static const struct soc_enum es9039q2m_filter_shape_enum =
    SOC_ENUM_SINGLE(0, 0, ES9039Q2M_NUM_FILTER_SHAPES, es9039q2m_filter_shape_texts);

static const DECLARE_TLV_DB_SCALE(es9039q2m_db_scale, -12750, 50, 0);

// Generalized I2C read/write with retry and optional verification for write
static int es9039q2m_i2c_xfer_with_retry(struct i2c_client *client, u8 reg, u8 *val, bool write, bool verify)
{
    int ret = 0, retries = 0;
    while (retries < 10) {
        if (write) {
            ret = i2c_smbus_write_byte_data(client, reg, *val);
            usleep_range(ES9039Q2M_I2C_DELAY_US, ES9039Q2M_I2C_DELAY_US + 1000);
            if (ret >= 0) {
                if (verify) {
                    int read_val = i2c_smbus_read_byte_data(client, reg);
                    if (read_val == *val) {
                        return 0;
                    } else {
                        dev_dbg(&client->dev, "I2C write verification failed: reg 0x%02x, wrote 0x%02x, read 0x%02x\n", reg, *val, read_val);
                    }
                } else {
                    return 0;
                }
            }
            // else fall through to retry
        } else {
            ret = i2c_smbus_read_byte_data(client, reg);
            usleep_range(ES9039Q2M_I2C_DELAY_US, ES9039Q2M_I2C_DELAY_US + 1000);
            if (ret >= 0) {
                *val = ret;
                return 0;
            }
        }
        retries++;
        if (retries < 10) {
            dev_dbg(&client->dev, "I2C %s failed (reg 0x%02x, val 0x%02x), retrying (%d/10)...\n", write ? "write" : "read", reg, write ? *val : 0, retries);
            usleep_range(1000, 2000);
        }
    }
    dev_err(&client->dev, "Failed to %s reg 0x%02x after 10 attempts: %d\n", write ? "write" : "read", reg, ret);
    return ret < 0 ? ret : -EIO;
}

static int es9039q2m_get_filter_shape(struct snd_kcontrol *kcontrol,
                                      struct snd_ctl_elem_value *ucontrol) {
    struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
    struct i2c_client *client = to_i2c_client(component->dev);
    u8 val = 0;
    int ret = es9039q2m_i2c_xfer_with_retry(client, ES9039Q2M_FILTER_SHAPE_REG, &val, false, false);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to read filter shape: %d\n", ret);
        return ret;
    }
    ucontrol->value.enumerated.item[0] = val & 0x07;
    const char *filter_shape_name = "Unknown";
    if ((val & 0x07) < ES9039Q2M_NUM_FILTER_SHAPES)
        filter_shape_name = es9039q2m_filter_shape_texts[val & 0x07];
    dev_dbg(&client->dev, "Read filter shape: 0x%02x (name: %s)\n", val, filter_shape_name);
    return 0;
}

static int es9039q2m_set_filter_shape_enum(struct snd_kcontrol *kcontrol,
                                           struct snd_ctl_elem_value *ucontrol) {
    struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
    struct i2c_client *client = to_i2c_client(component->dev);
    unsigned int val = ucontrol->value.enumerated.item[0];
    const char *filter_shape_name = "Unknown";
    if (val < ES9039Q2M_NUM_FILTER_SHAPES)
        filter_shape_name = es9039q2m_filter_shape_texts[val];
    u8 regval = val | 0x60;
    dev_info(&client->dev, "Setting filter shape to %u (name: %s)\n", regval, filter_shape_name);
    int ret = es9039q2m_i2c_xfer_with_retry(client, ES9039Q2M_FILTER_SHAPE_REG, &regval, true, true);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to set filter shape: %d\n", ret);
        return ret;
    }
    dev_info(&client->dev, "Filter shape confirmed at 0x%02x (%s)\n", regval, filter_shape_name);
    return 0;
}

// Set volume: value is 0-255 representing 0dB to -127.5dB in 0.5dB steps
static int es9039q2m_set_vol(struct snd_kcontrol *kcontrol,
                             struct snd_ctl_elem_value *ucontrol) {
    struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
    struct i2c_client *client = to_i2c_client(component->dev);
    unsigned int val = ucontrol->value.integer.value[0];
    int db_centi, ret;
    u8 regval;
    val = 255 - val; // Invert the value: 0 becomes 255, 255 becomes 0
    db_centi = -50 * val; // -0.5 dB per step, in hundredths
    dev_info(&client->dev, "Setting volume to %u (dB: %d.%02d)\n", val, db_centi / 100, abs(db_centi % 100));

    // Hold volume
    regval = 0x14;
    ret = es9039q2m_i2c_xfer_with_retry(client, ES9039Q2M_VOLUME_HOLD_REG, &regval, true, true);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to hold volume: %d\n", ret);
        return ret;
    }
    dev_info(&client->dev, "Volume hold set to 0x14\n");

    // Set Ch1 volume
    regval = val;
    ret = es9039q2m_i2c_xfer_with_retry(client, ES9039Q2M_CH1_VOLUME_REG, &regval, true, true);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to set Ch1 volume: %d\n", ret);
        return ret;
    }
    dev_info(&client->dev, "Ch1 volume confirmed at 0x%02x\n", val);

    // Set Ch2 volume
    regval = val;
    ret = es9039q2m_i2c_xfer_with_retry(client, ES9039Q2M_CH2_VOLUME_REG, &regval, true, true);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to set Ch2 volume: %d\n", ret);
        return ret;
    }
    dev_info(&client->dev, "Ch2 volume confirmed at 0x%02x\n", val);

    // Release volume hold
    regval = 0x04;
    ret = es9039q2m_i2c_xfer_with_retry(client, ES9039Q2M_VOLUME_HOLD_REG, &regval, true, true);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to release volume hold: %d\n", ret);
        return ret;
    }
    dev_info(&client->dev, "Volume hold released to 0x04\n");
    return 0;
}

static int es9039q2m_get_vol(struct snd_kcontrol *kcontrol,
                             struct snd_ctl_elem_value *ucontrol) {
    struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
    struct i2c_client *client = to_i2c_client(component->dev);
    u8 val = 0;
    int ret = es9039q2m_i2c_xfer_with_retry(client, ES9039Q2M_CH1_VOLUME_REG, &val, false, false);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to read volume: %d\n", ret);
        return ret;
    }
    int db_centi = -50 * val; // -0.5 dB per step, in hundredths
    dev_info(&client->dev, "Read volume: 0x%02x (dB: %d.%02d)\n", val, db_centi / 100, abs(db_centi % 100));
    ucontrol->value.integer.value[0] = 255 - val; // Invert the value back for the UI
    return 0;
}

static int es9039q2m_enable_output(struct snd_soc_component *component) {
    struct i2c_client *client = to_i2c_client(component->dev);
    int ret;
    u8 regval;

    dev_info(&client->dev, "Enabling DAC output...\n");

    // Set soft ramp
    regval = 0x09;
    ret = es9039q2m_i2c_xfer_with_retry(client, ES9039Q2M_SOFT_RAMP_REG, &regval, true, true);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to set soft ramp: %d\n", ret);
        return ret;
    }
    dev_info(&client->dev, "Soft ramp set to 0x09\n");

    // Disable automute
    regval = 0x00;
    ret = es9039q2m_i2c_xfer_with_retry(client, ES9039Q2M_AUTOMUTE_REG, &regval, true, true);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to disable automute: %d\n", ret);
        return ret;
    }
    dev_info(&client->dev, "Automute disabled\n");

    // Enable DAC
    regval = 0x02;
    ret = es9039q2m_i2c_xfer_with_retry(client, ES9039Q2M_MODE_REG, &regval, true, true);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to enable DAC: %d\n", ret);
        return ret;
    }
    dev_info(&client->dev, "DAC enabled (mode set to 0x02)\n");

    return 0;
}

static int es9039q2m_disable_output(struct snd_soc_component *component) {
    struct i2c_client *client = to_i2c_client(component->dev);
    int ret;
    u8 regval = 0x00;

    dev_info(&client->dev, "Disabling DAC output...\n");

    // Disable DAC
    ret = es9039q2m_i2c_xfer_with_retry(client, ES9039Q2M_MODE_REG, &regval, true, true);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to disable DAC: %d\n", ret);
        return ret;
    }
    dev_info(&client->dev, "DAC disabled (mode set to 0x00)\n");

    return 0;
}

static int es9039q2m_component_probe(struct snd_soc_component *component) {
    dev_info(component->dev, "ES9039Q2M component probe called\n");
    return es9039q2m_enable_output(component);
}

static void es9039q2m_component_remove(struct snd_soc_component *component) {
    dev_info(component->dev, "ES9039Q2M component remove called\n");
    es9039q2m_disable_output(component);
}

static const struct snd_kcontrol_new es9039q2m_controls[] = {
    SOC_SINGLE_EXT_TLV("DAC Volume", 0, 0, 255, 0,
                   es9039q2m_get_vol, es9039q2m_set_vol, es9039q2m_db_scale),
    SOC_ENUM_EXT("Filter Shape", es9039q2m_filter_shape_enum,
                 es9039q2m_get_filter_shape, es9039q2m_set_filter_shape_enum),
};

static const struct snd_soc_component_driver soc_codec_dev_es9039q2m = {
    .probe        = es9039q2m_component_probe,
    .remove       = es9039q2m_component_remove,
    .controls     = es9039q2m_controls,
    .num_controls = ARRAY_SIZE(es9039q2m_controls),
};

// Dummy DAI ops
static const struct snd_soc_dai_ops es9039q2m_dummy_dai_ops = {};

static struct snd_soc_dai_driver es9039q2m_dummy_dai = {
    .name = "es9039q2m-dummy-dai",
    .playback = {
        .stream_name = "Dummy Playback",
        .channels_min = 2,
        .channels_max = 2,
        .rates = SNDRV_PCM_RATE_8000_192000,
        .formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
    },
    .ops = &es9039q2m_dummy_dai_ops,
};

static int es9039q2m_i2c_probe(struct i2c_client *client) {
    dev_info(&client->dev, "ES9039Q2M I2C probe called\n");
    // Register codec component with dummy DAI
    return devm_snd_soc_register_component(&client->dev,
                                           &soc_codec_dev_es9039q2m,
                                           &es9039q2m_dummy_dai, 1);
}

static const struct i2c_device_id es9039q2m_i2c_id[] = {
    { "es9039q2m-i2c", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, es9039q2m_i2c_id);

static const struct of_device_id es9039q2m_of_match[] = {
    { .compatible = "espressif,es9039q2m-i2c" },
    { }
};
MODULE_DEVICE_TABLE(of, es9039q2m_of_match);

static struct i2c_driver es9039q2m_i2c_driver = {
    .driver = {
        .name           = "es9039q2m-i2c",
        .of_match_table = es9039q2m_of_match,
    },
    .probe    = es9039q2m_i2c_probe,
    .id_table = es9039q2m_i2c_id,
};
module_i2c_driver(es9039q2m_i2c_driver);

MODULE_AUTHOR("David Allen");
MODULE_DESCRIPTION("ES9039Q2M I2C Audio Driver");
MODULE_LICENSE("GPL");
