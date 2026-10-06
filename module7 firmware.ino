#include <Arduino.h>
#include "stm32f4xx_hal.h"
#include <math.h>

#define N 256
#define HALF 128

ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;
UART_HandleTypeDef huart1;
TIM_HandleTypeDef htim2;

uint16_t adc[N], block[HALF];

uint8_t phase = 0;
uint32_t samples = 0;
uint32_t lastCross = 0;

uint16_t filtered = 0;
bool filterReady = false;
bool below = true;

uint32_t freq = 0;
uint8_t faultCount = 0;

uint32_t avg, rms, minADC, maxADC;
uint32_t lastPrint;

void printNum(uint32_t n)
{
  char s[12];
  int i = 0;

  if (!n) {
    HAL_UART_Transmit(&huart1, (uint8_t*)"0", 1, 100);
    return;
  }

  while (n) {
    s[i++] = '0' + n % 10;
    n /= 10;
  }

  while (i--)
    HAL_UART_Transmit(&huart1, (uint8_t*)&s[i], 1, 100);
}

void printText(const char *s)
{
  while (*s)
    HAL_UART_Transmit(&huart1, (uint8_t*)s++, 1, 100);
}

void process(uint16_t *b)
{
  uint32_t sum = 0;
  uint64_t squares = 0;

  minADC = 4095;
  maxADC = 0;

  for (int i = 0; i < HALF; i++)
  {
    uint16_t x = b[i];

    sum += x;
    squares += (uint32_t)x * x;

    if (x < minADC) minADC = x;
    if (x > maxADC) maxADC = x;
  }

  avg = sum / HALF;

  uint32_t rmsADC =
    (uint32_t)sqrt((double)squares / HALF);

  rms = (rmsADC * 3300UL) / 4095UL;

  for (int i = 0; i < HALF; i++)
  {
    uint16_t x = b[i];

    if (!filterReady)
    {
      filtered = x;
      filterReady = true;
    }
    else
    {
      filtered += ((int32_t)x - filtered) / 4;
    }

    if (below && filtered > avg + 20)
    {
      uint32_t crossing = samples + i;

      if (lastCross)
      {
        uint32_t period = crossing - lastCross;
        if (period)
          freq = 10000UL / period;
      }

      lastCross = crossing;
      below = false;
    }

    if (!below && filtered < avg - 20)
      below = true;
  }

  samples += HALF;

  if (minADC <= 20 || maxADC >= 4075)
  {
    if (faultCount < 10)
      faultCount++;
  }
  else
  {
    faultCount = 0;
  }
}

void printOutput()
{
  printText("\r\n-----------------------------\r\n");

  printText("ADC AVERAGE: ");
  printNum(avg);
  printText(" | ");
  printNum((avg * 3300UL) / 4095UL);
  printText(" mV\r\n");

  printText("RMS VOLTAGE: ");
  printNum(rms);
  printText(" mV\r\n");

  printText("FILTERED ADC: ");
  printNum(filtered);
  printText("\r\n");

  printText("FREQUENCY: ");
  printNum(freq);
  printText(" Hz\r\n");

  printText("MIN ADC: ");
  printNum(minADC);
  printText("\r\n");

  printText("MAX ADC: ");
  printNum(maxADC);
  printText("\r\n");

  printText("SENSOR FAULT: ");
  printText(faultCount >= 10 ? "YES\r\n" : "NO\r\n");
}

void UART_Init()
{
  __HAL_RCC_USART1_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitTypeDef g = {};

  g.Pin = GPIO_PIN_9 | GPIO_PIN_10;
  g.Mode = GPIO_MODE_AF_PP;
  g.Pull = GPIO_PULLUP;
  g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  g.Alternate = GPIO_AF7_USART1;

  HAL_GPIO_Init(GPIOA, &g);

  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;

  HAL_UART_Init(&huart1);
}

void TIM2_Init()
{
  __HAL_RCC_TIM2_CLK_ENABLE();

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 83;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 99;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;

  HAL_TIM_Base_Init(&htim2);

  TIM_MasterConfigTypeDef m = {};

  m.MasterOutputTrigger = TIM_TRGO_UPDATE;
  m.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

  HAL_TIMEx_MasterConfigSynchronization(&htim2, &m);
}

void ADC_Init()
{
  __HAL_RCC_ADC1_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();

  GPIO_InitTypeDef g = {};

  g.Pin = GPIO_PIN_0;
  g.Mode = GPIO_MODE_ANALOG;
  g.Pull = GPIO_NOPULL;

  HAL_GPIO_Init(GPIOA, &g);

  hdma_adc1.Instance = DMA2_Stream0;
  hdma_adc1.Init.Channel = DMA_CHANNEL_0;
  hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
  hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;
  hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  hdma_adc1.Init.Mode = DMA_CIRCULAR;
  hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;
  hdma_adc1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

  HAL_DMA_Init(&hdma_adc1);
  __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);

  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = ENABLE;

  HAL_ADC_Init(&hadc1);

  ADC_ChannelConfTypeDef c = {};

  c.Channel = ADC_CHANNEL_0;
  c.Rank = 1;
  c.SamplingTime = ADC_SAMPLETIME_84CYCLES;

  HAL_ADC_ConfigChannel(&hadc1, &c);

  HAL_ADC_Start_DMA(
    &hadc1,
    (uint32_t*)adc,
    N
  );
}

void setup()
{
  UART_Init();
  TIM2_Init();
  ADC_Init();

  printText("MODULE 7 STARTED\r\n");

  HAL_TIM_Base_Start(&htim2);

  lastPrint = millis();
}

void loop()
{
  uint32_t left =
    __HAL_DMA_GET_COUNTER(&hdma_adc1);

  if (left <= HALF && phase == 0)
  {
    memcpy(block, adc, sizeof(block));
    phase = 1;
    process(block);
  }

  if (left > HALF && phase == 1)
  {
    memcpy(block, &adc[HALF], sizeof(block));
    phase = 0;
    process(block);
  }

  if (millis() - lastPrint >= 500)
  {
    lastPrint = millis();
    printOutput();
  }
}
