/*
 * @Description: 
 * @Author: TOTHTOT
 * @Date: 2022-05-22 15:46:32
 * @LastEditTime: 2022-05-22 19:29:36
 * @LastEditors: TOTHTOT
 * @FilePath: \USER\main.c
 */
/* SYSTEM */
#include "sys.h"
#include "delay.h"
/* HARDWARE */
#include "led.h"
#include "dsp.h"
#include "fft.h"
#include "timer.h"
#include "oled.h"
/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"
#include "list.h"
#include "queue.h"
#include "semphr.h"

//�������ȼ�
#define START_TASK_PRIO 1
//�����ջ��С
#define START_STK_SIZE 128
//������
TaskHandle_t StartTask_Handler;
//������
void start_task(void *pvParameters);

//�������ȼ�
#define LED0_TASK_PRIO 2
//�����ջ��С
#define LED0_STK_SIZE 50
//������
TaskHandle_t LED0Task_Handler;
//������
void led0_task(void *pvParameters);

//�������ȼ�
#define LED1_TASK_PRIO 3
//�����ջ��С
#define LED1_STK_SIZE 50
//������
TaskHandle_t LED1Task_Handler;
//������
void led1_task(void *pvParameters);

//�������ȼ�
#define FFT_TASK_PRIO 6
//�����ջ��С
#define FFT_STK_SIZE 150
//������
TaskHandle_t FFTTask_Handler;
//������
void FFT_task(void *pvParameters);

//�������ȼ�
#define OLED_TASK_PRIO 5
//�����ջ��С
#define OLED_STK_SIZE 250
//������
TaskHandle_t OLEDTask_Handler;
//������
void oled_task(void *pvParameters);


/*���ٸ���Ҷ�任����*/
#define NPT 256

long IBufLedArray_tmp[NPT/4];

u16 NPT_Cnt=0;

/*�����ֵ�ź������*/
SemaphoreHandle_t FFT_Semaphore;
SemaphoreHandle_t FFTMag_Semaphore;
SemaphoreHandle_t OLED_Semaphore;
/*....................................*/

int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4); //����ϵͳ�ж����ȼ�����4
	AllClock_Init();								//��ʼ��ϵͳʱ��
	ALL_NVIC_Configuration();						//�ж�����
	delay_init();									//��ʱ������ʼ��
	uart_init(921600);								//��ʼ������
	LED_Init();										//��ʼ��LED
	OLED_Init();									//��ʼ��OLED
	OLED_Clear();									//����
	OLED_Set_Pos(0, 0);								//���ù��λ��
	OLED_WR_Byte(0x01,OLED_DATA);
	// TIM2_Configuration(72 - 1, 44 - 1);				// 100 10k 50 20k  ������
	// TIM4_Configuration(2500 - 1, 9 - 1);			// 10ms ��Ļˢ��
	//������ʼ����
	xTaskCreate((TaskFunction_t)start_task,			 //������
				(const char *)"start_task",			 //��������
				(uint16_t)START_STK_SIZE,			 //�����ջ��С
				(void *)NULL,						 //���ݸ��������Ĳ���
				(UBaseType_t)START_TASK_PRIO,		 //�������ȼ�
				(TaskHandle_t *)&StartTask_Handler); //������
	vTaskStartScheduler();							 //�����������
}

