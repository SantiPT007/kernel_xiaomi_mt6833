# SPDX-License-Identifier: GPL-2.0
ccflags-y += -I$(srctree)/$(src) -I$(srctree)/$(src)/include -I$(srctree)/$(src)/focaltech_test
ccflags-y += -DCONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
obj-m += camellia-ft8722-ts.o
camellia-ft8722-ts-objs := focaltech_core.o focaltech_ex_fun.o focaltech_ex_mode.o \
	focaltech_gesture.o focaltech_esdcheck.o focaltech_point_report_check.o \
	focaltech_flash.o focaltech_spi.o \
	focaltech_test/focaltech_test.o focaltech_test/focaltech_test_ini.o \
	focaltech_test/supported_ic/focaltech_test_ft8722.o
CFLAGS_focaltech_test/focaltech_test.o += -Wframe-larger-than=4608
