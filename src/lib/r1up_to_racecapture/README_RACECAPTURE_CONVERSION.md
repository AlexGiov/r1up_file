# R1UP to RaceCapture CSV Converter

Conversione di file telemetrici R1UP binari in formato RaceCapture CSV compatibile con Track Attack.

## Architettura

```
┌─────────────────────────────────────┐
│  r1up_to_racecapture (Adapter)      │  ← Livello di conversione
│  • Mapping 28→26 canali             │
│  • GPS conversion (6→2 vars)        │
│  • Interval/UTC generation          │
│  • Fixed-point→Float + scaling      │
│  • Sparse logic based on Hz         │
└────────┬──────────────┬─────────────┘
         │              │
         ▼              ▼
┌────────────────┐  ┌──────────────────┐
│  r1up_file     │  │ racecapture_file │  ← Librerie I/O
│  (binary read) │  │  (CSV write)     │
│  • Body parser │  │  • Sparse format │
│  • Fixed-point │  │  • Configurable  │
│  • GPS helper  │  │  • Track Attack  │
└────────┬───────┘  └────────┬─────────┘
         │                   │
         ▼                   ▼
    ┌─────────┐         ┌─────────┐
    │ ag_fsal │         │ ag_fsal │
    └─────────┘         └─────────┘
```

## Librerie

### 1. **racecapture_file** (Generic CSV Writer)
**Posizione:** `src/lib/racecapture_file/`

**Responsabilità:**
- Scrivere file CSV in formato RaceCapture
- Gestire sparse logic (campi vuoti = riusa ultimo valore)
- Configurabile a runtime (qualsiasi numero/tipo di canali)

**API Pubblica:**
```c
racecapture_file_handle_t racecapture_file_create(const char* filepath);
racecapture_file_result_t racecapture_file_add_channel(handle, const racecapture_channel_t* ch);
racecapture_file_result_t racecapture_file_write_header(handle);
racecapture_file_result_t racecapture_file_begin_row(handle, uint64_t sample_idx);
racecapture_file_result_t racecapture_file_write_value(handle, double value);
racecapture_file_result_t racecapture_file_write_empty(handle);
racecapture_file_result_t racecapture_file_end_row(handle);
racecapture_file_result_t racecapture_file_close(handle);
```

**Caratteristiche:**
- ✅ Zero dipendenze da R1UP (100% generico)
- ✅ Sparse logic automatica basata su sample_idx e Hz canale
- ✅ Formattazione con precisione configurabile
- ✅ Validazione stato (ordine chiamate API)

---

### 2. **r1up_file** (R1UP Binary Reader)
**Posizione:** `src/mut/`

**Aggiunte per conversione:**

**GPS Helper Functions:**
```c
double r1up_file_convert_gps_latitude(uint8_t deg, int8_t min, uint16_t deci);
double r1up_file_convert_gps_longitude(uint8_t deg, int8_t min, uint16_t deci);
```

**Formula conversione (con workaround bug R1UP):**
```c
if (min < 0):
    decimal = -(deg + (abs(min) + deci/10000) / 60)
else:
    decimal = deg + (abs(min) + deci/10000) / 60
```

**Motivazione:** R1UP ha un bug dove coordinate negative usano `min < 0` invece di `deg < 0`.

---

### 3. **r1up_to_racecapture** (Adapter Layer)
**Posizione:** `src/lib/r1up_to_racecapture/`

**Responsabilità:**
- Coordinare conversione completa R1UP → RaceCapture
- Generare canali sistema (Interval, Utc)
- Mappare 28 variabili R1UP → 26 canali RaceCapture
- Convertire GPS (6 vars → 2 decimal degrees)
- Applicare scaling unità (rad/s → deg/s per gyro)

**API Pubblica:**
```c
r1up_to_rc_result_t r1up_to_racecapture_convert(
    const char* r1up_filepath,
    const char* racecapture_filepath
);
const char* r1up_to_rc_get_error_string(r1up_to_rc_result_t result);
```

