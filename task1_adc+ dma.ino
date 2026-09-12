#include <Arduino.h>
#include <LiquidCrystal.h>
#include <math.h>

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

#define ADC_INPUT A1
#define BUFFER_SIZE 64

volatile uint16_t adcBuffer[BUFFER_SIZE];

volatile uint8_t writeIndex = 0;
volatile bool bufferReady = false;

void setupADC()
{
  ADMUX = (1 << REFS0) | 1;

  ADCSRA =
      (1 << ADEN) |
      (1 << ADIE) |
      (1 << ADPS2) |
      (1 << ADPS1) |
      (1 << ADPS0);

  ADCSRA |= (1 << ADSC);
}

ISR(ADC_vect)
{
  uint16_t value = ADC;

  adcBuffer[writeIndex] = value;

  writeIndex++;

  if (writeIndex >= BUFFER_SIZE)
  {
    writeIndex = 0;
    bufferReady = true;
  }
  else
  {
    ADCSRA |= (1 << ADSC);
  }
}

void processSamples()
{
  uint16_t sampleCopy[BUFFER_SIZE];

  ADCSRA &= ~(1 << ADIE);

  for (uint8_t i = 0; i < BUFFER_SIZE; i++)
  {
    sampleCopy[i] = adcBuffer[i];
  }

  writeIndex = 0;
  bufferReady = false;

  ADCSRA |= (1 << ADIF);
  ADCSRA |= (1 << ADIE);
  ADCSRA |= (1 << ADSC);

  uint32_t sum = 0;

  for (uint8_t i = 0; i < BUFFER_SIZE; i++)
  {
    sum += sampleCopy[i];
  }

  float averageADC =
      (float)sum / BUFFER_SIZE;

  uint32_t sumSquares = 0;

  for (uint8_t i = 0; i < BUFFER_SIZE; i++)
  {
    sumSquares +=
        (uint32_t)sampleCopy[i] * sampleCopy[i];
  }

  float rmsADC =
      sqrt((float)sumSquares / BUFFER_SIZE);

  float averageVoltage =
      averageADC * 5.0 / 1023.0;

  float rmsVoltage =
      rmsADC * 5.0 / 1023.0;

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("ADC:");
  lcd.print((int)averageADC);

  lcd.setCursor(0, 1);
  lcd.print("RMS:");
  lcd.print(rmsVoltage, 2);
  lcd.print("V");

  Serial.print("Samples: ");
  Serial.print(BUFFER_SIZE);

  Serial.print(" | ADC: ");
  Serial.print(averageADC, 1);

  Serial.print(" | Avg V: ");
  Serial.print(averageVoltage, 3);

  Serial.print(" V | RMS: ");
  Serial.print(rmsVoltage, 3);

  Serial.println(" V");

  Serial.println(
      "Frequency: N/A - potentiometer provides DC input"
  );
}

void setup()
{
  lcd.begin(16, 2);

  Serial.begin(9600);

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("MODULE 7");

  lcd.setCursor(0, 1);
  lcd.print("TASK 1 ADC RMS");

  Serial.println("================================");
  Serial.println("MODULE 7 - TASK 1");
  Serial.println("ADC BUFFER + RMS");
  Serial.println("================================");

  Serial.println("ADC input: A1");
  Serial.println("Buffer size: 64 samples");

  Serial.println("UNO has no hardware DMA.");
  Serial.println("Using ADC interrupt + circular buffer.");
  Serial.println();

  setupADC();
}

void loop()
{
  if (bufferReady)
  {
    processSamples();
  }
}
