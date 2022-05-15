#include "app.h"

#include "oled.h"
#include "user_button.h"
#include <time.h>
/* 线程控制块 */
// u8g2_ctrl_thread // Static thread btn ctrl oled method
static rt_thread_t oled_dynamic_t = RT_NULL;
static rt_thread_t oled_static_t = RT_NULL;
static rt_thread_t oled_text_t = RT_NULL; // l168 changetostatic
// static rt_thread_t btn_t = RT_NULL; // Static thread -scan btn
/* 邮箱 */
static char mb_pool[32];          //地址信息
static char mb_once[] = "once";   //单击
static char mb_doub[] = "double"; //双击
// static rt_mutex_t oled_d_mutex = RT_NULL; // 动态的处理

static rt_sem_t oled_sem_event;
struct rt_semaphore sem_empty, sem_full;
/* time */
time_t now;
static rt_timer_t timer1;
/******* OLED ******/
static U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0,
                                                /* clock=*/OLED_I2C_PIN_SCL,
                                                /* data=*/OLED_I2C_PIN_SDA,
                                                /* reset=*/U8X8_PIN_NONE);
#define MAX_STR 3 * 20 + 1 // 3个字节为一个汉字
static u8g2_uint_t offset; // current offset for the scrolling text 当前文本的位置
static u8g2_uint_t width;  // pixel width of the scrolling text (must be lesser than 128 unless U8G2_16BIT is defined
// static const char *text = "U8g2你 ";     // scroll this text from right to left 要右到左边的文本
static char text[MAX_STR]; // = "启动成功"; // 设置为3*20个字符以内

