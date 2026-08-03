/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app/task_sensor.h"
#include "app/task_modbus.h"
#include "app/task_can.h"
#include "app/task_display.h"
#include "app/app_global.h"
#include "utils/ringbuf.h"
#include "drivers/max485.h"
#include "semphr.h"
#include "usb_device.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
volatile uint32_t adc_dma_buf[2];  /* PA0=电流, PA1=光照 */
ringbuf_t g_modbus_rb;             /* USART3 Modbus 接收缓冲区 */
osMutexId_t i2c1MutexHandle;      /* I2C1 总线锁, 定义在这, extern 在 app_global.h */
/* USER CODE END Variables */
/* Definitions for sensorTask */
osThreadId_t sensorTaskHandle;
const osThreadAttr_t sensorTask_attributes = {
  .name = "sensorTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for modbusTask */
osThreadId_t modbusTaskHandle;
const osThreadAttr_t modbusTask_attributes = {
  .name = "modbusTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for canTask */
osThreadId_t canTaskHandle;
const osThreadAttr_t canTask_attributes = {
  .name = "canTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for displayTask */
osThreadId_t displayTaskHandle;
const osThreadAttr_t displayTask_attributes = {
  .name = "displayTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for sensorDataMutex */
osMutexId_t sensorDataMutexHandle;
const osMutexAttr_t sensorDataMutex_attributes = {
  .name = "sensorDataMutex"
};
/* Definitions for modbusRxSemaphore */
osSemaphoreId_t modbusRxSemaphoreHandle;
const osSemaphoreAttr_t modbusRxSemaphore_attributes = {
  .name = "modbusRxSemaphore"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void SensorTaskFunc(void *argument);
void ModbusTaskFunc(void *argument);
void CANTaskFunc(void *argument);
void DisplayTaskFunc(void *argument);

extern void MX_USB_DEVICE_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);
void vApplicationMallocFailedHook(void);

/* USER CODE BEGIN 4 */
volatile uint32_t dbg_stack_overflow = 0;  /* 诊断: 栈溢出触发计数 */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
   (void)xTask;       /* CubeMX 生成, 任务句柄 */
   (void)pcTaskName;  /* CubeMX 生成, 任务名 */
   /* configCHECK_FOR_STACK_OVERFLOW 设为 Method 2 时, 栈溢出会触发这里。
      可以在这里加 LED 闪烁/日志记录来诊断哪个任务栈溢出。 */
   dbg_stack_overflow++;  /* 记录溢出事件 */
}
/* USER CODE END 4 */

/* USER CODE BEGIN 5 */
void vApplicationMallocFailedHook(void)
{
   /* vApplicationMallocFailedHook() will only be called if
   configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h. It is a hook
   function that will get called if a call to pvPortMalloc() fails.
   pvPortMalloc() is called internally by the kernel whenever a task, queue,
   timer or semaphore is created. It is also called by various parts of the
   demo application. If heap_1.c or heap_2.c are used, then the size of the
   heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
   FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
   to query the size of free heap space that remains (although it does not
   provide information on how the remaining heap might be fragmented). */
}
/* USER CODE END 5 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* creation of sensorDataMutex */
  sensorDataMutexHandle = osMutexNew(&sensorDataMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* 保护 I2C1 总线: SensorTask 和 DisplayTask 共用 hi2c1 */
  const osMutexAttr_t i2c1Mutex_attributes = { .name = "i2c1Mutex" };
  i2c1MutexHandle = osMutexNew(&i2c1Mutex_attributes);
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of modbusRxSemaphore */
  modbusRxSemaphoreHandle = osSemaphoreNew(1, 1, &modbusRxSemaphore_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of sensorTask */
  sensorTaskHandle = osThreadNew(SensorTaskFunc, NULL, &sensorTask_attributes);

  /* creation of modbusTask */
  modbusTaskHandle = osThreadNew(ModbusTaskFunc, NULL, &modbusTask_attributes);

  /* creation of canTask */
  canTaskHandle = osThreadNew(CANTaskFunc, NULL, &canTask_attributes);

  /* creation of displayTask */
  displayTaskHandle = osThreadNew(DisplayTaskFunc, NULL, &displayTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/**
 * @brief  USART3 接收事件回调 (IDLE 中断)
 * @note   在 stm32f4xx_it.c 的 USART3_IRQHandler 中由 HAL 调用
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART3 && Size > 0) {
        ringbuf_commit_rx(&g_modbus_rb, Size);
        /* 唤醒 ModbusTask 解析帧 */
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(modbusRxSemaphoreHandle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/* USER CODE END Application */

