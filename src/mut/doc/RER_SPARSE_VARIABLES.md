# Variabili Sparse nel Formato Legacy ReRace (RER)

## Problema: ID Non-Sequenziali nel File RER

Durante l'implementazione della conversione streaming per file Legacy ReRace, è emerso un **problema critico di disallineamento** tra l'ordine fisico delle variabili nel file e i loro ID logici.

### Contesto

Il formato RER memorizza le variabili in un ordine fisico specifico nel file, ma assegna ID che **non sono sequenziali** rispetto a questa posizione fisica.

#### Ordine Fisico nel File vs ID Logici

| Posizione Fisica | Nome Variabile   | ID nel File (1-based) | ID Normalizzato (0-based) |
| ---------------- | ---------------- | --------------------- | ------------------------- |
| 0                | RPM              | 1                     | 0                         |
| 1                | EXAUST           | 2                     | 1                         |
| 2                | COOLANT          | 3                     | 2                         |
| 3                | X_ACC            | 4                     | 3                         |
| 4                | Y_ACC            | 5                     | 4                         |
| 5                | Z_ACC            | 6                     | 5                         |
| 6                | X_ROOL           | 7                     | 6                         |
| 7                | Y_PITCH          | 8                     | 7                         |
| 8                | Z_YAW            | 9                     | 8                         |
| **9**            | **X_GYRO**       | **24** ❌              | **23** ❌                  |
| **10**           | **Y_GYRO**       | **25** ❌              | **24** ❌                  |
| **11**           | **Z_GYRO**       | **26** ❌              | **25** ❌                  |
| **12**           | **GPS_VEL**      | **10** ❌              | **9** ❌                   |
| 13               | LAT_DEG          | 11                    | 10                        |
| 14               | LAT_MIN          | 12                    | 11                        |
| 15               | LAT_DECI         | 13                    | 12                        |
| 16               | LONG_DEG         | 14                    | 13                        |
| 17               | LONG_MIN         | 15                    | 14                        |
| 18               | LONG_DECI        | 16                    | 15                        |
| 19               | GEAR             | 17                    | 16                        |
| 20               | THROTTLE         | 18                    | 17                        |
| **21**           | **MSL_ALT**      | **19** ✓              | **18** ✓                  |
| **22**           | **GPS_USED_SAT** | **20** ✓              | **19** ✓                  |
| 23               | INTRNL_TMP       | 21                    | 20                        |
| 24               | BAT_VOLT         | 22                    | 21                        |
| 25               | EXT_PS_VOLT      | 23                    | 22                        |
| 26               | RPM_HALL         | 27                    | 26                        |
| 27               | PWSS_HALL        | 28                    | 27                        |

### Analisi del Disallineamento

**Variabili "Fuori Posto":**
- **Giroscopi** (X_GYRO, Y_GYRO, Z_GYRO): Posizioni fisiche 9-11, ma ID 23-25 (gap di +14 posizioni)
- **GPS_VEL**: Posizione fisica 12, ma ID 9 (gap di -3 posizioni)
- **Altitudine e Satelliti**: Posizioni 21-22, ID 18-19 (lievemente sfasati)

**Ipotesi sull'Origine:**
Il formato RER probabilmente ha subito modifiche durante lo sviluppo del firmware ReRace:
1. Versione iniziale con variabili base (RPM, temperatura, accelerometro, GPS)
2. Aggiunta successiva dei giroscopi inseriti fisicamente dopo YAW ma con ID alla fine della lista
3. Questo ha creato un "buco" negli ID che è stato poi riempito da GPS_VEL

## Impatto sul Codice

### Problema Originale: Array Indexing

Il codice di conversione assume che **ID = Posizione Array**:

```c
// In r1up_to_racecapture.c - Macro WRITE_CHANNEL_STREAMING
#define WRITE_CHANNEL_STREAMING(chan_type, r1up_var_id, name, units, min, max, hz, prec, scale) \
    { \
        // ... \
        else if (chan_type == DATA_GPS_LAT) { \
            uint8_t lat_deg = (uint8_t)r1up_row[LAT_DEG_ID];    // Assume LAT_DEG_ID = posizione! \
            // ... \
        } else if (chan_type == DATA_SIMPLE || chan_type == DATA_SCALED) { \
            value = r1up_row[r1up_var_id] * scale;  // ❌ PROBLEMA: r1up_var_id ≠ posizione! \
        } \
        // ... \
    }
```

