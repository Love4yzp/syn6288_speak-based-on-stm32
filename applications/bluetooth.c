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
// #define BSP_UART2_TX_PIN       "PB10"
// #define BSP_UART2_RX_PIN       "PB11"
#ifdef BLE

#define BLE_UART_NAME       "uart3"
static rt_device_t BLE_uart;
static struct rt_messagequeue rx_mq; // 消息队列
struct serial_configure ble_config = RT_SERIAL_CONFIG_DEFAULT;  /* 初始化配置参数 */


/* 接收数据回调函数 */
static rt_err_t uart_input(rt_device_t dev, rt_size_t size)
{
    struct rx_msg msg;
    rt_err_t result;
    msg.dev = dev;
    msg.size = size;

    result = rt_mq_send(&rx_mq, &msg, sizeof(msg)); // 接收到后发送邮箱
    if ( result == -RT_EFULL)
    {
        /* 消息队列满 */
        rt_kprintf("message queue full！\n");
    }
    return result;
}

static void serial_thread_entry(void *parameter)  // 对外接口
{
    struct rx_msg msg;// 局部邮箱
    rt_err_t result;
    rt_uint32_t rx_length;
    static char rx_buffer[RT_SERIAL_RB_BUFSZ + 1];

    while (1)
    {
        rt_memset(&msg, 0, sizeof(msg));// 清零
        /* 从消息队列中读取消息*/
        result = rt_mq_recv(&rx_mq, &msg, sizeof(msg), RT_WAITING_FOREVER);
        if (result == RT_EOK)
        {
            /* 从串口读取数据*/
            rx_length = rt_device_read(msg.dev, 0, rx_buffer, msg.size);
            rx_buffer[rx_length] = '\0';
            /* 通过串口设备 serial 输出读取到的消息 */
            rt_device_write(BLE_uart, 0, rx_buffer, rx_length);//发送给蓝牙
            // /* 打印数据 */
            // rt_kprintf("%s\n",rx_buffer); // 发送给电脑

            // 邮箱发送给 OLED,6288
            rt_mb_send(&ble_mb_6288, (rt_uint32_t)&rx_buffer);
            rt_mb_send(&ble_mb_oled, (rt_uint32_t)&rx_buffer);
        }
    }
}

rt_err_t bluetooth_init(void)
{
    rt_err_t ret = RT_EOK;
    static char msg_pool[256]; // 不会销毁

    /* 查找串口设备 */
    BLE_uart = rt_device_find(BLE_UART_NAME);
    if (!BLE_uart)
    {
        rt_kprintf("find %s failed!\n", BLE_UART_NAME);
        return RT_ERROR;
    }

    /* 初始化消息队列 */
    rt_mq_init(&rx_mq, "blue_rx_mq",
               msg_pool,                 /* 存放消息的缓冲区 */
               sizeof(struct rx_msg),    /* 一条消息的最大长度 */
               sizeof(msg_pool),         /* 存放消息的缓冲区大小 */
               RT_IPC_FLAG_FIFO);        /* 如果有多个线程等待，按照先来先得到的方法分配消息 */

    ble_config.baud_rate = BAUD_RATE_9600;        //修改波特率为 9600
    ble_config.bufsz     = 128;                   //64修改缓冲区 buff size 为 128
    rt_device_control(BLE_uart, RT_DEVICE_CTRL_CONFIG, &ble_config);/*控制9600参数*/

    /* 以 DMA 接收及轮询发送方式打开串口设备 */
    rt_device_open(BLE_uart, RT_DEVICE_FLAG_DMA_RX);
    /* 设置接收回调函数 */
    rt_device_set_rx_indicate(BLE_uart, uart_input);
    /* 发送字符串 */
    // rt_device_write(BLE_uart, 0, str, (sizeof(str) - 1));

    /* 创建 serial 线程 */
    rt_thread_t thread = rt_thread_create("bluetooth", serial_thread_entry, RT_NULL, 1024, 25, 10);
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
/* 导出到 msh 命令列表中 */
// MSH_CMD_EXPORT(uart_dma_sample, uart device dma sample);

#endif
