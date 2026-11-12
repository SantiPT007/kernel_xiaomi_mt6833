// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/thermal.h>
#include <linux/workqueue.h>

#include "camellia_thermal_message.h"
#include "charger_class.h"

#define CAMELLIA_CHG_PSY	"primary_chg"
#define CAMELLIA_MASTER_PSY	"mtk-master-charger"
#define CAMELLIA_UNLIMIT_CURRENT	0x10000000

static const int camellia_thermal_icl[] = {
	2000000, 2000000, 2000000, 1600000, 1600000,
	1600000, 1200000, 1200000, 1000000, 1000000,
	1000000, 2000000, 2000000, 2000000, 2000000,
	2000000, 2000000, 2000000, 2000000, 2000000,
	2000000, 2000000, 0, 0, 0,
};

static const int camellia_thermal_icl_hvdcp[] = {
	2000000, 1800000, 1400000, 1200000, 1000000,
	1000000, 800000, 800000, 600000, 500000,
	400000, 2000000, 1700000, 1500000, 1400000,
	1300000, 1300000, 1200000, 1200000, 1200000,
	1100000, 1100000, 0, 0, 0,
};
static_assert(ARRAY_SIZE(camellia_thermal_icl_hvdcp) == ARRAY_SIZE(camellia_thermal_icl));

struct camellia_mt_charger {
	struct device *dev;
	struct power_supply *ac;
	struct power_supply *usb;
	struct power_supply *chg;
	struct charger_device *chg_dev;
	struct notifier_block nb;
	struct work_struct changed_work;
	struct thermal_cooling_device *cdev;
	struct mutex lock;
	unsigned long temp_level;
	bool thermal_hvdcp;
};

static struct power_supply *camellia_chg_psy(struct camellia_mt_charger *mc)
{
	if (!mc->chg)
		mc->chg = power_supply_get_by_name(CAMELLIA_CHG_PSY);
	return mc->chg;
}

static int camellia_chg_kind(struct camellia_mt_charger *mc)
{
	struct power_supply *chg = camellia_chg_psy(mc);
	union power_supply_propval val;

	if (!chg)
		return 0;
	if (power_supply_get_property(chg, POWER_SUPPLY_PROP_ONLINE, &val) || !val.intval)
		return 0;
	if (power_supply_get_property(chg, POWER_SUPPLY_PROP_USB_TYPE, &val))
		return 2;
	if (val.intval == POWER_SUPPLY_USB_TYPE_SDP || val.intval == POWER_SUPPLY_USB_TYPE_CDP)
		return 1;
	return 2;
}

static int camellia_chg_prop(struct camellia_mt_charger *mc,
			     enum power_supply_property psp, int def)
{
	struct power_supply *chg = camellia_chg_psy(mc);
	union power_supply_propval val;

	if (!chg || power_supply_get_property(chg, psp, &val))
		return def;
	return val.intval;
}

static int camellia_real_type(struct camellia_mt_charger *mc)
{
	if (!camellia_chg_prop(mc, POWER_SUPPLY_PROP_ONLINE, 0))
		return POWER_SUPPLY_REAL_TYPE_UNKNOWN;
	if (camellia_chg_prop(mc, POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE, 0) > 0)
		return POWER_SUPPLY_REAL_TYPE_HVDCP;
	switch (camellia_chg_prop(mc, POWER_SUPPLY_PROP_USB_TYPE, POWER_SUPPLY_USB_TYPE_UNKNOWN)) {
	case POWER_SUPPLY_USB_TYPE_SDP:
		return POWER_SUPPLY_REAL_TYPE_USB;
	case POWER_SUPPLY_USB_TYPE_CDP:
		return POWER_SUPPLY_REAL_TYPE_USB_CDP;
	case POWER_SUPPLY_USB_TYPE_DCP:
		return camellia_chg_prop(mc, POWER_SUPPLY_PROP_TYPE, 0) == POWER_SUPPLY_TYPE_USB_DCP ?
		       POWER_SUPPLY_REAL_TYPE_USB_DCP : POWER_SUPPLY_REAL_TYPE_USB_FLOAT;
	default:
		return POWER_SUPPLY_REAL_TYPE_UNKNOWN;
	}
}

