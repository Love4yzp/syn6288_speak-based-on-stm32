/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-04-14     YZP       the first version
 */
#ifndef APPLICATIONS_USER_BUTTON_H_
#define APPLICATIONS_USER_BUTTON_H_
    #define Button
#ifdef Button
#ifdef __cplusplus
extern "C" {
#endif

#include <button.h>
#include <rtdevice.h>
#include <drv_common.h>
#include <drivers/pin.h>
#include "app.h"
void button_thread_init(void);// Include:LED、KEY
#ifdef __cplusplus
}
#endif
#endif
#endif /* APPLICATIONS_USER_BUTTON_H_ */
