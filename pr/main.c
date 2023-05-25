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
#include "delay.h"
#pragma interrupt_handler Freq_INT0_ISR:2
#pragma interrupt_handler INT_2ms_ISR:20

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

volatile bit key_display_ok = 0,  freq_display_ok = 0;
volatile int key_display_cnt = 0, freq_cnt = 0, freq_display_cnt = 0;
volatile unsigned int freq;
volatile short _2ms_cnt = 0; 
volatile int adc_value;
volatile float temperature;

#define Key_mask   0b00000101
#define NoKey   255  
#define F_CPU   8000000

// 音调频率（单位：Hz）
#define NOTE_C4 261
#define NOTE_D4 293
#define NOTE_E4 329
#define NOTE_F4 349
#define NOTE_G4 391
#define NOTE_A4 440
#define NOTE_B4 493

/*配置外部中断*/
void init_INT0(void)                
{
    DDRD = 0xFB;                      //设置PD2（INT0）为输入
    PORTD = 0x04;                     //启用PD2上的上拉电阻
                                  
    MCUCR |= (1 << ISC01);            //设置INT0触发方式为下降沿
    GICR |= (1 << INT0);              //启用外部中断0
    #asm("sei");                      //开放全局中断 
}               
                
/*初始化计数器1,频率测量*/
void init_Timer1_freq(void)              
{
    TCCR1A = 0x00;                    //设置Timer1为正常模式
    TCCR1B |= (1 << CS10);            //设置时钟源为系统时钟（不分频）
    TCNT1 = 0x00;                     //清零计数器
}

/*初始化计数器1,播放音乐*/
void init_Timer1_music(void)
{
  // 设置 PD5 为输出
  DDRD |= (1 << PD5);

  // 配置 Timer1 为 Fast PWM 模式
  TCCR1A |= (1 << WGM10) | (1 << WGM11);
  TCCR1B |= (1 << WGM12) | (1 << WGM13);

  // 设置预分频器为 8
  TCCR1B |= (1 << CS11);

  // 设置 COM1A1 位以清除 OC1A（PD5）上的匹配
  TCCR1A |= (1 << COM1A1);
}

/*T/C0 初始化*/  
void init_Timer0(void)              
{ 
    TCCR0=0x0B;                     // 内部时钟，64 分频（4M/64=62.5KHz），CTC 模式 
    TCNT0=0x00;                     // 清零
    OCR0=0x7C;                      // OCR0 = 0x7C(124),(124+1)/62.5=2ms 
    TIMSK=0x02;                     // 允许 T/C0 比较匹配中断
}

/*ADC初始化*/
void init_ADC(void)
{
    ADMUX |= (1<<REFS0);    //使用AVCC作为参考电压
    ADCSRA |= (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0);  // 使能ADC，设置128分频  
}                                                                                       

/* 读ADC值 */
int read_ADC(char channel)
{
    ADMUX = (ADMUX & 0xE0) | (channel & 0x1F);   // 选择ADC通道
    ADCSRA |= (1<<ADSC);  // 启动转换
    while(ADCSRA & (1<<ADSC));  // 等待转换完成
    return ADC;   
}

/* 加一秒*/
void addTime(void)             
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
 
/*LED扫描显示*/
void display(void)          
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

/*
将频率值转化为 BCD 码并送入显示缓冲区
*/
void freq_to_disbuff(void)               
{ 
    char i; 
    for (i=0;i<=5;i++) 
    { 
        freq_dis_buff[5-i] = freq % 10; 
        freq = freq / 10; 
    } 
}

/*
@brief 外部中断0服务程序，信号下降沿触发
@param:
        temp 当前周期测出的频率值
        freq_cnt     已经测次数
        freq_display_ok  频率显示使能
        freq_display_cnt 频率显示的次数（2ms计数）
        freq            累计计算得到的频率值
*/
void Freq_INT0_ISR(void) 
{         
    int temp;
    int current_count = TCNT1;              // 读取Timer1的值
    TCNT1 = 0x00;                                   // 重置计数器
    temp = F_CPU / current_count;         // 使用当前计数值计算频率
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
        freq = k1 * freq + k2 * temp;
        freq_cnt = 0;
    }
}

// 计数器0比较匹配中断服务程序
void INT_2ms_ISR(void)
{
    if(++_2ms_cnt == 500)             // 一秒
    {   
        adc_value = read_ADC(1);        // 从1通道读取
        temperature = (adc_value * 5.0 / 1024) *100;    // 转换为温度
        addTime();
        _2ms_cnt = 0;
    }
    if(key_display_ok && freq_display_ok)                           // 后来的中断覆盖前面
    {
        bit k = key_display_cnt < freq_display_cnt ? 1 : 0;
        key_display_ok = k;
        key_display_cnt *= k;
        freq_display_ok = ~k;
        freq_display_cnt *= ~k;
    }
    else if(key_display_ok == 1)
        if(++key_display_cnt == 1000)       // 显示2秒
            key_display_ok = 0;
    else if(freq_display_ok == 1)
        if(++freq_display_cnt == 1000)
            freq_display_ok = 0;      
    display();  
}

// 播放音频
void play_tone(uint16_t frequency) 
{
  uint16_t top_value = F_CPU / (8 * frequency) - 1;

  // 设置 ICR1 以确定 PWM 的频率
  ICR1 = top_value;

  // 设置 OCR1A 以确定 PWM 的占空比
  OCR1A = top_value / 2;
}

// 停止音频
void stop_tone() 
{
  // 清除 COM1A1 位以停止音调
  TCCR1A &= ~(1 << COM1A1);
}

// delay
void delay_ms(short t)
{
    while(t--)
        _delay_ms(1);    
}

// 读取按键
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
    init_Timer0();    
    init_ADC();
    
    time[0] = time[1] = time[2] = 0;
    
    while (1)
    {   
        if(_2ms_cnt % 5 == 0)                   // 每10ms检测一次
        {
            int key_temp = read_key();
            if(key_temp != NoKey)
            {      
                if(key_temp == 7)       // 测量频率
                {       
                    init_INT0();
                    init_Timmer1_freq();
                                                   
                }                                 
                else if(key_temp == 8)  // 播放音乐
                {
                    init_Timmer1_music();
                    play_tone(NOTE_C4);
                    delay_ms(500);
                    play_tone(NOTE_D4);
                    delay_ms(500);
                    play_tone(NOTE_E4);
                    delay_ms(500);
                    play_tone(NOTE_F4);
                    delay_ms(500);
                    play_tone(NOTE_G4);
                    delay_ms(500);
                    play_tone(NOTE_A4);
                    delay_ms(500);     
                    play_tone(NOTE_B4);
                    delay_ms(500);
                    stop_tone();
                }
                else                    // 显示数字
                {
                    key_display_ok = 1;
                    key_display_cnt = 0;
                    dis_key = key_temp;
                }             
            }
        }          
    }
}
