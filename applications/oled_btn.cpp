#include "oled.h"
#include "app.h"
#include "user_button.h"
#ifdef Button
static void BSP_Init(void);
/**
 * @brief 按下一次静态，按下两次动态！
 */

static void led_on(void);
static void led_off(void);
static void led_toggle(void);

#define KEY0 GET_PIN(C, 1)
#define LED0 GET_PIN(A, 8)

// rt_uint8_t LED1 = GET_PIN(D, 2)// PD2

Button_t u8g2_Button;
static char mb_str1[] = "once";//单击
static char mb_str2[] = "double";//双击

void Btn1_Dowm_CallBack(void *btn)// 对外接口
{
  led_off();
  rt_kprintf("KEY1 Click!");
}

void Btn1_Double_CallBack(void *btn) // 对外接口
{
  led_on();
  rt_kprintf("KEY1 Double click!");
}

uint8_t Read_u8g2b_Level(void)
{
  return rt_pin_read(KEY0);
}
void button_thread_init(void)
{
  BSP_Init();
  Button_Create("KEY1",
                &u8g2_Button,
                Read_u8g2b_Level,
                PIN_LOW);
  Button_Attach(&u8g2_Button, BUTTON_DOWM, Btn1_Dowm_CallBack);     // Click
  Button_Attach(&u8g2_Button, BUTTON_DOUBLE, Btn1_Double_CallBack); // Double click

  /* 程序区域 */
  Get_Button_Event(&u8g2_Button);

  while (1)
  {
    Button_Process(); // Need to call the button handler function periodically

    rt_thread_mdelay(20);
  }
}

void led_init(void)
{rt_pin_mode(LED0, PIN_MODE_OUTPUT);}

static void led_on(void)
{rt_pin_write(LED0, PIN_LOW);}
static void led_off(void)
{rt_pin_write(LED0, PIN_HIGH);}
// static void led_toggle(void)
// {rt_pin_write(LED0, !rt_pin_read(LED0));}

void key_init(void)
{
  rt_pin_mode(KEY0, PIN_MODE_INPUT_PULLUP);
}

static void BSP_Init(void)
{
  /* LED Config */
  led_init();
  key_init();
}
#endif

/**
 * SCL_PIN    GET_PIN(B, 6)
 * SDA_PIN    GET_PIN(B, 7)
 */

#ifdef OLED
static U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0,
                                         /* clock=*/ OLED_I2C_PIN_SCL,
                                         /* data=*/ OLED_I2C_PIN_SDA,
                                         /* reset=*/ U8X8_PIN_NONE);


static u8g2_uint_t offset;             // current offset for the scrolling text 当前文本的位置
static u8g2_uint_t width;              // pixel width of the scrolling text (must be lesser than 128 unless U8G2_16BIT is defined
// static const char *text = "U8g2你 ";     // scroll this text from right to left 要右到左边的文本
char* text_t;
char text[128] = "H你好世界";
static rt_mutex_t oled_mutex = RT_NULL;
#define THREAD_PRIORITY         25
#define THREAD_STACK_SIZE       1024
#define THREAD_TIMESLICE        5

static rt_thread_t oled_show = RT_NULL;
static rt_thread_t oled_get = RT_NULL;

static char mb_str1[] = "once";//单击
static char mb_str2[] = "double";//双击
static void u8g2_ctrl(void *parameter)
{

}
static void u8g2_get(void *parameter)
{
    /* 从邮箱中收取邮件 */
    while(1)
    {
        if (rt_mb_recv(&ble_mb_oled, (rt_ubase_t *)&text_t, RT_WAITING_FOREVER) == RT_EOK)
        {
            rt_kprintf("OLED_info:%s\n",text_t);// 发送给电脑
            rt_mutex_take(oled_mutex, RT_WAITING_FOREVER); // 获取OLED操控text失败
            rt_strncpy(text,text_t,1024); // !!! 中文要大！
            rt_mutex_release(oled_mutex);
        }
    }

}

// TODO OLED 设置只有两个字
// TODO OLED 实现多个字显示
static void u8g2_show_thread(void *parameter)
{
  u8g2.begin();

  u8g2.setFont(u8g2_font_inb30_mr);    // set the target font to calculate the pixel width
  width = u8g2.getUTF8Width(text);        // calculate the pixel width of the text, 字体大小

  u8g2.setFontMode(0);        // enable transparent mode, which is faster, 只要改一个字符就更新，所以快些
  while(1)
  {
    u8g2_uint_t x; //16bit
//    rt_strncpy(text,"请连接蓝牙",128);
    u8g2.firstPage();
    do {
      // draw the scrolling text at current offset
      x = offset;
      u8g2.setFont(u8g2_font_unifont_t_chinese3);       // set the target font
      do {                                    // repeated drawing of the scrolling text
        rt_mutex_take(oled_mutex, RT_WAITING_FOREVER);

        u8g2.drawUTF8(x, 30, text);           // draw the scolling text
        x += width;                           // add the pixel width of the scrolling text

        rt_mutex_release(oled_mutex);
      } while( x < u8g2.getDisplayWidth() );  // draw again until the complete display is filled

      u8g2.setFont(u8g2_font_inb16_mr);       // draw the current pixel width
      u8g2.setCursor(0, 58);
      u8g2.print(width);                      // this value must be lesser than 128 unless U8G2_16BIT is set
    } while ( u8g2.nextPage() );

    offset-=1;                                // scroll by one pixel
    if ( (u8g2_uint_t)offset < (u8g2_uint_t)-width )
      offset = 0;                             // start over again

    rt_thread_mdelay(10);                     // do some small delay
  }
}

