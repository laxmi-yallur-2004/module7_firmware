# MODULE 7 FIRMWARE

## Platform

* Arduino Uno
* ATmega328P
* CPU: 16 MHz
* ADC: 10-bit
* Serial: 115200 baud

---

# Tasks

## Task 1: ADC Sampling and RMS Measurement

The ADC continuously reads the signal from **A1**.

The ADC interrupt stores each sample into a **64-sample circular-style double buffer**.

Two buffers are used:

```text
ADC
 ↓
ADC Interrupt
 ↓
64-sample Buffer 0
64-sample Buffer 1
 ↓
Main Loop
 ↓
Average + RMS
```

The main loop processes a completed buffer and accumulates the samples until **2048 samples** are processed.

### Calculations

Average ADC:

```text
Average = Sum of ADC samples / Number of samples
```

RMS ADC:

```text
RMS ADC = √(Sum of ADC² / Number of samples)
```

RMS voltage:

```text
RMS Voltage = RMS ADC × 5.0 / 1023.0
```

The value **1023** is used because the ATmega328P ADC is **10-bit**, so its output range is:

```text
0 to 1023
```

---

# Task 2: Sensor Fault Detection and Frequency Measurement

## Frequency Measurement

Timer1 Input Capture measures the period of the signal arriving at **D8 / ICP1**.

Timer1 uses:

```text
CPU clock = 16 MHz
Prescaler = 8

Timer1 clock = 16 MHz / 8
             = 2 MHz
```

Therefore each Timer1 count is:

```text
1 / 2 MHz = 0.5 µs
```

Frequency is calculated using:

```text
Frequency = 2,000,000 / Capture Period
```

For the 976.56 Hz PWM signal:

```text
Capture Period ≈ 2048 counts

Frequency = 2,000,000 / 2048
          = 976.56 Hz
```

---

## Stable Filtering

A simple weighted filter is used:

```text
filteredADC = (filteredADC × 7 + average) / 8
```

This gives:

```text
87.5% previous filtered value
12.5% new ADC average
```

Therefore sudden changes are smoothed instead of immediately changing the reported value.

---

## Sensor Fault Detection

The filtered ADC value is checked against the valid ADC range:

```text
filteredADC < 5
OR
filteredADC > 1018
```

If the value is outside this range, a fault counter is increased.

A fault is reported only after:

```text
4 consecutive fault readings
```

If a valid reading is received, the fault counter is reset.

This prevents a single short abnormal reading from immediately producing a fault.

---

# Connections

| Arduino Pin | Connection                       |
| ----------- | -------------------------------- |
| D3          | PWM output                       |
| A1          | ADC input                        |
| D8 / ICP1   | Timer1 Input Capture             |
| D3 → A1     | Jumper for ADC measurement       |
| D3 → D8     | Jumper for frequency measurement |
| GND         | Common ground                    |

### Normal Test

```text
D3 → A1
D3 → D8
```

D3 generates the test PWM signal.

A1 measures the PWM signal.

D8 receives the same signal for Timer1 frequency measurement.

---

# Normal Output

```text
MODULE 7 FIRMWARE
------------------
SENSOR OK
AVERAGE ADC: 511.5
FILTERED ADC: 506.1
RMS: 3.536 V
FREQUENCY: 976.56 Hz
```

### Interpretation

```text
Average ADC  = 511.5
RMS          = 3.536 V
Frequency    = 976.56 Hz
Sensor       = OK
```

The RMS value is approximately:

```text
5 / √2 = 3.5355 V
```

which agrees with the measured:

```text
3.536 V
```

---

# Sensor Fault Test

For the fault test:

```text
A1 → GND
```

D3 → D8 remains connected so frequency measurement can continue.

Measured output:

```text
SENSOR FAULT
AVERAGE ADC: 0.0
FILTERED ADC: 0.0
RMS: 0.000 V
FREQUENCY: 976.56 Hz
```

### Interpretation

```text
ADC input       = 0
Filtered ADC    = 0
RMS voltage     = 0 V
Sensor status   = FAULT
Frequency       = 976.56 Hz
```

This proves that the ADC fault condition is detected while Timer1 continues measuring the independent frequency signal.

---

# Verification Result

| Test                         | Result |
| ---------------------------- | ------ |
| ADC sampling                 | PASS   |
| Double-buffer sampling       | PASS   |
| Average calculation          | PASS   |
| RMS calculation              | PASS   |
| Stable filtering             | PASS   |
| Sensor fault detection       | PASS   |
| Timer1 frequency measurement | PASS   |
| PWM generation               | PASS   |

---

# Timer Usage

| Timer  | Purpose                               |
| ------ | ------------------------------------- |
| Timer0 | Arduino timing functions              |
| Timer1 | Input Capture / frequency measurement |
| Timer2 | PWM generation on D3                  |

Timer1 and Timer2 are configured directly through AVR registers.

---

# Failure Handling

### Sensor disconnected / input at GND

```text
ADC ≈ 0
↓
Filtered ADC ≈ 0
↓
Four consecutive fault readings
↓
SENSOR FAULT
```

### Normal signal

```text
ADC ≈ 511.5
↓
Filtered ADC stable
↓
SENSOR OK
```

### No valid Timer1 capture

If no valid capture period is available:

```text
FREQUENCY = 0.00 Hz
```

The ADC processing continues independently.

---

# Resource Usage

The ADC buffers contain:

```text
2 × 64 × 2 bytes
= 256 bytes
```

No large 2048-sample array is allocated.

No dynamic memory is used.

No `delay()` is used.

---

# Final Result

Module 7 implements two embedded-firmware functions:

### Task 1

```text
ADC Interrupt
    ↓
64-sample double buffer
    ↓
2048-sample processing
    ↓
Average + RMS voltage
```

### Task 2

```text
Timer1 Input Capture
    ↓
Frequency measurement

ADC average
    ↓
Stable filtering
    ↓
Fault threshold
    ↓
4 consecutive faults
    ↓
SENSOR FAULT
```

**Overall Module 7: PASS**

The measured results demonstrate correct ADC sampling, RMS calculation, filtering, sensor fault detection, PWM generation, and Timer1 frequency measurement.