### Esempio Concreto del Bug

**Accesso a GPS_VEL (ID=9 normalizzato):**
```c
// Codice esegue:
value = r1up_row[GPS_VEL_ID];  // GPS_VEL_ID = 9

// File RER ha:
// r1up_row[9] = X_GYRO (ID 23, ma posizione fisica 9)
// r1up_row[12] = GPS_VEL (ID 9, ma posizione fisica 12)

// Risultato: legge X_GYRO invece di GPS_VEL! ❌
```

### Strutture Dati Coinvolte

```c
// Durante la lettura streaming del file RER:

// 1. stream_last_values[] - indicizzato per ID normalizzato (CORRETTO)
file->stream_last_values[0] = RPM_value;       // ID 0 = RPM
file->stream_last_values[23] = X_GYRO_value;   // ID 23 = X_GYRO
file->stream_last_values[9] = GPS_VEL_value;   // ID 9 = GPS_VEL

// 2. VSF table - definisce l'ordine di lettura dal file (PROBLEMA!)
// Prima della fix: ordinata per posizione fisica nel file
R1UP_FILE_RR_FILE_VARS_STREAM_FORMAT_TABLE:
  VSF_ENTRY[0]: RR_RPM_ID (ID=0)
  VSF_ENTRY[9]: RR_X_GYRO_RDS_ID (ID=23)  ❌ Posizione 9, ID 23
  VSF_ENTRY[12]: RR_GPS_VEL_ID (ID=9)     ❌ Posizione 12, ID 9

// 3. Copy loop - copia in r1up_row[] per posizione sequenziale
for (uint32_t var_idx = 0; var_idx < 28; var_idx++) {
    // var_idx = posizione array r1up_row
    // vsf[var_idx].id = ID logico della variabile
    r1up_row[var_idx] = stream_last_values[vsf[var_idx].id];
}

// Risultato PRIMA della fix:
// r1up_row[0] = stream_last_values[0] = RPM ✓
// r1up_row[9] = stream_last_values[23] = X_GYRO ❌ Dovrebbe essere GPS_VEL!
// r1up_row[12] = stream_last_values[9] = GPS_VEL ❌ Dovrebbe essere X_GYRO!
```

## Soluzione Implementata: Riordinamento VSF Table

### Approccio

Invece di mantenere l'ordine fisico del file, **riordinare la tabella VSF per ID normalizzato**:

```c
// r1up_file_cfg.h - PRIMA (ordinata per posizione fisica):
#define R1UP_FILE_RR_FILE_VARS_STREAM_FORMAT_TABLE(VSF_ENTRY) \
    VSF_ENTRY(RR_RPM_ID, ...)           // pos=0, ID=0
    VSF_ENTRY(RR_EXAUSTEMP_ID, ...)     // pos=1, ID=1
    /* ... */
    VSF_ENTRY(RR_X_GYRO_RDS_ID, ...)    // pos=9, ID=23 ❌ MISMATCH
    VSF_ENTRY(RR_Y_GYRO_RDS_ID, ...)    // pos=10, ID=24 ❌
    VSF_ENTRY(RR_Z_GYRO_RDS_ID, ...)    // pos=11, ID=25 ❌
    VSF_ENTRY(RR_GPS_VEL_ID, ...)       // pos=12, ID=9 ❌ MISMATCH
    /* ... */

// DOPO (ordinata per ID normalizzato):
#define R1UP_FILE_RR_FILE_VARS_STREAM_FORMAT_TABLE(VSF_ENTRY) \
    VSF_ENTRY(RR_RPM_ID, ...)           // pos=0, ID=0 ✓
    VSF_ENTRY(RR_EXAUSTEMP_ID, ...)     // pos=1, ID=1 ✓
    /* ... */
    VSF_ENTRY(RR_GPS_VEL_ID, ...)       // pos=9, ID=9 ✓ ALLINEATO
    /* ... */
    VSF_ENTRY(RR_X_GYRO_RDS_ID, ...)    // pos=23, ID=23 ✓ ALLINEATO
    VSF_ENTRY(RR_Y_GYRO_RDS_ID, ...)    // pos=24, ID=24 ✓
    VSF_ENTRY(RR_Z_GYRO_RDS_ID, ...)    // pos=25, ID=25 ✓
    /* ... */
```

