# MODULE 7 FIRMWARE

## Platform

* Arduino Uno
* ATmega328P
* CPU: 16 MHz
* ADC: 10-bit
* Serial: 115200 baud

## Tasks

### Task 1 – ADC Sampling + Average ADC + RMS Voltage + Frequency Measurement

* ADC reads the signal from **A1**.
* ADC interrupt stores samples in two 64-sample buffers.
* The main loop processes 2048 samples.
* Average ADC is calculated.
* RMS voltage is calculated.
* Timer1 Input Capture measures frequency through **D8 / ICP1**.

### Task 2 – Stable Filtering + Sensor Fault Detection

* ADC average is passed through a simple weighted filter.
* The filter makes the ADC reading more stable.
* The filtered ADC value is checked against the valid range.
* Four consecutive fault readings are required before reporting `SENSOR FAULT`.
* A valid reading resets the fault counter.

## Test Signal

Timer2 generates a PWM test signal on **D3**.

```text
Timer2 → D3 PWM
```

Connections for normal testing:

```text
D3 → A1 → ADC → Average + RMS
D3 → D8 → Timer1 → Frequency
```

PWM is used only as an internal test signal.

## Connections

| Pin       | Connection             |
| --------- | ---------------------- |
| D3        | PWM test output        |
| A1        | ADC input              |
| D8 / ICP1 | Timer1 frequency input |
| D3 → A1   | Jumper                 |
| D3 → D8   | Jumper                 |
| GND       | Common ground          |

## Normal Test Output

```text
SENSOR OK
AVERAGE ADC: 511.5
FILTERED ADC: 511.5
RMS: 3.536 V
FREQUENCY: 976.56 Hz
```

## Fault Test

First remove the **D3 → A1** jumper.

Then connect:

```text
A1 → GND
```

Keep **D3 → D8** connected.

Expected output:

```text
SENSOR FAULT
AVERAGE ADC: 0.0
FILTERED ADC: 0.0
RMS: 0.000 V
FREQUENCY: 976.56 Hz
```

## Timer Usage

| Timer  | Purpose               |
| ------ | --------------------- |
| Timer0 | Arduino timing        |
| Timer1 | Frequency measurement |
| Timer2 | PWM test signal       |

## Verification

| Test                   | Result |
| ---------------------- | ------ |
| ADC sampling           | PASS   |
| Average ADC            | PASS   |
| RMS calculation        | PASS   |
| Frequency measurement  | PASS   |
| Stable filtering       | PASS   |
| Sensor fault detection | PASS   |
| PWM test signal        | PASS   |

## Resource Usage

* Two ADC buffers
* 64 samples per buffer
* No large 2048-sample array
  

## Result

**MODULE 7: PASS**

**Task 1:** ADC sampling, Average ADC, RMS voltage and frequency measurement.

**Task 2:** Stable filtering and sensor fault detection.
