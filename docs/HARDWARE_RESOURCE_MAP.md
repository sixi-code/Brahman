# Brahman 硬件资源总表

> 芯片：STM32F405RGT6（LQFP64，1MB Flash / 128KB SRAM / 64KB CCM）
> 下列引脚、DMA 与中断分配均可与 `ware/BSP/*.c` 逐行对照。

---

## 一、引脚分配

LQFP64 共 51 个可用 I/O，本工程使用 47 个：

- 未用：**PA6**（SPI1_MISO，屏幕只写不读）与 **PC13**。
- **PC14 / PC15** 接 32.768 kHz 晶振供 LSE（RTC 时钟源，`rtc_clock.c:96-108`）。
- **PH0 / PH1** 接 8 MHz 晶振供 HSE（系统主时钟，PLL_M=8 折算到 168 MHz），不在下表中单列。

### GPIOA（15/16，PA6 未接）
| 引脚 | 功能 | 方向 | 复用 | 外设 |
|------|------|------|------|------|
| PA0 | WKUP 顶部按键 / UF2 模式判定 | 输入(下拉) | — | 顶部按键 |
| PA1 | USB CC2 检测 (ADC1_IN1) | 模拟输入 | — | Type-C |
| PA2 | USB CC1 检测 (ADC1_IN2) | 模拟输入 | — | Type-C |
| PA3 | 电池电压 2/3 分压 (ADC1_IN3) | 模拟输入 | — | 电池监测 |
| PA4 | LCD_DC 数据/命令选择 | 输出 | — | LCD |
| PA5 | SPI1_SCK | 复用 AF5 | SPI1 | LCD |
| PA6 | SPI1_MISO（未接，屏幕只写不读） | — | — | — |
| PA7 | SPI1_MOSI | 复用 AF5 | SPI1 | LCD |
| PA8 | BAT 充电检测（0=充电中） | 输入 | — | 电源管理 |
| PA9 | HDP_EN 耳放供电 | 输出 | — | 音频 |
| PA10 | USB_CTRL 主/从切换（0从 1主） | 输出 | — | USB |
| PA11 | PWD_EN 不断电控制 | 输出 | — | 电源管理 |
| PA12 | USB_EN 对外供电 | 输出 | — | USB |
| PA13 | SWDIO | 调试 | — | SWD |
| PA14 | SWCLK | 调试 | — | SWD |
| PA15 | SPI3_NSS (W25Q128 片选) | 输出 | — | SPI Flash |

### GPIOB（16/16）
| 引脚 | 功能 | 方向 | 复用 | 外设 |
|------|------|------|------|------|
| PB0 | 左摇杆 X (ADC1_IN8) | 模拟输入 | — | 摇杆 |
| PB1 | 左摇杆 Y (ADC1_IN9) | 模拟输入 | — | 摇杆 |
| PB2 | 左摇杆中键 | 输入(上拉) | — | 按键 |
| PB3 | SPI3_SCK | 复用 AF6 | SPI3 | W25Q128 |
| PB4 | SPI3_MISO | 复用 AF6 | SPI3 | W25Q128 |
| PB5 | SPI3_MOSI | 复用 AF6 | SPI3 | W25Q128 |
| PB6 | ES9018 IIC_SCL | 开漏输出 | — | DAC |
| PB7 | ES9018 IIC_SDA | 开漏输出 | — | DAC |
| PB8 | SDIO_CD TF卡检测 | 输入(上拉) | — | TF 卡 |
| PB9 | I2S2_WS | 复用 AF5 | I2S2 | 音频 |
| PB10 | I2S2_CK | 复用 AF5 | I2S2 | 音频 |
| PB11 | I2S_CTRL 耳机/喇叭切换 | 输出 | — | 音频 |
| PB12 | LCD_RST 复位 | 输出 | — | LCD |
| PB13 | VBUS 检测 | 输入 | — | USB 供电 |
| PB14 | USB_HS_DM | 复用 AF14 | OTG_HS FS | USB |
| PB15 | USB_HS_DP | 复用 AF14 | OTG_HS FS | USB |

