#include <Arduino.h>
#include <LiquidCrystal.h>
#include <math.h>

/*
==========================================================
MODULE 7 FIRMWARE
==========================================================

TASK 1:
Acquire waveform using ADC interrupt + circular buffer
Compute RMS voltage
Compute frequency when an AC waveform is present

TASK 2:
Sensor fault detection
Stable filtering
No false alarms from short spikes



Potentiometer / Analog sensor:
    VCC  -> 5V
    GND  -> GND
    Wiper -> A1

16x2 LCD:
    RS -> D8
    EN -> D9
    D4 -> D4
    D5 -> D5
    D6 -> D6
    D7 -> D7

Serial:
    9600 baud

==========================================================
*/


// ========================================================
// LCD
// ========================================================

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);


// ========================================================
// ADC CONFIGURATION
// ========================================================

#define ADC_CHANNEL A1

#define ADC_REFERENCE 5.0f

#define ADC_MAX_VALUE 1023.0f


// ========================================================
// TASK 1 - WAVEFORM BUFFER
// ========================================================

#define ADC_BUFFER_SIZE 64

volatile uint16_t adcBuffer[ADC_BUFFER_SIZE];

volatile uint8_t adcWriteIndex = 0;

volatile bool adcBufferReady = false;


// ========================================================
// TASK 1 - SAMPLING
// ========================================================

/*
ADC clock:

CPU = 16 MHz
ADC prescaler = 128

ADC clock = 16 MHz / 128
          = 125 kHz

One ADC conversion takes approximately
13 ADC clock cycles.

Sampling rate is approximately:

125000 / 13
≈ 9615 samples/sec
*/

#define ADC_SAMPLE_RATE 9615UL


// ========================================================
// TASK 1 - RESULTS
// ========================================================

float averageVoltage = 0.0f;

float rmsVoltage = 0.0f;

float measuredFrequency = 0.0f;

bool frequencyValid = false;


// ========================================================
// TASK 2 - FILTER
// ========================================================

#define FILTER_SIZE 8

uint16_t filterBuffer[FILTER_SIZE];

uint8_t filterIndex = 0;

bool filterReady = false;


// ========================================================
// TASK 2 - FAULT LIMITS
// ========================================================

/*
For demonstration with a 0-5 V sensor:

Below 0.15 V  -> LOW sensor fault
Above 4.85 V  -> HIGH sensor fault

These limits can be changed according
to the actual sensor specification.
*/

#define LOW_FAULT_LIMIT 0.15f

#define HIGH_FAULT_LIMIT 4.85f


// ========================================================
// TASK 2 - FAULT CONFIRMATION
// ========================================================

/*
A single noisy sample must NOT create a fault.

Fault is confirmed only after several
consecutive filtered measurements.
*/

#define FAULT_CONFIRM_COUNT 3

uint8_t lowFaultCounter = 0;

uint8_t highFaultCounter = 0;


// ========================================================
// TASK 2 - SENSOR STATE
// ========================================================

enum SensorState
{
    SENSOR_OK,
    SENSOR_CHECK,
    SENSOR_LOW_FAULT,
    SENSOR_HIGH_FAULT
};

SensorState sensorState = SENSOR_CHECK;


// ========================================================
// TIMING
// ========================================================

unsigned long lastDisplayTime = 0;

unsigned long lastTask2Time = 0;

#define DISPLAY_INTERVAL 500UL

#define FAULT_SAMPLE_INTERVAL 50UL


// ========================================================
// ADC INITIALIZATION
// ========================================================

void setupADC()
{
    /*
    AVCC used as ADC reference.
    ADC1 = A1 selected.
    */

    ADMUX =
        (1 << REFS0) |
        (1 << MUX0);


    /*
    ADEN  = Enable ADC
    ADIE  = Enable ADC interrupt

    ADPS2:0 = 111
    ADC clock = F_CPU / 128
    */

    ADCSRA =
        (1 << ADEN) |
        (1 << ADIE) |
        (1 << ADPS2) |
        (1 << ADPS1) |
        (1 << ADPS0);


    /*
    Start first conversion.
    */

    ADCSRA |= (1 << ADSC);
}


// ========================================================
// ADC INTERRUPT
// ========================================================

