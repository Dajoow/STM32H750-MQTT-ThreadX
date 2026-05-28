#include "dht11.h"
#include "dwt_delay.h"

static void dht11_reset(void)
{
    DHT11_DQ(0);
    DWT_Delay_ms(20);    // 使用 DWT 延时
    DHT11_DQ(1);
    DWT_Delay_us(30);    // 使用 DWT 延时
}

uint8_t dht11_check(void)
{
    uint8_t retry = 0;
    
    while ((DHT11_DQ_READ != 0) && (retry < 100))
    {
        retry++;
        DWT_Delay_us(1); // 使用 DWT 延时
    }
    if (retry >= 100) return 1;
    
    retry = 0;
    while ((DHT11_DQ_READ == 0) && (retry < 100))
    {
        retry++;
        DWT_Delay_us(1); // 使用 DWT 延时
    }
    if (retry >= 100) return 1;
    
    return 0;
}

static uint8_t dht11_read_bit(void)
{
    uint8_t retry = 0;
    while ((DHT11_DQ_READ != 0) && (retry < 100))
    {
        retry++;
        DWT_Delay_us(1);
    }
    
    retry = 0;
    while ((DHT11_DQ_READ == 0) && (retry < 100))
    {
        retry++;
        DWT_Delay_us(1);
    }
    
    DWT_Delay_us(40); // 核心延时：判断高电平长度
    
    return DHT11_DQ_READ;
}

static uint8_t dht11_read_byte(void)
{
    uint8_t i, byte = 0;
    for (i = 0; i < 8; i++)
    {
        byte <<= 1;
        byte |= dht11_read_bit();
    }
    return byte;
}

uint8_t dht11_init(void)
{
    GPIO_InitTypeDef gpio_init_struct = {0};
    
    DHT11_DQ_GPIO_CLK_ENABLE();
    
    gpio_init_struct.Pin = DTH11_Pin;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_OD; // 开漏输出，无需切换方向
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DTH11_GPIO_Port, &gpio_init_struct);
    
    dht11_reset();
    return dht11_check();
}

uint8_t dht11_read_data(uint8_t *temp, uint8_t *humi)
{
    uint8_t i, buf[5];
    
    dht11_reset();
    if (dht11_check() == 0)
    {
        for (i = 0; i < 5; i++)
        {
            buf[i] = dht11_read_byte();
        }
        if ((buf[0] + buf[1] + buf[2] + buf[3]) == buf[4])
        {
            *humi = buf[0];
            *temp = buf[2];
            return 0;
        }
    }
    return 1;
}