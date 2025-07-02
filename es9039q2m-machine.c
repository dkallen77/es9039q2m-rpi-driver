#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <sound/soc.h>

#define DRV_NAME "es9039q2m-machine"

static struct snd_soc_dai_link_component cpu, platform;
static struct snd_soc_dai_link_component codecs[2];  // dummy + i2c

static struct snd_soc_dai_link es9039q2m_dai_link = {
    .name           = "es9039q2m",
    .stream_name    = "HiFi",
    .dai_fmt        = SND_SOC_DAIFMT_I2S |
                      SND_SOC_DAIFMT_NB_NF |
                      SND_SOC_DAIFMT_CBS_CFS,
    .cpus           = &cpu,
    .num_cpus       = 1,
    .platforms      = &platform,
    .num_platforms  = 1,
    .codecs         = codecs,
    .num_codecs     = 2,
};

static struct snd_soc_card es9039q2m_card = {
    .name = "es9039q2m",
    .owner = THIS_MODULE,
    .dai_link = &es9039q2m_dai_link,
    .num_links = 1,
};

static int es9039q2m_machine_probe(struct platform_device *pdev)
{
    struct device_node *np = pdev->dev.of_node;
    struct device_node *i2s_node, *codec_node;
    int ret;

    dev_info(&pdev->dev, "Probing es9039q2m machine driver...\n");

    // Get I2S controller node
    i2s_node = of_parse_phandle(np, "i2s-controller", 0);
    if (!i2s_node) {
        dev_err(&pdev->dev, "Missing 'i2s-controller' DT property\n");
        return -EINVAL;
    }

    // Get codec node for I2C controls
    codec_node = of_parse_phandle(np, "codec", 0);
    if (!codec_node) {
        dev_err(&pdev->dev, "Missing 'codec' DT property\n");
        of_node_put(i2s_node);
        return -EINVAL;
    }

    // Set up CPU (I2S) component
    cpu.of_node = i2s_node;
    cpu.dai_name = NULL;
    cpu.name = NULL;

    // Set up platform component (same as CPU for I2S)
    platform.of_node = i2s_node;
    platform.dai_name = NULL;
    platform.name = NULL;

    // Set up dummy codec component for audio path (first codec)
    codecs[0].of_node = NULL;
    codecs[0].dai_name = "snd-soc-dummy-dai";
    codecs[0].name = "snd-soc-dummy";

    // Set up I2C codec component for controls (second codec, dummy DAI)
    codecs[1].of_node = codec_node;
    codecs[1].dai_name = "es9039q2m-dummy-dai";
    codecs[1].name = NULL;

    // Set card device
    es9039q2m_card.dev = &pdev->dev;

    // Register the sound card
    ret = snd_soc_register_card(&es9039q2m_card);
    if (ret) {
        dev_err(&pdev->dev, "Failed to register sound card: %d\n", ret);
        goto cleanup;
    }

    dev_info(&pdev->dev, "ES9039Q2M machine driver registered successfully\n");

cleanup:
    of_node_put(i2s_node);
    of_node_put(codec_node);
    return ret;
}

static void es9039q2m_machine_remove(struct platform_device *pdev)
{
    snd_soc_unregister_card(&es9039q2m_card);
    dev_info(&pdev->dev, "ES9039Q2M machine driver unregistered\n");
}

static const struct of_device_id es9039q2m_machine_of_match[] = {
    { .compatible = "espressif,es9039q2m-machine" },
    {},
};
MODULE_DEVICE_TABLE(of, es9039q2m_machine_of_match);

static struct platform_driver es9039q2m_machine_driver = {
    .driver = {
        .name = DRV_NAME,
        .of_match_table = es9039q2m_machine_of_match,
    },
    .probe = es9039q2m_machine_probe,
    .remove = es9039q2m_machine_remove,
};

module_platform_driver(es9039q2m_machine_driver);

MODULE_AUTHOR("David Allen");
MODULE_DESCRIPTION("ASoC ES9039Q2M Machine Driver");
MODULE_LICENSE("GPL");