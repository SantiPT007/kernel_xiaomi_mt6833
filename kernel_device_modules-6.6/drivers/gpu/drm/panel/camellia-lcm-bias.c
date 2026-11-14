// SPDX-License-Identifier: GPL-2.0
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/kernel.h>
#include <linux/string.h>

#define LCM_I2C_ID_NAME "camellia_lcm_bias"

static struct i2c_client *_lcm_i2c_client;

int real_refresh;
EXPORT_SYMBOL(real_refresh);
int temp_refresh;
EXPORT_SYMBOL(temp_refresh);

bool camellia_bootargs_contains(const char *needle)
{
	struct device_node *chosen = of_find_node_by_path("/chosen");
	const char *str = NULL;
	bool found = false;

	if (!chosen)
		return false;
	if (!of_property_read_string(chosen, "bootargs", &str) && str && strstr(str, needle))
		found = true;
	else if (!of_property_read_string(chosen, "atag,videolfb-lcmname", &str) && str &&
		 strstr(str, needle))
		found = true;
	of_node_put(chosen);
	return found;
}
EXPORT_SYMBOL(camellia_bootargs_contains);

int camellia_bklic_id(void)
{
	struct device_node *chosen = of_find_node_by_path("/chosen");
	const char *str = NULL, *p;
	int id = -1;

	if (!chosen)
		return -1;
	if (!of_property_read_string(chosen, "bootargs", &str) && str) {
		p = strstr(str, ":bklic=");
		if (p && kstrtoint(p + strlen(":bklic="), 10, &id))
			id = (int)simple_strtol(p + strlen(":bklic="), NULL, 10);
	}
	of_node_put(chosen);
	return id;
}
EXPORT_SYMBOL_GPL(camellia_bklic_id);

int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	struct i2c_client *client = _lcm_i2c_client;
	char write_data[2] = { addr, value };
	int ret;

	if (!client) {
		pr_info("[LCM][I2C] bias client not probed, skip 0x%02x=0x%02x\n",
			addr, value);
		return 0;
	}
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_info("[LCM][I2C] bias write 0x%02x=0x%02x failed %d\n",
			addr, value, ret);
	return ret;
}
EXPORT_SYMBOL(_lcm_i2c_write_bytes);

static int camellia_lcm_bias_probe(struct i2c_client *client)
{
	pr_info("[LCM][I2C] bias: name=%s addr=0x%x\n", client->name, client->addr);
	_lcm_i2c_client = client;
	return 0;
}

static void camellia_lcm_bias_remove(struct i2c_client *client)
{
	_lcm_i2c_client = NULL;
}

static const struct of_device_id camellia_lcm_bias_of_match[] = {
	{ .compatible = "mediatek,I2C_LCD_BIAS" },
	{},
};
MODULE_DEVICE_TABLE(of, camellia_lcm_bias_of_match);

static const struct i2c_device_id camellia_lcm_bias_id[] = {
	{ LCM_I2C_ID_NAME, 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, camellia_lcm_bias_id);

static struct i2c_driver camellia_lcm_bias_driver = {
	.id_table = camellia_lcm_bias_id,
	.probe = camellia_lcm_bias_probe,
	.remove = camellia_lcm_bias_remove,
	.driver = {
		.name = LCM_I2C_ID_NAME,
		.of_match_table = camellia_lcm_bias_of_match,
	},
};
static const char *const camellia_lcm_ids[][2] = {
	{ "dsi_panel_K19_36_02_0a_vdo", "incell,vendor:36,IC:0a" },
	{ "dsi_panel_K19_36_02_0c_vdo", "incell,vendor:36,IC:0c" },
	{ "dsi_panel_K19_43_02_0b_vdo", "incell,vendor:43,IC:0b" },
	{ "dsi_panel_K19_36_02_0d_vdo", "incell,vendor:36,IC:0d" },
	{ "dsi_panel_K19_43_02_0b2_vdo", "incell,vendor:43,IC:0b2" },
};

static ssize_t lcm_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(camellia_lcm_ids); i++)
		if (camellia_bootargs_contains(camellia_lcm_ids[i][0]))
			return sysfs_emit(buf, "%s\n", camellia_lcm_ids[i][1]);
	return sysfs_emit(buf, "LCM error\n");
}
static DEVICE_ATTR_RO(lcm);

static struct attribute *hw_info_attrs[] = {
	&dev_attr_lcm.attr,
	NULL,
};

static const struct attribute_group hw_info_group = {
	.name = "hw_info",
	.attrs = hw_info_attrs,
};

static const struct attribute_group *hw_info_groups[] = {
	&hw_info_group,
	NULL,
};

static const struct class huaqin_class = {
	.name = "huaqin",
};

static struct device *huaqin_dev;

static int __init camellia_lcm_bias_init(void)
{
	if (class_register(&huaqin_class)) {
		pr_err("[LCM] huaqin class register failed\n");
	} else {
		huaqin_dev = device_create_with_groups(&huaqin_class, NULL, 0, NULL,
						       hw_info_groups, "interface");
		if (IS_ERR(huaqin_dev)) {
			pr_err("[LCM] hw_info create failed %ld\n", PTR_ERR(huaqin_dev));
			huaqin_dev = NULL;
			class_unregister(&huaqin_class);
		}
	}
	return i2c_add_driver(&camellia_lcm_bias_driver);
}
module_init(camellia_lcm_bias_init);

static void __exit camellia_lcm_bias_exit(void)
{
	i2c_del_driver(&camellia_lcm_bias_driver);
	if (huaqin_dev) {
		device_unregister(huaqin_dev);
		class_unregister(&huaqin_class);
	}
}
module_exit(camellia_lcm_bias_exit);

MODULE_DESCRIPTION("camellia LCD bias I2C regulator");
MODULE_LICENSE("GPL");
