# Report Allineamento Specifiche VSF - Approccio Lettura File

**Data**: 28 Gennaio 2026  
**Autore**: GitHub Copilot  
**Revisione con**: Alex Giov  

---

## 🎯 OBIETTIVO

Definire chiaramente le specifiche per la **lettura dinamica** dei file R1UP, basandosi solo su informazioni note a priori (lookup tables hardcoded) e tutto il resto estratto runtime dal file.

---

## 📋 LE TRE FONTI DI VERITÀ

### 1️⃣ Tabella VARIABLE_ID (Hardcoded in Codice)

**Scopo**: Mappare ID numerici → Nomi semantici delle variabili

**Formato**:
```c
| Variable Identifier | Valore Numerico (uint16_t) | Descrizione           |
| ------------------- | -------------------------- | --------------------- |
| SIG_ID_RPM          | 0x0011                     | "RPM"                 |
| SIG_ID_EXAUST_TEMP  | 0x0003                     | "Exhaust Temperature" |
| SIG_ID_COOLANT      | 0x0004                     | "Coolant Temperature" |
| SIG_ID_SPEED        | 0x0002                     | "GPS Speed"           |
| SIG_ID_LAT_DEG      | 0x0013                     | "Latitude Degrees"    |
| ...                 | ...                        | ...                   |
```

**Caratteristiche**:
- ✅ Definita **staticamente** nel codice (compile-time)
- ✅ Valori numerici **arbitrari** (non sequenziali, possono avere "buchi")
- ✅ Serve per dare **nomi human-readable** alle variabili
- ✅ Usata per identificare quale variabile è presente nel file
- ✅ **Completa**: contiene TUTTE le variabili possibili nel mondo
- ✅ **Separata in file dedicato**: `r1up_file_variable_type_database.h`

**Implementazione**:
```c
// In r1up_file_variable_type_database.h - LOOKUP TABLE STATICA COMPLETA
#define R1UP_FILE_VARIABLE_ID_TABLE(ENTRY) \
    ENTRY(SIG_ID_RPM,         0x0011, "RPM") \
    ENTRY(SIG_ID_EXAUST_TEMP, 0x0003, "Exhaust Temperature") \
    ENTRY(SIG_ID_COOLANT,     0x0004, "Coolant Temperature") \
    ENTRY(SIG_ID_SPEED,       0x0002, "GPS Speed") \
    ENTRY(SIG_ID_LAT_DEG,     0x0013, "Latitude Degrees") \
    ENTRY(SIG_ID_LAT_MIN,     0x0014, "Latitude Minutes") \
    ENTRY(SIG_ID_LAT_DECI,    0x0015, "Latitude Decimal Minutes") \
    ENTRY(SIG_ID_LONG_DEG,    0x0016, "Longitude Degrees") \
    ENTRY(SIG_ID_LONG_MIN,    0x0017, "Longitude Minutes") \
    ENTRY(SIG_ID_LONG_DECI,   0x0018, "Longitude Decimal Minutes") \
    ENTRY(SIG_ID_X_ACC,       0x0005, "X Acceleration") \
    ENTRY(SIG_ID_Y_ACC,       0x0006, "Y Acceleration") \
    ENTRY(SIG_ID_Z_ACC,       0x0007, "Z Acceleration") \
    ENTRY(SIG_ID_X_GYRO,      0x0009, "X Gyroscope") \
    ENTRY(SIG_ID_Y_GYRO,      0x000A, "Y Gyroscope") \
    ENTRY(SIG_ID_Z_GYRO,      0x000B, "Z Gyroscope") \
    ENTRY(SIG_ID_GPS_SAT,     0x0020, "GPS Satellites Used") \
    ENTRY(SIG_ID_ALTITUDE,    0x0019, "MSL Altitude") \
    ENTRY(SIG_ID_GEAR,        0x0021, "Gear Position") \
    ENTRY(SIG_ID_THROTTLE,    0x0022, "Throttle Position") \
    ENTRY(SIG_ID_BATT_VOLT,   0x0024, "Battery Voltage") \
    ENTRY(SIG_ID_INT_TEMP,    0x0023, "Internal Temperature") \
    ENTRY(SIG_ID_EXT_PS_VOLT, 0x0025, "External Power Supply Voltage") \
    ENTRY(SIG_ID_RPM_HALL,    0x0026, "RPM Hall Sensor") \
    ENTRY(SIG_ID_PWSS_HALL,   0x0027, "Pulse Width Hall Sensor")
    // ... tutte le variabili possibili nel mondo Race1Up

// Genera enum
typedef enum {
    #define VARIABLE_ID_AS_ENUM(name, value, desc) name = value,
    R1UP_FILE_VARIABLE_ID_TABLE(VARIABLE_ID_AS_ENUM)
    #undef VARIABLE_ID_AS_ENUM
} r1up_variable_id_t;
```

