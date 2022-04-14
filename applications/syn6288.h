#ifndef __SYN6288_H
#define __SYN6288_H
    #define SYN6288
#ifdef SYN6288
#ifdef __cplusplus
extern "C" {
#endif
#include <rtdef.h>
#include <rtthread.h>
#include <rtdevice.h>
#include "string.h"
#include "app.h"

/**************芯片设置命令*********************/
extern char SYN_StopCom[]; //停止合成
extern char SYN_SuspendCom[]; //暂停合成
extern char SYN_RecoverCom[]; //恢复合成
extern char SYN_ChackCom[]; //状态查询
extern char SYN_PowerDownCom[]; //进入POWER DOWN 状态命令
rt_err_t SYN6288_init(void);
/**
 * @brief 文本控制标记列表
 * @param Music 选择背景音乐2(0：无背景音乐  1-15：背景音乐可选)
 * m[0~16]:0背景音乐为静音，16背景音乐音量最大
 * v[0~16]:0朗读音量为静音，16朗读音量最大
 * t[0~5]:0朗读语速最慢，5朗读语速最快
 * SYN_FrameInfo(2, "[v7][m1][t5]欢迎使用绿深旗舰店SYN6288语音合成模块");
 */
void SYN_FrameInfo(uint8_t Music, char *HZdata);

// /***********************************************************
// * 名    称： YS_SYN_Set(u8 *Info_data)
// * 功    能： 主函数   程序入口
// * 入口参数： *Info_data:固定的配置信息变量
// * 出口参数：
// * 说    明：本函数用于配置，停止合成、暂停合成等设置 ，默认波特率9600bps。
// * 调用方法：通过调用已经定义的相关数组进行配置。
// **********************************************************/

/**
 * @brief 用于状态控制，非发声音
 * @param Info_data 芯片设置命令
 * @param length sizeof(Info_data)
 */
void YS_SYN_Set(char *Info_data,int_fast8_t length);
#ifdef __cplusplus
}
#endif
#endif
#endif