### Perché Funziona

**La VSF table ha due ruoli:**
1. **Generazione switch cases** - per leggere dal file
2. **Definizione ordine array** - per l'output r1up_row[]

```c
// 1. Switch statement (macro expansion):
#define R1UP_FILE_RR_VARS_STREAM_FORMAT_TABLE_SAME_AS_VSF_READ_FROM_FILE(var_id, ...) \
    case var_id: \
        /* Leggi dal file e salva in stream_last_values[var_id] */ \
        break;

// L'ordine dei case NON importa! Il compilatore li riordina automaticamente
switch (variable_id_from_file) {
    case 0:  // RPM
    case 23: // X_GYRO  <- case 23 funziona ovunque nella lista
    case 9:  // GPS_VEL <- case 9 funziona ovunque nella lista
}

// 2. Copy loop - ORDINE CRITICO
for (var_idx = 0; var_idx < 28; var_idx++) {
    // var_idx è l'indice in r1up_row[]
    // vsf[var_idx].id è l'ID da cui leggere in stream_last_values[]
    r1up_row[var_idx] = stream_last_values[vsf[var_idx].id];
}

// Con VSF ordinata per ID:
// var_idx=0: vsf[0].id=0 → r1up_row[0] = stream_last_values[0] = RPM ✓
// var_idx=9: vsf[9].id=9 → r1up_row[9] = stream_last_values[9] = GPS_VEL ✓
// var_idx=23: vsf[23].id=23 → r1up_row[23] = stream_last_values[23] = X_GYRO ✓
```

### Garanzia di Non-Breaking

**Q: Riordinare la VSF table rompe la lettura dei file RER esistenti?**

**A: NO!** Perché:

1. **File reading usa gli ID dal file**, non la posizione nella tabella:
   ```c
   // Lettura da file:
   uint8_t variable_id = read_byte_from_file();  // Legge ID fisico (1-28)
   variable_id--;  // Normalizza a 0-based
   
   switch (variable_id) {  // Match sull'ID, non sulla posizione
       case RR_RPM_ID: ...
       case RR_X_GYRO_RDS_ID: ...  // Funziona anche se nella tabella è alla pos 23
   }
   ```

2. **Lo switch si basa sul valore dell'ID**, non sull'ordine dei case

3. **L'ordine nella tabella VSF influenza solo:**
   - L'ordine dei `case` nel codice generato (irrilevante per il compilatore)
   - La posizione nell'array `vsf[]` usato dal copy loop

## Risultati

### Test Output - Prima della Fix

```
--- File Comparison ---
Batch output:     597,216 bytes
Streaming output: 633,079 bytes  ❌ DIFFERENTE!

Row Analysis:
Same row count (4061), but GPS/altitude/gear data CORRUPTED in streaming
GPS latitude: batch = 43.123°, streaming = 0.00156° (valore giroscopio!)
```

### Test Output - Dopo la Fix

```
--- File Comparison ---
[ INFO ] Files are identical (597216 bytes) ✓

All 14 tests PASS ✓
```

## Lezioni Apprese

1. **Legacy file formats** possono avere schemi ID non-ovvi dovuti a evoluzioni storiche
2. **Array indexing assumptions** devono essere validati quando ID ≠ posizione
3. **VSF table ordering** è critico quando la stessa tabella definisce sia I/O che layout memoria
4. **Switch statement order** è irrilevante, ma l'ordine della struttura dati che lo genera può essere critico
5. **Table reordering** può essere una soluzione elegante se l'ordine non ha semantica nel dominio originale

## Riferimenti

- File: `r1up_file_cfg.h` linee 281-308 (R1UP_FILE_RR_FILE_VARS_STREAM_FORMAT_TABLE)
- File: `r1up_file.c` linee 3650-3658 (copy loop streaming)
- File: `r1up_to_racecapture.c` linee 390-428 (WRITE_CHANNEL_STREAMING macro)
- Discussione: GitHub Copilot chat session, 25 Gennaio 2026