---

### 2️⃣ Tabella TYPE_ID (Hardcoded in Codice)

**Scopo**: Mappare TYPE numerici → Descrizione tipo e metadati per conversione

**Formato**:
```c
| Type Identifier | Valore Numerico (uint16_t) | Descrizione Tipo                                                        |
| --------------- | -------------------------- | ----------------------------------------------------------------------- |
| TYPE_UQ16_0     | 0x0001                     | "Unsigned fixed-point 16 bit parte intera, 0 frazionaria (uint16_t)"    |
| TYPE_UQ12_4     | 0x0012                     | "Unsigned fixed-point 12 bit intera, 4 bit frazionaria (16 bit totali)" |
| TYPE_Q31_0      | 0x0050                     | "Signed fixed-point 31 bit intera, 1 segno (int32_t)"                   |
| TYPE_UQ32_0     | 0x0003                     | "Unsigned fixed-point 32 bit parte intera, 0 frazionaria (uint32_t)"    |
| TYPE_Q12_3      | 0x0011                     | "Signed fixed-point 12 bit intera, 3 bit frazionaria, 1 segno"          |
| TYPE_UQ10_6     | 0x0016                     | "Unsigned fixed-point 10 bit intera, 6 bit frazionaria (16 bit totali)" |
| TYPE_Q4_11      | 0x0411                     | "Signed fixed-point 4 bit intera, 11 bit frazionaria, 1 segno"          |
| TYPE_Q3_12      | 0x0312                     | "Signed fixed-point 3 bit intera, 12 bit frazionaria, 1 segno"          |
| TYPE_Q3_28      | 0x0328                     | "Signed fixed-point 3 bit intera, 28 bit frazionaria, 1 segno"          |
| TYPE_UQ8_0      | 0x0800                     | "Unsigned fixed-point 8 bit parte intera, 0 frazionaria (uint8_t)"      |
| TYPE_Q7_0       | 0x0700                     | "Signed fixed-point 7 bit intera, 1 segno (int8_t)"                     |
| TYPE_UQ6_10     | 0x060A                     | "Unsigned fixed-point 6 bit intera, 10 bit frazionaria (16 bit totali)" |
| ...             | ...                        | ...                                                                     |
```

**Caratteristiche**:
- ✅ Definita **staticamente** nel codice (compile-time)
- ✅ Valori numerici **arbitrari** (non sequenziali)
- ✅ Contiene **solo descrizione tipo** (size e conversione derivate da libreria ag_fixedpoint)
- ✅ Serve per **identificare** il tipo, **non** per convertire (conversione gestita da switch/case)
- ✅ **Completa**: contiene TUTTI i tipi fixed-point supportati
- ✅ **Separata in file dedicato**: `r1up_file_variable_type_database.h`