//��ʼ����������
void start_task(void *pvParameters)
{
	taskENTER_CRITICAL(); //�����ٽ���
	/*��ʼ���ź���*/
	FFT_Semaphore = xSemaphoreCreateBinary();
	if (FFT_Semaphore == NULL)
	{
		printf("FFT_Semaphore Creat err!\r\n");
	}
	FFTMag_Semaphore = xSemaphoreCreateBinary();
	if (FFTMag_Semaphore == NULL)
	{
		printf("FFTMag_Semaphore Creat err!\r\n");
	}
	OLED_Semaphore = xSemaphoreCreateBinary();
	if (OLED_Semaphore == NULL)
	{
		printf("FFTMag_Semaphore Creat err!\r\n");
	}
	//����LED0����
	xTaskCreate((TaskFunction_t)led0_task,
				(const char *)"led0_task",
				(uint16_t)LED0_STK_SIZE,
				(void *)NULL,
				(UBaseType_t)LED0_TASK_PRIO,
				(TaskHandle_t *)&LED0Task_Handler);
/* 	//����LED1����
	xTaskCreate((TaskFunction_t)led1_task,
				(const char *)"led1_task",
				(uint16_t)LED1_STK_SIZE,
				(void *)NULL,
				(UBaseType_t)LED1_TASK_PRIO,
				(TaskHandle_t *)&LED1Task_Handler); */
	//��������
	xTaskCreate((TaskFunction_t )FFT_task,     	
							(const char*    )"FFT_task",   	
							(uint16_t       )FFT_STK_SIZE, 
							(void*          )NULL,				
							(UBaseType_t    )FFT_TASK_PRIO,	
							(TaskHandle_t*  )&FFTTask_Handler);
	//��������
	xTaskCreate((TaskFunction_t )oled_task,     	
							(const char*    )"oled_task",   	
							(uint16_t       )OLED_STK_SIZE, 
							(void*          )NULL,				
							(UBaseType_t    )OLED_TASK_PRIO,	
							(TaskHandle_t*  )&OLEDTask_Handler);
	vTaskDelete(StartTask_Handler); //ɾ����ʼ����
	taskEXIT_CRITICAL();			//�˳��ٽ���
}

//LED0������
void led0_task(void *pvParameters)
{
	while (1)
	{
		LED0 = ~LED0;
		vTaskDelay(500);
	}
}

//LED1������
void led1_task(void *pvParameters)
{
	while (1)
	{
		LED1 = 0;
		vTaskDelay(200);
		LED1 = 1;
		vTaskDelay(800);
	}
}

/**
 * @name: FFT_task
 * @msg: ����fft
 * @param undefined
 * @return {*}
 */
void FFT_task(void *pvParameters)
{
	printf("FFT_task start!\r\n");
	while (1)
	{
		//�ȴ��ź���
		// if (xSemaphoreTake(FFT_Semaphore, portMAX_DELAY) == pdTRUE)
		// {

		printf("FFT_task running!\r\n");
		taskENTER_CRITICAL(); //�����ٽ���

		// ��������
		InitBufInArray();
		// ����FFT
		cr4_fft_256_stm32(lBufOutArray, lBufInArray, NPT);
		//�����ֵ
		GetPowerMag();
		taskEXIT_CRITICAL(); //�˳��ٽ���

		//�ͷ��ź���,oled����
		xSemaphoreGive(OLED_Semaphore);
		// }
		delay_ms(1000);
	}
}

/**
 * @name: oled_task
 * @msg: oled����,��ʾ��������
 * @param undefined
 * @return {*}
 */
void oled_task(void *pvParameters)
{
	u16 i = 0;
	printf("oled_task start!\r\n");
	while (1)
	{
		//�ȴ��ź���
		if (xSemaphoreTake(OLED_Semaphore, portMAX_DELAY) == pdTRUE)
		{
			/* for (i = 0; i < NPT; i++)
			{
				printf("%6d\r\n", (s32)(lBufInArray[i] >> 16));
			} */
			//��ʾ����
			printf("  ���   Ƶ��   ��ֵ   ʵ��   �鲿\r\n");
			for (i = 0; i < NPT / 2; i++)
			{
				// printf("%6d,%6d,%6d,%6d,%6d\r\n", i, Fs * i / NPT, (u32)lBufMagArray[i], (s16)(lBufOutArray[i] & 0x0000ffff), (s16)(lBufOutArray[i] >> 16));
				printf("%6d,%6d,%6d,%6d,%6d\r\n", i, Fs * i / NPT, (u32)lBufMagArray[i], (s16)(lBufOutArray[i] & 0x0000ffff), (s16)(lBufOutArray[i] >> 16));
			}
			lBufMagArray[0] = lBufMagArray[1];
			for (i = 0; i < NPT / 4; i++)
			{
				IBufLedArray_tmp[i] = (((lBufMagArray[2 * i] + lBufMagArray[2 * i + 1]) / 2) * (32 / 100.0));
				if (IBufLedArray_tmp[i] > 31)
					IBufLedArray_tmp[i] = 31;
				oled_draw_line(i, 0, IBufLedArray_tmp[i]);
			}
		}
		delay_ms(10);
	}
}
