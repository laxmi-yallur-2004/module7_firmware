#include <Arduino.h>
#include "stm32f4xx_hal.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define TEST_PWM_FREQ_HZ      0UL    

#define N                     256
#define HALF                  (N / 2)

#define SAMPLE_RATE_HZ        10000UL
#define VREF_MV               3300UL
#define ADC_MAX               4095UL

#define FREQ_TIMEOUT_SECONDS  2UL
#define THRESH_WINDOW_BLOCKS  156
#define MIN_P2P_ADC           80
#define MIN_HYST_ADC          20

#define FILT_FRAC             8

#define SENSOR_AVG_MIN_ADC    100
#define SENSOR_AVG_MAX_ADC    4000
#define CLIP_LOW_ADC          2
#define CLIP_HIGH_ADC         4093
#define CLIP_FAULT_PERCENT    25
#define FAULT_SET_BLOCKS      10
#define FAULT_CLEAR_BLOCKS    10

#define REPORT_INTERVAL_MS    500

ADC_HandleTypeDef  hadc1;
DMA_HandleTypeDef  hdma_adc1;
UART_HandleTypeDef huart1;
TIM_HandleTypeDef  htim2;
TIM_HandleTypeDef  htim3;

uint16_t adc[N];
uint16_t block[HALF];

static uint8_t  phase = 0;
static uint32_t samples = 0;
static uint32_t missedBlocks = 0;
static uint32_t adcOverruns = 0;
static uint32_t txDropped = 0;

static uint32_t sampleRateHz = 0;
static uint32_t timerClockHz = 0;

static int32_t  filtQ = 0;
static bool     filterReady = false;
static int32_t  filtered = 0;
static bool     below = true;
static bool     haveRef = false;
static uint32_t lastCross = 0;
static uint32_t freq_mHz = 0;

static bool     sigValid = false;
static int32_t  thrHigh = 0, thrLow = 0;
static int32_t  winMin = (int32_t)ADC_MAX, winMax = 0;
static uint16_t winBlocks = 0;

static int32_t  avg = 0;
static uint32_t rmsTotal_mV = 0, rmsAC_mV = 0;
static uint32_t minADC = 0, maxADC = 0;

static bool     sensorFault = false;
static uint8_t  faultReason = 0;
static uint8_t  badBlocks = 0, goodBlocks = 0;

static uint32_t lastPrint = 0;

#define TXQ_SIZE 512
static uint8_t  txq[TXQ_SIZE];
static uint16_t txHead = 0, txTail = 0;

static uint16_t txFree()
{
  return (uint16_t)((txTail - txHead - 1) & (TXQ_SIZE - 1));
}

static bool txEnqueue(const char *s, uint16_t n)
{
  if (n > txFree()) return false;
  for (uint16_t i = 0; i < n; i++) {
    txq[txHead] = (uint8_t)s[i];
    txHead = (uint16_t)((txHead + 1) & (TXQ_SIZE - 1));
  }
  return true;
}

static void txPump()
{
  while (txTail != txHead && __HAL_UART_GET_FLAG(&huart1, UART_FLAG_TXE)) {
    huart1.Instance->DR = txq[txTail];
    txTail = (uint16_t)((txTail + 1) & (TXQ_SIZE - 1));
  }
}

static void printBlocking(const char *s)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)strlen(s), 200);
}

static void fatal(const char *msg)
{
#ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);
#endif
  uint32_t last = millis() - 500;
  while (true) {
    if (millis() - last >= 500) {
      last = millis();
      printBlocking("FATAL: ");
      printBlocking(msg);
      printBlocking("\r\n");
#ifdef LED_BUILTIN
      digitalToggle(LED_BUILTIN);
#endif
    }
  }
}

static void resetFrequencyReference()
{
  haveRef = false;
  freq_mHz = 0;
}

static void updateThreshold()
{
  int32_t p2p = winMax - winMin;

  if (p2p < MIN_P2P_ADC) {
    sigValid = false;
    resetFrequencyReference();
  } else {
    int32_t mid  = (winMax + winMin) / 2;
    int32_t hyst = p2p / 6;
    if (hyst < MIN_HYST_ADC) hyst = MIN_HYST_ADC;

    thrHigh = mid + hyst;
    thrLow  = mid - hyst;

    if (!sigValid) {
      below = (filtered < mid);
      sigValid = true;
    }
  }

  winMin = (int32_t)ADC_MAX;
  winMax = 0;
  winBlocks = 0;
}