**X-Macro Mapping Table (`r1up_to_racecapture_cfg.h`):**
```c
#define R1UP_TO_RC_MAPPING_TABLE(ENTRY) \
    ENTRY(SYSTEM_INTERVAL,  0,           "Interval",  "ms",   0,    0,    1,  0,  1.0    ) \
    ENTRY(SYSTEM_UTC,       0,           "Utc",       "ms",   0,    0,    1,  0,  1.0    ) \
    ENTRY(DATA_SIMPLE,      RPM_ID,      "rpm",       "RPM",  0,    12000,10, 0,  1.0    ) \
    ENTRY(DATA_SCALED,      X_GYRO_RDS_ID,"x_gyro",   "Deg/s",-500, 500,  10, 1,  57.2958) \
    ENTRY(DATA_GPS_LAT,     LAT_DEG_ID,  "latitude",  "Deg",  -90,  90,   10, 6,  1.0    ) \
    ENTRY(DATA_GPS_LON,     LONG_DEG_ID, "longitude", "Deg",  -180, 180,  10, 6,  1.0    ) \
    /* ... 20 altri canali ... */
```

**Tipi di Canale:**
- `SYSTEM_INTERVAL`: Uptime calcolato = `sample_idx * 100 ms`
- `SYSTEM_UTC`: Timestamp Unix = `time(NULL) * 1000` (temporaneo, futuro: GPS-derived)
- `DATA_SIMPLE`: Conversione diretta fixed-point → float
- `DATA_SCALED`: Conversione + moltiplicatore (es. gyro rad/s → deg/s)
- `DATA_GPS_LAT/LON`: Conversione da 3 componenti R1UP a decimal degrees

---

## Formato RaceCapture CSV

### Header
```
"<Name>"|"<Units>"|<Min>|<Max>|<Hz>|<Precision>,...
```

**Esempio (26 canali):**
```csv
"Interval"|"ms"|0|0|1,"Utc"|"ms"|0|0|1,"rpm"|"RPM"|0|12000|10,"exaust"|"C"|0|1000|10|1,...
```

### Data Rows (Sparse CSV)
```csv
0,1234567890000,3000,850.5,45.2,...
100,1234567890100,3050,850.5,,...       <- coolant vuoto (sparse)
200,1234567890200,3100,851.0,,...
```

**Semantica Sparse:**
- Campo vuoto (`,,`) = riusa ultimo valore
- Riduce dimensione file (canali a 1 Hz hanno molti campi vuoti)

**Sample Rate Decimation:**
- Base rate: 10 Hz (sample ogni 100 ms)
- Canali 10 Hz: scritti ogni sample (divisor = 1)
- Canali 1 Hz: scritti ogni 10 samples (divisor = 10)
- Formula: `should_write = (sample_idx % (base_hz / channel_hz)) == 0`

---

## Mapping Canali (28 R1UP → 26 RaceCapture)

| # RC  | Nome RC        | # R1UP | Nome R1UP                       | Note                     |
| ----- | -------------- | ------ | ------------------------------- | ------------------------ |
| 1     | Interval       | -      | (calcolato)                     | sample_idx * 100 ms      |
| 2     | Utc            | -      | (calcolato)                     | time(NULL) * 1000        |
| 3     | rpm            | 1      | RPM_ID                          | uq16_0 → double          |
| 4     | exaust         | 2      | EXAUSTEMP_ID                    | uq12_4 → double          |
| 5     | coolant        | 3      | FLUIDTEMP_ID                    | uq10_6 → double (1 Hz)   |
| 6-8   | xacc/yacc/zacc | 4-6    | X_ACC_ID, Y_ACC_ID, Z_ACC_ID    | q4_11 → double           |
| 9-11  | rool/pitch/yaw | 7-9    | X_ROOL_ID, Y_PITCH_ID, Z_YAW_ID | q3_12 → double           |
| 12-14 | x/y/z_gyro     | 10-12  | X/Y/Z_GYRO_RDS_ID               | q3_28 → double × 57.2958 |
| 15    | gps_speed      | 13     | GPS_VEL_ID                      | uq16_0 → double          |
| 16    | latitude       | 14-16  | LAT_DEG/MIN/DECI                | GPS conversion           |
| 17    | longitude      | 17-19  | LONG_DEG/MIN/DECI               | GPS conversion           |
| 18    | gear           | 22     | GEAR_ID                         | uq8_0 → double           |
| 19    | throttle       | 23     | THROTTLE_ID                     | uq8_0 → double           |
| 20    | altitude       | 20     | MSL_ALT_ID                      | uq16_0 → double          |
| 21    | gps_sat        | 21     | GPS_USED_SAT_ID                 | uq16_0 → double          |
| 22    | intTemp        | 24     | INTRNL_TMP                      | uq12_4 → double (1 Hz)   |
| 23    | battVolt       | 25     | BAT_VOLT_ID                     | uq6_10 → double (1 Hz)   |
| 24    | extps          | 26     | EXT_PS_VOLT_ID                  | uq12_4 → double (1 Hz)   |
| 25    | rpm_hall       | 27     | RPM_HALL_ID                     | uq16_0 → double          |
| 26    | pwss_hall      | 28     | PWSS_HALL_ID                    | uq32_0 → double          |