ISR(ADC_vect)
{
    /*
    Read ADC result.
    */

    uint16_t value = ADC;


    /*
    Store sample in circular buffer.
    */

    adcBuffer[adcWriteIndex] = value;

    adcWriteIndex++;


    /*
    When 64 samples are collected,
    notify main loop.
    */

    if (adcWriteIndex >= ADC_BUFFER_SIZE)
    {
        adcWriteIndex = 0;

        adcBufferReady = true;
    }


    /*
    Immediately start next conversion.
    */

    ADCSRA |= (1 << ADSC);
}


// ========================================================
// COPY ADC BUFFER
// ========================================================

bool copyADCBuffer(uint16_t *destination)
{
    if (!adcBufferReady)
    {
        return false;
    }


    /*
    Temporarily disable ADC interrupt
    while copying the completed block.

    This prevents the ISR from changing
    the buffer during the copy.
    */

    uint8_t oldADIE = ADCSRA & (1 << ADIE);

    ADCSRA &= ~(1 << ADIE);


    for (uint8_t i = 0; i < ADC_BUFFER_SIZE; i++)
    {
        destination[i] = adcBuffer[i];
    }


    adcBufferReady = false;


    /*
    Restore ADC interrupt.
    */

    if (oldADIE)
    {
        ADCSRA |= (1 << ADIE);
    }


    return true;
}


// ========================================================
// TASK 1 - RMS CALCULATION
// ========================================================

void calculateWaveformValues(uint16_t *samples)
{
    uint32_t sum = 0;

    uint64_t sumSquares = 0;


    /*
    Calculate average and RMS.
    */

    for (uint8_t i = 0; i < ADC_BUFFER_SIZE; i++)
    {
        uint16_t sample = samples[i];

        sum += sample;

        sumSquares +=
            (uint32_t)sample * sample;
    }


    float averageADC =
        (float)sum / ADC_BUFFER_SIZE;


    float rmsADC =
        sqrt(
            (float)sumSquares /
            ADC_BUFFER_SIZE
        );


    averageVoltage =
        averageADC *
        ADC_REFERENCE /
        ADC_MAX_VALUE;


    rmsVoltage =
        rmsADC *
        ADC_REFERENCE /
        ADC_MAX_VALUE;
}


// ========================================================
// TASK 1 - FREQUENCY MEASUREMENT
// ========================================================

void calculateFrequency(uint16_t *samples)
{
    /*
    Frequency is measured using
    rising crossings of the average level.

    This works for an AC waveform.

    A DC potentiometer does not produce
    a periodic waveform, so frequency
    will be reported as N/A.
    */

    float averageADC = 0.0f;

    uint32_t sum = 0;


    for (uint8_t i = 0; i < ADC_BUFFER_SIZE; i++)
    {
        sum += samples[i];
    }


    averageADC =
        (float)sum / ADC_BUFFER_SIZE;


    /*
    Small hysteresis prevents noise
    from creating false crossings.
    */

    float upperThreshold =
        averageADC + 15.0f;

    float lowerThreshold =
        averageADC - 15.0f;


    bool aboveThreshold = false;

    uint16_t firstCrossing = 0;

    uint16_t secondCrossing = 0;

    bool firstFound = false;

    bool secondFound = false;


    for (uint8_t i = 0; i < ADC_BUFFER_SIZE; i++)
    {
        uint16_t sample = samples[i];


        /*
        Detect rising transition.
        */

        if (!aboveThreshold &&
            sample >= upperThreshold)
        {
            aboveThreshold = true;


            if (!firstFound)
            {
                firstCrossing = i;

                firstFound = true;
            }
            else if (!secondFound)
            {
                secondCrossing = i;

                secondFound = true;

                break;
            }
        }


        /*
        Wait until waveform goes below
        the lower threshold before allowing
        another rising crossing.
        */

        if (aboveThreshold &&
            sample <= lowerThreshold)
        {
            aboveThreshold = false;
        }
    }


    /*
    Need two rising crossings
    to calculate one period.
    */

    if (firstFound && secondFound)
    {
        uint16_t sampleDifference =
            secondCrossing - firstCrossing;


        if (sampleDifference > 0)
        {
            float periodSamples =
                (float)sampleDifference;


            measuredFrequency =
                (float)ADC_SAMPLE_RATE /
                periodSamples;


            /*
            Reject impossible/noisy values.
            */

            if (measuredFrequency >= 1.0f &&
                measuredFrequency <= 4000.0f)
            {
                frequencyValid = true;
            }
            else
            {
                frequencyValid = false;
            }
        }
        else
        {
            frequencyValid = false;
        }
    }
    else
    {
        frequencyValid = false;
    }
}


