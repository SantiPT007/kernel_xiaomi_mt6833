# SPDX-License-Identifier: GPL-2.0
ccflags-y += -I$(srctree)/$(src) -I$(srctree)/$(src)/../camellia_ft8722
ccflags-y += -DCONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
obj-m += camellia-nt36672c-ts.o
camellia-nt36672c-ts-objs := nt36xxx.o nt36xxx_fw_update.o nt36xxx_ext_proc.o nt36xxx_mp_ctrlram.o
CFLAGS_nt36xxx_mp_ctrlram.o += -Wframe-larger-than=4608
