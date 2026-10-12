# Module 7: ADC + DMA signal measurement (STM32 Black Pill)

The board reads the voltage on PA0 (ADC1, 10 kHz, DMA) and prints results on the serial monitor (115200 baud).

Startup line: `TIM2 clock: 84000000 Hz | sample rate: 10000 Hz`

---

## Task 1: Acquire waveform via ADC + DMA, compute RMS and frequency

**What we did**
- TIM2 triggers the ADC at 10 kHz and DMA stores the samples. The code reads one finished half of the buffer at a time.
- For each block it computes the average, RMS with DC, RMS of the AC part only, and the frequency (by counting threshold crossings).
- Printing does not stop sampling, so `MISSED BLOCKS`, `ADC OVR` and `TX DROPPED` stay 0.

**Connection**

DC test: potentiometer ends to 3.3V and GND, middle pin to PA0.

Frequency test: we had no function generator, so the board's own PA6 pin makes a 0 to 3.3 V square wave (set by `TEST_PWM_FREQ_HZ`: 10, 50, 100, 200). The submitted code has it set to 0 (off).

```
PA6 ---[ 9.77 kOhm ]---+--- PA0
                       |
                    ===== 1 uF
                       |
                      GND
```

**Output: potentiometer (DC)**
```
ADC AVERAGE: 2607 | 2100 mV
RMS VOLTAGE: 2100 mV
FILTERED ADC: 2608
FREQUENCY: 0 Hz
MIN ADC: 2604
MAX ADC: 2614
SENSOR FAULT: NO
```

**Result: frequency**

| Set in code | Board shows |
|---|---|
| 10 Hz | 10.00 Hz |
| 50 Hz | 50.00 Hz |
| 100 Hz | 100.00 Hz |
| 200 Hz | 200.00 Hz |

**Output at 10 Hz**
```
----------------------------
ADC AVERAGE: 1296 | 1044 mV
RMS TOTAL (with DC): 1724 mV
RMS AC ONLY: 1371 mV
FILTERED ADC: 4021
FREQUENCY: 10.00 Hz
MIN ADC: 0
MAX ADC: 4039
SENSOR FAULT: NO
MISSED BLOCKS: 0 | ADC OVR: 0 | TX DROPPED: 0
```

**Output at 50 Hz**
```
ADC AVERAGE: 2146 | 1729 mV
RMS TOTAL (with DC): 2280 mV
RMS AC ONLY: 1486 mV
FILTERED ADC: 16
FREQUENCY: 50.00 Hz
MIN ADC: 13
MAX ADC: 4094
SENSOR FAULT: NO
MISSED BLOCKS: 0 | ADC OVR: 0 | TX DROPPED: 0
```

**Output at 100 Hz**
```
-----------------------------
ADC AVERAGE: 2411 | 1942 mV
RMS TOTAL (with DC): 2301 mV
RMS AC ONLY: 1232 mV
FILTERED ADC: 3964
FREQUENCY: 100.00 Hz
MIN ADC: 52
MAX ADC: 4042
SENSOR FAULT: NO
MISSED BLOCKS: 0 | ADC OVR: 0 | TX DROPPED: 0
```

**Output at 200 Hz**
```
ADC AVERAGE: 2206 | 1777 mV
RMS TOTAL (with DC): 1980 mV
RMS AC ONLY: 872 mV
FILTERED ADC: 1659
FREQUENCY: 200.00 Hz
MIN ADC: 374
MAX ADC: 3712
SENSOR FAULT: NO
MISSED BLOCKS: 0 | ADC OVR: 0 | TX DROPPED: 0
```

---

## Task 2: Sensor fault detection and stable filtering (no false alarms)

**What we did**
- Fault detection: each block is checked for average too low (below 100), too high (above 4000), or clipping (25% or more samples at 0 or 4095).
- No false alarms: a fault is shown only after 10 bad blocks in a row, and cleared only after 10 good blocks in a row.
- Stable filtering: a smoothing filter (`FILTERED ADC`) with extra fractional bits. It is used for frequency measurement, with a hysteresis band, so noise does not cause extra crossings.

**Test**
Pull the wire from PA6 so PA0 is near 0 V. All normal runs above show `SENSOR FAULT: NO`. With the signal removed the fault appears, and the frequency returns to 0 after a 2 second timeout:

```
ADC AVERAGE: 6 | 4 mV
RMS TOTAL (with DC): 9 mV
RMS AC ONLY: 7 mV
FILTERED ADC: 0
FREQUENCY: 0.00 Hz
MIN ADC: 0
MAX ADC: 26
SENSOR FAULT: YES (low / disconnected)
MISSED BLOCKS: 0 | ADC OVR: 0 | TX DROPPED: 0
```
---

## Notes
- Average, RMS and min/max use short 12.8 ms blocks, so they change between reports. The frequency is the reliable value.
- A floating (unplugged) input can look like a normal signal. A pull-down resistor on PA0 is needed to detect it reliably.
