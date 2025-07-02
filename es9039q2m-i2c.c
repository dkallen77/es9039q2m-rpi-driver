#include <linux/module.h>
#include <linux/i2c.h>
#include <sound/soc.h>

#define ES9039Q2M_MODE_REG           0x00
#define ES9039Q2M_SOFT_RAMP_REG      0x82
#define ES9039Q2M_CH1_VOLUME_REG     0x4A
#define ES9039Q2M_CH2_VOLUME_REG     0x4B
#define ES9039Q2M_VOLUME_HOLD_REG    0x59
#define ES9039Q2M_FILTER_SHAPE_REG   0x58

// Set volume: value is 0-255 representing 0dB to -127.5dB in 0.5dB steps
static int es9039q2m_set_vol(struct snd_kcontrol *kcontrol,
                             struct snd_ctl_elem_value *ucontrol) {
    struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
    struct i2c_client *client = to_i2c_client(component->dev);
    unsigned int val = ucontrol->value.integer.value[0];
    unsigned int inv_val = 255 - val; // Invert ALSA value for DAC
    int ret;

    dev_info(&client->dev, "Setting volume to %u (inverted: %u)\n", val, inv_val);

    ret = i2c_smbus_write_byte_data(client, ES9039Q2M_VOLUME_HOLD_REG, 0x14); // Hold volume
    if (ret < 0) {
        dev_err(&client->dev, "Failed to hold volume: %d\n", ret);
        return ret;
    }
    dev_info(&client->dev, "Volume hold set to 0x14\n");

    ret = i2c_smbus_write_byte_data(client, ES9039Q2M_CH1_VOLUME_REG, inv_val); // Set Ch1 volume
    if (ret < 0) {
        dev_err(&client->dev, "Failed to set Ch1 volume: %d\n", ret);
        return ret;
    }
    dev_info(&client->dev, "Ch1 volume set to 0x%02x\n", inv_val);

    ret = i2c_smbus_write_byte_data(client, ES9039Q2M_CH2_VOLUME_REG, inv_val); // Set Ch2 volume
    if (ret < 0) {
        dev_err(&client->dev, "Failed to set Ch2 volume: %d\n", ret);
        return ret;
    }
    dev_info(&client->dev, "Ch2 volume set to 0x%02x\n", inv_val);

    ret = i2c_smbus_write_byte_data(client, ES9039Q2M_VOLUME_HOLD_REG, 0x04); // Release volume hold
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

    int val = i2c_smbus_read_byte_data(client, ES9039Q2M_CH1_VOLUME_REG);
    if (val < 0) {
        dev_err(&client->dev, "Failed to read volume: %d\n", val);
        return val;
    }

    dev_info(&client->dev, "Read volume: 0x%02x (inverted: %u)\n", val, 255 - val);
    ucontrol->value.integer.value[0] = 255 - val; // Invert for ALSA
    return 0;
}

static int es9039q2m_enable_output(struct snd_soc_component *component) {
    struct i2c_client *client = to_i2c_client(component->dev);
    int ret;

    dev_info(&client->dev, "Enabling DAC output...\n");

    ret = i2c_smbus_write_byte_data(client, ES9039Q2M_SOFT_RAMP_REG, 0x09); // 170ms ramp (adjust if clock differs)
    if (ret < 0) {
        dev_err(&client->dev, "Failed to set soft ramp: %d\n", ret);
        return ret;
    }
    dev_info(&client->dev, "Soft ramp set to 0x09\n");

    ret = i2c_smbus_write_byte_data(client, ES9039Q2M_MODE_REG, 0x02); // Enable DAC
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

    dev_info(&client->dev, "Disabling DAC output...\n");

    ret = i2c_smbus_write_byte_data(client, ES9039Q2M_MODE_REG, 0x00); // Disable DAC
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
    SOC_SINGLE_EXT("DAC Volume", 0, 0, 255, 0,
                   es9039q2m_get_vol, es9039q2m_set_vol),
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