**Implementazione**:
```c
// In r1up_file_variable_type_database.h - LOOKUP TABLE STATICA COMPLETA
#define R1UP_FILE_TYPE_ID_TABLE(ENTRY) \
    ENTRY(TYPE_UQ16_0, 0x0001, "Unsigned fixed-point 16.0 (uint16_t)") \
    ENTRY(TYPE_UQ12_4, 0x0012, "Unsigned fixed-point 12.4 (16 bit totali)") \
    ENTRY(TYPE_Q31_0,  0x0050, "Signed fixed-point 31.0 con segno (int32_t)") \
    ENTRY(TYPE_UQ32_0, 0x0003, "Unsigned fixed-point 32.0 (uint32_t)") \
    ENTRY(TYPE_Q12_3,  0x0011, "Signed fixed-point 12.3 con segno (16 bit totali)") \
    ENTRY(TYPE_UQ10_6, 0x0016, "Unsigned fixed-point 10.6 (16 bit totali)") \
    ENTRY(TYPE_Q4_11,  0x0411, "Signed fixed-point 4.11 con segno (16 bit totali)") \
    ENTRY(TYPE_Q3_12,  0x0312, "Signed fixed-point 3.12 con segno (16 bit totali)") \
    ENTRY(TYPE_Q3_28,  0x0328, "Signed fixed-point 3.28 con segno (32 bit totali)") \
    ENTRY(TYPE_UQ8_0,  0x0800, "Unsigned fixed-point 8.0 (uint8_t)") \
    ENTRY(TYPE_Q7_0,   0x0700, "Signed fixed-point 7.0 con segno (int8_t)") \
    ENTRY(TYPE_UQ6_10, 0x060A, "Unsigned fixed-point 6.10 (16 bit totali)")
    // ... tutti i tipi fixed-point supportati dalla libreria ag_fixedpoint

// Genera enum
typedef enum {
    #define TYPE_ID_AS_ENUM(name, value, desc) name = value,
    R1UP_FILE_TYPE_ID_TABLE(TYPE_ID_AS_ENUM)
    #undef TYPE_ID_AS_ENUM
} r1up_type_id_t;
```

**Nota Importante**: Size in bytes e funzione di conversione sono **derivate runtime** tramite switch/case nel codice di lettura, basandosi sul tipo ag_fixedpoint corrispondente.

---

### 3️⃣ VSF Array (Letto dal File Runtime)

**Scopo**: Descrivere **quali variabili** sono presenti nel file, in **che ordine**, con **che tipo**, e con **quale frequenza**

**Formato nel file** (block header):
```
+-------------------+
| R1UP_FILE_VARS_STRM_FRMT_BID (16 bytes)
+-------------------+
| Block Size (4 bytes) = N * sizeof(r1up_file_vsf_t)
+-------------------+
| VSF Entry 0       |
|   id:   0x0011    | ← SIG_ID_RPM (lookup in Tabella 1)
|   type: 0x0012    | ← TYPE_UQ12_4 (lookup in Tabella 2)
|   sz:   2         | ← 2 bytes (ridondante, già in Tabella 2)
|   fd:   1         | ← Sample rate divisor
+-------------------+
| VSF Entry 1       |
|   id:   0x0002    | ← SIG_ID_SPEED
|   type: 0x0003    | ← TYPE_UQ32_0
|   sz:   4         |
|   fd:   10        | ← Campionato ogni 10 sample
+-------------------+
| VSF Entry 2       |
|   id:   0x0015    | ← SIG_ID_TEMPERATURE
|   type: 0x0011    | ← TYPE_Q12_3
|   sz:   2         |
|   fd:   1         |
+-------------------+
| ... N entries ... |
+-------------------+
```

**Struttura C**:
```c
typedef struct {
    uint16_t id;    // Riferimento a Tabella 1 (VARIABLE_ID)
    uint16_t type;  // Riferimento a Tabella 2 (TYPE_ID)
    uint16_t sz;    // Size in bytes - OTTIMIZZAZIONE: evita lookup ripetuti in hot path
    uint16_t fd;    // Frequency divisor (informazione runtime-only)
} r1up_file_vsf_t;
```

