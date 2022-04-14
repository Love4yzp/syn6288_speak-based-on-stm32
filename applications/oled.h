#ifndef __OLED_H
#define __OLED_H
    /* OLED 开关 */
    #define OLED
    #ifdef OLED
#ifdef __cplusplus
#include <U8g2lib.h>
extern "C" {
#endif
//#include <u8g2_port.h>

#define OLED_I2C_PIN_SCL                    22  // PB6
#define OLED_I2C_PIN_SDA                    23  // PB7
/* 函数区域 */
rt_err_t u8g2_init();
//void gui_init();
/* GUI 布局 */
//void time_set();
//void str_set();

#ifdef __cplusplus
}
#endif
    #endif
#endif
