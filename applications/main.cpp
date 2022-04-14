/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-04-12     RT-Thread    first version
 */

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include "app.h"

#include "oled.h"
#include "syn6288.h"

struct rt_semaphore ble_6288_sem; /* 蓝牙接收到数据后，syn6288打断语音 */
struct rt_mailbox ble_mb_6288;    // 发送给6288
struct rt_mailbox ble_mb_oled;    // OLED

char mb_pool_6288[128];
char mb_pool_oled[128];
// char mb_str1[] = "this is another mail!";
rt_err_t mailbox_init(void)
{
    rt_err_t result;

    /* 初始化静态 mailbox */
    result = rt_mb_init(&ble_mb_6288,
                        "mb_6288",                /* 名称是 mbt */
                        &mb_pool_6288[0],         /* 邮箱用到的内存池是 mb_pool */
                        sizeof(mb_pool_6288) / 4, /* 邮箱中的邮件数目，因为一封邮件占 4 字节 */
                        RT_IPC_FLAG_FIFO);        /* 采用 FIFO 方式进行线程等待 */
    if (result != RT_EOK)
    {
        rt_kprintf("init 6288mailbox failed.\n");
        return -1;
    }

    result = rt_mb_init(&ble_mb_oled,
                        "mb_oled",                /* 名称是 mbt */
                        &mb_pool_oled[0],         /* 邮箱用到的内存池是 mb_pool */
                        sizeof(mb_pool_oled) / 4, /* 邮箱中的邮件数目，因为一封邮件占 4 字节 */
                        RT_IPC_FLAG_FIFO);        /* 采用 FIFO 方式进行线程等待 */
    if (result != RT_EOK)
    {
        rt_kprintf("init oledmailbox failed.\n");
        return -1;
    }

    /* 初始化静态 1 个信号量  初始值是 0 */
    rt_sem_init(&ble_6288_sem, "lock", 0, RT_IPC_FLAG_PRIO);
    return RT_EOK;
}

int main(void)
{
    /* 系统初始化 */
    mailbox_init();

/* 蓝牙通信功能 */
#ifdef BLE
    // BLE_init(0,NULL);
#endif

/* 显示功能 */
#ifdef OLED
    u8g2_init();

#endif
/* 按键功能 */
#ifdef Button
    button_thread_init();
#endif
/* 语音合成功能 */
#ifdef SYN6288
    SYN6288_init();
    //    SYN_FrameInfo(0,"你好，我的朋友");

    // LOG_D("Hello RT-Thread!");

    // 接收到蓝牙数据，发送个 6288
    char *str;
    char head[] = "[v7][m1][t5]";
    while (1)
    {
        // /* 从邮箱中收取邮件 */
        // if (rt_mb_recv(&ble_mb_6288, (rt_ubase_t *)&str, RT_WAITING_FOREVER) == RT_EOK)
        // {
        //     // rt_kprintf("thread1: get a mail from mailbox, the content:%s\n", str);
        //     // if (str == mb_str3)
        //     //     break;

        //     // 组合 字符串
        //     /* 发送 str 到 语音 */
        //     strcat(head, str); //连接字符串 s2 到字符串 s1 的末尾。
        //     YS_SYN_Set(head, sizeof(head) - 1);
        //     rt_kprintf("蓝牙给8266的信息:%s\n", head);
        //     /* 延时 100ms */
        rt_thread_mdelay(100);
        // }
    }
    /* 执行邮箱对象脱离 */
#endif

    return RT_EOK;
}

static void ble2oled_test(int argc, char *argv[])
{
    char rx_buffer[1024];

    if (argc == 2)
    {
        rt_strncpy(rx_buffer, argv[1], 1024);
        if(rt_mb_send(&ble_mb_oled, (rt_uint32_t)&rx_buffer) == RT_EOK)
        {
            rt_kprintf("发送邮箱成功\n");
        }
    }
}
/* 导出到 msh 命令列表中 */
MSH_CMD_EXPORT(ble2oled_test, test upload);

static void ble2syn_test(int argc, char *argv[])
{
    char rx_buffer[1024];

    if (argc == 2)
    {
        rt_strncpy(rx_buffer, argv[1], 1024);
        if(rt_mb_send(&ble_mb_oled, (rt_uint32_t)&rx_buffer) == RT_EOK)
        {
            rt_kprintf("发送邮箱成功\n");
        }
    }
}
/* 导出到 msh 命令列表中 */
MSH_CMD_EXPORT(ble2syn_test, syn upload);