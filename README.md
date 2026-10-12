# Module 7: ADC + DMA signal measurement (STM32 Black Pill)

Module 7 has two tasks:
- **Task 1:** Acquire a waveform via ADC + DMA and compute RMS and frequency (where required).
- **Task 2:** Implement sensor fault detection and stable filtering (no false alarms).

The board reads the voltage on pin PA0 (ADC1, 10 kHz sampling, DMA) and prints the results on the serial monitor (115200 baud).

---

## Task 1: Acquire waveform via ADC + DMA, compute RMS and frequency

**What we did**
- A timer (TIM2) triggers the ADC at 10 kHz. DMA stores the samples in a buffer without using the CPU.
- The code reads one finished half of the buffer at a time, so the data does not change while it is being measured.
- For each block it computes the average, RMS with DC (`RMS TOTAL`), and RMS of the AC part only (`RMS AC ONLY`).
- Frequency is measured by counting how often the signal crosses a threshold. The frequency is shown as 0 when there is no signal.
- Printing does not stop sampling, so `MISSED BLOCKS`, `ADC OVR` and `TX DROPPED` stay 0.
- Startup line printed by the board: `TIM2 clock: ______ Hz | sample rate: ______ Hz`

**Connection**

Part A: DC voltage (potentiometer)
- Potentiometer one end -> 3.3V
- Potentiometer other end -> GND
- Potentiometer middle pin (wiper) -> PA0

Part B: frequency measurement (test signal from the board)
- We had no function generator, so the board makes its own square wave on pin PA6 (frequency set by `TEST_PWM_FREQ_HZ` in the code: 10, 50, 100 or 200).
- Step 1: connect one end of the 9.77 kOhm resistor to PA6.
- Step 2: connect the other end of the resistor to PA0.
- Step 3: connect one leg of the 1 uF capacitor to the same point as the resistor's PA0 end.
- Step 4: connect the other leg of the capacitor to GND.
- Input limit on PA0: 0 to 3.3 V only.
- The test signal comes from the board's PA6 pin (0 to 3.3 V), so no external function generator offset was needed.
- The submitted code has `TEST_PWM_FREQ_HZ` set to 0 (test signal off). For the frequency tests, we changed it to 10, 50, 100 and 200, then uploaded again.

```
PA6 (test square wave) ---[ 9.77 kOhm ]---+--- PA0 (ADC input)
                                           |
                                        ===== 1 uF
                                           |
                                          GND
```

To remove the signal, pull the wire from PA6. The board then reads no signal and the frequency returns to 0.

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
ADC AVERAGE: 1319 | 1062 mV
RMS TOTAL (with DC): 1064 mV
RMS AC ONLY: 33 mV
FILTERED ADC: 1278
FREQUENCY: 10.00 Hz
MIN ADC: 1247
MAX ADC: 1418
SENSOR FAULT: NO
MISSED BLOCKS: 0 | ADC OVR: 0 | TX DROPPED: 0
```

**Output at 50 Hz**
```
ADC AVERAGE: 3155 | 2542 mV
RMS TOTAL (with DC): 2745 mV
RMS AC ONLY: 1034 mV
FILTERED ADC: 330
FREQUENCY: 50.00 Hz
MIN ADC: 237
MAX ADC: 4092
SENSOR FAULT: NO
MISSED BLOCKS: 0 | ADC OVR: 0 | TX DROPPED: 0
```

**Output at 100 Hz**
```
ADC AVERAGE: 2335 | 1881 mV
RMS TOTAL (with DC): 2235 mV
RMS AC ONLY: 1205 mV
FILTERED ADC: 3861
FREQUENCY: 100.00 Hz
MIN ADC: 46
MAX ADC: 4038
SENSOR FAULT: NO
MISSED BLOCKS: 0 | ADC OVR: 0 | TX DROPPED: 0
```

**Output at 200 Hz**
```
ADC AVERAGE: 2023 | 1630 mV
RMS TOTAL (with DC): 1866 mV
RMS AC ONLY: 907 mV
FILTERED ADC: 666
FREQUENCY: 200.00 Hz
MIN ADC: 359
MAX ADC: 3730
SENSOR FAULT: NO
MISSED BLOCKS: 0 | ADC OVR: 0 | TX DROPPED: 0
```

---

## Task 2: Sensor fault detection and stable filtering (no false alarms)

**What we did**

Sensor fault detection:
- The code checks each block for three problems: average too low (below 100 ADC counts, "low / disconnected"), average too high (above 4000), or clipping (25% or more of the samples stuck at 0 or 4095).
- To avoid false alarms, a fault is shown only after the problem lasts 10 blocks in a row, and it clears only after 10 good blocks in a row. A single noisy block cannot trigger it.

Stable filtering:
- The signal goes through a smoothing filter (`FILTERED ADC`). It keeps extra fractional bits, so small changes are not lost and the value follows the input.
- The filtered value is used for the frequency measurement, so noise does not cause extra crossings. Thresholds have a hysteresis band for the same reason.
- For the potentiometer (Task 1 output), the average is 2607 and the filtered value is 2608, so the filter is stable.

**Connection**
- Normal operation: same as Task 1 (potentiometer or PA6 -> 9.77 kOhm -> PA0 with 1 uF to GND).
- Fault test: pull the wire from PA6 so that PA0 is left near 0 V.

**Result**
- No false alarms: every normal run above (potentiometer, 10, 50, 100 and 200 Hz) shows `SENSOR FAULT: NO`.
- Fault detected: when the signal is removed, the code shows the fault, and the frequency returns to 0 after a 2 second timeout.

**Output after removing the signal**
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
- Average, RMS and min/max are calculated from short 12.8 ms blocks, so they change from report to report. The frequency is the reliable value.
- A floating (unplugged) input can look like a normal signal. A pull-down resistor on PA0 is needed to detect it reliably.
- A full-swing 0 to 3.3 V square wave can trigger the clipping fault. It did not appear in our runs.
