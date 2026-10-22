// SPDX-License-Identifier: GPL-2.0

#include <linux/cpufreq.h>
#include <linux/device.h>
#include <linux/mtk_disp_notify.h>
#include <linux/pm_qos.h>
#include <linux/thermal.h>

#include "camellia_thermal_message.h"

#define CAMELLIA_BIG_CPU	6

static struct device *tm_dev;
static atomic_t switch_mode = ATOMIC_INIT(-1);
static atomic_t temp_state = ATOMIC_INIT(0);
static atomic_t lighter_event = ATOMIC_INIT(0);
static int screen_state = 1;
static char boost_buf[128];
static char board_sensor_temp[128];
static const char *board_sensor = "VIRTUAL-SENSOR";
static struct freq_qos_request cpu_limit_req[2];
static DEFINE_MUTEX(cpu_limit_lock);

static ssize_t sconfig_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%d\n", atomic_read(&switch_mode));
}

static ssize_t sconfig_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t len)
{
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	atomic_set(&switch_mode, val);
	return len;
}
static DEVICE_ATTR_RW(sconfig);

static ssize_t boost_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%s", boost_buf);
}

static ssize_t boost_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t len)
{
	strscpy(boost_buf, buf, sizeof(boost_buf));
	return len;
}
static DEVICE_ATTR(boost, 0644, boost_show, boost_store);

static ssize_t temp_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%d\n", atomic_read(&temp_state));
}

static ssize_t temp_state_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t len)
{
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	atomic_set(&temp_state, val);
	return len;
}
static DEVICE_ATTR_RW(temp_state);

static ssize_t lighter_event_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%d\n", atomic_read(&lighter_event));
}

static ssize_t lighter_event_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t len)
{
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	atomic_set(&lighter_event, val);
	return len;
}
static DEVICE_ATTR_RW(lighter_event);

static ssize_t cpu_limits_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t cpu_limits_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct cpufreq_policy *policy;
	unsigned int cpu, max;
	int idx, ret = 0;

	if (sscanf(buf, "cpu%u %u", &cpu, &max) != 2)
		return -EINVAL;
	idx = cpu < CAMELLIA_BIG_CPU ? 0 : 1;
	if (!max || max == UINT_MAX)
		max = FREQ_QOS_MAX_DEFAULT_VALUE;

	mutex_lock(&cpu_limit_lock);
	if (!freq_qos_request_active(&cpu_limit_req[idx])) {
		policy = cpufreq_cpu_get(idx ? CAMELLIA_BIG_CPU : 0);
		if (!policy) {
			ret = -ENODEV;
			goto out;
		}
		ret = freq_qos_add_request(&policy->constraints, &cpu_limit_req[idx],
					   FREQ_QOS_MAX, max);
		cpufreq_cpu_put(policy);
	} else {
		ret = freq_qos_update_request(&cpu_limit_req[idx], max);
	}
out:
	mutex_unlock(&cpu_limit_lock);
	return ret < 0 ? ret : len;
}
static DEVICE_ATTR_RW(cpu_limits);

static ssize_t screen_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%d\n", READ_ONCE(screen_state));
}
static DEVICE_ATTR(screen_state, 0664, screen_state_show, NULL);

static ssize_t board_sensor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%s", board_sensor);
}
static DEVICE_ATTR(board_sensor, 0664, board_sensor_show, NULL);

static ssize_t board_sensor_temp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%s", board_sensor_temp);
}

static ssize_t board_sensor_temp_store(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t len)
{
	strscpy(board_sensor_temp, buf, sizeof(board_sensor_temp));
	return len;
}
static DEVICE_ATTR(board_sensor_temp, 0664, board_sensor_temp_show, board_sensor_temp_store);

static struct attribute *thermal_message_attrs[] = {
	&dev_attr_sconfig.attr,
	&dev_attr_boost.attr,
	&dev_attr_temp_state.attr,
	&dev_attr_lighter_event.attr,
	&dev_attr_cpu_limits.attr,
	&dev_attr_screen_state.attr,
	&dev_attr_board_sensor.attr,
	&dev_attr_board_sensor_temp.attr,
	NULL,
};
ATTRIBUTE_GROUPS(thermal_message);

static int screen_state_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	int *blank = data;

	if (event != MTK_DISP_EARLY_EVENT_BLANK || !blank)
		return NOTIFY_DONE;
	if (*blank == MTK_DISP_BLANK_POWERDOWN)
		WRITE_ONCE(screen_state, 0);
	else if (*blank == MTK_DISP_BLANK_UNBLANK)
		WRITE_ONCE(screen_state, 1);
	return NOTIFY_DONE;
}

static struct notifier_block screen_state_nb = {
	.notifier_call = screen_state_notifier,
};

int camellia_thermal_message_init(struct thermal_cooling_device *cdev)
{
	int ret;

	tm_dev = device_create_with_groups(cdev->device.class, NULL, MKDEV(0, 0), NULL,
					   thermal_message_groups, "thermal_message");
	if (IS_ERR(tm_dev)) {
		ret = PTR_ERR(tm_dev);
		tm_dev = NULL;
		return ret;
	}
	ret = mtk_disp_notifier_register("camellia_thermal_message", &screen_state_nb);
	if (ret)
		dev_warn(tm_dev, "no display notifier (%d), screen_state stays on\n", ret);
	return 0;
}

void camellia_thermal_message_exit(void)
{
	int i;

	if (!tm_dev)
		return;
	mtk_disp_notifier_unregister(&screen_state_nb);
	for (i = 0; i < ARRAY_SIZE(cpu_limit_req); i++)
		if (freq_qos_request_active(&cpu_limit_req[i]))
			freq_qos_remove_request(&cpu_limit_req[i]);
	device_unregister(tm_dev);
	tm_dev = NULL;
}
