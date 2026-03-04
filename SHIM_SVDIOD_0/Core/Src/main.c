/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "main.h"
#include <stdio.h>
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lcd_i2c.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define KEY_1 GPIO_PIN_15
#define KEY_2 GPIO_PIN_14
#define KEY_3 GPIO_PIN_13
#define KEY_4 GPIO_PIN_12

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */

LCD_I2C_HandleTypeDef hlcd;

// Состояние кнопок для антидребезга
typedef enum {
    KEY_STATE_RELEASED,
    KEY_STATE_PRESSED,
    KEY_STATE_HOLD
} KeyState_t;

typedef struct {
    uint8_t pin;
    KeyState_t state;      // Текущее состояние
    uint32_t press_time;   // Время последнего нажатия (в тиках)
    uint8_t is_pressed;    // Флаг, что кнопка нажата в данный момент
    uint32_t last_process_time; // Время последней обработки удержания
} KeyDebounce_t;

KeyDebounce_t key1 = {.pin = KEY_1, .state = KEY_STATE_RELEASED, .is_pressed = 0};
KeyDebounce_t key2 = {.pin = KEY_2, .state = KEY_STATE_RELEASED, .is_pressed = 0};
KeyDebounce_t key3 = {.pin = KEY_3, .state = KEY_STATE_RELEASED, .is_pressed = 0};
KeyDebounce_t key4 = {.pin = KEY_4, .state = KEY_STATE_RELEASED, .is_pressed = 0};

#define DEBOUNCE_TIME_MS    20   // Время антидребезга
#define HOLD_DELAY_MS       400  // Задержка перед началом ускоренного изменения
#define REPEAT_DELAY_MS     50   // Интервал изменения при удержании

// Состояния меню
typedef enum {
    MENU_MODE_SELECT,   // Выбор режима (грубо/плавно)
    MENU_STEP_SELECT,   // Выбор шага (для грубого режима)
    MENU_RUN_COARSE,    // Рабочий режим "Грубо"
    MENU_RUN_SMOOTH     // Рабочий режим "Плавно"
} MenuState_t;

MenuState_t menu_state = MENU_MODE_SELECT;
uint8_t step_size = 10; // Шаг для грубого режима (10% по умолчанию)
uint8_t current_step_select_index = 0; // 0 для 1%, 1 для 10%
const uint8_t step_options[] = {1, 10};

// Флаги для меню
uint8_t need_menu_redraw = 1;

uint8_t current_duty = 30; // начальное значение
uint8_t last_displayed_duty = 255; // для отслеживания изменений

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_I2C1_Init(void);

/* USER CODE BEGIN PFP */

void process_key(KeyDebounce_t *key) {
    uint8_t physical_state = (HAL_GPIO_ReadPin(GPIOB, key->pin) == GPIO_PIN_RESET);
    uint32_t now = HAL_GetTick();

    switch (key->state) {
        case KEY_STATE_RELEASED:
            if (physical_state) {
                // Потенциальное нажатие
                key->press_time = now;
                key->state = KEY_STATE_PRESSED;
            }
            break;

        case KEY_STATE_PRESSED:
            if (!physical_state) {
                // Ложное срабатывание
                key->state = KEY_STATE_RELEASED;
            } else if ((now - key->press_time) > DEBOUNCE_TIME_MS) {
                // Нажатие подтверждено
                key->state = KEY_STATE_HOLD;
                key->is_pressed = 1; // Сигнал для обработки в главном цикле
                key->press_time = now; // Сброс времени для отсчета HOLD_DELAY
                key->last_process_time = now;
            }
            break;

        case KEY_STATE_HOLD:
            if (!physical_state) {
                // Кнопка отпущена
                key->state = KEY_STATE_RELEASED;
                key->is_pressed = 0;
            }
            break;
    }
}

int8_t is_hold_action_needed(KeyDebounce_t *key) {
    if (key->state != KEY_STATE_HOLD) return 0;
    uint32_t now = HAL_GetTick();

    // Проверяем, прошла ли задержка перед началом ускорения
    if ((now - key->press_time) > HOLD_DELAY_MS) {
        // Проверяем, прошло ли достаточно времени с последнего изменения
        if ((now - key->last_process_time) > REPEAT_DELAY_MS) {
            key->last_process_time = now;
            return 1;
        }
    }
    return 0;
}

void set_push_pull_duty(uint8_t percent)
{
    if (percent > 100) percent = 100;
    uint32_t max_val = __HAL_TIM_GET_AUTORELOAD(&htim2); //опрос регистра ARR
    uint32_t val = (percent * max_val) / 100;

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, val);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, max_val - val);
}

void set_led_brightness(uint8_t percent)
{
    if (percent > 100) percent = 100;
    uint32_t max_val = __HAL_TIM_GET_AUTORELOAD(&htim2);
    uint32_t val = (percent * max_val) / 100;
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, val);
}

