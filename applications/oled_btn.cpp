#include "app.h"

#include "oled.h"
#include "user_button.h"
/* 线程控制块 */
// u8g2_ctrl_thread // Static thread btn ctrl oled method
static rt_thread_t oled_dynamic_t = RT_NULL;
static rt_thread_t oled_static_t = RT_NULL;
// static rt_thread_t oled_text = RT_NULL; // l168 changetostatic
// static rt_thread_t btn_t = RT_NULL; // Static thread -scan btn
/* 邮箱 */
static char mb_pool[32];
static char mb_once[] = "once";   //单击
static char mb_doub[] = "double"; //双击
static struct rt_mailbox btn_oled_mbt;

#ifdef Btn_OLED
/******* Button ******/
/* #region  ___按键,LED 初始化___ */
uint8_t Read_u8g2b_Level(void)
{
  return rt_pin_read(KEY0);
}

void led_init(void)
{
  rt_pin_mode(LED0, PIN_MODE_OUTPUT);
}
static void led_on(void)
{
  rt_pin_write(LED0, PIN_LOW);
}
static void led_off(void)
{
  rt_pin_write(LED0, PIN_HIGH);
}
void key_init(void)
{
  rt_pin_mode(KEY0, PIN_MODE_INPUT_PULLUP);
}
/* #endregion */
Button_t u8g2_Button;
/* #region  按键修改区 */
void Btn1_Dowm_CallBack(void *btn) // 对外接口
{
  led_off();
  // rt_kprintf("Click!");
  rt_mb_send(&btn_oled_mbt, (rt_uint32_t)&mb_once);
}
void Btn1_Double_CallBack(void *btn) // 对外接口
{
  led_on();
  // rt_kprintf("Double click!");
  rt_mb_send(&btn_oled_mbt, (rt_uint32_t)&mb_doub);
}
/* #endregion */
static void BSP_Init(void)
{
  rt_err_t result;
  result = rt_mb_init(&btn_oled_mbt,
                      "btn_mbt",           /* 名称是mbt */
                      &mb_pool[0],         /* 邮箱用到的内存池是mb_pool */
                      sizeof(mb_pool) / 4, /* 邮箱中的邮件数目，因为一封邮件占4字节 */
                      RT_IPC_FLAG_FIFO);   /* 采用FIFO方式进行线程等待 */
  if (result != RT_EOK)
  {
    rt_kprintf("init mailbox failed.\n");
    // return -1;
  }
  led_init();
  key_init();
}
/* #region  按键入口 */
ALIGN(RT_ALIGN_SIZE)
static char btn_stack[1024];
static struct rt_thread btn_thread;
static void button_entry(void *parameter)
{
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
/******* OLED ******/

static U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0,
                                                /* clock=*/OLED_I2C_PIN_SCL,
                                                /* data=*/OLED_I2C_PIN_SDA,
                                                /* reset=*/U8X8_PIN_NONE);

static u8g2_uint_t offset; // current offset for the scrolling text 当前文本的位置
static u8g2_uint_t width;  // pixel width of the scrolling text (must be lesser than 128 unless U8G2_16BIT is defined
// static const char *text = "U8g2你 ";     // scroll this text from right to left 要右到左边的文本
char *text_t;
char text[128] = "I love";
static rt_mutex_t oled_mutex = RT_NULL;
#define THREAD_PRIORITY 25
#define THREAD_STACK_SIZE 1024
#define THREAD_TIMESLICE 5

// TODO OLED 设置只有两个字
// TODO OLED 实现多个字显示
// TODO 实现共用坐标
static void u8g2_d_thread(void *parameter)
{
  // u8g2.setFont(u8g2_font_inb30_mr); // set the target font to calculate the pixel width

  u8g2.setFont(u8g2_font_unifont_t_chinese3); // set the target font
  while (1)
  {
    u8g2_uint_t x;                   // 16bit
    width = u8g2.getUTF8Width(text); // calculate the pixel width of the text, 获取字体大小, 也就是偏移量

    u8g2.firstPage();
    do
    {
      // draw the scrolling text at current offset
      x = offset;
      do
      { // repeated drawing of the scrolling text
        rt_mutex_take(oled_mutex, RT_WAITING_FOREVER);

        u8g2.drawUTF8(x, 30, text); // draw the scolling text
        x += width;                 // add the pixel width of the scrolling text

        rt_mutex_release(oled_mutex);
      } while (x < u8g2.getDisplayWidth()); // draw again until the complete display is filled

      // u8g2.setFont(u8g2_font_inb16_mr); // draw the current pixel width
      // u8g2.setCursor(0, 58);
      // u8g2.print(width); // this value must be lesser than 128 unless U8G2_16BIT is set
    } while (u8g2.nextPage());

    offset -= 1; // scroll by one pixel
    if ((u8g2_uint_t)offset < (u8g2_uint_t)-width)
      offset = 0; // start over again

    rt_thread_mdelay(10); // do some small delay
  }
}
static void u8g2_s_thread(void *parameter)
{
  u8g2.clear();
  u8g2.setFont(u8g2_font_unifont_t_chinese3);

  while (1)
  {

    rt_mutex_take(oled_mutex, RT_WAITING_FOREVER);

    u8g2.drawUTF8(0, 30, text); // draw the scolling text
    u8g2.sendBuffer();

    rt_mutex_release(oled_mutex);
    rt_thread_mdelay(10000); // do some small delay
  }
}
ALIGN(RT_ALIGN_SIZE)
static char u8g2_ctrl_stack[1024];
static struct rt_thread u8g2_ctrl_thread;
static void btn_ctrl_entry(void *parameter)
{
  while (1)
  {
    char *str;
    if (rt_mb_recv(&btn_oled_mbt, (rt_ubase_t *)&str, RT_WAITING_FOREVER) == RT_EOK)
    {
      if (str == mb_once) // 按下一次 滚动,创建线程
      {
        // 非动态进程运行
        if (oled_dynamic_t == RT_NULL) //动态进程不存在
        {
          if (oled_static_t != RT_NULL) //且运行着静态线程
          {
            rt_thread_delete(oled_static_t);
            oled_static_t = RT_NULL;
          }
          // TODO 避免重复刷新
          oled_dynamic_t = rt_thread_create("dynamic_show",
                                            u8g2_d_thread, RT_NULL,
                                            THREAD_STACK_SIZE,
                                            THREAD_PRIORITY - 3, THREAD_TIMESLICE);

          /* 如果获得线程控制块，启动这个线程 */
          if (oled_dynamic_t != RT_NULL)
            rt_thread_startup(oled_dynamic_t);
        }
        else
          ;
      }
      else if (str == mb_doub) // 按下两次静止，关闭线程，运行一次
      {
        // 非静态进程运行
        if (oled_static_t == RT_NULL) //静态进程不存在
        {
          if (oled_dynamic_t != RT_NULL) //且运行动态这个线程可有可无
          {
            rt_thread_delete(oled_dynamic_t);
            oled_dynamic_t = RT_NULL;
          }
          oled_static_t = rt_thread_create("static_show",
                                           u8g2_s_thread, RT_NULL,
                                           THREAD_STACK_SIZE,
                                           THREAD_PRIORITY - 3, THREAD_TIMESLICE);

          /* 如果获得线程控制块，启动这个线程 */
          if (oled_static_t != RT_NULL)
            rt_thread_startup(oled_static_t);
        }
        else
          ;
      }
    }

    rt_thread_mdelay(100);
  }
}

