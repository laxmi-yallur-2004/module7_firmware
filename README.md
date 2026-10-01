# MODULE 7 FIRMWARE

## Platform

* Arduino Uno
* ATmega328P
* CPU: 16 MHz

## Tasks

### Task 1: ADC Sampling and RMS

* ADC reads the signal from **A1**.
* ADC interrupt stores samples in two 64-sample buffers.
* The main loop processes the buffers.
* It calculates:

  * Average ADC value
  * RMS voltage

### Task 2: Sensor Fault Detection and Frequency

* Timer1 Input Capture measures the frequency from **D8**.
* A simple filter makes the ADC value stable.
* Sensor fault is detected when the filtered ADC value is outside the valid range.
* Four consecutive fault readings are required before reporting `SENSOR FAULT`.

## Connections

| Arduino Pin | Connection           |
| ----------- | -------------------- |
| D3          | PWM output           |
| A1          | ADC input            |
| D8          | Timer1 input capture |
| D3 → A1     | Jumper               |
| D3 → D8     | Jumper               |

## Normal Output

```text
MODULE 7 FIRMWARE
------------------
SENSOR OK
AVERAGE ADC: 511.5
FILTERED ADC: 506.1
RMS: 3.536 V
FREQUENCY: 976.56 Hz
```

## Sensor Fault Test

For the fault test, **A1 was connected to GND**.

Output:

```text
SENSOR FAULT
AVERAGE ADC: 0.0
FILTERED ADC: 0.0
RMS: 0.000 V
FREQUENCY: 976.56 Hz
```

## Result

* ADC sampling: PASS
* Average calculation: PASS
* RMS calculation: PASS
* Stable filtering: PASS
* Sensor fault detection: PASS
* Frequency measurement: PASS

* Timer1 is used for frequency measurement.
* Timer2 generates the PWM signal.
