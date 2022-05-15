#include "app.h"

#include "oled.h"
#include "user_button.h"
#include <time.h>
/* 线程控制块 */
#define THREAD_PRIORITY 25
#define THREAD_STACK_SIZE 1024
#define THREAD_TIMESLICE 5
static rt_thread_t oled_dst_t = RT_NULL;
static rt_thread_t oled_text_t = RT_NULL; // l168 changetostatic
/* 邮箱 */
static char mb_pool[32];          //地址信息
static char mb_once[] = "once";   //单击
static char mb_doub[] = "double"; //双击
// static rt_mutex_t oled_d_mutex = RT_NULL; // 动态的处理

static rt_sem_t oled_sem_event;
struct rt_semaphore sem_empty, sem_full;
/* time */
time_t now;
// static rt_timer_t timer1;
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


void date(char **argv)
{
  /* set time and date */
  uint16_t year;
  uint8_t month, day, hour, min, sec;
  year = atoi(argv[0]);
  month = atoi(argv[1]);
  day = atoi(argv[2]);
  hour = atoi(argv[3]);
  min = atoi(argv[4]);
  sec = atoi(argv[5]);
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
  // rt_kprintf("%d",month);
  set_time(hour, min, sec);
  set_date(year, month, day);
}
void num2str(char *c, int i)
{
  c[0] = i / 10 + '0';
  c[1] = i % 10 + '0';
}
char *getStrTime()
{
  /* 格式化 */
  static char buf[20];
  struct tm *t;
  now = time(RT_NULL); //获取目前秒时间
  t = localtime(&now); //转为本地时间

  // 2022-01-01 21:22:08\n
  num2str(buf, (t->tm_year + 1900) / 100);
  num2str(buf + 2, (t->tm_year + 1900) % 100);
  buf[4] = '-';
  num2str(buf + 5, t->tm_mon+1);
  buf[7] = '-';
  num2str(buf + 8, t->tm_mday);
  // if (buf[8] == '0')
  //   buf[8] = ' ';

  buf[10] = ' ';
  num2str(buf + 11, t->tm_hour);
  buf[13] = ':'; // 第十四个位置
  num2str(buf + 14, t->tm_min);
  buf[16] = ':';
  num2str(buf + 17, t->tm_sec);
  buf[19] = '\0'; // 最后一个字节
  return buf;
}
void changeTime()
{
  const char spl[] = "- :";
  static char time_get[MAX_STR]; //？
  static uint8_t cnt = 0;        // 解析字符串
  static char *time_parse[6];    // = {(char*)"2022",(char*)"05",(char*)"15",(char*)"12",(char*)"00",(char*)"00"}; //  char *token;
  static char *time_input;       // 蓝牙传递的信息

  for(int i=0;i<6;i++)
  {
    time_parse[i] = (char *)malloc(sizeof(char)*4);
  }
  char* token;
  if (rt_mb_recv(&ble_mb_time, (rt_ubase_t *)&time_input, RT_WAITING_NO) == RT_EOK)
  {
    rt_memset(time_get, 0, MAX_STR);
    rt_strncpy(time_get, time_input+1, MAX_STR); //复制到显示字符Buffer中
                                               // rt_kprintf("OLED_info:%s\n", text); // 发送给电脑
                                               /* 解析时间数据 */
    // rt_kprintf("%s\n",time_get);

    token = strtok(time_get,spl);
    while(token!=NULL)
    {
      // strcpy(time_buf[cnt],token);
      strcpy(time_parse[cnt],token);
      cnt+=1;
      token = strtok(NULL,spl); // 解析一次
    }

    cnt = 0;
    date(time_parse);
    // date(time_buf);
  }
}
#define mode_static 0
#define mode_dynamic 1
static rt_uint8_t mode = mode_static;
static void u8g2_dst_thread(void *parameter)
{
  static char* time_buf;

  static char d_text[MAX_STR]; // 动态文字
  u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
  width = u8g2.getUTF8Width(d_text);
  while (1)
  {
    changeTime();
    time_buf = getStrTime();

    switch (mode)
    {
    case mode_static:
      /* code */
      u8g2.firstPage(); // 第一帧
      do
      {
          u8g2.drawUTF8(5, 48, d_text);
          u8g2.drawStr(5,24,time_buf);
      } while (u8g2.nextPage()); // 整个8行结束 8*8=64 行 64

      break;
    case mode_dynamic:
      u8g2_uint_t x;
      u8g2.firstPage(); // 第一帧
      do
      {
        x = offset;
        do
        {
          // u8g2.setCursor(x, 48); // 第一对字
          // u8g2.print(d_text);
          u8g2.drawUTF8(x, 48, text);
          // u8g2.drawUTF8(x, 30, text);       // 在offset这里开始写字
          x += (width+8); // 以一个字为偏移
          // x += 64;
        } while (x < u8g2.getDisplayWidth()); // 8个像素一行的结束，屏幕宽度128
        u8g2.drawStr(5,24,time_buf);
      } while (u8g2.nextPage()); // 整个8行结束 8*8=64 行 64

      offset -= 4;                                   // 从右到左平移，偏移量为4
      if ((u8g2_uint_t)offset < (u8g2_uint_t)-width) //循环一次结束
                                                     // if ((u8g2_uint_t)offset < (u8g2_uint_t)-64)
        offset = 0;                                  // start over again
    break;
  default:
    break;
  }
  if (rt_sem_take(oled_sem_event, RT_WAITING_NO) == RT_EOK) // 已经更改过了文本
  {
    rt_memset(d_text, 0, MAX_STR);
    rt_strncpy(d_text, text, MAX_STR);
    // rt_kprintf("d_text=%s\n", d_text);
    width = u8g2.getUTF8Width(d_text); // 字体宽度
  }
  rt_thread_mdelay(10);
  }
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
       if (str == mb_once)
      { // 按下一次 滚动,创建线程
        mode = mode_dynamic;
      }
      else if (str == mb_doub)
      {
        mode = mode_static;
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
      rt_strncpy(text, text_t, MAX_STR); //复制到显示字符Buffer中
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
  set_date(2022, 5, 15);
  set_time(18, 00, 00);
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

  oled_dst_t = rt_thread_create("dst_show",
                                   u8g2_dst_thread, RT_NULL,
                                   THREAD_STACK_SIZE,
                                   THREAD_PRIORITY - 2, THREAD_TIMESLICE);
  if (oled_dst_t != RT_NULL)
  {
    rt_thread_startup(oled_dst_t);
  }
  else
  {
    rt_kprintf("创建OLED显示线程失败\n");
  }
  return RT_EOK;
}

#endif