// Функция обновления дисплея
void update_display(uint8_t duty)
{
    char buffer[17]; // для строки 16 символов

    // Первая строка - значение ШИМ
        LCD_I2C_SetCursor(&hlcd, 0, 0);
        sprintf(buffer, "PWM: %3d%%    ", duty);
        LCD_I2C_Print(&hlcd, buffer);

    }

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */

  // Инициализация дисплея
   LCD_I2C_Init(&hlcd, &hi2c1, 0x27);
   LCD_I2C_Clear(&hlcd);

    // Установка начальных значений
    set_push_pull_duty(current_duty);
    set_led_brightness(current_duty);
    update_display(current_duty);
    last_displayed_duty = current_duty;

  //Ш�?М и светодиод
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  // 1. Обрабатываем состояния кнопок (вызываем для каждой)
	        process_key(&key1);
	        process_key(&key2);
	        process_key(&key3);
	        process_key(&key4);

	        // 2. Обработка в зависимости от состояния меню
	        switch (menu_state) {
	            case MENU_MODE_SELECT:
	                if (need_menu_redraw) {
	                    LCD_I2C_Clear(&hlcd);
	                    LCD_I2C_SetCursor(&hlcd, 0, 0);
	                    LCD_I2C_Print(&hlcd, "Mode: 1.Coarse");
	                    LCD_I2C_SetCursor(&hlcd, 0, 1);
	                    LCD_I2C_Print(&hlcd, "      2.Smooth");
	                    need_menu_redraw = 0;
	                }

	                if (key1.is_pressed) { // Кнопка 1 - выбор "Грубо"
	                    menu_state = MENU_STEP_SELECT;
	                    current_step_select_index = 1; // По умолчанию 10%
	                    step_size = step_options[current_step_select_index];
	                    need_menu_redraw = 1;
	                    key1.is_pressed = 0; // Сбрасываем флаг
	                }
	                if (key2.is_pressed) { // Кнопка 2 - выбор "Плавно"
	                    menu_state = MENU_RUN_SMOOTH;
	                    LCD_I2C_Clear(&hlcd);
	                    need_menu_redraw = 1; // Чтобы обновился основной экран
	                    key2.is_pressed = 0;
	                }
	                break;

	            case MENU_STEP_SELECT:
	                if (need_menu_redraw) {
	                    LCD_I2C_Clear(&hlcd);
	                    LCD_I2C_SetCursor(&hlcd, 0, 0);
	                    LCD_I2C_Print(&hlcd, "Step: 1=1% 2=10%");
	                    LCD_I2C_SetCursor(&hlcd, 0, 1);
	                    LCD_I2C_Print(&hlcd, "Current:   ");
	                    LCD_I2C_SetCursor(&hlcd, 9, 1);
	                    char buf[3];
	                    sprintf(buf, "%d%%", step_options[current_step_select_index]);
	                    LCD_I2C_Print(&hlcd, buf);
	                    need_menu_redraw = 0;
	                }
	                // Навигация по шагам
	                if (key3.is_pressed) { // Кнопка 3 - выбор 1%
	                    current_step_select_index = 0;
	                    step_size = step_options[current_step_select_index];
	                    need_menu_redraw = 1;
	                    key3.is_pressed = 0;
	                }
	                if (key4.is_pressed) { // Кнопка 4 - выбор 10%
	                    current_step_select_index = 1;
	                    step_size = step_options[current_step_select_index];
	                    need_menu_redraw = 1;
	                    key4.is_pressed = 0;
	                }
	                // Подтверждение выбора (например, кнопкой 1)
	                if (key1.is_pressed) {
	                    step_size = step_options[current_step_select_index];
	                    menu_state = MENU_RUN_COARSE;
	                    LCD_I2C_Clear(&hlcd);
	                    need_menu_redraw = 1;
	                    key1.is_pressed = 0;
	                }
	                break;

	            case MENU_RUN_COARSE:
	                {
	                    uint8_t need_update = 0;

	                    // Обработка нажатий (однократных)
	                    if (key1.is_pressed) {
	                        if (current_duty + step_size <= 100) {
	                            current_duty += step_size;
	                            need_update = 1;
	                        }
	                        key1.is_pressed = 0;
	                    }
	                    if (key2.is_pressed) {
	                        if (current_duty >= step_size) {
	                            current_duty -= step_size;
	                        } else {
	                            current_duty = 0;
	                        }
	                        need_update = 1;
	                        key2.is_pressed = 0;
	                    }
	                    // Обработка удержания
	                    if (is_hold_action_needed(&key1)) {
	                        if (current_duty + 1 <= 100) current_duty += 1;
	                        need_update = 1;
	                    }
	                    if (is_hold_action_needed(&key2)) {
	                        if (current_duty >= 1) current_duty -= 1;
	                        else current_duty = 0;
	                        need_update = 1;
	                    }

	                    // Кнопки для смены шага (3 и 4)
	                    if (key3.is_pressed) {
	                        step_size = 1;
	                        key3.is_pressed = 0;
	                        // Можно кратковременно показать сообщение на дисплее
	                    }
	                    if (key4.is_pressed) {
	                        step_size = 10;
	                        key4.is_pressed = 0;
	                    }

	                    if (need_update) {
	                        set_push_pull_duty(current_duty);
	                        set_led_brightness(current_duty);
	                        update_display(current_duty);
	                    }
	                }
	                break;

	            case MENU_RUN_SMOOTH:
	                {
	                    uint8_t need_update = 0;
	                    // В плавном режиме работаем только через удержание
	                    if (is_hold_action_needed(&key1)) {
	                        if (current_duty < 100) current_duty++;
	                        need_update = 1;
	                    }
	                    if (is_hold_action_needed(&key2)) {
	                        if (current_duty > 0) current_duty--;
	                        need_update = 1;
	                    }
	                    // Кнопки 3 и 4 можно использовать для выхода в меню или сброса
	                    if (key3.is_pressed) {
	                        current_duty = 0;
	                        need_update = 1;
	                        key3.is_pressed = 0;
	                    }
	                    if (key4.is_pressed) {
	                        current_duty = 100;
	                        need_update = 1;
	                        key4.is_pressed = 0;
	                    }

	                    if (need_update) {
	                        set_push_pull_duty(current_duty);
	                        set_led_brightness(current_duty);
	                        update_display(current_duty);
	                    }
	                }
	                break;

	            default:
	                break;
	        }

	        HAL_Delay(10);

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 71;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 99;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM2;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pins : PB12 PB13 PB14 PB15 */
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */



/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
