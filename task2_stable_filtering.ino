#include <LiquidCrystal.h>

// ==================================================
// LCD
// ==================================================
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// ==================================================
// ADC
// ==================================================
const uint8_t ADC_PIN = A1;

const float ADC_REFERENCE = 5.0;

// ==================================================
// SAMPLING
// ==================================================
const unsigned long SAMPLE_INTERVAL = 50;

unsigned long lastSampleTime = 0;

// ==================================================
// MOVING AVERAGE
// ==================================================
#define FILTER_SIZE 8

uint16_t samples[FILTER_SIZE];

uint8_t sampleIndex = 0;

bool filterReady = false;

// ==================================================
// FAULT LIMITS
// ==================================================
//
// Potentiometer normally gives 0-5 V.
//
// We define:
// below 0.15 V = LOW fault
// above 4.85 V = HIGH fault
//
// These are practical demonstration limits.
// ==================================================

const float LOW_LIMIT = 0.15;
const float HIGH_LIMIT = 4.85;

// Require several consecutive bad readings
// before declaring a fault.
const uint8_t FAULT_CONFIRM_COUNT = 3;

uint8_t lowFaultCount = 0;
uint8_t highFaultCount = 0;

// ==================================================
// SETUP
// ==================================================
void setup()
{
  lcd.begin(16, 2);

  Serial.begin(9600);

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("MODULE 7");

  lcd.setCursor(0, 1);
  lcd.print("TASK 2 FILTER");

  Serial.println("================================");
  Serial.println("MODULE 7 - TASK 2");
  Serial.println("FILTER + FAULT DETECTION");
  Serial.println("================================");

  delay(1500);

  lcd.clear();

  // Initialize filter buffer
  for (uint8_t i = 0; i < FILTER_SIZE; i++)
  {
    samples[i] = 0;
  }
}

// ==================================================
// ADD SAMPLE TO FILTER
// ==================================================
void addSample(uint16_t value)
{
  samples[sampleIndex] = value;

  sampleIndex++;

  if (sampleIndex >= FILTER_SIZE)
  {
    sampleIndex = 0;
    filterReady = true;
  }
}

// ==================================================
// CALCULATE MOVING AVERAGE
// ==================================================
float calculateFilteredVoltage()
{
  uint32_t sum = 0;

  for (uint8_t i = 0; i < FILTER_SIZE; i++)
  {
    sum += samples[i];
  }

  float averageADC =
      (float)sum / FILTER_SIZE;

  return averageADC * ADC_REFERENCE / 1023.0;
}

// ==================================================
// FAULT DETECTION
// ==================================================
const char* checkFault(float voltage)
{
  // LOW voltage
  if (voltage < LOW_LIMIT)
  {
    lowFaultCount++;
    highFaultCount = 0;

    if (lowFaultCount >= FAULT_CONFIRM_COUNT)
    {
      return "LOW FAULT";
    }

    return "CHECK";
  }

  // HIGH voltage
  if (voltage > HIGH_LIMIT)
  {
    highFaultCount++;
    lowFaultCount = 0;

    if (highFaultCount >= FAULT_CONFIRM_COUNT)
    {
      return "HIGH FAULT";
    }

    return "CHECK";
  }

  // Normal range
  lowFaultCount = 0;
  highFaultCount = 0;

  return "OK";
}

// ==================================================
// DISPLAY
// ==================================================
void displayResult(float rawVoltage,
                   float filteredVoltage,
                   const char* status)
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("F:");
  lcd.print(filteredVoltage, 2);
  lcd.print("V");

  lcd.setCursor(0, 1);
  lcd.print(status);

  Serial.print("Raw: ");
  Serial.print(rawVoltage, 3);

  Serial.print(" V | Filtered: ");
  Serial.print(filteredVoltage, 3);

  Serial.print(" V | Status: ");
  Serial.println(status);
}

// ==================================================
// LOOP
// ==================================================
void loop()
{
  unsigned long currentTime = millis();

  if (currentTime - lastSampleTime >= SAMPLE_INTERVAL)
  {
    lastSampleTime = currentTime;

    // ==============================================
    // READ ADC
    // ==============================================

    uint16_t adcValue = analogRead(ADC_PIN);

    float rawVoltage =
        adcValue * ADC_REFERENCE / 1023.0;

    // ==============================================
    // ADD TO MOVING AVERAGE
    // ==============================================

    addSample(adcValue);

    // Wait until 8 samples are available
    if (!filterReady)
    {
      return;
    }

    // ==============================================
    // FILTER
    // ==============================================

    float filteredVoltage =
        calculateFilteredVoltage();

    // ==============================================
    // FAULT DETECTION
    // ==============================================

    const char* status =
        checkFault(filteredVoltage);

    // ==============================================
    // DISPLAY
    // ==============================================

    displayResult(
        rawVoltage,
        filteredVoltage,
        status
    );
  }
}