static void process(const uint16_t *b)
{
  uint32_t sum = 0;
  uint64_t squares = 0;
  uint32_t mn = ADC_MAX, mx = 0, clipped = 0;

  for (int i = 0; i < HALF; i++) {
    uint32_t x = b[i];
    sum += x;
    squares += (uint64_t)x * x;
    if (x < mn) mn = x;
    if (x > mx) mx = x;
    if (x <= CLIP_LOW_ADC || x >= CLIP_HIGH_ADC) clipped++;
  }

  minADC = mn;
  maxADC = mx;
  avg = (int32_t)(sum / HALF);

  float msTotal = (float)(squares / HALF);
  rmsTotal_mV = (uint32_t)(sqrtf(msTotal) * (float)VREF_MV / (float)ADC_MAX + 0.5f);

  uint64_t num = squares * HALF - (uint64_t)sum * sum;
  float var = (float)num / (float)(HALF * HALF);
  rmsAC_mV = (uint32_t)(sqrtf(var) * (float)VREF_MV / (float)ADC_MAX + 0.5f);

  for (int i = 0; i < HALF; i++) {
    int32_t x = (int32_t)b[i];

    if (!filterReady) {
      filtQ = x << FILT_FRAC;
      filterReady = true;
    } else {
      filtQ += ((x << FILT_FRAC) - filtQ) >> 2;
    }
    filtered = (filtQ + (1 << (FILT_FRAC - 1))) >> FILT_FRAC;

    if (filtered < winMin) winMin = filtered;
    if (filtered > winMax) winMax = filtered;

    if (!sigValid) continue;

    if (below && filtered > thrHigh) {
      uint32_t crossing = samples + (uint32_t)i;

      if (haveRef) {
        uint32_t period = crossing - lastCross;
        if (period)
          freq_mHz = (uint32_t)(((uint64_t)sampleRateHz * 1000ULL + period / 2) / period);
      }
      lastCross = crossing;
      haveRef = true;
      below = false;
    }
    else if (!below && filtered < thrLow) {
      below = true;
    }
  }

  samples += HALF;

  if (++winBlocks >= THRESH_WINDOW_BLOCKS)
    updateThreshold();

  if (haveRef && (samples - lastCross) > FREQ_TIMEOUT_SECONDS * sampleRateHz)
    resetFrequencyReference();

  uint8_t reason = 0;
  if (avg < SENSOR_AVG_MIN_ADC)                      reason = 1;
  else if (avg > SENSOR_AVG_MAX_ADC)                 reason = 2;
  else if (clipped * 100UL >= (uint32_t)HALF * CLIP_FAULT_PERCENT) reason = 3;

  if (reason) {
    goodBlocks = 0;
    if (badBlocks < 255) badBlocks++;
    if (badBlocks >= FAULT_SET_BLOCKS) { sensorFault = true; faultReason = reason; }
  } else {
    badBlocks = 0;
    if (goodBlocks < 255) goodBlocks++;
    if (goodBlocks >= FAULT_CLEAR_BLOCKS) { sensorFault = false; faultReason = 0; }
  }
}

static void pollDma()
{
  if (__HAL_ADC_GET_FLAG(&hadc1, ADC_FLAG_OVR)) {
    adcOverruns++;
    __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_OVR);
  }

  bool ht = __HAL_DMA_GET_FLAG(&hdma_adc1, DMA_FLAG_HTIF0_4);
  bool tc = __HAL_DMA_GET_FLAG(&hdma_adc1, DMA_FLAG_TCIF0_4);

  if (!ht && !tc) return;

  if (phase == 0) {
    if (ht && tc) {
      __HAL_DMA_CLEAR_FLAG(&hdma_adc1, DMA_FLAG_HTIF0_4);
      __HAL_DMA_CLEAR_FLAG(&hdma_adc1, DMA_FLAG_TCIF0_4);
      missedBlocks++;
      samples += HALF;
      resetFrequencyReference();
      memcpy(block, &adc[HALF], sizeof(block));
      process(block);
      phase = 0;
    } else if (ht) {
      __HAL_DMA_CLEAR_FLAG(&hdma_adc1, DMA_FLAG_HTIF0_4);
      memcpy(block, adc, sizeof(block));
      process(block);
      phase = 1;
    } else {
      __HAL_DMA_CLEAR_FLAG(&hdma_adc1, DMA_FLAG_TCIF0_4);
    }
  } else {
    if (tc && ht) {
      __HAL_DMA_CLEAR_FLAG(&hdma_adc1, DMA_FLAG_HTIF0_4);
      __HAL_DMA_CLEAR_FLAG(&hdma_adc1, DMA_FLAG_TCIF0_4);
      missedBlocks++;
      samples += HALF;
      resetFrequencyReference();
      memcpy(block, adc, sizeof(block));
      process(block);
      phase = 1;
    } else if (tc) {
      __HAL_DMA_CLEAR_FLAG(&hdma_adc1, DMA_FLAG_TCIF0_4);
      memcpy(block, &adc[HALF], sizeof(block));
      process(block);
      phase = 0;
    }
  }
}