**Nota su campo `sz`**: Tecnicamente ridondante (size derivabile da TYPE_ID), ma **mantenuto per performance**:
- ✅ Evita lookup TYPE_ID→size in loop di lettura (milioni di iterazioni)
- ✅ Validazione: confronto sz dal file vs sz atteso da TYPE_ID
- ✅ Flessibilità futura: supporto tipi custom senza estendere TYPE_ID table

**Caratteristiche**:
- ✅ Letto **dinamicamente** dal file (runtime)
- ✅ Numero di entries **variabile** (N determinato da block size)
- ✅ Ordine entries = **ordine variabili nel body stream**
- ✅ Può contenere **subset** delle variabili possibili (non tutte devono esserci)
- ✅ Può avere variabili in **ordine arbitrario**

---

## 🔄 FLUSSO DI LETTURA FILE

### Step 1: Apertura File e Lettura VSF

```c
r1up_file_handle_t handle = r1up_file_open("data.r1up", R1UP_FILE_MODE_READ_ONLY);

// Internamente:
// 1. Legge block R1UP_FILE_VARS_STRM_FRMT_BID
// 2. Estrae array VSF
// 3. Determina N = block_size / sizeof(r1up_file_vsf_t)
```

**Risultato**:
- `file->vsf` → puntatore all'array VSF (N elementi)
- `file->varible_number` → N (numero variabili nel file)

### Step 2: Costruzione Registry Dinamico

```c
// Per ogni entry in VSF[]:
for (i = 0; i < N; i++) {
    uint16_t var_id = file->vsf[i].id;      // es. 0x0011 (SIG_ID_RPM)
    uint16_t type_id = file->vsf[i].type;   // es. 0x0012 (TYPE_UQ12_4)
    
    // Lookup in Tabella 1: ID → Nome
    // Lookup in Tabella 2: TYPE → Size + Conversione
    
    // Costruisci mapping: ID → Position nell'array
    registry.id_to_position[var_id] = i;  // Position fisica = i
}
```

**Risultato**:
- Registry permette: `ID variabile → Position nell'array VSF/buffers`
- Possiamo avere variabili sparse (ID non sequenziali)
- Possiamo avere ordine arbitrario

### Step 3: Lettura Body Stream

```c
// Il body contiene stream binario di variabili:
// [RPM_sample0][SPEED_sample0][TEMP_sample0][RPM_sample1][TEMP_sample1]...
//     2 bytes      4 bytes        2 bytes       2 bytes      2 bytes
//     (fd=1)       (fd=10)        (fd=1)        (fd=1)    (fd=1, SPEED skip)

// Lettura sequenziale usando VSF[]:
for (sample = 0; sample < total_samples; sample++) {
    for (i = 0; i < N; i++) {  // i = position fisica
        r1up_file_vsf_t* entry = &file->vsf[i];
        
        // Check frequency divider
        if (sample % entry->fd == 0) {
            // Leggi entry->sz bytes dal file
            uint8_t raw_data[entry->sz];
            fsal_read(raw_data, entry->sz, ...);
            
            // Conversione basata su TYPE_ID tramite switch statement
            switch (entry->type) {
                case 0x0001:  // TYPE_UQ16_0
                    value = ag_fixedpoint_uq16_0_to_float(*(uint16_t*)raw_data);
                    break;
                case 0x0012:  // TYPE_UQ12_4
                    value = ag_fixedpoint_uq12_4_to_float(*(uint16_t*)raw_data);
                    break;
                case 0x0050:  // TYPE_Q31_0
                    value = ag_fixedpoint_q31_0_to_float(*(int32_t*)raw_data);
                    break;
                case 0x0003:  // TYPE_UQ32_0
                    value = ag_fixedpoint_uq32_0_to_float(*(uint32_t*)raw_data);
                    break;
                case 0x0011:  // TYPE_Q12_3
                    value = ag_fixedpoint_q12_3_to_float(*(int16_t*)raw_data);
                    break;
                // ... tutti i casi dalla TYPE_ID table
                default:
                    // Tipo sconosciuto - error handling
                    return R1UP_FILE_ERROR_INVALID_FORMAT;
            }
            
            buffer[i][sample] = value;
        } else {
            // Sample-and-hold: usa valore precedente
            buffer[i][sample] = buffer[i][sample-1];
        }
    }
}
```