// ========================================================
// TASK 2 - ADD SAMPLE TO FILTER
// ========================================================

void addFilterSample(uint16_t value)
{
    filterBuffer[filterIndex] = value;

    filterIndex++;


    if (filterIndex >= FILTER_SIZE)
    {
        filterIndex = 0;

        filterReady = true;
    }
}


// ========================================================
// TASK 2 - CALCULATE FILTERED ADC
// ========================================================

uint16_t calculateFilteredADC()
{
    uint32_t sum = 0;


    for (uint8_t i = 0; i < FILTER_SIZE; i++)
    {
        sum += filterBuffer[i];
    }


    return
        (uint16_t)
        (sum / FILTER_SIZE);
}


// ========================================================
// TASK 2 - FAULT DETECTION
// ========================================================

SensorState updateSensorFault(float voltage)
{
    /*
    LOW fault condition.
    */

    if (voltage < LOW_FAULT_LIMIT)
    {
        lowFaultCounter++;

        highFaultCounter = 0;


        if (lowFaultCounter >=
            FAULT_CONFIRM_COUNT)
        {
            sensorState = SENSOR_LOW_FAULT;
        }
        else
        {
            sensorState = SENSOR_CHECK;
        }


        return sensorState;
    }


    /*
    HIGH fault condition.
    */

    if (voltage > HIGH_FAULT_LIMIT)
    {
        highFaultCounter++;

        lowFaultCounter = 0;


        if (highFaultCounter >=
            FAULT_CONFIRM_COUNT)
        {
            sensorState = SENSOR_HIGH_FAULT;
        }
        else
        {
            sensorState = SENSOR_CHECK;
        }


        return sensorState;
    }


    /*
    Normal range.

    Reset fault counters.
    */

    lowFaultCounter = 0;

    highFaultCounter = 0;

    sensorState = SENSOR_OK;


    return sensorState;
}


// ========================================================
// TASK 2 - SENSOR STATE TEXT
// ========================================================

const char* getSensorStateText()
{
    switch (sensorState)
    {
        case SENSOR_OK:
            return "OK";

        case SENSOR_CHECK:
            return "CHECK";

        case SENSOR_LOW_FAULT:
            return "LOW FAULT";

        case SENSOR_HIGH_FAULT:
            return "HIGH FAULT";

        default:
            return "CHECK";
    }
}


// ========================================================
// TASK 1 DISPLAY
// ========================================================

void displayTask1()
{
    lcd.clear();

    lcd.setCursor(0, 0);

    lcd.print("RMS:");

    lcd.print(rmsVoltage, 2);

    lcd.print("V");


    lcd.setCursor(0, 1);


    if (frequencyValid)
    {
        lcd.print("F:");

        lcd.print(measuredFrequency, 0);

        lcd.print("Hz");
    }
    else
    {
        lcd.print("F:N/A");
    }
}


// ========================================================
// TASK 1 SERIAL OUTPUT
// ========================================================

void printTask1Results()
{
    Serial.println();
    Serial.println("--------------------------------");
    Serial.println("MODULE 7 - TASK 1");
    Serial.println("ADC WAVEFORM + RMS + FREQUENCY");
    Serial.println("--------------------------------");


    Serial.print("Samples       : ");

    Serial.println(ADC_BUFFER_SIZE);


    Serial.print("Average Voltage: ");

    Serial.print(averageVoltage, 3);

    Serial.println(" V");


    Serial.print("RMS Voltage   : ");

    Serial.print(rmsVoltage, 3);

    Serial.println(" V");


    if (frequencyValid)
    {
        Serial.print("Frequency     : ");

        Serial.print(measuredFrequency, 2);

        Serial.println(" Hz");
    }
    else
    {
        Serial.println("Frequency     : N/A");
    }
}


// ========================================================
// TASK 2 SERIAL OUTPUT
// ========================================================

