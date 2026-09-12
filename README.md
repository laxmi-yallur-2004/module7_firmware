# Module 7 Firmware

## Overview

Module 7 demonstrates ADC sampling, stable voltage measurement, RMS calculation, and ADC accuracy testing using an Arduino Uno.

The input is provided using a potentiometer connected to analog pin A1.

## Hardware Used

* Arduino Uno
* 16x2 LCD
* Potentiometer
* USB cable

## LCD Connections

```text
RS = 8
EN = 9
D4 = 4
D5 = 5
D6 = 6
D7 = 7
```

ADC input:

```text
Potentiometer → A1
```

## Tasks

### Task 1 - ADC Sampling and RMS

The Arduino collects 64 ADC samples using the ADC interrupt.

The samples are stored in a buffer.

The program calculates:

* Average ADC value
* Average voltage
* RMS voltage

For a DC potentiometer input, average voltage and RMS voltage are approximately equal.

Example:

```text
Samples: 64 | ADC: 471.5 | Avg V: 2.305 V | RMS: 2.305 V
Frequency: N/A - potentiometer provides DC input
```

The Arduino Uno does not have hardware DMA, so an ADC interrupt and buffer are used for sample collection.

### Task 2 - Stable Filtering

The ADC input is sampled repeatedly and the readings are processed to obtain a stable voltage value.

The purpose is to reduce small ADC reading variations and obtain a more stable measurement.

### Task 3 - ADC Accuracy and Error

The measured ADC voltage is compared with a reference voltage.

The program calculates:

```text
Absolute Error = |Measured Voltage - Reference Voltage|

Percentage Error =
(Absolute Error / Reference Voltage) × 100
```

Example:

```text
ADC: 470
Measured: 2.297 V
Reference: 2.310 V
Error: 0.013 V
Error: 0.56 %
```

## ADC Voltage Formula

The Arduino Uno uses a 10-bit ADC.

```text
ADC range = 0 to 1023

Voltage = ADC × 5.0 / 1023
```

## Result

Module 7 demonstrates:

1. ADC sampling and buffering
2. Average and RMS calculation
3. Stable ADC measurement
4. ADC voltage conversion
5. Accuracy and error calculation

All tasks were tested using Arduino Uno and a potentiometer input.
