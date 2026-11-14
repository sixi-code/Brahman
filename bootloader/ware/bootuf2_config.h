/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef BOOTUF2_CONFIG_H
#define BOOTUF2_CONFIG_H

#define CONFIG_BOOTUF2_CACHE_SIZE         4096
#define CONFIG_BOOTUF2_SECTOR_SIZE        512
#define CONFIG_BOOTUF2_SECTOR_PER_CLUSTER 2
#define CONFIG_BOOTUF2_SECTOR_RESERVED    1
#define CONFIG_BOOTUF2_NUM_OF_FAT         2
#define CONFIG_BOOTUF2_ROOT_ENTRIES       64

/* APP 起始地址(Bootloader 占 0x08000000~0x0800FFFF 共 64KB)。
 * 集中在这里定义, 供 bootuf2.c 的地址校验与 chip_rom.c 的擦写保护共用。 */
#define CONFIG_BOOTUF2_APP_START     0x08010000UL

#define CONFIG_BOOTUF2_FAMILYID      0x57755a57
/* 可升级区上限: 1MB Flash 减去 Bootloader 的 64KB, 即 0xF0000 (960KB)。
 * 写成 0x100000 会让最后 64KB 被当成合法目标, 地址落到 Flash 之外。 */
#define CONFIG_BOOTUF2_FLASHMAX      0xF0000
#define CONFIG_BOOTUF2_PAGE_COUNTMAX 1024

#endif