### Step 4: API Utente

```c
// Utente chiede variabile per ID semantico
uint64_t num_samples;
const float* rpm_data = r1up_file_get_rpm(handle, &num_samples);

// Internamente:
r1up_file_result_t r1up_file_get_variable(handle, r1up_variable_id_t var_id, 
                                          ag_fixedpoint_float_t** out_buffer,
                                          uint64_t* out_samples) {
    // 1. Lookup ID → Position
    int32_t pos = file->registry.id_to_position[var_id];
    if (pos < 0) return R1UP_FILE_ERROR_NOT_FOUND;  // Variabile non nel file
    
    // 2. Ritorna buffer alla posizione fisica
    *out_buffer = file->vsd_read_buffers[pos];
    *out_samples = file->total_sample_number;
    return R1UP_FILE_SUCCESS;
}
```

---

## ✅ VANTAGGI APPROCCIO PROPOSTO

### 1. **Massima Flessibilità Runtime**
- ✅ File può contenere **subset arbitrario** di variabili
- ✅ Variabili in **ordine qualsiasi**
- ✅ Nuove variabili aggiunte → basta estendere le lookup tables
- ✅ Backward compatibility: vecchi file leggibili anche con nuove versioni libreria

### 2. **Separazione Chiara delle Responsabilità**
- **Compile-time** (lookup tables):
  - Quali variabili **esistono nel mondo**
  - Come **convertire** i tipi di dato
  
- **Runtime** (VSF dal file):
  - Quali variabili **sono in questo file specifico**
  - In che **ordine**
  - Con quale **frequenza**

### 3. **Eliminazione Problemi Identificati**
- ❌ **ELIMINA** il problema P0 (buffer overflow su ID non sequenziali)
  - Non c'è più assunzione `ID == Position`
  - Registry gestisce mapping arbitrario

- ❌ **ELIMINA** confusione ID vs Position
  - Chiaro: ID = lookup table entry, Position = indice VSF array

---

## 🆚 CONFRONTO: APPROCCIO ATTUALE vs PROPOSTO

| Aspetto                     | Approccio Attuale (X-Macro)               | Approccio Proposto (Lookup Tables)       |
| --------------------------- | ----------------------------------------- | ---------------------------------------- |
| **Definizione variabili**   | Singola X-macro in `r1up_file_cfg.h`      | Due lookup tables separate               |
| **ID variabili**            | Generati come enum sequenziale (0,1,2...) | Valori arbitrari (0x11, 0x15, 0x02...)   |
| **Ordine variabili**        | Fisso (ordine in X-macro)                 | Dinamico (ordine nel file)               |
| **Subset variabili**        | ❌ Difficile (tutte devono esserci)        | ✅ Facile (legge solo quelle presenti)    |
| **Aggiunta variabili**      | ⚠️ Modifica X-macro, ricompila tutto       | ✅ Estendi lookup tables, backward compat |
| **Size buffer VSF**         | `N * sizeof(vsf)` (N fisso)               | `file_N * sizeof(vsf)` (N variabile)     |
| **Registry**                | Costruito ma poco usato                   | Fondamentale per tutto                   |
| **Rischio buffer overflow** | 🔴 ALTO (se ID non sequenziali)            | 🟢 NULLO (ID disaccoppiato da position)   |

---

## 🔍 ESEMPIO CONCRETO

### File 1: Acquisizione Completa

