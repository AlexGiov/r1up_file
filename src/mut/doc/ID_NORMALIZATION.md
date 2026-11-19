# ID Normalization per Compatibilità File R1UP e Legacy ReRace

## Problema Identificato

Durante l'implementazione della modalità streaming per la lettura dei file, è emersa una **incompatibilità critica** tra il formato **R1UP** (corrente) e il formato **Legacy ReRace (.rer)** riguardo alla numerazione degli ID delle variabili.

### Differenze di Numerazione ID

| Formato File          | Primo ID       | Secondo ID         | Range Completo | Note                   |
| --------------------- | -------------- | ------------------ | -------------- | ---------------------- |
| **R1UP (.r1up)**      | `RPM_ID = 0`   | `EXAUSTEMP_ID = 1` | `0-27`         | Indicizzazione 0-based |
| **Legacy RER (.rer)** | `id = 1` (RPM) | `id = 2` (EXAUST)  | `1-28`         | Indicizzazione 1-based |

### Impatto sul Codice

La libreria `r1up_file` genera automaticamente switch statement da macro X-macros per leggere le variabili:

```c
switch (vsf_entry->id) {
    case RPM_ID:        // Espanso a: case 0:
        // Leggi RPM
        break;
    case EXAUSTEMP_ID:  // Espanso a: case 1:
        // Leggi temperatura scarico
        break;
    // ... altre variabili
}
```

**Problema**: Con file Legacy RER, `vsf_entry->id` vale `1, 2, 3...` mentre i case statement sono `0, 1, 2...`

**Conseguenza**: Tutti i case cadono nel `default`, i valori rimangono a `0.0`, lo streaming mode non funziona.

### Esempio Test Fallito

```c
// Test output con file RR_0006.rer PRIMA della normalizzazione:
// File Legacy: vsf_entry->id = 1, 2, 3...
// Index 0: ID: 1  <- Dovrebbe essere 0!
// Index 1: ID: 2  <- Dovrebbe essere 1!
// 
// Switch generato:
//   case RPM_ID (0):  <- vsf_entry->id = 1, NON MATCH!
//   case EXAUST_ID (1): <- vsf_entry->id = 2, NON MATCH!
//   default: <- Tutti finiscono qui, valori = 0.0
```

## Soluzioni Considerate

### Opzione 1: Normalizzazione ID (✅ SCELTA)
**Approccio**: Convertire ID legacy `1-based` a `0-based` durante l'inizializzazione.

**Implementazione**:
```c
// In r1up_file_rr_file_init_variables_stream_format()
// Linea ~2920 di r1up_file.c
file->vsf[i].id = rer_file_vsf[i].id - 1;  // Normalizza da 1-based a 0-based
```

**Vantaggi**:
- ✅ Modifica minimale (1 linea)
- ✅ API unificata per l'utente
- ✅ Stesso enum pubblico `RPM_ID, EXAUSTEMP_ID, ...`
- ✅ Test esistenti continuano a funzionare

**Svantaggi**:
- ⚠️ Fragile se le tabelle divergono in futuro

### Opzione 2: Switch Separati + Wrapper (✅ IMPLEMENTATA)
**Approccio**: Funzioni di lettura separate con switch generati da tabelle indipendenti.

**Implementazione**:
```c
// Enum separati (generati da macro)
typedef enum {
    RPM_ID = 0,         // R1UP IDs (pubblici)
    EXAUSTEMP_ID = 1,
    // ... 
} r1up_file_vars_stream_format_ids_t;

typedef enum {
    RR_RPM_ID = 0,      // Legacy RER IDs (interni, normalizzati)
    RR_EXAUSTEMP_ID = 1,
    // ...
} r1up_file_rr_vars_stream_format_ids_t;

// Funzioni separate
static r1up_file_result_t r1up_file_r1up_stream_body_next_row(...);  // Switch R1UP
static r1up_file_result_t r1up_file_rr_stream_body_next_row(...);    // Switch RER

// Wrapper pubblico con dispatch
r1up_file_result_t r1up_file_stream_body_next_row(...) {
    switch (file->file_type) {
        case R1UP_FILE_FILETYPE_RACE1UP:
            return r1up_file_r1up_stream_body_next_row(...);
        case R1UP_FILE_FILETYPE_RERACE:
            return r1up_file_rr_stream_body_next_row(...);
    }
}
```

