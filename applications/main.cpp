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
#include "gbk2utf8.h"
// struct rt_semaphore ble_6288_sem; /* 蓝牙接收到数据后，syn6288打断语音 */
struct rt_mailbox ble_mb_6288; // 发送给6288
struct rt_mailbox ble_mb_oled; // OLED
struct rt_mailbox ble_mb_time; // Time

char mb_pool_6288[32];
char mb_pool_oled[32];
char mb_pool_time[32];
/**
 * @brief 蓝牙到语音的邮箱
 * @TODO 成功
 */
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

    result = rt_mb_init(&ble_mb_time,
                        "mb_time",                /* 名称是 mbt */
                        &mb_pool_time[0],         /* 邮箱用到的内存池是 mb_pool */
                        sizeof(mb_pool_time) / 4, /* 邮箱中的邮件数目，因为一封邮件占 4 字节 */
                        RT_IPC_FLAG_FIFO);        /* 采用 FIFO 方式进行线程等待 */
    if (result != RT_EOK)
    {
        rt_kprintf("init timebox failed.\n");
        return -1;
    }
    return RT_EOK;
}

int main(void)
{
    /* 系统初始化 */
    mailbox_init();

/* 蓝牙通信功能 */
#ifdef BLE
    bluetooth_init();
#endif

/* 显示功能 */
#ifdef Btn_OLED
    button_u8g2_init();
#endif
/* 语音合成功能 */
#ifdef SYN6288

    SYN6288_init();

    //    SYN_FrameInfo(0,"你好，我的朋友");

    // LOG_D("Hello RT-Thread!");

    // 接收到蓝牙数据，发送个 6288
// [v?] 音量0~16
// [m?] 背景音乐音量
// [t?] 语速
// [x?] 提示音
// [o?] 0:自然朗读
// [y?] 号码1的读法 ，一、幺
    char *synInitCMD_t = NULL;
    char synInitCMD[] = "[v16][t3][y1][x1]msga[x0]"; //默认为16级音量，
    utf82gbk(&synInitCMD_t, (void *)synInitCMD, strlen(synInitCMD));
    SYN_FrameInfo(0, synInitCMD_t);
    rt_free((void *)synInitCMD_t);
    while (1)
    {
        // char head[] = "[v16][t3]"; //控制头
        char head[] = ""; //控制头
        char *str;
        char *name = NULL;
        // /* 从邮箱中收取邮件 */
        if (rt_mb_recv(&ble_mb_6288, (rt_ubase_t *)&str, RT_WAITING_FOREVER) == RT_EOK)
        {
            strcat(head, str); //连接字符串 s2 到字符串 s1 的末尾。

            utf82gbk(&name, (void *)head, strlen(head));
            SYN_FrameInfo(0, name);
            // rt_kprintf(" 8266收到:%s\n", str);
            rt_free((void *)name);
        }
    }
    return RT_EOK;
}
#endif

/**
 * 状态：成功
 */
static void ble2oled_test(int argc, char *argv[])
{
    char rx_buffer[1024];

    if (argc == 2)
    {
        rt_strncpy(rx_buffer, argv[1], 1024);
        if (rt_mb_send(&ble_mb_oled, (rt_uint32_t)&rx_buffer) == RT_EOK)
        {
            // rt_kprintf("发给OLED邮箱成功\n");// 更改显示的TEXT
        }
    }
}
/* 导出到 msh 命令列表中 */
MSH_CMD_EXPORT(ble2oled_test, test upload);

/**
 * 状态 成功
 */
static void ble2syn_test(int argc, char *argv[])
{
    char rx_buffer[1024];

    if (argc == 2)
    {
        rt_strncpy(rx_buffer, argv[1], 1024);
        if (rt_mb_send(&ble_mb_6288, (rt_uint32_t)&rx_buffer) == RT_EOK)
        {
            // rt_kprintf("发给语音邮箱成功\n");
        }
    }
}
/* 导出到 msh 命令列表中 */
MSH_CMD_EXPORT(ble2syn_test, syn upload);