static char *text_t; //接收邮箱发来的地址
// char change_status = 1;
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
  rt_mb_send(&btn_oled_mbt, (rt_uint32_t)&mb_doub); // 静止
}
void Btn1_Double_CallBack(void *btn) // 对外接口
{
  led_on();
  // rt_kprintf("Double click!");
  rt_mb_send(&btn_oled_mbt, (rt_uint32_t)&mb_once); // 动态
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

#define THREAD_PRIORITY 25
#define THREAD_STACK_SIZE 1024
#define THREAD_TIMESLICE 5

// DONE OLED 设置只有两个字
// TODO OLED 实现多个字显示
// TODO 实现共用坐标
// TODO 动态下切换字符不成功
static void u8g2_d_thread(void *parameter)
{
  static char d_text[MAX_STR];
//  static int str_len;
  // rt_kprintf("动态线程启动\n");
  while (1)
  {
    u8g2_uint_t x; // 16bit

    u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
    width = u8g2.getUTF8Width(d_text);

//    str_len = strlen(d_text)/3;

    u8g2.firstPage(); // 第一帧
    do
    {
      x = offset;
      do
      {
        // u8g2.setCursor(x, 48); // 第一对字
        // u8g2.print(d_text);
        u8g2.drawUTF8(x, 48, d_text);
        // u8g2.drawUTF8(x, 30, text);       // 在offset这里开始写字
        x += width;                         // 以一个字为偏移
        // x += 64;
      } while (x < u8g2.getDisplayWidth()); // 8个像素一行的结束，屏幕宽度128
      //////////////
    } while (u8g2.nextPage()); // 整个8行结束 8*8=64 行 64

    offset -= 4; // 从右到左平移，偏移量为4
    if ((u8g2_uint_t)offset < (u8g2_uint_t)-width) //循环一次结束
    // if ((u8g2_uint_t)offset < (u8g2_uint_t)-64)
      offset = 0; // start over again

    if (rt_sem_take(oled_sem_event, 0) == RT_EOK)
    {
      rt_memset(d_text, 0, MAX_STR);
      rt_strncpy(d_text, text, MAX_STR);
      // rt_kprintf("d_text=%s\n", d_text);
      // width = u8g2.getUTF8Width(d_text); // 字体宽度
    }
    rt_thread_mdelay(10);
  }
}
// TODO 切换进程导致 互斥量出现问题！
static void u8g2_s_thread(void *parameter)
{
  // rt_kprintf("静态线程启动\n");
  while (1)
  {
    if (rt_sem_take(oled_sem_event, RT_WAITING_FOREVER) == RT_EOK) // 获取成功 静态等待就行
    {
      // rt_kprintf("text=%s\n", text);
      // width = u8g2.getUTF8Width(text);
      u8g2.setFont(u8g2_font_wqy12_t_gb2312b); // use chinese2 for all the glyphs of "你好世界"
      u8g2.setFontDirection(0);
      u8g2.firstPage();
      do
      {
        u8g2.setCursor(5, 48);
        u8g2.print(text); // Chinese "Hello World"
      } while (u8g2.nextPage());
    }
  }
  // rt_thread_mdelay(100); // do some small delay
}
static void date(uint8_t argc, char **argv)
{
    if (argc == 1)
    {
        time_t now;
        /* output current time */
        now = time(RT_NULL);
        rt_kprintf("%.*s", 25, ctime(&now));
    }
    else if (argc >= 7)
    {
        /* set time and date */
        uint16_t year;
        uint8_t month, day, hour, min, sec;
        year = atoi(argv[1]);
        month = atoi(argv[2]);
        day = atoi(argv[3]);
        hour = atoi(argv[4]);
        min = atoi(argv[5]);
        sec = atoi(argv[6]);
        if (year > 2099 || year < 2000)
        {
            rt_kprintf("year is out of range [2000-2099]\n");
            return;
        }
        if (month == 0 || month > 12)
        {
            rt_kprintf("month is out of range [1-12]\n");
            return;
        }
        if (day == 0 || day > 31)
        {
            rt_kprintf("day is out of range [1-31]\n");
            return;
        }
        if (hour > 23)
        {
            rt_kprintf("hour is out of range [0-23]\n");
            return;
        }
        if (min > 59)
        {
            rt_kprintf("minute is out of range [0-59]\n");
            return;
        }
        if (sec > 59)
        {
            rt_kprintf("second is out of range [0-59]\n");
            return;
        }
        set_time(hour, min, sec);
        set_date(year, month, day);
    }
    else
    {
        rt_kprintf("please input: date [year month day hour min sec] or date\n");
        rt_kprintf("e.g: date 2018 01 01 23 59 59 or date\n");
    }
}
/*
 * 日期时间
 * T2018 02 16 01 15 30
 **/
static void u8g2_time_task(void *parameter)
{

  const char spl[] = " ";
  static char* time_input; // 蓝牙传递的信息
  static char time_get[MAX_STR]; //？
  static char* time_parse[] = {"date","2022","05","15","12","00","00","\0"}; //  char *token;
  static char cnt = 1; // 解析字符串
  /* 格式化 */
  char str_buf[50];
  struct tm* info;
  /*
   * T12:00
   */
  if(rt_mb_recv(&ble_mb_time, (rt_ubase_t *)&time_input, RT_WAITING_NO) == RT_EOK)
    {
      rt_memset(time_get, 0, sizeof(time_get));
      rt_strncpy(time_get, time_input, MAX_STR);  //复制到显示字符Buffer中
      // rt_kprintf("OLED_info:%s\n", text); // 发送给电脑
      /* 解析时间数据 */
      if(time_get[0] == 'T')
      {


//        char ss[]="解析时间中!\n";
//        rt_device_write(ble_serial, 0, ss, (sizeof(ss) - 1));
//        strchr(time_show,'d'); // 更改日期

          time_parse[cnt] = strtok(time_get, spl);        /* 获取第一个子字符串 */
        while (time_parse[cnt] != NULL)
        {
            if(cnt++ <7)
            {
                //          printf ("%s\n",pch);
                time_parse[cnt] = strtok (NULL, spl);
            }else {
                rt_kprintf("解析失败\n");
                break;
            }
        }
        cnt=1;
        date(7,time_parse);
        // switch (cnt) {
        //     case 3: // 12:00:00
        //         set_time(atoi(time_buf_t[0]), atoi(time_buf_t[1]), atoi(time_buf_t[2]));
        //         break;
        //     case 5: // 11-26/12:00:00
        //         set_date(2022, atoi(time_buf_t[0]), atoi(time_buf_t[1]));
        //         set_time(atoi(time_buf_t[2]), atoi(time_buf_t[3]), atoi(time_buf_t[4]));
        //         break;
        //     case 6: // 2020/11-26/12:00:00
        //         set_date(atoi(time_buf_t[0]), atoi(time_buf_t[1]),atoi(time_buf_t[2]));
        //         set_time(atoi(time_buf_t[3]), atoi(time_buf_t[4]),atoi(time_buf_t[5]));
        //         break;
        //     default:
        //         break;
        // }

      }
    }
    now = time(RT_NULL); //获取目前秒时间
    info = localtime(&now); //转为本地时间

    // ctime(&now); // 字符串
    strftime(str_buf, 50,"%Y-%m-%d %H:%M",info);
    /* 时间格式化 */
    u8g2.setFont(u8g2_font_helvR24_tn); // use chinese2 for all the glyphs of "你好世界"
    u8g2.firstPage();
    do
    {
      u8g2.setCursor(0, 30);
      u8g2.print(str_buf); // Chinese "Hello World"
    } while (u8g2.nextPage());

}
ALIGN(RT_ALIGN_SIZE)
static char u8g2_ctrl_stack[1024];
static struct rt_thread u8g2_ctrl_thread;
/* #region  按键控制OLED线程入口 */
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
          // DONE 避免重复刷新
          oled_dynamic_t = rt_thread_create("dynamic_show",
                                            u8g2_d_thread, RT_NULL,
                                            THREAD_STACK_SIZE,
                                            THREAD_PRIORITY - 3, THREAD_TIMESLICE);

          /* 如果获得线程控制块，启动这个线程 */
          if (oled_dynamic_t != RT_NULL)
            rt_thread_startup(oled_dynamic_t);
          rt_sem_release(oled_sem_event);
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
          rt_sem_release(oled_sem_event);
        }
        else
          ; //已经存在了
      }
    }
    // rt_thread_mdelay(100);
  }
}