**Totale:** 26 canali RaceCapture da 28 variabili R1UP (2 GPS aggregati)

---

## Principi Architetturali

### 1. **Separation of Concerns (SoC)**
- `racecapture_file`: conosce SOLO formato CSV
- `r1up_file`: conosce SOLO formato R1UP
- `r1up_to_racecapture`: conosce ENTRAMBI (adapter)

### 2. **Single Responsibility Principle (SRP)**
- `racecapture_file`: scrive CSV sparse
- `r1up_file`: legge R1UP binary + GPS conversion
- `r1up_to_racecapture`: coordina mapping e conversione

### 3. **Open/Closed Principle (OCP)**
- `racecapture_file` può scrivere CSV per QUALSIASI fonte dati (non solo R1UP)
- Estensibile a `sensor_to_racecapture`, `r1up_to_json`, ecc.

### 4. **Dependency Inversion Principle (DIP)**
- Adapter (high-level) dipende da I/O libraries (low-level)
- Nessuna dipendenza circolare

---

## Uso

### Esempio Semplice
```c
#include "r1up_to_racecapture.h"

int main(void) {
    r1up_to_rc_result_t result = r1up_to_racecapture_convert(
        "telemetry.r1up",
        "telemetry.csv"
    );
    
    if (result != R1UP_TO_RC_SUCCESS) {
        printf("Error: %s\n", r1up_to_rc_get_error_string(result));
        return 1;
    }
    
    printf("Conversion successful!\n");
    return 0;
}
```

### Output CSV (esempio)
```csv
"Interval"|"ms"|0|0|1,"Utc"|"ms"|0|0|1,"rpm"|"RPM"|0|12000|10,"exaust"|"C"|0|1000|10|1,...
0,1737648000000,3000,850.5,45.2,-0.02,-0.03,0.93,0.5,-6.2,-0.2,10.5,20.3,5.1,120.5,45.508333,12.508333,3,50,125.0,5,12.5,3.3,5.0,3000,250.5
100,1737648000100,3050,850.5,,-0.02,-0.03,0.95,0.2,-6.0,-0.3,10.2,20.1,5.0,121.0,45.508333,12.508333,3,50,125.0,5,,,3050,250.5
200,1737648000200,3100,851.0,45.2,-0.01,-0.03,0.97,0.1,-5.5,-0.2,10.0,19.8,4.9,121.5,45.508333,12.508333,3,51,125.0,5,,,3100,251.0
```

**Note:**
- Riga 1: Header
- Riga 2: Sample 0 (tutti i canali popolati)
- Riga 3: Sample 1 (coolant vuoto per sparse, 1 Hz channel)
- Riga 4: Sample 2 (coolant popolato ogni 10 samples)

---

## Performance

**Conversione Tipica:**
- File R1UP: 10,000 samples @ 10 Hz
- Dimensione: ~500 KB binary → ~1.5 MB CSV sparse
- Tempo: ~100 ms (100,000 samples/sec)
- Memoria: ~2 MB (buffer conversione float)

**Ottimizzazioni:**
- Batch conversion (tutto in memoria prima della scrittura)
- Sparse CSV riduce dimensione file del 30-50%

---

## Compatibilità

**Track Attack:**
- ✅ Header format conforme
- ✅ Sparse CSV supportato nativamente
- ✅ 26 canali standard per auto da corsa
- ✅ GPS decimal degrees

**RaceCapture Pro:**
- ✅ Formato documentato: https://wiki.autosportlabs.com/RaceCapturePro2_logfile_reference
- ✅ Canali configurabili (non limitati a 26)

---

## TODO / Miglioramenti Futuri

1. **UTC da GPS:** Sostituire `time(NULL)` con timestamp GPS da R1UP (quando disponibile)
2. **ag_time Library:** Sostituire `time.h` con libreria portabile embedded
3. **Validazione GPS:** Range check coordinate (-90/+90 lat, -180/+180 lon)
4. **Streaming Mode:** Conversione chunk-by-chunk per file enormi (riduce memoria)
5. **Error Recovery:** Gestione sample corrotti (skip vs abort)
6. **Unit Testing:** Suite completa per tutte le librerie

---

## Autore
Alex Giov - Race1Up Project  
Data: Gennaio 2026  
Versione: 1.0.0
