# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2025 OpenWrt.org
#

define Device/softbank_air5
  $(call Device/FitImageLzma)
  DEVICE_VENDOR := SoftBank
  DEVICE_MODEL := Air 5
  SOC := ipq8072
  DEVICE_DTS := ipq8072-softbank-air5
  DEVICE_DTS_CONFIG := config@hk09
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  KERNEL_SIZE := 12288k
  KERNEL := kernel-bin | gzip | fit gzip $$(KDIR)/image-$$(DEVICE_DTS).dtb
  IMAGES := sysupgrade.bin factory.bin
  IMAGE/sysupgrade.bin := append-kernel | pad-to 128k | append-rootfs | pad-rootfs | append-metadata
  IMAGE/factory.bin := append-kernel | pad-to $$(KERNEL_SIZE) | append-rootfs | pad-rootfs
endef
TARGET_DEVICES += softbank_air5
