/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-04-14     YZP       the first version
 */

#include "bluetooth.h"
// #define BSP_UART3_TX_PIN       "PB10"
// #define BSP_UART3_RX_PIN       "PB11"
#ifdef BLE

#define BLE_UART_NAME "uart3"
#define DATA_CMD_END 'a'    /* 结束位设置为 \r，即回车符 */
#define ONE_DATA_MAXLEN 30+1 /* 不定长数据的最大长度 3个字节一个中文+'\0' 传递的长度！*/

/* 用于接收消息的信号量 */
static struct rt_semaphore ble_rxsem;
static rt_device_t ble_serial;

/* 接收数据回调函数 */
static rt_err_t uart_rx_ind(rt_device_t dev, rt_size_t size)
{
    /* 串口接收到数据后产生中断，调用此回调函数，然后发送接收信号量 */
    if (size > 0)
    {
        rt_sem_release(&ble_rxsem);
    }
    return RT_EOK;
}

static char uart_sample_get_char(void)
{
    char ch;

    while (rt_device_read(ble_serial, 0, &ch, 1) == 0)
    {
        rt_sem_control(&ble_rxsem, RT_IPC_CMD_RESET, RT_NULL);
        rt_sem_take(&ble_rxsem, RT_WAITING_FOREVER);
    }
    return ch;
}

/* 数据解析线程 */
static void data_parsing(void)
{

    char ch;
    char _data_[ONE_DATA_MAXLEN];
    static char i = 0;

    while (1)
    {
        ch = uart_sample_get_char();
        rt_device_write(ble_serial, 0, &ch, 1); // 回传
        if (ch == DATA_CMD_END)
        {
            _data_[i++] = '\0';

    // "我是你爸爸的爸爸的"// 28长度 28/9=3 3个字节为一个字符，最后为'\0'
            // rt_kprintf("蓝牙data=%s", _data_); // DONE 发送给电脑
            // DONE 邮箱发送给 OLED,6288  // 对外接口
            if(_data_[0]!='T')
            {
                rt_mb_send(&ble_mb_6288, (rt_uint32_t)&_data_);
                rt_mb_send(&ble_mb_oled, (rt_uint32_t)&_data_);
            }else // 传递的是时间
            {
                rt_mb_send(&ble_mb_oled, (rt_uint32_t)&_data_);
            }



            i = 0;
            continue;
        }
        i = (i >= ONE_DATA_MAXLEN - 1) ? ONE_DATA_MAXLEN - 1 : i;
        _data_[i++] = ch;
    }
}

rt_err_t bluetooth_init(void)
{
    rt_err_t ret = RT_EOK;

    char str[] = "BLE:10个字符!\n";

    /* 查找系统中的串口设备 */
    ble_serial = rt_device_find(BLE_UART_NAME);
    if (!ble_serial)
    {
        rt_kprintf("find %s failed!\n", BLE_UART_NAME);
        return RT_ERROR;
    }

    /* 初始化信号量 */
    rt_sem_init(&ble_rxsem, "ble_sem", 0, RT_IPC_FLAG_FIFO);
    /* 以中断接收及轮询发送模式打开串口设备 */
    rt_device_open(ble_serial, RT_DEVICE_FLAG_INT_RX);
    /* 设置接收回调函数 */
    rt_device_set_rx_indicate(ble_serial, uart_rx_ind);
    /* 发送字符串 */
    rt_device_write(ble_serial, 0, str, (sizeof(str) - 1));

    /* 创建 serial 线程 */
    rt_thread_t thread = rt_thread_create("ble", (void (*)(void *parameter))data_parsing, RT_NULL, 1024, 25, 10);
    /* 创建成功则启动线程 */
    if (thread != RT_NULL)
    {
        rt_thread_startup(thread);
    }
    else
    {
        ret = RT_ERROR;
    }

    return ret;
}

#endif