### GPIOC（13/16，PC13-15 未用）
| 引脚 | 功能 | 方向 | 复用 | 外设 |
|------|------|------|------|------|
| PC0 | 右摇杆 X (ADC1_IN10) | 模拟输入 | — | 摇杆 |
| PC1 | 右摇杆 Y (ADC1_IN11) | 模拟输入 | — | 摇杆 |
| PC2 | 右摇杆中键 | 输入(上拉) | — | 按键 |
| PC3 | I2S2_SD 音频数据 | 复用 AF5 | I2S2 | 音频 |
| PC4 | ES9018 DPLL Lock 状态 | 输入 | — | DAC |
| PC5 | HDP_IN 耳机插入 / ES9018 automute | 输入 | — | 音频/DAC |
| PC6 | LCD_BLK 背光 PWM | 复用 AF1 | TIM8_CH1 | LCD 背光 |
| PC7 | SND_EN 喇叭供电 | 输出 | — | 音频 |
| PC8 | SDIO_D0 | 复用 AF12 | SDIO | TF 卡 |
| PC9 | SDIO_D1 | 复用 AF12 | SDIO | TF 卡 |
| PC10 | SDIO_D2 | 复用 AF12 | SDIO | TF 卡 |
| PC11 | SDIO_D3 | 复用 AF12 | SDIO | TF 卡 |
| PC12 | SDIO_CK | 复用 AF12 | SDIO | TF 卡 |
| PC13 | 未用 | — | — | — |
| PC14 | OSC32_IN（LSE 晶振） | — | LSE | RTC |
| PC15 | OSC32_OUT（LSE 晶振） | — | LSE | RTC |

### GPIOD（1/16）
| 引脚 | 功能 | 方向 | 复用 | 外设 |
|------|------|------|------|------|
| PD2 | SDIO_CMD | 复用 AF12 | SDIO | TF 卡 |

---

## 二、外设与总线汇总

| 外设 | 总线/接口 | 关键引脚 | DMA | 驱动文件 |
|------|-----------|----------|-----|----------|
| LCD 1.54" IPS 240×240 | SPI1 + DMA2 | PA5(SCK) PA7(MOSI) PA4(DC) PB12(RST) PC6(PWM) | DMA2 S5 Ch3 | lcd_bsp.c / lcd_pwm.c |
| W25Q128 16MB | SPI3 + DMA1 | PA15(NSS) PB3/4/5 | DMA1 S5/0 Ch0 | w25q128.c |
| TF 卡 (SDIO 4-bit) | SDIO + DMA2 | PC8-11 D0-3 PC12 CK PD2 CMD PB8 CD | DMA2 S3 Ch4 | sdio_sdcard.c |
| ES9018K2M DAC | 软件 I2C | PB6 SCL PB7 SDA PC4 DPLL PC5 automute | — | es9018k2m.c |
| I2S2 音频输出 | I2S2(SPI2) + DMA1 | PB9 WS PB10 CK PC3 SD | DMA1 S4 Ch0 | i2s.c |
| MAX98357A 功放 | GPIO | PC7 SND_EN PB11 I2S_CTRL PA9 HDP_EN | — | max98357A.c / pin_ctrl.c |
| 双摇杆 (ADC1) | ADC1 + DMA2 | PB0/1 LX/LY PC0/1 RX/RY | DMA2 S0 Ch0 | adc.c |
| Type-C/USB | ADC1 + OTG_HS | PA1/2 CC1/2 PA3 vbat PA10/12 控制 PB14/15 DM/DP | — | adc.c / usb_glue_st.c |
| 电源/充电 | GPIO | PA8 BAT PB13 VBUS PA11 PWD_EN PA12 USB_EN | — | pin_ctrl.c |
| 按键 | GPIO | PA0 WKUP PB2/PC2 摇杆中键 | — | key.c |
| 定时器 | TIM8 | PC6 PWM | — | lcd_pwm.c |
| RTC | LSE 外部晶振 | — | — | rtc_clock.c |

---

## 三、DMA 分配

| 外设 | DMA | 流 | 通道 | 方向 | 优先级 |
|------|-----|----|------|------|--------|
| LCD (SPI1_TX) | DMA2 | Stream5 | Ch3 | M→P | 高 |
| W25Q128 (SPI3_TX/RX) | DMA1 | Stream5/0 | Ch0 | M↔P | 高 |
| ADC1 (8ch) | DMA2 | Stream0 | Ch0 | P→M | 低 |
| I2S2_TX | DMA1 | Stream4 | Ch0 | M→P | 非常高 |
| SDIO | DMA2 | Stream3 | Ch4 | 双向 | 非常高 |

---

## 四、中断优先级

| 中断源 | 抢占优先级 | 说明 |
|--------|------------|------|
| SysTick | 6 | FreeRTOS tick（`nvic_conf.c:19`） |
| TIM1_BRK_TIM9 | 15 | CPU 占用统计时基 |
| DMA2_S0 (ADC) | 14 | ADC DMA 完成 |
| DMA2_S5 (LCD) | 13 | LCD DMA 完成 |
| DMA1_S0 / S5 (W25Q) | 12 | SPI3 RX / TX DMA 完成 |
| DMA2_S3 (SDIO) | 11 | TF 卡 DMA 完成 |
| SDIO | 10 | TF 卡中断 |
| OTG_HS | 9 | USB 主从 |
| DMA1_S4 (I2S2) | 8 | 音频 DMA |

