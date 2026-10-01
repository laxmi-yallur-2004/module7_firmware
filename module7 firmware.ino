#include <Arduino.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <math.h>

#define ADC_BUFFER_SIZE 64
#define PWM_PIN 3

volatile uint16_t adcBuffer[2][ADC_BUFFER_SIZE];
volatile uint8_t activeBuffer = 0;
volatile uint8_t bufferIndex = 0;
volatile uint8_t readyBuffer = 255;

volatile uint16_t captureLast = 0;
volatile uint16_t capturePeriod = 0;
volatile uint8_t captureReady = 0;

uint32_t sampleCount = 0;
uint64_t squareSum = 0;
uint32_t sum = 0;

float filteredADC = 0;
uint8_t faultCount = 0;

ISR(ADC_vect)
{
    adcBuffer[activeBuffer][bufferIndex++] = ADC;

    if (bufferIndex == ADC_BUFFER_SIZE)
    {
        readyBuffer = activeBuffer;
        activeBuffer ^= 1;
        bufferIndex = 0;
    }
}

ISR(TIMER1_CAPT_vect)
{
    uint16_t capture = ICR1;

    capturePeriod = capture - captureLast;
    captureLast = capture;
    captureReady = 1;
}

void setup()
{
    Serial.begin(115200);

    // D3 PWM = 976.56 Hz
    pinMode(PWM_PIN, OUTPUT);

    TCCR2A = (1 << COM2B1) | (1 << WGM21) | (1 << WGM20);
    TCCR2B = (1 << CS22);
    OCR2B = 128;

    // ADC A1
    ADMUX = (1 << REFS0) | 1;

    ADCSRA = (1 << ADEN) |
             (1 << ADATE) |
             (1 << ADIE) |
             (1 << ADPS2) |
             (1 << ADPS1) |
             (1 << ADPS0);

    ADCSRB = 0;

    // Timer1 input capture on D8, rising edge, prescaler 8
    TCCR1A = 0;
    TCCR1B = (1 << ICES1) | (1 << CS11);
    TIMSK1 = (1 << ICIE1);

    ADCSRA |= (1 << ADSC);

    Serial.println("MODULE 7 FIRMWARE");
    Serial.println("------------------");
}

void loop()
{
    if (readyBuffer != 255)
    {
        uint8_t buffer = readyBuffer;
        readyBuffer = 255;

        for (uint8_t i = 0; i < ADC_BUFFER_SIZE; i++)
        {
            uint16_t value = adcBuffer[buffer][i];

            sum += value;
            squareSum += (uint32_t)value * value;
            sampleCount++;
        }
    }

    if (sampleCount >= 2048)
    {
        float average = (float)sum / sampleCount;

        float rmsADC = sqrt((float)squareSum / sampleCount);
        float rmsVoltage = rmsADC * 5.0 / 1023.0;

        filteredADC = (filteredADC * 7.0 + average) / 8.0;

        if (filteredADC < 5 || filteredADC > 1018)
        {
            faultCount++;

            if (faultCount >= 4)
                Serial.println("SENSOR FAULT");
        }
        else
        {
            faultCount = 0;
            Serial.println("SENSOR OK");
        }

        float frequency = 0;

        if (captureReady && capturePeriod > 0)
            frequency = 2000000.0 / capturePeriod;

        Serial.print("AVERAGE ADC: ");
        Serial.println(average, 1);

        Serial.print("FILTERED ADC: ");
        Serial.println(filteredADC, 1);

        Serial.print("RMS: ");
        Serial.print(rmsVoltage, 3);
        Serial.println(" V");

        Serial.print("FREQUENCY: ");
        Serial.print(frequency, 2);
        Serial.println(" Hz");

        Serial.println();

        sampleCount = 0;
        sum = 0;
        squareSum = 0;
    }
}
