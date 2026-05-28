#include "dwt_delay.h"

/**
 * @brief 初始化 DWT 周期计数器
 */
void DWT_Init(void) 
{
    /* 1. 使能 TRCENA (Trace enable) */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    
    /* 2. Cortex-M7 需要解锁 DWT 的 LAR 寄存器 */
    DWT->LAR = 0xC5ACCE55;
    
    /* 3. 清零循环计数器 */
    DWT->CYCCNT = 0;
    
    /* 4. 启动 DWT 循环计数器 */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * @brief 微秒级精准延时
 */
void DWT_Delay_us(uint32_t us) 
{
    /* 计算需要的 CPU 周期数 (SystemCoreClock 通常为 480000000) */
    uint32_t sys_clk = HAL_RCC_GetSysClockFreq(); 
    uint32_t ticks = us * (sys_clk / 1000000);
    uint32_t start_tick = DWT->CYCCNT;
    
    /* 等待周期耗尽 (利用无符号数回绕特性，无需担心溢出) */
    while ((DWT->CYCCNT - start_tick) < ticks);
}

/**
 * @brief 毫秒级精准延时
 */
void DWT_Delay_ms(uint32_t ms) 
{
    uint32_t sys_clk = HAL_RCC_GetSysClockFreq();
    uint32_t ticks = ms * (sys_clk / 1000);
    uint32_t start_tick = DWT->CYCCNT;
    
    while ((DWT->CYCCNT - start_tick) < ticks);
}