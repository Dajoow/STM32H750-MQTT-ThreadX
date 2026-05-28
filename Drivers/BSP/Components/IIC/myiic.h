
#ifndef __MYIIC_H
#define __MYIIC_H

#include "main.h"

/* 引脚定义 */
#define IIC_SCL_GPIO_PORT           GPIOH
#define IIC_SCL_GPIO_PIN            GPIO_PIN_4
#define IIC_SCL_GPIO_CLK_ENABLE()   do { __HAL_RCC_GPIOH_CLK_ENABLE(); } while (0)
#define IIC_SDA_GPIO_PORT           GPIOH
#define IIC_SDA_GPIO_PIN            GPIO_PIN_5
#define IIC_SDA_GPIO_CLK_ENABLE()   do { __HAL_RCC_GPIOH_CLK_ENABLE(); } while (0)

/* IO操作 */
#define IIC_SCL(x)                  do { (x) ?                                                                  \
                                        HAL_GPIO_WritePin(IIC_SCL_GPIO_PORT, IIC_SCL_GPIO_PIN, GPIO_PIN_SET):   \
                                        HAL_GPIO_WritePin(IIC_SCL_GPIO_PORT, IIC_SCL_GPIO_PIN, GPIO_PIN_RESET); \
                                    } while (0)
#define IIC_SDA(x)                  do { (x) ?                                                                  \
                                        HAL_GPIO_WritePin(IIC_SDA_GPIO_PORT, IIC_SDA_GPIO_PIN, GPIO_PIN_SET):   \
                                        HAL_GPIO_WritePin(IIC_SDA_GPIO_PORT, IIC_SDA_GPIO_PIN, GPIO_PIN_RESET); \
                                    } while (0)
#define IIC_SDA_READ                ((HAL_GPIO_ReadPin(IIC_SDA_GPIO_PORT, IIC_SDA_GPIO_PIN) == GPIO_PIN_RESET) ? 0 : 1)

/* 函数声明 */
void iic_init(void);                /* 初始化IIC */
void iic_start(void);               /* 产生IIC起始信号 */
void iic_stop(void);                /* 产生IIC停止信号 */
uint8_t iic_wait_ack(void);         /* 等待IIC应答信号 */
void iic_ack(void);                 /* 产生IIC ACK信号 */
void iic_nack(void);                /* 产生IIC NACK信号 */
void iic_send_byte(uint8_t data);   /* IIC发送一个字节 */
uint8_t iic_read_byte(uint8_t ack); /* IIC读取一个字节 */

#endif
