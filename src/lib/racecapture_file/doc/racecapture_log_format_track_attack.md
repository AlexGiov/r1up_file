# RaceCapture Log File Format (Track Attack Compatible)

This document describes the log file format based on the **RaceCapture** sparse CSV log standard.  
Official format reference: see the [RaceCapturePro2 logfile documentation](https://wiki.autosportlabs.com/RaceCapturePro2_logfile_reference).  
Log files generated in this format are **compatible with [Track Attack](https://trackattack.io/)** for data analysis.

---

## 1. Overview

The log file is a **sparsely populated CSV**:

- The **first line** is a header describing all channels.
- The **following lines** form the data segment, one time sample per line.
- Many fields in the data rows are **empty**: an empty value means  
  _"reuse the last known value for this channel"_.

The column order in every data row is identical to the order of channel descriptors in the header.

---

## 2. Header Line

The first line of the log file is the header.  
It consists of one **channel descriptor** per column, separated by commas, on a **single line**.

### 2.1 Channel Descriptor Format

Each channel descriptor has the following format:

```text
"<Name>"|"Units"|<Min>|<Max>|<SampleRateHz>|<Precision (optional)>
```

Where:

- `Name`  
  Channel name (string), wrapped in double quotes.

- `Units`  
  Units label (string), wrapped in double quotes.  
  If the channel is unitless, this may be an empty string (`""`).

- `Min`, `Max`  
  Minimum and maximum expected values for the channel (integer or float).  
  These are metadata hints and can be used for scaling, display ranges, etc.

- `SampleRateHz`  
  Nominal sample rate in Hertz (integer or float).  
  The actual logging is sparse, but this describes how frequently new values are expected.

- `Precision` (optional)  
  Number of decimal places for display.  
  If omitted or zero, a default behavior is used by the consumer.

Multiple channel descriptors are concatenated with commas:

```text
"<Name1>"|"Units1"|min1|max1|rate1[|precision1],
"<Name2>"|"Units2"|min2|max2|rate2[|precision2],
...
"<NameN>"|"UnitsN"|minN|maxN|rateN[|precisionN]
```

> In the actual file, the header appears on **one single line**, without line breaks.

---

## 3. Special Channels: `Interval` and `Utc`

Two system channels are conventionally present in every log:

1. **Interval**  
   - Name: `Interval`  
   - Units: `ms`  
   - Description: milliseconds of system uptime; represents elapsed time since device startup.  
   - Typically, this is a monotonically increasing counter, with increments corresponding to the logging period (e.g. ~40 ms).

2. **Utc**  
   - Name: `Utc`  
   - Units: `ms`  
   - Description: UTC time in milliseconds since the Unix epoch (1 January 1970).  
   - If no valid GPS time is available, this value can be `0`.

These channels:

- Are defined in the header like any other channel.
- Are expected to have **non-empty values on every data row**.

---

## 4. Channel List and Metadata

> **NOTE:** The following channel set is a **SPECIFIC EXAMPLE** for the Race1Up project.  
> The RaceCapture format is **fully configurable**: different log files can have different channels.  
> Only `Interval` and `Utc` are mandatory system channels.  
> This table defines the 26 channels used for Race1Up telemetry conversion.

The following is the channel set used in this specific implementation, in the exact order in which they appear in the file:

1. `Interval` – time interval in milliseconds since device startup  
2. `Utc` – UTC timestamp in milliseconds since Unix epoch  
3. `rpm` – engine revolutions per minute  
4. `exaust` – exhaust gas temperature  
5. `coolant` – coolant fluid temperature  
6. `xacc` – longitudinal acceleration along the X axis  
7. `yacc` – lateral acceleration along the Y axis  
8. `zacc` – vertical acceleration along the Z axis  
9. `rool` – vehicle roll angle  
10. `pitch` – vehicle pitch angle  
11. `yaw` – vehicle yaw angle  
12. `gps_speed` – vehicle speed derived from GPS  
13. `latitude` – latitude measured by GPS  
14. `longitude` – longitude measured by GPS  
15. `gear` – engaged gearbox gear  
16. `throttle` – throttle opening angle  
17. `altitude` – altitude derived from GPS  
18. `gps_sat` – number of currently tracked GPS satellites  
19. `intTemp` – internal temperature of the acquisition device  
20. `battVolt` – battery voltage  
21. `extps` – internal supply voltage  
22. `x_gyro` – angular velocity around X axis from gyroscope  
23. `y_gyro` – angular velocity around Y axis from gyroscope  
24. `z_gyro` – angular velocity around Z axis from gyroscope  
25. `rpm_hall` – engine speed measured by Hall-effect sensor  
26. `pwss_hall` – wheel speed signal from toothed wheel / Hall sensor on vehicle wheel  

### 4.1 Channel Definition Table

This table summarises the metadata used to build the header:

| #   | Name      | Units | Min  | Max   | SampleRateHz | Precision | Notes                                              |
| --- | --------- | ----- | ---- | ----- | ------------ | --------- | -------------------------------------------------- |
| 1   | Interval  | ms    | 0    | 0     | 1            | 0         | System uptime, in milliseconds                     |
| 2   | Utc       | ms    | 0    | 0     | 1            | 0         | UTC time in ms since Unix epoch (0 if unavailable) |
| 3   | rpm       | RPM   | 0    | 12000 | 10           | 0         | Engine revolutions per minute                      |
| 4   | exaust    | C     | 0    | 1000  | 10           | 1         | Exhaust gas temperature (°C)                       |
| 5   | coolant   | C     | -40  | 150   | 1            | 1         | Coolant fluid temperature (°C)                     |
| 6   | xacc      | G     | -5   | 5     | 10           | 2         | Longitudinal acceleration (X axis)                 |
| 7   | yacc      | G     | -5   | 5     | 10           | 2         | Lateral acceleration (Y axis)                      |
| 8   | zacc      | G     | -5   | 5     | 10           | 2         | Vertical acceleration (Z axis)                     |
| 9   | rool      | Deg   | -180 | 180   | 10           | 1         | Vehicle roll angle                                 |
| 10  | pitch     | Deg   | -180 | 180   | 10           | 1         | Vehicle pitch angle                                |
| 11  | yaw       | Deg   | -180 | 180   | 10           | 1         | Vehicle yaw angle                                  |
| 12  | gps_speed | km/h  | 0    | 350   | 10           | 1         | GPS-derived speed                                  |
| 13  | latitude  | Deg   | -90  | 90    | 10           | 6         | GPS latitude                                       |
| 14  | longitude | Deg   | -180 | 180   | 10           | 6         | GPS longitude                                      |
| 15  | gear      |       | -1   | 8     | 10           | 0         | Gear: -1 reverse, 0 neutral, 1..7 forward          |
| 16  | throttle  | %     | 0    | 100   | 10           | 1         | Throttle opening (percent)                         |
| 17  | altitude  | m     | -500 | 5000  | 10           | 1         | GPS altitude (meters)                              |
| 18  | gps_sat   |       | 0    | 20    | 10           | 0         | Number of tracked GPS satellites                   |
| 19  | intTemp   | C     | -40  | 125   | 1            | 1         | Internal device temperature                        |
| 20  | battVolt  | V     | 0    | 20    | 1            | 2         | Battery voltage                                    |
| 21  | extps     | V     | 0    | 20    | 1            | 2         | Internal supply voltage                            |
| 22  | x_gyro    | Deg/s | -500 | 500   | 10           | 1         | Angular rate around X axis                         |
| 23  | y_gyro    | Deg/s | -500 | 500   | 10           | 1         | Angular rate around Y axis                         |
| 24  | z_gyro    | Deg/s | -500 | 500   | 10           | 1         | Angular rate around Z axis                         |
| 25  | rpm_hall  | RPM   | 0    | 12000 | 10           | 0         | Engine rpm from Hall-effect sensor                 |
| 26  | pwss_hall | Hz    | 0    | 500   | 10           | 1         | Wheel speed pulses from toothed wheel              |

---

## 5. Example Header Line

Below is an example header line built from the definitions above.  
(Line breaks are added here for clarity; the actual file uses a single line.)

```text
"Interval"|"ms"|0|0|1,
"Utc"|"ms"|0|0|1,
"rpm"|"RPM"|0|12000|10,
"exaust"|"C"|0|1000|10|1,
"coolant"|"C"|-40|150|1|1,
"xacc"|"G"|-5|5|10|2,
"yacc"|"G"|-5|5|10|2,
"zacc"|"G"|-5|5|10|2,
"rool"|"Deg"|-180|180|10|1,
"pitch"|"Deg"|-180|180|10|1,
"yaw"|"Deg"|-180|180|10|1,
"gps_speed"|"km/h"|0|350|10|1,
"latitude"|"Deg"|-90|90|10|6,
"longitude"|"Deg"|-180|180|10|6,
"gear"||-1|8|10,
"throttle"|"% "|0|100|10|1,
"altitude"|"m"|-500|5000|10|1,
"gps_sat"||0|20|10,
"intTemp"|"C"|-40|125|1|1,
"battVolt"|"V"|0|20|1|2,
"extps"|"V"|0|20|1|2,
"x_gyro"|"Deg/s"|-500|500|10|1,
"y_gyro"|"Deg/s"|-500|500|10|1,
"z_gyro"|"Deg/s"|-500|500|10|1,
"rpm_hall"|"RPM"|0|12000|10,
"pwss_hall"|"Hz"|0|500|10|1
```

In the actual log file, this must be written as:

```text
"Interval"|"ms"|0|0|1,"Utc"|"ms"|0|0|1,"rpm"|"RPM"|0|12000|10,"exaust"|"C"|0|1000|10|1,...
```

all on one line, with no line breaks.

---

## 6. Data Segment (Sparse CSV)

After the header, each subsequent line represents a time sample and contains:

- One comma-separated value per channel, **in the same order** as the header.
- Values are plain numbers (integer or float).
- Empty fields are allowed and are meaningful.

### 6.1 Semantics of Empty Fields

For any non-empty field:

- The numeric value is a **new sample** for that channel.

For any empty field (i.e. two commas in a row `,,`):

- No new sample is provided for that channel.
- The consumer should use the **last non-empty value** previously seen for that column.

This is what makes the format a **sparse CSV**: channels sampled at lower rates only provide new values periodically, leaving empty cells in between.

### 6.2 Example Data Rows

Example (simplified, with only a subset of channels):

```text
"Interval"|"ms"|0|0|1,"Utc"|"ms"|0|0|1,"xacc"|"G"|-5|5|10|2,"yacc"|"G"|-5|5|10|2,"rpm"|"RPM"|0|12000|10
402,0,-0.02,-0.03,
442,0,-0.02,-0.03,1500
482,0,-0.01,-0.03,
```

Interpretation:

- At `Interval = 402 ms`: new `xacc = -0.02`, `yacc = -0.03`, `rpm` unchanged (empty).
- At `Interval = 442 ms`: new `xacc = -0.02`, `yacc = -0.03`, `rpm = 1500`.
- At `Interval = 482 ms`: new `xacc = -0.01`, `yacc = -0.03`, `rpm` unchanged (still 1500).

---

## 7. Implementation Notes (for C or Other Languages)

When implementing a writer for this log format:

1. **Define channel metadata**  
   Use a table/array of structs containing:
   - `name`
   - `units`
   - `min`
   - `max`
   - `sampleRateHz`
   - `precision`

2. **Determine base time step**  
   Select a base period as the inverse of the highest sample rate  
   (e.g. for a maximum of 10 Hz, base period = 100 ms).  

3. **Write the header**  
   - For each channel, format a descriptor string:
     `"<Name>"|"Units"|min|max|rate[|precision]`
   - Join all descriptors with commas.
   - Write this as the first line of the file.

4. **Write data rows (sparse)**  
   - For each time step:
     - Compute and write `Interval` (ms since startup).
     - Compute and write `Utc` (if available, else 0).
     - For each channel:
       - If this time step corresponds to a new sample (according to `sampleRateHz`), write the numeric value.
       - Otherwise, write an empty field.
   - End each row with a newline (`\n`).

5. **Compatibility**  
   As long as the file follows this header structure and sparse CSV behavior,  
   it can be imported by software that supports the RaceCapture log format,  
   including Track Attack.