static int camellia_adc_now(struct camellia_mt_charger *mc, enum power_supply_property psp)
{
	u32 v = 0;
	int ret;

	if (!mc->chg_dev)
		mc->chg_dev = get_charger_by_name(CAMELLIA_CHG_PSY);
	if (!mc->chg_dev)
		return 0;
	if (psp == POWER_SUPPLY_PROP_VOLTAGE_NOW)
		ret = charger_dev_get_vbus(mc->chg_dev, &v);
	else
		ret = charger_dev_get_ibus(mc->chg_dev, &v);
	return ret < 0 ? 0 : v / 1000;
}

static int camellia_mt_get_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	struct camellia_mt_charger *mc = power_supply_get_drvdata(psy);
	int want = psy == mc->usb ? 1 : 2;
	int online = camellia_chg_kind(mc) == want;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = online;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = online ? camellia_chg_prop(mc, POWER_SUPPLY_PROP_CURRENT_MAX, 500000) : 0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = online ? 5000000 : 0;
		break;
	case POWER_SUPPLY_PROP_REAL_TYPE:
		val->intval = camellia_real_type(mc);
		break;
	case POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE:
		val->intval = camellia_real_type(mc) == POWER_SUPPLY_REAL_TYPE_HVDCP;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = camellia_adc_now(mc, psp);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property camellia_mt_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static enum power_supply_property camellia_usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_REAL_TYPE,
	POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE,
};

static const struct power_supply_desc camellia_ac_desc = {
	.name = "ac",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.properties = camellia_mt_props,
	.num_properties = ARRAY_SIZE(camellia_mt_props),
	.get_property = camellia_mt_get_property,
};

static const struct power_supply_desc camellia_usb_desc = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB,
	.properties = camellia_usb_props,
	.num_properties = ARRAY_SIZE(camellia_usb_props),
	.get_property = camellia_mt_get_property,
};

static bool camellia_hvdcp(struct camellia_mt_charger *mc)
{
	return camellia_real_type(mc) == POWER_SUPPLY_REAL_TYPE_HVDCP;
}

static int camellia_apply_temp_level(struct camellia_mt_charger *mc,
				     unsigned long state, bool hvdcp);

static void camellia_mt_changed_work(struct work_struct *work)
{
	struct camellia_mt_charger *mc =
		container_of(work, struct camellia_mt_charger, changed_work);
	bool hvdcp = camellia_hvdcp(mc);

	power_supply_changed(mc->ac);
	power_supply_changed(mc->usb);

	mutex_lock(&mc->lock);
	if (mc->temp_level && hvdcp != mc->thermal_hvdcp)
		camellia_apply_temp_level(mc, mc->temp_level, hvdcp);
	mutex_unlock(&mc->lock);
}

static int camellia_mt_psy_notifier(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct camellia_mt_charger *mc = container_of(nb, struct camellia_mt_charger, nb);
	struct power_supply *psy = data;

	if (event == PSY_EVENT_PROP_CHANGED && psy->desc &&
	    !strcmp(psy->desc->name, CAMELLIA_CHG_PSY))
		schedule_work(&mc->changed_work);
	return NOTIFY_DONE;
}

static int camellia_cdev_get_max_state(struct thermal_cooling_device *cdev,
				       unsigned long *state)
{
	*state = ARRAY_SIZE(camellia_thermal_icl) - 1;
	return 0;
}

static int camellia_cdev_get_cur_state(struct thermal_cooling_device *cdev,
				       unsigned long *state)
{
	struct camellia_mt_charger *mc = cdev->devdata;

	*state = mc->temp_level;
	return 0;
}