ALIGN(RT_ALIGN_SIZE)
static char txt_stack[1024];
static struct rt_thread oled_text_thread;
static void u8g2_get_txt(void *parameter)
{
  /* 从蓝牙邮箱中收取邮件 */
  while (1)
  {
    rt_kprintf("等待等待获取文字\n");
    if (rt_mb_recv(&ble_mb_oled, (rt_ubase_t *)&text_t, RT_WAITING_FOREVER) == RT_EOK)
    {
      rt_kprintf("OLED_info:%s\n", text_t);          // 发送给电脑
      rt_mutex_take(oled_mutex, RT_WAITING_FOREVER); // 获取OLED操控text失败
      rt_strncpy(text, text_t, 64);                  // !!! 中文要大！
      rt_mutex_release(oled_mutex);
      rt_thread_mdelay(200);
    }
  }
}

/* #endregion */
rt_err_t button_u8g2_init(void)
{
  rt_err_t result;
  /* 硬件初始化 */
  BSP_Init();
  /*视图初始化*/
  u8g2.begin();
  u8g2.setFontMode(0); // enable transparent mode, which is faster, 只要改一个字符就更新，所以快些
                       /* OLED 抢占资源初始化 */
  /* #region  操作 text 的互斥量 */
  oled_mutex = rt_mutex_create("oled_mut", RT_IPC_FLAG_PRIO);
  if (oled_mutex == RT_NULL)
  {
    rt_kprintf("create oled mutex failed.\n");
    return -1;
  }
  /* #endregion */
  /* #region  按键扫描线程 静态*/
  rt_thread_init(&btn_thread,
                 "btn_scan",
                 button_entry,
                 RT_NULL,
                 &btn_stack[0],
                 sizeof(btn_stack),
                 THREAD_PRIORITY - 4, THREAD_TIMESLICE);
  rt_thread_startup(&btn_thread);
  /* #endregion */
  /* #region  按键信号控制oled线程 静态 */
  rt_thread_init(&u8g2_ctrl_thread,
                 "btn_ctrl",
                 btn_ctrl_entry,
                 RT_NULL,
                 &u8g2_ctrl_stack[0],
                 sizeof(u8g2_ctrl_stack),
                 THREAD_PRIORITY - 4, THREAD_TIMESLICE);
  rt_thread_startup(&u8g2_ctrl_thread);
  /* #endregion */
  /* #region 获取文字的永久静态线程 TODO 失败*/
  result = rt_thread_init(&oled_text_thread, // TODO 无法创建
                          "oled_text",
                          u8g2_get_txt,
                          RT_NULL,
                          &txt_stack[0],
                          sizeof(txt_stack),
                          THREAD_PRIORITY - 3, THREAD_TIMESLICE + 10);
  if (result != RT_NULL)
  {
    rt_thread_startup(&oled_text_thread);
  }
  else
  {
    rt_kprintf("创建oled_text失败\n");
  }
  /* #endregion */
  /* #region 默认静态OLED的线程 动态 */
  // 交互可以：蓝牙连接成功后再创建静态OLED线程和按键控制线程
  //    rt_strncpy(text,"请连接蓝牙",128);
  /*  */
  oled_static_t = rt_thread_create("static_show",
                                   u8g2_s_thread, RT_NULL,
                                   THREAD_STACK_SIZE,
                                   THREAD_PRIORITY - 3, THREAD_TIMESLICE);
  if (oled_static_t != RT_NULL)
  {
    rt_thread_startup(oled_static_t);
  }
  else
  {
    rt_kprintf("创建静态OLED线程失败\n");
  }
  /* #endregion */

  return RT_EOK;
}
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
