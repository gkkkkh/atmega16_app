/*
 * main.c
 *
 * Created: 2023/4/6 21:21:33
 * Author: gkkkh                        PB2         PB0
 *Scan0-Scan3: PA2:PA5      ����         1           5         PA2
                                         2           6         PA3
                                         3           7         PA4
                                         4           8         PA5
 *Key0,Key1:PB0,PB2             ����
 
  
 *2ms:      1.�л������
            2.ɨ����    
            
  ��ʾ��    1.ʱ���루�ް�����
            2.������1~2s ���а�����
            3.�¶�/s ������ֵ������ʾ��
            4.Ƶ�ʲ���
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
                                                      
// ������,�� 3 λΪ ��A������b������-��
flash char led[13]={0x3F,0x06,0x5B,0x4F,0x66,0x6D, 0x7D,0x07,0x7F,0x6F,0x77,0x7C,0x40};
 
char time_dis_buff[6], freq_dis_buff[6], dis_key, posit = 0;
char time[3];
bit key_display_ok = 0�� freq_diplay_ok = 0;
int key_display_cnt = 0, freq_cnt = 0, freq_display_cnt = 0;
volatile unsigned int freq;
volatile short _2ms_cnt = 0;

#define Key_mask   0b00000101
#define NoKey   255  
#define F_CPU   8000000


void init_INT0(void)                //�����ⲿ�ж�
{
    DDRD = 0xFB;                      //����PD2��INT0��Ϊ����
    PORTD = 0x04;                     //����PD2�ϵ���������
                                  
    MCUCR |= (1 << ISC01);            //����INT0������ʽΪ�½���
    GICR |= (1 << INT0);              //�����ⲿ�ж�0
    #asm("sei");                      //����ȫ���ж� 
}                               

void init_Timer1(void)              //��ʼ��������
{
    TCCR1A = 0x00;                    //����Timer1Ϊ����ģʽ
    TCCR1B |= (1 << CS10);            //����ʱ��ԴΪϵͳʱ�ӣ�����Ƶ��
    TCNT1 = 0x00;                     //���������
}

void init_Timer0(void)              // T/C0 ��ʼ��
{ 
    TCCR0=0x0B;                     // �ڲ�ʱ�ӣ�64 ��Ƶ��4M/64=62.5KHz����CTC ģʽ 
    TCNT0=0x00;                     // ����
    OCR0=0x7C;                      // OCR0 = 0x7C(124),(124+1)/62.5=2ms 
    TIMSK=0x02;                     // ���� T/C0 �Ƚ�ƥ���ж�
}

void addTime(void)             // ��һ��
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

void display(void)          // LEDɨ����ʾ
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

void freq_to_disbuff(void)              // ��Ƶ��ֵת��Ϊ BCD �벢������ʾ������ 
{ 
    char i; 
    for (i=0;i<=5;i++) 
    { 
        freq_dis_buff[5-i] = freq % 10; 
        freq = freq / 10; 
    } 
}

// �ⲿ�ж�0�������
#pragma interrupt_handler Freq_INT0_ISR:2
void Freq_INT0_ISR(void) 
{
    int current_count = TCNT1;            // ��ȡTimer1��ֵ
    TCNT1 = 0x00;                         // ���ü�����
    int freq_temp = F_CPU / current_count;         // ʹ�õ�ǰ����ֵ����Ƶ��
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

// ������0�Ƚ�ƥ���жϷ������
#pragma interrupt_handler INT_2ms_ISR:20
void INT_2ms_ISR(void)
{
    if(++_2ms_cnt == 500)
    {
        addTime();
        _2ms_cnt = 0;
    }
    if(key_display_ok == 1)
        if(++key_display_cnt == 1000)       // ��ʾ2��
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
               for(i=0; i<4; i++)                   //ɨ�����
               {
                    PORTA = key_line;               //������ߵ�ƽ
                    PORTA = key_line;               //�������
                    key_value = Key_mask & PINB;    //���е�ƽ
                    if(key_value == Key_mask)       //û�а���
                         key_line <<= 1;
                    else                            //�а���
                    {
                         key_state++;               //����
                         break;
                    }
               }    
               break;
          case 1:
              if(key_value == (Key_mask & PINB))    //�ٴζ��е�ƽ
              {    
                    //��һ��    PA2
                    if(key_line == 0b00000100 && key_value == 0b00000100)   key_return = K1_2;         //PB0
                    if(key_line == 0b00000100 && key_value == 0b00000001)   key_return = K1_1;         //PB2
                    //�ڶ���    PA3
                    if(key_line == 0b00001000 && key_value == 0b00000100)   key_return = K2_2;         //PB0
                    if(key_line == 0b00001000 && key_value == 0b00000001)   key_return = K2_1;         //PB2
                    //������    PA4
                    if(key_line == 0b00010000 && key_value == 0b00000100)   key_return = K3_2;         //PB0
                    if(key_line == 0b00010000 && key_value == 0b00000001)   key_return = K3_1;         //PB2
                    //������     PA5                                                         
                    if(key_line == 0b00100000 && key_value == 0b00000100)   key_return = K4_2;         //PB0
                    if(key_line == 0b00100000 && key_value == 0b00000001)   key_return = K4_1;         //PB2 
                    
                    key_state++;               //������һ�׶�
              }                                              
              else
                    key_state--;
              break;
          case 2:                                   //�ȴ������ͷ�״̬
              PORTA=0b11111100;                     //��������͵�ƽ
              PORTA=0b11111100;                     //������
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
        if(_2ms_cnt % 5 == 0)                   // ÿ10ms���һ��
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
