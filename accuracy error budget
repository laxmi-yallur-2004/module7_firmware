#include <LiquidCrystal.h>

// ==================================================
// LCD
// ==================================================
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// ==================================================
// ADC
// ==================================================
const uint8_t ADC_PIN = A1;

// Nominal Arduino UNO ADC reference.
// For best accuracy, compare against the actual
// measured 5V supply if available.
const float ADC_REFERENCE = 5.0;

// ==================================================
// SAMPLING
// ==================================================
const unsigned long SAMPLE_INTERVAL = 500;

unsigned long lastSampleTime = 0;

// ==================================================
// REFERENCE VOLTAGE
// ==================================================
float referenceVoltage = 0.0;

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
  lcd.print("TASK 3 ACCURACY");

  Serial.println("================================");
  Serial.println("MODULE 7 - TASK 3");
  Serial.println("ADC ACCURACY TEST");
  Serial.println("================================");

  Serial.println();
  Serial.println("Measure A1 voltage with");
  Serial.println("a multimeter.");
  Serial.println();
  Serial.println("Enter reference voltage.");
  Serial.println("Example: 2.310");
  Serial.println();

  delay(1500);

  lcd.clear();
}

// ==================================================
// READ REFERENCE VOLTAGE
// ==================================================
void readReferenceVoltage()
{
  if (Serial.available() > 0)
  {
    float value = Serial.parseFloat();

    if (value > 0.0 && value <= 5.5)
    {
      referenceVoltage = value;

      Serial.print("Reference Voltage = ");
      Serial.print(referenceVoltage, 3);
      Serial.println(" V");
    }

    // Clear remaining serial characters
    while (Serial.available() > 0)
    {
      Serial.read();
    }
  }
}

// ==================================================
// MEASURE AND CALCULATE ERROR
// ==================================================
void performAccuracyTest()
{
  uint16_t adcValue =
      analogRead(ADC_PIN);

  float measuredVoltage =
      adcValue * ADC_REFERENCE / 1023.0;

  float errorVoltage = 0.0;

  float errorPercent = 0.0;

  if (referenceVoltage > 0.0)
  {
    errorVoltage =
        measuredVoltage - referenceVoltage;

    if (errorVoltage < 0)
    {
      errorVoltage = -errorVoltage;
    }

    errorPercent =
        (errorVoltage / referenceVoltage) * 100.0;
  }

  // ==================================================
  // LCD
  // ==================================================

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("V:");
  lcd.print(measuredVoltage, 3);

  lcd.setCursor(0, 1);
  lcd.print("Err:");
  lcd.print(errorPercent, 2);
  lcd.print("%");

  // ==================================================
  // SERIAL
  // ==================================================

  Serial.print("ADC: ");
  Serial.print(adcValue);

  Serial.print(" | Measured: ");
  Serial.print(measuredVoltage, 3);

  Serial.print(" V | Reference: ");
  Serial.print(referenceVoltage, 3);

  Serial.print(" V | Error: ");
  Serial.print(errorVoltage, 3);

  Serial.print(" V | Error: ");
  Serial.print(errorPercent, 2);

  Serial.println(" %");
}

// ==================================================
// LOOP
// ==================================================
void loop()
{
  // Check for multimeter reference
  readReferenceVoltage();

  unsigned long currentTime = millis();

  if (currentTime - lastSampleTime >= SAMPLE_INTERVAL)
  {
    lastSampleTime = currentTime;

    performAccuracyTest();
  }
}
