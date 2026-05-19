# STM32H750 + ThreadX + NetX Duo LED 卡死问题分析

## 现象

工程 Rebuild/Download 后运行，LED 只闪几下后停止。  
调试 Watch 中可见：

- `blink_thread_count` 只增加到 3 左右
- `nx_app_init_step = 7`
- `nx_driver_debug_step` 后续定位到 `HAL_ETH_Start_IT(&eth_handle)` 附近
- 最终 LED 线程不再继续调度，表现为系统像“卡死”

## 初步排查

### 1. ThreadX 本身正常

关闭或延后 NetX 初始化时，LED 线程可以正常闪烁，说明：

- ThreadX 内核启动正常
- SysTick / ThreadX tick 正常
- LED GPIO 控制正常
- blink thread 本身没有问题

因此问题不是 LED 线程，也不是 ThreadX 基础调度失败。

### 2. NetX 初始化卡在 ETH driver

Watch 中显示：

```c
nx_app_init_step = 7
    
```

nx_ip_create(...)
nx_ip_create() 

会调用 STM32 Ethernet driver，因此继续在 driver 内部加调试变量。

后续定位到：

NX_LINK_ENABLE -> _nx_driver_enable() -> _nx_driver_hardware_enable()
也就是 ETH driver 启用阶段。

3. DMA 描述符对齐已排除
加入 DMA descriptor 地址和对齐检查后，Watch 显示：

eth_dma_rx_desc_addr   = 0x24000000
eth_dma_tx_desc_addr   = 0x24000060
eth_dma_rx_desc_align  = 0
eth_dma_tx_desc_align  = 0
说明：

Rx DMA descriptor 是 32 字节对齐
Tx DMA descriptor 是 32 字节对齐
Rx/Tx descriptor 没有重叠
descriptor 地址位于 AXI SRAM，ETH DMA 可访问
因此“DMA 描述符未 32 字节对齐”不是本次卡死的根因。

最终定位
继续加 Fault 调试变量后，Watch 显示：

fault_debug_handler = 1
fault_debug_cfsr    = 0x400
fault_debug_hfsr    = 0x40000000
含义：

fault_debug_handler = 1 表示进入了 HardFault_Handler
CFSR = 0x400 对应 BusFault 中的 IMPRECISERR
说明程序不是普通死循环，而是触发了异常
同时 nx_driver_debug_rx_alloc_step = 5，对应位置在：

HAL_ETH_RxAllocateCallback()
中的这句附近：

invalidate_cache_by_addr(...)
也就是说，程序在 ETH 启动过程中，为 Rx descriptor 分配 packet buffer 时，执行 DCache invalidate 操作触发了 HardFault。

根因
STM32H750 是 Cortex-M7，芯片本身存在 DCache，因此宏：

__DCACHE_PRESENT == 1
成立。

ST 的 NetX Duo Ethernet driver 中原逻辑大致是：

#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
#define invalidate_cache_by_addr(...) SCB_InvalidateDCache_by_Addr(...)
#define clean_cache_by_addr(...)      SCB_CleanDCache_by_Addr(...)
#endif
问题是：
__DCACHE_PRESENT == 1 只表示“芯片有 DCache”，并不表示“当前工程已经启用了 DCache”。

当前工程里没有调用：

SCB_EnableDCache();
也就是说 DCache 实际没有开启，但 driver 仍然调用了：

SCB_InvalidateDCache_by_Addr(...)
最终在 ETH Rx buffer 分配流程中触发 BusFault/HardFault，导致 LED 线程停止调度。

修复思路
不要只根据 __DCACHE_PRESENT 判断是否执行 cache 维护操作，而应先判断 DCache 是否真的开启。

使用：

SCB->CCR & SCB_CCR_DC_Msk
判断 DCache 当前是否处于 Enable 状态。

修复逻辑：

if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
{
    SCB_InvalidateDCache_by_Addr(...);
}
CleanDCache 同理：

if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
{
    SCB_CleanDCache_by_Addr(...);
}
修复后结果
修复后重新 Rebuild/Download，再运行：

LED 正常闪烁
不再进入 HardFault
HAL_ETH_Start_IT() 不再因为 DCache invalidate 卡死
ThreadX 调度恢复正常
结论
本次问题的根因不是：

LED 线程问题
ThreadX 内存池不足
NetX packet pool 分配失败
ETH DMA descriptor 未 32 字节对齐
PHY link 不通
真正原因是：

STM32H750 芯片存在 DCache，但工程没有启用 DCache；ST NetX Duo ETH driver 仍然无条件执行 DCache invalidate/clean，导致 HAL_ETH_Start_IT() 中 Rx buffer 分配阶段触发 HardFault。

最终修复方式是：

在执行 SCB_InvalidateDCache_by_Addr() / SCB_CleanDCache_by_Addr() 前，先判断 DCache 是否真的开启。