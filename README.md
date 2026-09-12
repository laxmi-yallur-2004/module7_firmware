# Module 7 – ADC Measurement

## Overview

This module demonstrates **ADC voltage measurement using Arduino Uno**.

The potentiometer is connected to **A1**. The Arduino reads the analog voltage and converts it into a digital ADC value from **0 to 1023**.

---

## Hardware Used

* Arduino Uno
* 16x2 LCD
* Potentiometer
* USB cable

### LCD Connections

```text
RS = 8
EN = 9
D4 = 4
D5 = 5
D6 = 6
D7 = 7
```

### ADC Input

```text
Potentiometer → A1
```

---

# Task 1 – ADC Buffer + RMS

The Arduino collects **64 ADC samples** using an ADC interrupt and stores them in a buffer.

The program calculates:

* Average ADC value
* Average voltage
* RMS voltage

For a DC potentiometer input, the average voltage and RMS voltage are approximately the same.

### Example Output

```text
Samples: 64 | ADC: 471.4 | Avg V: 2.304 V | RMS: 2.304 V
Frequency: N/A - potentiometer provides DC input
```

### Purpose

To collect multiple ADC samples and calculate average and RMS voltage measurement.

---

# Task 2 – Fixed-Rate ADC Sampling

The Arduino reads the ADC at a fixed interval of **100 ms**.

The program uses `millis()` to control the sampling time without continuously waiting.

### Example Output

```text
ADC Value: 471
Voltage: 2.30 V
```

### Purpose

To perform ADC measurements at a controlled and fixed sampling rate.

---

# Task 3 – ADC Accuracy and Error

The Arduino ADC measurement is compared with a reference voltage measured using a multimeter.

The program calculates:

```text
Voltage Error = |Measured Voltage - Reference Voltage|

Percentage Error =
Voltage Error / Reference Voltage × 100
```

### Example Output

```text
ADC: 470
Measured: 2.297 V
Reference: 2.310 V
Error: 0.013 V
Error: 0.56 %
```

### Purpose

To check how accurate the Arduino ADC voltage measurement is.

---

# Summary

```text
Task 1 → 64 ADC samples + Average + RMS

Task 2 → Fixed-rate ADC sampling every 100 ms

Task 3 → ADC accuracy + Error calculation
```

## Result

Module 7 demonstrates **ADC sampling, voltage calculation, RMS measurement, fixed-rate sampling, and ADC accuracy checking** using Arduino Uno.