static void queueReport()
{
  static char rep[400];

  uint32_t avg_mV = ((uint32_t)avg * VREF_MV) / ADC_MAX;
  const char *faultText =
      !sensorFault ? "NO" :
      faultReason == 1 ? "YES (low / disconnected)" :
      faultReason == 2 ? "YES (high)" : "YES (clipping)";

  int n = snprintf(rep, sizeof(rep),
    "\r\n-----------------------------\r\n"
    "ADC AVERAGE: %ld | %lu mV\r\n"
    "RMS TOTAL (with DC): %lu mV\r\n"
    "RMS AC ONLY: %lu mV\r\n"
    "FILTERED ADC: %ld\r\n"
    "FREQUENCY: %lu.%02lu Hz\r\n"
    "MIN ADC: %lu\r\n"
    "MAX ADC: %lu\r\n"
    "SENSOR FAULT: %s\r\n"
    "MISSED BLOCKS: %lu | ADC OVR: %lu | TX DROPPED: %lu\r\n",
    (long)avg, (unsigned long)avg_mV,
    (unsigned long)rmsTotal_mV,
    (unsigned long)rmsAC_mV,
    (long)filtered,
    (unsigned long)(freq_mHz / 1000), (unsigned long)((freq_mHz % 1000) / 10),
    (unsigned long)minADC,
    (unsigned long)maxADC,
    faultText,
    (unsigned long)missedBlocks, (unsigned long)adcOverruns, (unsigned long)txDropped);

  if (n <= 0) return;
  if (n >= (int)sizeof(rep)) n = sizeof(rep) - 1;

  if (!txEnqueue(rep, (uint16_t)n))
    txDropped++;
}

static bool UART_Init()
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

  return HAL_UART_Init(&huart1) == HAL_OK;
}

static uint32_t getTim2ClockHz()
{
  uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
  bool apb1Div1 = ((RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV1);
  return apb1Div1 ? pclk1 : pclk1 * 2UL;
}

static const char *TIM2_Init()
{
  timerClockHz = getTim2ClockHz();

  if (timerClockHz == 0 || (timerClockHz % SAMPLE_RATE_HZ) != 0)
    return "TIM2 clock not an integer multiple of sample rate";

  uint32_t ticks = timerClockHz / SAMPLE_RATE_HZ;
  sampleRateHz = timerClockHz / ticks;

  __HAL_RCC_TIM2_CLK_ENABLE();

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = ticks - 1;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  if (HAL_TIM_Base_Init(&htim2) != HAL_OK) return "TIM2 base init failed";

  TIM_MasterConfigTypeDef m = {};
  m.MasterOutputTrigger = TIM_TRGO_UPDATE;
  m.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &m) != HAL_OK)
    return "TIM2 master config failed";

  return nullptr;
}

static const char *ADC_Init()
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

  if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) return "DMA init failed";
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

  if (HAL_ADC_Init(&hadc1) != HAL_OK) return "ADC init failed";

  ADC_ChannelConfTypeDef c = {};
  c.Channel = ADC_CHANNEL_0;
  c.Rank = 1;
  c.SamplingTime = ADC_SAMPLETIME_84CYCLES;

  if (HAL_ADC_ConfigChannel(&hadc1, &c) != HAL_OK) return "ADC channel config failed";

  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc, N) != HAL_OK)
    return "ADC DMA start failed";

  return nullptr;
}

static const char *TestPWM_Init()
{
  if (TEST_PWM_FREQ_HZ == 0) return nullptr;
  if ((timerClockHz % 100000UL) != 0) return "PWM timer clock not a multiple of 100 kHz";

  __HAL_RCC_TIM3_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitTypeDef g = {};
  g.Pin = GPIO_PIN_6;
  g.Mode = GPIO_MODE_AF_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  g.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(GPIOA, &g);

  uint32_t arr = 100000UL / TEST_PWM_FREQ_HZ - 1;

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = timerClockHz / 100000UL - 1;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = arr;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) return "TIM3 PWM init failed";

  TIM_OC_InitTypeDef oc = {};
  oc.OCMode = TIM_OCMODE_PWM1;
  oc.Pulse = (arr + 1) / 2;
  oc.OCPolarity = TIM_OCPOLARITY_HIGH;
  oc.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_1) != HAL_OK)
    return "TIM3 channel config failed";

  if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK)
    return "TIM3 start failed";

  return nullptr;
}

void setup()
{
  if (!UART_Init())
    fatal("UART init failed");

  printBlocking("MODULE 7 STARTING\r\n");

  const char *err = TIM2_Init();
  if (err) fatal(err);

  err = ADC_Init();
  if (err) fatal(err);

  err = TestPWM_Init();
  if (err) fatal(err);

  char msg[96];
  snprintf(msg, sizeof(msg), "TIM2 clock: %lu Hz | sample rate: %lu Hz\r\n",
           (unsigned long)timerClockHz, (unsigned long)sampleRateHz);
  printBlocking(msg);

  if (HAL_TIM_Base_Start(&htim2) != HAL_OK)
    fatal("TIM2 start failed");

  printBlocking("MODULE 7 STARTED\r\n");

  lastPrint = millis();
}

void loop()
{
  txPump();
  pollDma();
  txPump();

  uint32_t now = millis();
  if (now - lastPrint >= REPORT_INTERVAL_MS) {
    lastPrint = now;
    queueReport();
  }
}