**Vantaggi**:
- ✅ Completa indipendenza delle tabelle
- ✅ Robustezza futura (modifiche R1UP non impattano RER)
- ✅ Pattern coerente con resto del codice
- ✅ Compilazione condizionale (`#if R1UP_FILE_ENABLE_RERACE_LEGACY_SUPPORT`)

**Svantaggi**:
- Più codice da mantenere

### Opzione 3: Doppia API Pubblica (❌ SCARTATA)
```c
r1up_file_stream_body_next_row_r1up(handle, row);  // Per file R1UP
r1up_file_stream_body_next_row_rer(handle, row);   // Per file RER
```

**Motivo scarto**: Pessima UX, l'utente deve sapere che tipo di file ha aperto.

### Opzione 4: Split Libreria (❌ SCARTATA)
Creare due librerie separate: `r1up_file.c` e `rer_file.c`.

**Motivo scarto**: 
- ~85% del codice è condiviso
- Overhead di manutenzione
- Codice duplicato (init, FSAL, block parsing, ecc.)

## Soluzione Implementata: Ibrida (Opzione 1 + Opzione 2)

**Strategia Finale**: Combinare normalizzazione + switch separati per massima robustezza.

### Componenti Implementati

#### 1. Normalizzazione ID Legacy
**File**: `r1up_file.c` linea ~2920  
**Funzione**: `r1up_file_rr_file_init_variables_stream_format()`

```c
for (uint32_t i = 0; i < file->varible_number; i++) {
    /* Normalize legacy 1-based IDs to 0-based for internal use */
    file->vsf[i].id = rer_file_vsf[i].id - 1;  // 1,2,3... → 0,1,2...
    file->vsf[i].type = rer_file_vsf[i].type;
    file->vsf[i].fd = rer_file_vsf[i].fd;
    // ...
}
```

#### 2. Enum Separati
**File**: `r1up_file.c` linea ~350-362

```c
#if (R1UP_FILE_ENABLE_RERACE_LEGACY_SUPPORT == 1)
typedef enum {
    R1UP_FILE_RR_FILE_VARS_STREAM_FORMAT_TABLE(R1UP_FILE_VARS_STREAM_FORMAT_TABLE_EXP_AS_ENUM)
    R1UP_FILE_RR_VARS_STREAM_FORMAT_ID_NUM
} r1up_file_rr_vars_stream_format_ids_t;
#endif
```

#### 3. Macro per Switch RER
**File**: `r1up_file_cfg.h` linea ~320-338

```c
#define R1UP_FILE_RR_VARS_STREAM_FORMAT_TABLE_SAME_AS_VSF_READ_FROM_FILE(       \
    variable_id, type_id, type, f_divisor, var_name, q_type)                    \
    case variable_id: /* RR_RPM_ID, RR_EXAUSTEMP_ID, etc. */                    \
        if (file->stream_current_sample % f_divisor == 0) {                     \
            /* Lettura valore raw */                                            \
            type raw_value = 0;                                                  \
            fsal_result_t fsal_res = fsal_read(&raw_value, 1, sizeof(type),     \
                                               file->fsal_handle_ptr, ...);      \
            /* Conversione fixed-point → float */                               \
            file->stream_last_values[variable_id] =                             \
                ag_fixedpoint_##q_type##_to_float(raw_value);                   \
        }                                                                        \
        break;
```

#### 4. Funzioni Separate
**File**: `r1up_file.c` linea ~3628-3690