rt_err_t u8g2_init(void)
{


   oled_mutex = rt_mutex_create("dmutex", RT_IPC_FLAG_PRIO);
    if (oled_mutex == RT_NULL)
    {
        rt_kprintf("create oled mutex failed.\n");
        return -1;
    }
  oled_show = rt_thread_create("oled_show",
                          u8g2_show_thread, RT_NULL,
                          THREAD_STACK_SIZE,
                          THREAD_PRIORITY, THREAD_TIMESLICE);

  /* 如果获得线程控制块，启动这个线程 */
  if (oled_show != RT_NULL)
    rt_thread_startup(oled_show);

    oled_get = rt_thread_create("oled_get",
                          u8g2_get, RT_NULL,
                          THREAD_STACK_SIZE,
                          THREAD_PRIORITY-1, THREAD_TIMESLICE);

  /* 如果获得线程控制块，启动这个线程 */
  if (oled_get != RT_NULL)
    rt_thread_startup(oled_get);

  return RT_EOK;
}
// MSH_CMD_EXPORT(u8g2_page_buffer_scrolling_text, u8g2 page buffer scrolling text sample);


// static u8g2_t u8g2;
/*********
 * 功能：静止显示，动态显示
 * 接口：
 * 变量：当前位置信息：为了中途中断而正常显示，按下一次：直接停止，两次后复位中间
 *
**********/

// static rt_uint8_t cursor_pos = 1; // 首字符当前位置
// static rt_uint8_t select_pos = 1; //

// static rt_uint8_t speed = 1;

// static void shiftChinese_once(int argc,char *argv[])
// {

// //    u8g2_SetFont(&u8g2, u8g2_font_unifont_t_symbols);
// //    u8g2_DrawGlyph(&u8g2, 112, 56, 0x2603 );
// //    u8g2_SendBuffer(&u8g2); // Draw a single character
//    for(int x=0;x<256;x+=speed)
//    {
//        u8g2_ClearBuffer(&u8g2);
//        u8g2_SetFont(&u8g2, u8g2_font_unifont_t_chinese3);
//        u8g2_DrawUTF8(&u8g2,x,18,"湖南人文科技学院");
//        u8g2_DrawUTF8(&u8g2,x-128,18,"湖南人文科技学院");
//        u8g2_SendBuffer(&u8g2);
//    }
// }
// // MSH_CMD_EXPORT(shiftChinese, u8g2ss);
// void u8g2_init()
// {
//     // I2c 通信
//     u8g2_Setup_ssd1306_i2c_128x64_noname_f( &u8g2, U8G2_R0, u8x8_byte_rtthread_hw_i2c, u8x8_gpio_and_delay_rtthread);
//     u8g2_InitDisplay(&u8g2);
//     u8g2_SetPowerSave(&u8g2, 0);
// }


/**
 * @attention 由于没延迟，如何打断施法？
 */
// static void oled_chinese_test(int argc,char *argv[])
// {
//     int y = 20;
//     u8g2_SetFont(&u8g2, u8g2_font_unifont_t_chinese3);
//    for(int x=0;x<256;x+=y)
//    {
//        u8g2_ClearBuffer(&u8g2);
//        u8g2_DrawUTF8(&u8g2,x,18,"湖南人文科技学院");
//        u8g2_DrawUTF8(&u8g2,x-128,18,"湖南人文科技学院");
//        u8g2_SendBuffer(&u8g2);
//        rt_thread_mdelay(50);
//    }
// }
// MSH_CMD_EXPORT(oled_chinese_test, u8g2ss);


// static void oled_time(int argc,char *argv[])
// {
//     rt_err_t ret = RT_EOK;
//     time_t now;
//     /* 设置日期 */
//     ret = set_date(2018, 12, 3);
//     if (ret != RT_EOK)
//     {
//         rt_kprintf("set RTC date failed\n");
//         return ret;
//     }
//     /* 设置时间 */
//     ret = set_time(11, 15, 50);
//     if (ret != RT_EOK)
//     {
//         rt_kprintf("set RTC time failed\n");
//         return ret;
//     }
//     /* 延时3秒 */
//     rt_thread_mdelay(3000);
//     /* 获取时间 */
//     now = time(RT_NULL);
//     rt_kprintf("%s\n", ctime(&now));
//     return ret;
//     u8g2_SetFont(&u8g2, u8g2_font_unifont_t_chinese3);
//    for(int x=0;x<256;x+=y)
//    {
//        u8g2_ClearBuffer(&u8g2);
//        u8g2_DrawUTF8(&u8g2,x,18,"湖南人文科技学院");
//        u8g2_DrawUTF8(&u8g2,x-128,18,"湖南人文科技学院");
//        u8g2_SendBuffer(&u8g2);
//        rt_thread_mdelay(50);
//    }

// }
// MSH_CMD_EXPORT(oled_time, u8g2ss);
#endif