/* #endregion */
/* #region  邮箱入口 */
static void u8g2_get_txt(void *parameter)
{
  /* 从蓝牙邮箱中收取邮件 */
  while (1)
  {
    if (rt_mb_recv(&ble_mb_oled, (rt_ubase_t *)&text_t, RT_WAITING_FOREVER) == RT_EOK)
    {
      /* 清除字符串 */
      rt_memset(text, 0, sizeof(text));
      rt_strncpy(text, text_t, MAX_STR);  //复制到显示字符Buffer中
      // rt_kprintf("OLED_info:%s\n", text); // 发送给电脑
      rt_sem_release(oled_sem_event);
    }
  }
}
/* #endregion */

/* #endregion */
rt_err_t button_u8g2_init(void)
{
  //  rt_err_t result;
//  rt_err_t ret = RT_EOK;
  /* 硬件初始化 */
  char t_str[] = "启动成功"; // 初始界面
  BSP_Init();
  rt_strncpy(text, t_str, rt_strlen(t_str));
  // rt_kprintf("t_str=%s\n",t_str);
  /*视图初始化*/
  u8g2.begin();
  u8g2.setFontMode(0); // enable transparent mode, which is faster, 只要改一个字符就更新，所以快些
                       /* OLED 抢占资源初始化 */
  u8g2.enableUTF8Print();


  // rt_kprintf("%s\n", ctime(&now));
  /* #region  操作 text 的信号量 */
  oled_sem_event = rt_sem_create("oled_sem", 1, RT_IPC_FLAG_PRIO);
  if (oled_sem_event == RT_NULL)
  {
    rt_kprintf("create dynamic oled_sem failed.\n");
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

  /* #region 获取文字的线程 TODO DONE*/
  oled_text_t = rt_thread_create("oled_text",
                                 u8g2_get_txt, RT_NULL,
                                 THREAD_STACK_SIZE,
                                 THREAD_PRIORITY - 5, 20);
  if (oled_text_t != RT_NULL)
  {
    rt_thread_startup(oled_text_t);
  }
  else
  {
    rt_kprintf("创建动态OLED线程失败\n");
  }

  oled_static_t = rt_thread_create("static_show",
                                   u8g2_s_thread, RT_NULL,
                                   THREAD_STACK_SIZE,
                                   THREAD_PRIORITY - 2, THREAD_TIMESLICE);
  if (oled_static_t != RT_NULL)
  {
    rt_thread_startup(oled_static_t);
  }
  else
  {
    rt_kprintf("创建静态OLED线程失败\n");
  }
  /* #endregion */
    /* 创建定时器1  周期定时器 */

  set_date(2022, 5, 14);
  set_time(13, 15, 50);

  // now = time(RT_NULL);
  // ctime(&now) // 字符串
  timer1 = rt_timer_create("timer1", u8g2_time_task,
                            RT_NULL, 10,
                            RT_TIMER_FLAG_PERIODIC);//周期性定时器
  /* 启动定时器1 */
  if (timer1 != RT_NULL)
      rt_timer_start(timer1);
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
