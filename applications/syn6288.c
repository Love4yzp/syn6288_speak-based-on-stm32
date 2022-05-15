 #include "syn6288.h"

#ifdef SYN6288

//#define BSP_UART2_TX_PIN       "PA2"
//#define BSP_UART2_RX_PIN       "PA3"
#define SYN6288_UART       "uart2"
struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;  /* 初始化配置参数 */
static struct rt_semaphore syn_rx_sem; /* 用于接收消息的信号量 */
static rt_device_t SYN6288_serial;
/**************芯片设置命令*********************/
char SYN_StopCom[] = {0xFD, 0X00, 0X02, 0X02, 0XFD}; //停止合成
char SYN_SuspendCom[] = {0XFD, 0X00, 0X02, 0X03, 0XFC}; //暂停合成
char SYN_RecoverCom[] = {0XFD, 0X00, 0X02, 0X04, 0XFB}; //恢复合成
char SYN_ChackCom[] = {0XFD, 0X00, 0X02, 0X21, 0XDE}; //状态查询
char SYN_PowerDownCom[] = {0XFD, 0X00, 0X02, 0X88, 0X77}; //进入POWER DOWN 状态命令

//Music:选择背景音乐。0:无背景音乐，1~15：选择背景音乐
void SYN_FrameInfo(uint8_t Music, char *HZdata) // 对外接口
{
  /****************需要发送的文本**********************************/
  unsigned  char  Frame_Info[50];
  unsigned  char  HZ_Length;
  unsigned  char  ecc  = 0;             //定义校验字节
  unsigned  int i = 0;
  HZ_Length = strlen((char*)HZdata);            //需要发送文本的长度

  /*****************帧固定配置信息**************************************/
  Frame_Info[0] = 0xFD ;            //构造帧头FD
  Frame_Info[1] = 0x00 ;            //构造数据区长度的高字节
  Frame_Info[2] = HZ_Length + 3;        //构造数据区长度的低字节
  Frame_Info[3] = 0x01 ;            //构造命令字：合成播放命令
  Frame_Info[4] = 0x01 | Music << 4 ; //构造命令参数：背景音乐设定

  /*******************校验码计算***************************************/
  for(i = 0; i < 5; i++)                //依次发送构造好的5个帧头字节
  {
    ecc = ecc ^ (Frame_Info[i]);        //对发送的字节进行异或校验
  }

  for(i = 0; i < HZ_Length; i++)        //依次发送待合成的文本数据
  {
    ecc = ecc ^ (HZdata[i]);                //对发送的字节进行异或校验
  }
  /*******************发送帧信息***************************************/
  memcpy(&Frame_Info[5], HZdata, HZ_Length);
  Frame_Info[5 + HZ_Length] = ecc;
  rt_device_write(SYN6288_serial, 0, Frame_Info, (5 + HZ_Length + 1)); //  USART3_SendString(Frame_Info, 5 + HZ_Length + 1);
}
void YS_SYN_Set(char *Info_data,int_fast8_t length) // 对外接口
{
    rt_device_write(SYN6288_serial, 0, Info_data, length);   //USART3_SendString(Info_data, Com_Len);

}


static rt_err_t syn6288_input(rt_device_t dev, rt_size_t size)
{
    rt_sem_release(&syn_rx_sem);
    return RT_EOK;
}
static void SYN6288_thread_entry(void *parameter) // 对外接口
{
    char ch;
    while (1)
    {
        /* 从串口读取一个字节的数据，没有读取到则等待接收信号量 */
        while (rt_device_read(SYN6288_serial, -1, &ch, 1) != 1)
        {
            rt_sem_take(&syn_rx_sem, RT_WAITING_FOREVER);
        }
       /*  rt_device_write(serial, 0, &ch, 1); */

        rt_kprintf("%x ",ch);//发送给电脑 // 显示代码
    }
}
rt_err_t SYN6288_init(void)
{
    rt_err_t ret = RT_EOK;
    SYN6288_serial = rt_device_find(SYN6288_UART);/* 查找系统中的串口设备 Global handler*/
    if(!SYN6288_serial)
    {
        rt_kprintf("find %s failed!\n", SYN6288_UART);
        return RT_ERROR;
    }
    config.baud_rate = BAUD_RATE_9600;        //修改波特率为 9600
    config.bufsz     = 128;                      //64修改缓冲区 buff size 为 128
    rt_device_control(SYN6288_serial, RT_DEVICE_CTRL_CONFIG, &config);/*控制9600参数*/

    rt_sem_init(&syn_rx_sem, "syn_rx_sem", 0, RT_IPC_FLAG_FIFO);/* 初始化信号量 */
    rt_device_open(SYN6288_serial, RT_DEVICE_FLAG_INT_RX); /* 以中断接收及轮询发送模式打开串口设备 */
    rt_device_set_rx_indicate(SYN6288_serial, syn6288_input); /* 设置接收回调函数 */

    //    rt_device_write(SYN6288_serial, 0, str, (sizeof(str) - 1));  /* 发送字符串 */ -1 去掉\0
    rt_thread_t thread = rt_thread_create("SYN_serial", SYN6288_thread_entry, RT_NULL, 1024, 25, 10); /* 创建 serial 线程 */
    /* 创建成功则启动线程 */
    if (thread != RT_NULL)
    {
        rt_thread_startup(thread);
    }
    else
    {
        rt_kprintf("线程启动失败\n");
        ret = RT_ERROR;
    }
    // rt_kprintf("成功syn6288\n");
    return ret;
}
// static void syn6288(int argc,char *argv[])
// {

//     if(argc>1)
//     {
//         if(argc == 2){
//             if(strcmp(argv[2],"status")) // 状态查询
//             {
//                 rt_kprintf("状态查询\n");
//                 YS_SYN_Set(SYN_ChackCom,sizeof(SYN_ChackCom));
//             }
//         }else if (argc>2) {
//             for(int i=2;i<argc;i++)
//             {

//                 SYN_FrameInfo(0,argv[i]);
//             }
//         }
//     }

// }
// MSH_CMD_EXPORT(syn6288, SYN2688);
#endif
