/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-04-14     YZP       the first version
 */
#ifndef APPLICATIONS_BLUETOOTH_H_
#define APPLICATIONS_BLUETOOTH_H_
    /* BLE 开关 */
    #define BLE
#ifdef BLE
#ifdef __cplusplus
extern "C" {
#endif
#include <rtthread.h>
#include <rtdevice.h>
#include <stdlib.h>
#include "app.h"
/* 串口接收消息结构*/
struct rx_msg
{
    rt_device_t dev;
    rt_size_t size;
};

rt_err_t bluetooth_init(void);
void ble_send(char* ble_str);
#ifdef __cplusplus
}
#endif
#endif
#endif /* APPLICATIONS_BLUETOOTH_H_ */