**VSF nel file**:
```c
N = 5 variabili
VSF[0]: id=0x0011 (RPM),         type=0x0012 (UQ12_4), fd=1
VSF[1]: id=0x0015 (TEMPERATURE), type=0x0011 (Q12_3),  fd=1
VSF[2]: id=0x0002 (SPEED),       type=0x0003 (UQ32_0), fd=10
VSF[3]: id=0x0003 (EXAUST),      type=0x0012 (UQ12_4), fd=1
VSF[4]: id=0x0004 (COOLANT),     type=0x0011 (Q12_3),  fd=5
```

**Registry costruito**:
```c
registry.id_to_position[0x0011] = 0  // RPM → pos 0
registry.id_to_position[0x0015] = 1  // TEMPERATURE → pos 1
registry.id_to_position[0x0002] = 2  // SPEED → pos 2
registry.id_to_position[0x0003] = 3  // EXAUST → pos 3
registry.id_to_position[0x0004] = 4  // COOLANT → pos 4
```

**API call**:
```c
float* rpm = r1up_file_get_variable(handle, 0x0011, &samples);
// → registry[0x0011] = 0 → vsd_read_buffers[0] ✅
```

### File 2: Subset Minimo (Solo RPM e Speed)

**VSF nel file**:
```c
N = 2 variabili
VSF[0]: id=0x0002 (SPEED), type=0x0003 (UQ32_0), fd=1
VSF[1]: id=0x0011 (RPM),   type=0x0012 (UQ12_4), fd=1
```

**Registry costruito**:
```c
registry.id_to_position[0x0002] = 0  // SPEED → pos 0
registry.id_to_position[0x0011] = 1  // RPM → pos 1
registry.id_to_position[0x0015] = -1 // TEMPERATURE non presente
registry.id_to_position[0x0003] = -1 // EXAUST non presente
registry.id_to_position[0x0004] = -1 // COOLANT non presente
```

**API call**:
```c
float* rpm = r1up_file_get_variable(handle, 0x0011, &samples);
// → registry[0x0011] = 1 → vsd_read_buffers[1] ✅

float* temp = r1up_file_get_variable(handle, 0x0015, &samples);
// → registry[0x0015] = -1 → return R1UP_FILE_ERROR_NOT_FOUND ✅
```

---

## ✅ PUNTI CHIAVE DEFINITI

### 1. **Lookup Tables Complete e Separate**
- ✅ **TABELLA 1** (VARIABLE_ID): Contiene TUTTE le variabili possibili nel mondo Race1Up
- ✅ **TABELLA 2** (TYPE_ID): Contiene TUTTI i tipi fixed-point supportati
- ✅ **File separato**: `r1up_file_variable_type_database.h` (o nome simile)
- ✅ **Formato**: ID + Valore Hex + Descrizione testuale

### 2. **Campo `sz` in VSF - Performance vs Ridondanza**
- ✅ **Mantenerlo**: Ottimizzazione legittima per hot path di lettura
- ✅ **Uso**: Evita lookup ripetuti TYPE_ID→size in loop (milioni iterazioni)
- ✅ **Bonus**: Validazione coerenza file (sz letto vs sz atteso da tipo)

### 3. **API - Prima Generica, Poi Specifiche**
- ✅ **Priorità**: `r1up_file_get_variable(handle, var_id, &buffer, &samples)` - API generica
- ✅ **Opzionale**: Wrapper specifici tipo `r1up_file_get_rpm()` come syntactic sugar
- ✅ **Implementazione**: Wrapper chiamano la generica internamente

### 4. **Conversione Tipo - Switch Statement (Best Practice Embedded)**
- ✅ **Scelta**: Switch statement (non function pointer table)
- ✅ **Motivazione**: 
  - Performance: compilatore genera jump table O(1)
  - Zero overhead indirection
  - Inlining possibile
  - Code size ridotto
  - Naturale per C embedded

