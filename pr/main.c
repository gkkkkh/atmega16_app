/*
 * main.c
 *
 * Created: 2023/4/6 21:21:33
 * Author: gkkkh                        PB2         PB0
 *Scan0-Scan3: PA2:PA5      行线         1           5         PA2
                                         2           6         PA3
                                         3           7         PA4
                                         4           8         PA5
 *Key0,Key1:PB0,PB2             列线
 
  
 *2ms:      1.切换数码管
            2.扫键盘    
            
  显示：    1.时分秒（无按键）
            2.按键数1~2s （有按键）
            3.温度/s （保存值，不显示）
            4.频率测量
 */

#include "io.h"
#include "mega16.h"

enum Key
{
     K1_1=1,
     K2_1,
     K3_1,
     K4_1,
     K1_2,
     K2_2,
     K3_2,
     K4_2   
}; 
                                                      
// 字型码,后 3 位为 “A”，“b”，“-”
flash char led[13]={0x3F,0x06,0x5B,0x4F,0x66,0x6D, 0x7D,0x07,0x7F,0x6F,0x77,0x7C,0x40};
 
char time_dis_buff[6], freq_dis_buff[6], dis_key, posit = 0;
char time[3];
bit key_display_ok = 0， freq_diplay_ok = 0;
int key_display_cnt = 0, freq_cnt = 0, freq_display_cnt = 0;
volatile unsigned int freq;
volatile short _2ms_cnt = 0;

#define Key_mask   0b00000101
#define NoKey   255  
#define F_CPU   8000000


void init_INT0(void)                //配置外部中断
{
    DDRD = 0xFB;                      //设置PD2（INT0）为输入
    PORTD = 0x04;                     //启用PD2上的上拉电阻
                                  
    MCUCR |= (1 << ISC01);            //设置INT0触发方式为下降沿
    GICR |= (1 << INT0);              //启用外部中断0
    #asm("sei");                      //开放全局中断 
}                               

void init_Timer1(void)              //初始化计数器
{
    TCCR1A = 0x00;                    //设置Timer1为正常模式
    TCCR1B |= (1 << CS10);            //设置时钟源为系统时钟（不分频）
    TCNT1 = 0x00;                     //清零计数器
}

void init_Timer0(void)              // T/C0 初始化
{ 
    TCCR0=0x0B;                     // 内部时钟，64 分频（4M/64=62.5KHz），CTC 模式 
    TCNT0=0x00;                     // 清零
    OCR0=0x7C;                      // OCR0 = 0x7C(124),(124+1)/62.5=2ms 
    TIMSK=0x02;                     // 允许 T/C0 比较匹配中断
}

void addTime(void)             // 加一秒
{
    if(++time[0] == 60)
    {
        time[0] = 0;
        if(++time[1] == 60)
        {
            time[1] = 0;
            if(++time[2] == 24)
                time[2] = 0;
        }
    }
    time_dis_buff[0] = time[2] / 10;
    time_dis_buff[1] = time[2] % 10;
    time_dis_buff[2] = time[1] / 10;
    time_dis_buff[3] = time[1] % 10;
    time_dis_buff[4] = time[0] / 10;
    time_dis_buff[5] = time[0] % 10;        
}  

void display(void)          // LED扫描显示
{
    if(freq_display_ok == 1)
        PORTC = led[freq_dis_buff[posit]]; 
    if(posit == 5 && key_display_ok)
        PORTC = led[dis_key];
    else
        PORTC = led[time_dis_buff[posit]];
    PORTA = 00000100 << posit;
    if(++posit == 6)
        posit = 0;               
}  

void freq_to_disbuff(void)              // 将频率值转化为 BCD 码并送入显示缓冲区 
{ 
    char i; 
    for (i=0;i<=5;i++) 
    { 
        freq_dis_buff[5-i] = freq % 10; 
        freq = freq / 10; 
    } 
}

// 外部中断0服务程序
#pragma interrupt_handler Freq_INT0_ISR:2
void Freq_INT0_ISR(void) 
{
    int current_count = TCNT1;            // 读取Timer1的值
    TCNT1 = 0x00;                         // 重置计数器
    int freq_temp = F_CPU / current_count;         // 使用当前计数值计算频率
    if(++freq_cnt == 200)
    {
        freq_display_ok = 1;
        freq_display_cnt = 0;
        freq_to_disbuff();
    }
    else
    {                                   
        float k1 = (freq_cnt - 1) / (freq_cnt);
        float k2 = 1 - k1;
        freq = k1 * freq + k2 * freq_temp;
        freq_cnt = 0;
    }
}

// 计数器0比较匹配中断服务程序
#pragma interrupt_handler INT_2ms_ISR:20
void INT_2ms_ISR(void)
{
    if(++_2ms_cnt == 500)
    {
        addTime();
        _2ms_cnt = 0;
    }
    if(key_display_ok == 1)
        if(++key_display_cnt == 1000)       // 显示2秒
            key_display_ok = 0;
    if(freq_display_ok == 1)
        if(++freq_display_cnt == 1000)
            freq_display_ok = 0;      
    display();  
}

char read_key(void)             
{
     static char key_state = 0,key_value, key_line;
     char key_return = NoKey, i;
     switch(key_state)
     {
          case 0:
               key_line = 0b00000100;
               for(i=0; i<4; i++)                   //扫描键盘
               {
                    PORTA = key_line;               //输出行线电平
                    PORTA = key_line;               //输出两次
                    key_value = Key_mask & PINB;    //读列电平
                    if(key_value == Key_mask)       //没有按键
                         key_line <<= 1;
                    else                            //有按键
                    {
                         key_state++;               //消抖
                         break;
                    }
               }    
               break;
          case 1:
              if(key_value == (Key_mask & PINB))    //再次读列电平
              {    
                    //第一行    PA2
                    if(key_line == 0b00000100 && key_value == 0b00000100)   key_return = K1_2;         //PB0
                    if(key_line == 0b00000100 && key_value == 0b00000001)   key_return = K1_1;         //PB2
                    //第二行    PA3
                    if(key_line == 0b00001000 && key_value == 0b00000100)   key_return = K2_2;         //PB0
                    if(key_line == 0b00001000 && key_value == 0b00000001)   key_return = K2_1;         //PB2
                    //第三行    PA4
                    if(key_line == 0b00010000 && key_value == 0b00000100)   key_return = K3_2;         //PB0
                    if(key_line == 0b00010000 && key_value == 0b00000001)   key_return = K3_1;         //PB2
                    //第四行     PA5                                                         
                    if(key_line == 0b00100000 && key_value == 0b00000100)   key_return = K4_2;         //PB0
                    if(key_line == 0b00100000 && key_value == 0b00000001)   key_return = K4_1;         //PB2 
                    
                    key_state++;               //进入下一阶段
              }                                              
              else
                    key_state--;
              break;
          case 2:                                   //等待按键释放状态
              PORTA=0b11111100;                     //行线输出低电平
              PORTA=0b11111100;                     //送两次
              if((Key_mask & PINB) == Key_mask) 
                    key_state = 0;
              break;
     }          
     return key_return;
}

void main(void)
{            
    init_INT0();
    init_Timer1();  
    init_Timer0();  
    time[0] = time[1] = time[2] = 0;
    
    while (1)
    {
        if(_2ms_cnt % 5 == 0)                   // 每10ms检测一次
        {
            int key_temp = read_key();
            if(key_temp != NoKey)
            {             
                key_display_ok = 1;
                key_display_cnt = 0;
                dis_key = key_temp;             
            }
        }          
    }
}
