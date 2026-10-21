/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __CAMELLIA_HBM_H
#define __CAMELLIA_HBM_H

enum backlight_hbm_mode {
	HBM_MODE_UN_SET,
	HBM_MODE_DEFAULT = 1,
	HBM_MODE_LEVEL1,
	HBM_MODE_LEVEL2,
	HBM_MODE_LEVEL_MAX
};

extern int camellia_bklic_id(void);

#endif