### 5. **Separazione READ vs WRITE Mode**
- ✅ **R1UP_FILE_VARS_STREAM_FORMAT_TABLE**: Usata **SOLO per WRITE mode**
- ✅ **READ mode**: Completamente ridefinito usando le 2 lookup tables + VSF dal file
- ✅ **Frequency divider**: Gestito nella configurazione WRITE (già funzionante)
- ✅ **Focus attuale**: SOLO implementazione READ mode dinamica

### 6. **Architettura Fondamentale: NO Legame Fisso VAR_ID ↔ TYPE_ID**

**CRUCIALE**: Stesso VAR_ID può avere TYPE_ID diversi in file diversi!

```c
// File A - Acquisizione Standard
RPM (id=0x0011) → TYPE_UQ16_0 (type=0x0001)  // 16 bit unsigned

// File B - Acquisizione Alta Precisione  
RPM (id=0x0011) → TYPE_Q31_0 (type=0x0050)   // 32 bit signed

// ✅ Libreria legge ENTRAMBI correttamente tramite VSF runtime!
```

**Implicazione**: Le lookup tables sono **solo dizionari**. Il legame VAR↔TYPE è determinato **runtime dal VSF nel file**.

---

## 🚀 ROADMAP IMPLEMENTAZIONE

### Fase 1: Setup Lookup Tables ⏱️ ~1 ora

1. **Creare nuovo file**: `r1up_file_variable_type_database.h`
2. **Definire R1UP_FILE_VARIABLE_ID_TABLE**: 
   - Tutte le variabili Race1Up conosciute
   - Formato: `ENTRY(SIG_ID_xxx, 0xNNNN, "Descrizione")`
3. **Definire R1UP_FILE_TYPE_ID_TABLE**:
   - Tutti i tipi fixed-point ag_fixedpoint supportati  
   - Formato: `ENTRY(TYPE_xxx, 0xNNNN, "Descrizione tipo")`
4. **Generare enum** da entrambe le tabelle

### Fase 2: Refactoring Read Mode ⏱️ ~2-3 ore

5. **Riscrivere `r1up_file_init_variables_stream_format()`** per READ_ONLY:
   - Leggere VSF dal file (già fatto)
   - Build registry ID→Position (già fatto)
   - Rimuovere dipendenze da R1UP_FILE_VARS_STREAM_FORMAT_TABLE
   
6. **Implementare switch statement conversione tipi**:
   - Generare da TYPE_ID_TABLE tramite macro
   - Usare in `r1up_file_read_body()` e `r1up_file_stream_body_next_row()`
   
7. **Implementare API generica**:
   ```c
   r1up_file_result_t r1up_file_get_variable(
       r1up_file_handle_t handle,
       uint16_t var_id,
       ag_fixedpoint_float_t** out_buffer,
       uint64_t* out_samples
   );
   ```

8. **Wrapper opzionali** (syntactic sugar):
   ```c
   static inline const float* r1up_file_get_rpm(handle, samples) {
       // Chiama API generica con SIG_ID_RPM
   }
   ```

### Fase 3: Testing & Validazione ⏱️ ~2 ore

9. **Test case 1**: File con subset variabili
10. **Test case 2**: File con ordine arbitrario variabili  
11. **Test case 3**: Stesso VAR_ID con TYPE_ID diversi (cruciale!)
12. **Test case 4**: Variabili sparse (ID non sequenziali)
13. **Backward compatibility**: File esistenti continuano a funzionare

### Fase 4: Cleanup & Documentazione ⏱️ ~1 ora

14. **Marcare R1UP_FILE_VARS_STREAM_FORMAT_TABLE** come "WRITE_ONLY"
15. **Aggiornare documentazione** API pubblica
16. **Code review** separazione READ/WRITE

---

**STATO ATTUALE**: Specifiche definite ✅  
**PROSSIMO STEP**: Creare `r1up_file_variable_type_database.h` con le lookup tables complete

**Totale stimato**: ~6-7 ore lavoro effettivo
