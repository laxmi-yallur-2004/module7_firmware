# Module 7 – ADC Waveform Acquisition and Sensor Fault Detection

## Overview

This module implements two embedded firmware tasks using an Arduino Uno and a 16x2 LCD.

### Task 1

**Acquire waveform using ADC + buffer, calculate RMS, and calculate frequency where required.**

### Task 2

**Implement sensor fault detection with stable filtering to avoid false alarms.**

> **Hardware note:** The Arduino Uno ATmega328P does not have a hardware DMA controller. Therefore, ADC interrupt-driven sampling with a software circular buffer is used to provide continuous buffered acquisition.

---

# Hardware Used

* Arduino Uno
* 16x2 LCD
* 10 kΩ potentiometer
* USB cable
* Jumper wires

---

# Connections

## Potentiometer

| Potentiometer      | Arduino Uno |
| ------------------ | ----------- |
| Outer pin 1        | 5V          |
| Middle pin / Wiper | A1          |
| Outer pin 2        | GND         |

The potentiometer is used as a simple analog sensor input.

Turning the potentiometer changes the voltage at A1 from approximately 0 V to 5 V.

## 16x2 LCD

| LCD | Arduino Uno |
| --- | ----------- |
| RS  | D8          |
| EN  | D9          |
| D4  | D4          |
| D5  | D5          |
| D6  | D6          |
| D7  | D7          |
| RW  | GND         |
| VSS | GND         |
| VDD | 5V          |

---

# Task 1 – ADC Waveform Acquisition + RMS + Frequency

## What was implemented

The firmware continuously acquires ADC samples from A1.

A block of **64 ADC samples** is stored in a software circular buffer using the ADC conversion-complete interrupt.

The collected samples are processed to calculate:

* Average ADC value
* Average voltage
* RMS voltage
* Frequency when a periodic AC waveform is present

## ADC Resolution

The Arduino Uno ADC is 10-bit.

Therefore:

```text
ADC value = 0 to 1023
```

The ADC voltage is calculated using:

```text
Voltage = ADC × 5.0 / 1023
```

## RMS Calculation

The firmware calculates RMS from the acquired samples:

```text
RMS = sqrt(sum(sample²) / number of samples)
```

For a potentiometer, the input is normally a DC voltage.

Therefore, average voltage and RMS voltage will normally be close to each other.

## Frequency

The potentiometer produces a DC voltage.

Therefore, when using only the potentiometer:

```text
Frequency: N/A
```

This is expected.

Frequency calculation becomes applicable when a periodic analog waveform is connected to A1.

---

# Task 1 Expected Serial Output

Example:

```text
================================
MODULE 7 FIRMWARE
TASK 1 + TASK 2
================================
ADC Input : A1
Buffer    : 64 samples
ADC mode  : Interrupt driven
DMA       : Not available on ATmega328P
Task 1    : RMS + Frequency
Task 2    : Fault + Filtering
================================
```

For a potentiometer at approximately 2.5 V:

```text
--------------------------------
MODULE 7 - TASK 1
ADC WAVEFORM + RMS + FREQUENCY
--------------------------------
Samples       : 64
Average Voltage: 2.487 V
RMS Voltage   : 2.487 V
Frequency     : N/A
```

The exact ADC and voltage values will change when the potentiometer is rotated.

---

# Task 1 LCD Output

For a potentiometer around 2.5 V:

```text
RMS:2.49V
F:N/A
```

When the potentiometer is rotated, the RMS value changes.

Example:

```text
RMS:1.20V
F:N/A
```

or:

```text
RMS:4.10V
F:N/A
```

---

# Task 2 – Sensor Fault Detection + Stable Filtering

## What was implemented

The potentiometer is used as a simulated analog sensor.

The sensor value is passed through an **8-sample moving average filter**.

The filtered value is then checked against sensor fault limits.

### Fault limits

```text
Below 0.15 V  → LOW FAULT

0.15 V to 4.85 V → NORMAL

Above 4.85 V → HIGH FAULT
```

## Stable Fault Detection

A single noisy ADC reading does not immediately create a fault.

The firmware requires the abnormal filtered condition to remain for:

```text
3 consecutive checks
```

before confirming the fault.

This prevents short noise spikes from producing false alarms.

---

# Task 2 Normal Output

When the potentiometer is in the normal range:

```text
--------------------------------
MODULE 7 - TASK 2
SENSOR FAULT + STABLE FILTERING
--------------------------------
Raw ADC       : 520
Filtered ADC  : 518
Raw Voltage   : 2.542 V
Filtered Volt : 2.532 V
Status        : OK
```

LCD:

```text
V:2.53V
OK
```

---

# Task 2 LOW FAULT Output

Turn the potentiometer close to 0 V.

After the fault condition remains stable:

```text
--------------------------------
MODULE 7 - TASK 2
SENSOR FAULT + STABLE FILTERING
--------------------------------
Raw ADC       : 12
Filtered ADC  : 14
Raw Voltage   : 0.059 V
Filtered Volt : 0.068 V
Status        : LOW FAULT
```

LCD:

```text
V:0.07V
LOW FAULT
```

---

# Task 2 HIGH FAULT Output

Turn the potentiometer close to 5 V.

After the fault condition remains stable:

```text
--------------------------------
MODULE 7 - TASK 2
SENSOR FAULT + STABLE FILTERING
--------------------------------
Raw ADC       : 1015
Filtered ADC  : 1011
Raw Voltage   : 4.961 V
Filtered Volt : 4.941 V
Status        : HIGH FAULT
```

LCD:

```text
V:4.94V
HIGH FAULT
```

---

# No False Alarm Demonstration

The filtering prevents a single abnormal sensor reading from immediately becoming a fault.

Example:

```text
Normal sensor
     ↓
Noise spike
     ↓
8-sample filtering
     ↓
CHECK
     ↓
Sensor returns normal
     ↓
OK
```

For a real persistent fault:

```text
Normal
  ↓
Abnormal sensor value
  ↓
Filtering
  ↓
CHECK
  ↓
CHECK
  ↓
CHECK
  ↓
FAULT CONFIRMED
```

Therefore, short noise or a single abnormal reading does not immediately generate a fault.

---

# Task 2 LCD Status

The LCD displays the filtered sensor voltage and status.

### Normal

```text
V:2.53V
OK
```

### Temporary abnormal condition

```text
V:4.90V
CHECK
```

### Confirmed high fault

```text
V:4.94V
HIGH FAULT
```

### Confirmed low fault

```text
V:0.07V
LOW FAULT
```

---

# Final Module 7 Output

The final firmware demonstrates:

```text
TASK 1
ADC acquisition
      ↓
64-sample buffer
      ↓
Average voltage
      ↓
RMS voltage
      ↓
Frequency when periodic waveform is available


TASK 2
Sensor input
      ↓
8-sample filtering
      ↓
Stable sensor value
      ↓
Fault confirmation
      ↓
OK / CHECK / LOW FAULT / HIGH FAULT
```

---

# Final Result

```text
MODULE 7 COMPLETE

Task 1:
ADC waveform acquisition
64-sample buffered acquisition
RMS calculation
Frequency calculation where applicable

Task 2:
Sensor fault detection
Stable filtering
False-alarm prevention

Task 3:
Not required
```

The potentiometer is used as the **analog sensor for testing**. The actual sensor can later replace the potentiometer while keeping the same A1 input and processing logic.

## Repository Structure

```text
module7_firmware/
│
├── module7_firmware.ino
└── README.md
```