```c
// Per file R1UP (usa RPM_ID, EXAUSTEMP_ID, ...)
static r1up_file_result_t r1up_file_r1up_stream_body_next_row(
    r1up_file_t* file, double* out_values) 
{
    for (uint32_t var_idx = 0; var_idx < file->varible_number; var_idx++) {
        switch (file->vsf[var_idx].id) {
            R1UP_FILE_VARS_STREAM_FORMAT_TABLE(R1UP_FILE_VARS_STREAM_FORMAT_TABLE_SAME_AS_VSF_READ_FROM_FILE)
            default: break;
        }
    }
    // ...
}

// Per file RER (usa RR_RPM_ID, RR_EXAUSTEMP_ID, ...)
static r1up_file_result_t r1up_file_rr_stream_body_next_row(
    r1up_file_t* file, double* out_values) 
{
    for (uint32_t var_idx = 0; var_idx < file->varible_number; var_idx++) {
        switch (file->vsf[var_idx].id) {
            R1UP_FILE_RR_FILE_VARS_STREAM_FORMAT_TABLE(R1UP_FILE_RR_VARS_STREAM_FORMAT_TABLE_SAME_AS_VSF_READ_FROM_FILE)
            default: break;
        }
    }
    // ...
}
```

#### 5. Wrapper Pubblico
**File**: `r1up_file.c` linea ~3698-3735

```c
r1up_file_result_t r1up_file_stream_body_next_row(
    r1up_file_handle_t handle, double* out_values, uint32_t array_size) 
{
    // ... validazione parametri ...
    
    /* Dispatch to appropriate reader based on file type */
    switch (file->file_type) {
        case R1UP_FILE_FILETYPE_RACE1UP:
            return r1up_file_r1up_stream_body_next_row(file, out_values);
            
#if (R1UP_FILE_ENABLE_RERACE_LEGACY_SUPPORT == 1)
        case R1UP_FILE_FILETYPE_RERACE:
            return r1up_file_rr_stream_body_next_row(file, out_values);
#endif
            
        default:
            return R1UP_FILE_ERROR_UNSUPPORTED;
    }
}
```

## Tabelle Variabili

### Tabella R1UP (Corrente)
**File**: `r1up_file_cfg.h` linea ~212-239  
**Macro**: `R1UP_FILE_VARS_STREAM_FORMAT_TABLE`

```c
#define R1UP_FILE_VARS_STREAM_FORMAT_TABLE(VSF_ENTRY)
    VSF_ENTRY(RPM_ID,           R1UP_FILE_UINT16, uint16_t, 1U,  rpm,      uq16_0) \
    VSF_ENTRY(EXAUSTEMP_ID,     R1UP_FILE_UINT16, uint16_t, 1U,  exaust,   uq12_4) \
    VSF_ENTRY(FLUIDTEMP_ID,     R1UP_FILE_UINT16, uint16_t, 10U, coolant,  uq10_6) \
    // ... 28 variabili totali
```

**Espansione enum**:
```c
typedef enum {
    RPM_ID = 0,
    EXAUSTEMP_ID = 1,
    FLUIDTEMP_ID = 2,
    // ...
    PWSS_HALL_ID = 27,
    R1UP_FILE_VARS_STREAM_FORMAT_ID_NUM = 28
} r1up_file_vars_stream_format_ids_t;
```

### Tabella Legacy RER (Congelata)
**File**: `r1up_file_cfg.h` linea ~281-308  
**Macro**: `R1UP_FILE_RR_FILE_VARS_STREAM_FORMAT_TABLE`

```c
#define R1UP_FILE_RR_FILE_VARS_STREAM_FORMAT_TABLE(VSF_ENTRY)
    VSF_ENTRY(RR_RPM_ID,        R1UP_FILE_UINT16, uint16_t, 1U,  rpm,      uq16_0) \
    VSF_ENTRY(RR_EXAUSTEMP_ID,  R1UP_FILE_UINT16, uint16_t, 1U,  exaust,   uq12_4) \
    VSF_ENTRY(RR_FLUIDTEMP_ID,  R1UP_FILE_UINT16, uint16_t, 10U, coolant,  uq10_6) \
    // ... 28 variabili totali (identica a R1UP nella v1.0)
```

**Espansione enum** (interna, non pubblica):
```c
typedef enum {
    RR_RPM_ID = 0,         // Normalizzato da id=1 nel file
    RR_EXAUSTEMP_ID = 1,   // Normalizzato da id=2 nel file
    RR_FLUIDTEMP_ID = 2,   // Normalizzato da id=3 nel file
    // ...
    RR_PWSS_HALL_ID = 27,  // Normalizzato da id=28 nel file
    R1UP_FILE_RR_VARS_STREAM_FORMAT_ID_NUM = 28
} r1up_file_rr_vars_stream_format_ids_t;
```

