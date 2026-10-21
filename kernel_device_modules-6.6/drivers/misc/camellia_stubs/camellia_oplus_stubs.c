// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>

unsigned int get_eng_version(void)
{
	return 0;
}
EXPORT_SYMBOL(get_eng_version);

unsigned int is_project(int project)
{
	return 0;
}
EXPORT_SYMBOL(is_project);

unsigned int get_PCB_Version(void)
{
	return 0;
}
EXPORT_SYMBOL(get_PCB_Version);

int register_device_proc_for_emmc(char *emmc, char *emmc_fw, void *mmc)
{
	return 0;
}
EXPORT_SYMBOL(register_device_proc_for_emmc);

MODULE_DESCRIPTION("camellia: neutral stubs for unpublished Oppo helper symbols");
MODULE_LICENSE("GPL");