static int camellia_apply_temp_level(struct camellia_mt_charger *mc,
				     unsigned long state, bool hvdcp)
{
	const int *icl = hvdcp ? camellia_thermal_icl_hvdcp : camellia_thermal_icl;
	union power_supply_propval val;
	struct power_supply *master;
	int ret;

	master = power_supply_get_by_name(CAMELLIA_MASTER_PSY);
	if (!master)
		return -ENODEV;
	val.intval = state ? icl[state] : CAMELLIA_UNLIMIT_CURRENT;
	ret = power_supply_set_property(master, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
	power_supply_put(master);
	if (ret)
		return ret;

	mc->temp_level = state;
	mc->thermal_hvdcp = hvdcp;
	dev_info(mc->dev, "system_temp_level:%lu thermal_icl_ua:%d hvdcp:%d\n", state,
		 state ? icl[state] : -1, hvdcp);
	return 0;
}

static int camellia_cdev_set_cur_state(struct thermal_cooling_device *cdev,
				       unsigned long state)
{
	struct camellia_mt_charger *mc = cdev->devdata;
	bool hvdcp = camellia_hvdcp(mc);
	int ret;

	if (state >= ARRAY_SIZE(camellia_thermal_icl))
		state = ARRAY_SIZE(camellia_thermal_icl) - 1;

	mutex_lock(&mc->lock);
	ret = camellia_apply_temp_level(mc, state, hvdcp);
	mutex_unlock(&mc->lock);
	return ret;
}

static const struct thermal_cooling_device_ops camellia_cdev_ops = {
	.get_max_state = camellia_cdev_get_max_state,
	.get_cur_state = camellia_cdev_get_cur_state,
	.set_cur_state = camellia_cdev_set_cur_state,
};

static int camellia_mt_charger_probe(struct platform_device *pdev)
{
	struct power_supply_config cfg = {};
	struct camellia_mt_charger *mc;
	int ret;

	mc = devm_kzalloc(&pdev->dev, sizeof(*mc), GFP_KERNEL);
	if (!mc)
		return -ENOMEM;
	mc->dev = &pdev->dev;
	mutex_init(&mc->lock);
	INIT_WORK(&mc->changed_work, camellia_mt_changed_work);
	platform_set_drvdata(pdev, mc);

	cfg.drv_data = mc;
	cfg.of_node = pdev->dev.of_node;
	mc->ac = devm_power_supply_register(&pdev->dev, &camellia_ac_desc, &cfg);
	if (IS_ERR(mc->ac))
		return PTR_ERR(mc->ac);
	mc->usb = devm_power_supply_register(&pdev->dev, &camellia_usb_desc, &cfg);
	if (IS_ERR(mc->usb))
		return PTR_ERR(mc->usb);

	mc->cdev = thermal_cooling_device_register("battery", mc, &camellia_cdev_ops);
	if (IS_ERR(mc->cdev))
		return PTR_ERR(mc->cdev);
	ret = camellia_thermal_message_init(mc->cdev);
	if (ret)
		dev_warn(&pdev->dev, "thermal_message: %d\n", ret);

	mc->nb.notifier_call = camellia_mt_psy_notifier;
	ret = power_supply_reg_notifier(&mc->nb);
	if (ret) {
		camellia_thermal_message_exit();
		thermal_cooling_device_unregister(mc->cdev);
		return ret;
	}
	schedule_work(&mc->changed_work);
	return 0;
}

static void camellia_mt_charger_remove(struct platform_device *pdev)
{
	struct camellia_mt_charger *mc = platform_get_drvdata(pdev);

	power_supply_unreg_notifier(&mc->nb);
	camellia_thermal_message_exit();
	thermal_cooling_device_unregister(mc->cdev);
	cancel_work_sync(&mc->changed_work);
	if (mc->chg)
		power_supply_put(mc->chg);
	if (mc->chg_dev)
		put_device(&mc->chg_dev->dev);
}

static const struct of_device_id camellia_mt_charger_of_match[] = {
	{ .compatible = "mediatek,mt-charger" },
	{ }
};
MODULE_DEVICE_TABLE(of, camellia_mt_charger_of_match);

static struct platform_driver camellia_mt_charger_driver = {
	.probe = camellia_mt_charger_probe,
	.remove_new = camellia_mt_charger_remove,
	.driver = {
		.name = "mt_charger",
		.of_match_table = camellia_mt_charger_of_match,
	},
};
module_platform_driver(camellia_mt_charger_driver);

MODULE_DESCRIPTION("camellia: 4.14 mt_charger ac/usb supplies, battery cooling device and thermal_message for stock userspace");
MODULE_LICENSE("GPL");
