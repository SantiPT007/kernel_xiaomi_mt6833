# SPDX-License-Identifier: GPL-2.0
subdir-ccflags-y += -I$(IMGSENSOR_DRIVER_PATH)/mt6853/camera_project/camellian/camera_hw
subdir-ccflags-y += -DCAMELLIA_SMARTLDO
imgsensor_isp6s_mon-objs += ../common/v1_1/camera_hw/regulator/regulator.o
imgsensor_isp6s_mon-objs += ../common/v1_1/camera_hw/gpio/gpio.o
imgsensor_isp6s_mon-objs += ../common/v1_1/camera_hw/mclk/mclk.o
imgsensor_isp6s_mon-objs += $(subdrv-rpath)/camera_hw/imgsensor_cfg_table.o
imgsensor_isp6s_mon-objs += $(subdrv-rpath)/camera_hw/smartldo/smartldo.o
