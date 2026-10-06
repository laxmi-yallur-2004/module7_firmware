# Module 7 – ADC + DMA + RMS + Frequency + Sensor Fault + Stable Processing Pipeline

## Objective

* ADC sampling using DMA
* RMS voltage measurement
* Frequency measurement
* Stable signal filtering
* Sensor fault detection

## Hardware

**STM32F401CCU6**

| Device              | STM32 |
| ------------------- | ----- |
| Potentiometer wiper | PA0   |
| Potentiometer 3.3V  | 3.3V  |
| Potentiometer GND   | GND   |
| USB-TTL RX          | PA9   |
| USB-TTL TX          | PA10  |
| USB-TTL GND         | GND   |
| ST-LINK SWDIO       | PA13  |
| ST-LINK SWCLK       | PA14  |
| ST-LINK GND         | GND   |

## Working

* ADC reads the analog signal from PA0.
* DMA continuously stores ADC samples in a circular buffer.
* The samples are processed in blocks.
* RMS voltage is calculated from the ADC samples.
* A filter reduces small ADC variations.
* Frequency is detected from repeated signal crossings.
* Sensor fault is detected when the input stays near the ADC limits.
* The processing pipeline continuously acquires, processes, filters, and displays the result.

## Voltage Calculation

```text
Voltage (mV) = ADC × 3300 / 4095
```

* 3300 = 3.3 V in millivolts
* 4095 = maximum 12-bit ADC value

## Test Result

Using the potentiometer:

```text
ADC AVERAGE: 2607 | 2100 mV
RMS VOLTAGE: 2100 mV
FILTERED ADC: 2608
FREQUENCY: 0 Hz
MIN ADC: 2604
MAX ADC: 2614
SENSOR FAULT: NO
```

## Result

* ADC average is **2607**.
* Voltage is approximately **2100 mV (2.10 V)**.
* RMS is approximately **2100 mV** because the potentiometer gives a nearly constant DC signal.
* Filtered ADC is stable.
* Frequency is **0 Hz** because the potentiometer produces DC, not a periodic signal.
* Small MIN/MAX changes are normal ADC noise.
* Sensor fault is **NO** because the input is not near the ADC limits.

## Frequency Test

For non-zero frequency, a periodic signal can be connected to PA0.

```text
Function Generator OUT → PA0
Function Generator GND → STM32 GND
```

Example: **100 Hz input → approximately 100 Hz frequency reading.**

## Conclusion

The module successfully demonstrates:

**ADC + DMA + RMS + Frequency + Sensor Fault + Stable Processing Pipeline**