未列出的通道在 `Nvic_Init()` 里统一置 0xFF（最低优先级）。

> **规则**：抢占优先级 **5–15** 才可调用 `FromISR` API（`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5`）；0–4 严禁调用，否则会命中 `configASSERT` 并把中断永久关掉。

---

## 五、存储器映射

| 区域 | 起始地址 | 大小 | 用途 |
|------|----------|------|------|
| Bootloader | 0x08000000 | 64 KB | UF2 引导、镜像 CRC 校验 |
| Flash IROM (APP) | 0x08010000 | 960 KB | 主程序代码 + RO 数据 |
| SRAM | 0x20000000 | 128 KB | 全局变量、任务栈、BSC 池 97 KB（DMA 可达） |
| CCM RAM | 0x10000000 | 64 KB | 2 × 864 B TLSF 控制块（0x10000000 / 0x10000360）+ CCM 池约 62 KB（DMA 不可达） |
| W25Q128 | — | 16 MB | 0–4 MB KVDB / 4–8 MB TSDB / 8–15 MB 字库 |

> 启动地址：0x08000000 (Bootloader) → 跳转 0x08010000 (主程序，`SCB->VTOR` 重定向)。
> 分区常量见 `ware/lib/TLSF/malloc.c:16-25`（池大小、控制块地址）与 `ware/lib/FlashDB/fal.c:52-69`（KVDB / TSDB 各 4 MB）。

---

## 六、FreeRTOS 资源

| 类型 | 名称 | 用途 |
|------|------|------|
| Mutex | `xFlashMutex` | W25Q128 互斥 |
| CountingSem | `xFlashSemaphore` | W25Q128 DMA 完成（计数上限 2） |
| BinarySem | `xI2SSemaphore` | I2S DMA 半/全传输同步 |
| EventGroup | `xLcdEventGroup` | LCD DMA 多任务同步 |
| Mutex | `xFDBSemaphore` | FlashDB KVDB / TSDB 互斥 |
| BinarySem | `xTaskManagerSemaphore` | 任务管理器请求通知 |
| Task | 8 个业务任务 | basic / lvgl / usb / music / media / game / font / fileop |

另有 `Task_Manager`（prio 7）与 `Start_Task`（prio 8）两个管理任务，不计入业务任务。
堆不用 FreeRTOS heap：`configTOTAL_HEAP_SIZE = 0`，`pvPortMalloc` 被 `#define` 成 `malloc_ccm`（`FreeRTOSConfig.h:70`）。

---

## 七、关键硬件说明

1. **LCD**：240×240 IPS，SPI1 16 bit 数据宽度（`SPI_DataSize_16b`，8 bit 命令/参数用 `F8T16` 填另一字节），DMA2_Stream5 发送，PC6 + TIM8_CH1 背光 PWM（PSC 60 / ARR 1400，约 1.96 kHz）。初始化序列为 ST77xx 兼容（0xB2 porch / 0xC6 帧率 / 0xD0 电源 / 0xE0-E1 伽马）。
2. **音频链**：各格式解码器按源位深调用 `I2S2_Init()`（16b / 16b 扩展 / 24b / 32b，Philips 标准、主机发送、CPOL Low，无 MCLK 输出）→ ES9018K2M（软件 I2C 配置）→ MAX98357A 或耳机，PB11 切通道、PA9/C7 分别控耳放与喇叭供电。
3. **Type-C**：双角色（DRP），ADC 采样 CC1（PA2）/ CC2（PA1）判断角色；PA10 输出 0 为从、1 为主，PA12 对外供电，PA11 不断电维持。
4. **SPI 复用隔离**：SPI1(LCD)、SPI2(I2S2)、SPI3(W25Q128) 三条总线互不共用引脚。
5. **DMA1 共享**：W25Q128（S5 TX / S0 RX）与 I2S2（S4 TX）共用 DMA1，靠流优先级（I2S 非常高 / W25Q 高）与 I2S 双缓冲隔离。
6. **CCM 只放 CPU 侧对象**：`configTOTAL_HEAP_SIZE=0`，`pvPortMalloc` 被 `#define` 成 `malloc_ccm`；DMA 缓冲区必须走 `malloc_bsc()`，因为 CCM 不挂在 DMA 总线矩阵上。
7. **Bootloader**：驻留 0x08000000–0x08010000（64 KB），负责 UF2 升级、三重向量校验与整镜像 CRC，升级标记存 RTC 备份寄存器。