## API Pubblica per l'Utente

L'utente usa **sempre** gli stessi ID indipendentemente dal formato file:

```c
#include "r1up_file.h"

r1up_file_handle_t handle;
r1up_file_open("data.r1up", R1UP_FILE_MODE_READ_ONLY, &handle);  // O "data.rer"

uint32_t var_count = r1up_file_get_variable_number(handle);
double* row = malloc(var_count * sizeof(double));

r1up_file_stream_body_begin(handle);

while (r1up_file_stream_body_next_row(handle, row, var_count) == R1UP_FILE_SUCCESS) {
    // Accesso SEMPRE con gli stessi ID pubblici
    double rpm = row[RPM_ID];           // Funziona per .r1up E .rer
    double exaust = row[EXAUSTEMP_ID];  // Funziona per .r1up E .rer
    double coolant = row[FLUIDTEMP_ID]; // Funziona per .r1up E .rer
    
    // Processa dati...
}

r1up_file_stream_body_end(handle);
r1up_file_close(handle);
```

**Nota**: Gli enum `RR_*_ID` NON sono esposti nell'header pubblico `.h`, sono interni al `.c`.

## Evoluzione Futura

### Scenario: Modifica Tabella R1UP

Se in futuro la tabella `R1UP_FILE_VARS_STREAM_FORMAT_TABLE` viene modificata:

```c
// Aggiunte nuove variabili
VSF_ENTRY(NEW_SENSOR_ID, R1UP_FILE_UINT32, uint32_t, 1U, new_sensor, uq32_0)
VSF_ENTRY(ANOTHER_ID,    R1UP_FILE_UINT16, uint16_t, 5U, another,    uq16_0)
```

**Impatto**:
- ✅ File R1UP: Funziona automaticamente con nuove variabili
- ✅ File RER: Continua a funzionare con tabella legacy congelata
- ✅ Nessuna modifica codice richiesta (switch separati)
- ✅ Compilazione condizionale esclude codice RER se disabilitato

### Testing

**File di test**: `reracefile/RR_0006.rer` (4060 campioni, 28 variabili)

**Test implementati**:
1. `Test_StreamingBodyRead_Success_Alex` - Verifica lettura streaming file RER
2. Altri 12 test batch mode - Verificano compatibilità complessiva

**Risultato**: ✅ 13/13 test passano

## Vantaggi Soluzione Finale

| Aspetto            | Vantaggio                                                         |
| ------------------ | ----------------------------------------------------------------- |
| **Manutenibilità** | Tabelle completamente indipendenti, modificabili senza conflitti  |
| **Robustezza**     | Switch separati, nessuna interferenza tra formati                 |
| **Performance**    | Zero overhead runtime (dispatch compile-time con enum)            |
| **API**            | Unificata e trasparente per l'utente                              |
| **Testing**        | Test esistenti continuano a passare senza modifiche               |
| **Compilazione**   | Codice RER escluso con `R1UP_FILE_ENABLE_RERACE_LEGACY_SUPPORT=0` |

## Lezioni Apprese

1. **X-Macros potenti ma fragili**: Richiedono allineamento perfetto di ID/indici
2. **Formati legacy**: Mai assumere compatibilità senza verifica esplicita
3. **Test critici**: Il bug è emerso solo con test su dati reali (RR_0006.rer)
4. **Architettura wrapper**: Pattern flessibile per gestire formati multipli
5. **Normalizzazione**: Strategia efficace per unificare rappresentazioni diverse

## Riferimenti

- **File principale**: `src/mut/r1up_file.c`
- **Configurazione**: `src/mut/r1up_file_cfg.h`
- **Header pubblico**: `src/mut/r1up_file.h`
- **Test**: `src/main.c` - `Test_StreamingBodyRead_Success_Alex`
- **File test**: `reracefile/RR_0006.rer`

---
**Autore**: Alex Giov  
**Data**: 25 Gennaio 2026  
**Versione**: 1.0
