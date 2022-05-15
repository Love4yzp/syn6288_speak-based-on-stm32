#ifndef __MAIN
#define __MAIN
#ifdef __cplusplus
extern "C" {
#endif

//#include <rtdbg.h>
#include <rtthread.h>
#include <rtdevice.h>
#include <stdlib.h>
#include <rthw.h>
#include <rtdef.h>
#include "bluetooth.h"


/* 信号量 */
void sem_init(void);

extern struct rt_semaphore ble_6288_sem; /* 蓝牙接收到数据后，syn6288打断语音 */

/* 邮箱 三个箭头 */
/* 按键直接用全局变量邮箱oled 要记住当前的滚动状态 */
extern struct rt_mailbox ble_mb_6288; // 发送给6288
extern struct rt_mailbox ble_mb_oled; // OLED
extern struct rt_mailbox ble_mb_time; // Time

rt_err_t mailbox_init(void);


#ifdef __cplusplus
}
#endif

#endif
