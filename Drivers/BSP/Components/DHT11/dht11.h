#ifndef __DHT11_H
#define __DHT11_H

#include "main.h"

/* DHT11 引脚定义 (默认为 PG9，请根据实际原理图核对) */
#define DHT11_DQ_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOG_CLK_ENABLE(); }while(0)

/* IO操作函数宏 */
#define DHT11_DQ(x)     do{ x ? \
                            HAL_GPIO_WritePin(DTH11_GPIO_Port, DTH11_Pin, GPIO_PIN_SET) : \
                            HAL_GPIO_WritePin(DTH11_GPIO_Port, DTH11_Pin, GPIO_PIN_RESET); \
                        }while(0)

#define DHT11_DQ_READ   HAL_GPIO_ReadPin(DTH11_GPIO_Port, DTH11_Pin)

/* 函数声明 */
uint8_t dht11_init(void);
uint8_t dht11_check(void);
uint8_t dht11_read_data(uint8_t *temp, uint8_t *humi);

#endif