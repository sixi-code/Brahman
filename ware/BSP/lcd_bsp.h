#ifndef __LCD_BSP_H__
#define __LCD_BSP_H__
#include "stm32f4xx.h"                  // Device header

#ifndef LCD_W
#define LCD_W 240   // 屏幕宽度 (与 lcd_bsp.c 内部定义保持一致)
#endif
#ifndef LCD_H
#define LCD_H 240   // 屏幕高度
#endif

void LCD_Init(void);//LCD初始化
void LCD_Address_Set(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2);
void LCD_Write_DMA(uint16_t *data, uint32_t pixel_count);
void LCD_Color_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t *color);

#endif