void printTask2Results(
    uint16_t rawADC,
    uint16_t filteredADC,
    float rawVoltage,
    float filteredVoltage)
{
    Serial.println();
    Serial.println("--------------------------------");
    Serial.println("MODULE 7 - TASK 2");
    Serial.println("SENSOR FAULT + STABLE FILTERING");
    Serial.println("--------------------------------");


    Serial.print("Raw ADC       : ");

    Serial.println(rawADC);


    Serial.print("Filtered ADC  : ");

    Serial.println(filteredADC);


    Serial.print("Raw Voltage   : ");

    Serial.print(rawVoltage, 3);

    Serial.println(" V");


    Serial.print("Filtered Volt : ");

    Serial.print(filteredVoltage, 3);

    Serial.println(" V");


    Serial.print("Status        : ");

    Serial.println(getSensorStateText());
}


// ========================================================
// TASK 2 LCD DISPLAY
// ========================================================

void displayTask2(float filteredVoltage)
{
    lcd.clear();


    lcd.setCursor(0, 0);

    lcd.print("V:");

    lcd.print(filteredVoltage, 2);

    lcd.print("V");


    lcd.setCursor(0, 1);

    lcd.print(getSensorStateText());
}


// ========================================================
// SETUP
// ========================================================

void setup()
{
    /*
    LCD
    */

    lcd.begin(16, 2);


    /*
    Serial
    */

    Serial.begin(9600);


    /*
    Startup message.
    */

    Serial.println();
    Serial.println("================================");
    Serial.println("MODULE 7 FIRMWARE");
    Serial.println("TASK 1 + TASK 2");
    Serial.println("================================");

    Serial.println("ADC Input : A1");

    Serial.println("Buffer    : 64 samples");

    Serial.println("ADC mode  : Interrupt driven");

    Serial.println("DMA       : Not available on ATmega328P");

    Serial.println("Task 1    : RMS + Frequency");

    Serial.println("Task 2    : Fault + Filtering");

    Serial.println("================================");


    /*
    Initialize filter.
    */

    for (uint8_t i = 0; i < FILTER_SIZE; i++)
    {
        filterBuffer[i] = 0;
    }


    /*
    Initialize ADC.
    */

    setupADC();


    /*
    Enable global interrupts.
    */

    sei();


    /*
    LCD startup.
    */

    lcd.clear();

    lcd.setCursor(0, 0);

    lcd.print("MODULE 7");

    lcd.setCursor(0, 1);

    lcd.print("TASK1 + TASK2");
}


// ========================================================
// LOOP
// ========================================================

void loop()
{
    /*
    ======================================================
    TASK 1
    ======================================================
    */

    uint16_t sampleCopy[ADC_BUFFER_SIZE];


    if (copyADCBuffer(sampleCopy))
    {
        /*
        Calculate RMS.
        */

        calculateWaveformValues(sampleCopy);


        /*
        Calculate frequency
        if waveform is periodic.
        */

        calculateFrequency(sampleCopy);


        /*
        Print results.
        */

        printTask1Results();


        /*
        Show Task 1 result temporarily.
        */

        displayTask1();
    }


    /*
    ======================================================
    TASK 2
    ======================================================
    */

    unsigned long currentTime = millis();


    if (currentTime - lastTask2Time >=
        FAULT_SAMPLE_INTERVAL)
    {
        lastTask2Time = currentTime;


        /*
        Read current sensor value.

        Task 1 is already continuously acquiring
        samples using the ADC interrupt.
        */

        uint16_t rawADC = ADC;


        /*
        Add current value to stable filter.
        */

        addFilterSample(rawADC);


        /*
        Wait until the filter has
        collected 8 samples.
        */

        if (filterReady)
        {
            /*
            Calculate stable filtered ADC.
            */

            uint16_t filteredADC =
                calculateFilteredADC();


            /*
            Convert to voltage.
            */

            float rawVoltage =
                (float)rawADC *
                ADC_REFERENCE /
                ADC_MAX_VALUE;


            float filteredVoltage =
                (float)filteredADC *
                ADC_REFERENCE /
                ADC_MAX_VALUE;


            /*
            Fault detection is performed
            on the FILTERED value.

            This prevents a short noise spike
            from immediately becoming a fault.
            */

            updateSensorFault(
                filteredVoltage
            );


            /*
            Print Task 2 result.
            */

            printTask2Results(
                rawADC,
                filteredADC,
                rawVoltage,
                filteredVoltage
            );


            /*
            Display stable sensor result.
            */

            displayTask2(filteredVoltage);
        }
    }
}
