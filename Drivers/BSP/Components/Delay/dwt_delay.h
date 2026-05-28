#ifndef __DWT_DELAY_H
#define __DWT_DELAY_H

#include "main.h"

/* DWT 延时初始化 */
void DWT_Init(void);

/* 微秒与毫秒延时函数 */
void DWT_Delay_us(uint32_t us);
void DWT_Delay_ms(uint32_t ms);

#endif