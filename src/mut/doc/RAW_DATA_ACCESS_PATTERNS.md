# Raw Data Access Patterns - Design Report

**Author:** Alex Giov  
**Date:** January 29, 2026  
**Project:** Race1Up File Format Library  
**Context:** Generic variable access API design for embedded systems

---

## 1. Executive Summary

Questo documento analizza diversi pattern architetturali per implementare l'accesso a dati raw (non convertiti in float) in un sistema embedded con vincoli di memoria e 13 tipi fixed-point eterogenei. Vengono confrontati 4 approcci principali con riferimenti a librerie professionali e raccomandazioni finali per il progetto R1UP.

**Problema risolto:**
- ✅ Accesso generico a variabili tramite `var_id` runtime (già implementato: `r1up_file_get_var()`)
- 🔄 Accesso a dati raw con informazione di tipo runtime (in discussione)

**Raccomandazione finale:** **Tagged Union (Opzione A)** per il caso d'uso R1UP.

---

## 2. Contesto del Problema

### 2.1 Requisiti di Sistema

**Vincoli embedded:**
- Sistema embedded con RAM limitata
- Zero allocazioni dinamiche dalla libreria
- Memoria controllata dall'utente (user-provided buffers)
- 13 tipi fixed-point con size variabili (1-4 byte)

**Variabili R1UP:**
- 28 variabili semantiche (RPM, temperatura, accelerometri, GPS, ecc.)
- ID sparsi non sequenziali (1,2,3,4,5,6,8,10...37)
- Mapping via registry: `var_id → position` (28 entry)
- Tipo runtime: determinato dal VSF (Variable Stream Format) nel file

**Caso d'uso principale:**
```c
// Conversione racecapture: lettura batch di tutte le variabili raw
for (uint64_t sample = 0; sample < total_samples; sample++) {
    for (uint8_t var = 0; var < 28; var++) {
        // Leggi raw data
        // Scrivi su file racecapture
    }
}
```

### 2.2 Implementazione Esistente

**API Float (già implementata e testata):**
```c
r1up_file_result_t r1up_file_get_var(
    r1up_file_handle_t handle,
    r1up_variable_id_t var_id,
    double* out_value
);
```

**Caratteristiche:**
- ✅ Switch su runtime `type_id` dal VSF
- ✅ Conversione automatica fixed-point → float
- ✅ Zero allocazioni
- ✅ Test passati (make test exit 0)

---

## 3. Pattern Architetturali Analizzati

### 3.1 Opzione A: Tagged Union (8 byte)

**Struttura:**
```c
typedef struct {
    r1up_type_id_t type_id;  // 4 byte (enum/uint32_t)
    union {
        ag_fixedpoint_uq8_0_t   as_uq8_0;   // 1 byte
        ag_fixedpoint_q7_0_t    as_q7_0;    // 1 byte
        ag_fixedpoint_uq16_0_t  as_uq16_0;  // 2 byte
        ag_fixedpoint_uq12_4_t  as_uq12_4;  // 2 byte
        ag_fixedpoint_uq10_6_t  as_uq10_6;  // 2 byte
        ag_fixedpoint_uq6_10_t  as_uq6_10;  // 2 byte
        ag_fixedpoint_q15_0_t   as_q15_0;   // 2 byte
        ag_fixedpoint_q12_3_t   as_q12_3;   // 2 byte
        ag_fixedpoint_q4_11_t   as_q4_11;   // 2 byte
        ag_fixedpoint_q3_12_t   as_q3_12;   // 2 byte
        ag_fixedpoint_uq32_0_t  as_uq32_0;  // 4 byte
        ag_fixedpoint_q31_0_t   as_q31_0;   // 4 byte
        ag_fixedpoint_q3_28_t   as_q3_28;   // 4 byte
    } value;  // 4 byte (sizeof(largest type))
} r1up_var_raw_union_t;  // Total: 8 byte
```

**API Function:**
```c
r1up_file_result_t r1up_file_get_var_raw(
    r1up_file_handle_t handle,
    r1up_variable_id_t var_id,
    r1up_var_raw_union_t* out_raw
);
```

**Utilizzo:**
```c
r1up_var_raw_union_t raw;
r1up_file_get_var_raw(handle, R1UP_FILE_VAR_ID_RPM, &raw);

switch(raw.type_id) {
    case R1UP_FILE_TYPE_ID_UQ16_0:
        uint16_t rpm_raw = raw.value.as_uq16_0;
        float rpm_float = ag_fixedpoint_uq16_0_to_float(rpm_raw);
        break;
    case R1UP_FILE_TYPE_ID_UQ32_0:
        uint32_t rpm_raw = raw.value.as_uq32_0;
        float rpm_float = ag_fixedpoint_uq32_0_to_float(rpm_raw);
        break;
    default:
        return R1UP_FILE_ERROR_INVALID_TYPE;
}
```

**Pro:**
- ✅ **Memoria minimale**: 8 byte totali
- ✅ **Type-safety completa**: accesso type-safe a compile-time
- ✅ **Nessun cast manuale**: il compilatore gestisce tutto
- ✅ **Robusto con type_id non sequenziali**: usa switch, non indici

**Contro:**
- ❌ Richiede switch per accesso (overhead cognitivo)
- ❌ Non consente accesso indexed diretto `raw.value[type_id]`

**Librerie che lo usano:**
- **SQLite3**: `Mem` structure per valori variant (int/float/text/blob)
- **Lua**: `TValue` con union + type tag per tutti i tipi Lua
- **JSON-C**: `json_object` con union + type discriminator
- **V8 JavaScript Engine**: `Tagged` values con type bits

---

### 3.2 Opzione B: Array di Puntatori Typed (104 byte)

**Struttura:**
```c
typedef struct {
    union {
        const ag_fixedpoint_uq8_0_t*   as_uq8_0;
        const ag_fixedpoint_q7_0_t*    as_q7_0;
        const ag_fixedpoint_uq16_0_t*  as_uq16_0;
        const ag_fixedpoint_uq12_4_t*  as_uq12_4;
        const ag_fixedpoint_uq10_6_t*  as_uq10_6;
        const ag_fixedpoint_uq6_10_t*  as_uq6_10;
        const ag_fixedpoint_q15_0_t*   as_q15_0;
        const ag_fixedpoint_q12_3_t*   as_q12_3;
        const ag_fixedpoint_q4_11_t*   as_q4_11;
        const ag_fixedpoint_q3_12_t*   as_q3_12;
        const ag_fixedpoint_uq32_0_t*  as_uq32_0;
        const ag_fixedpoint_q31_0_t*   as_q31_0;
        const ag_fixedpoint_q3_28_t*   as_q3_28;
    } ptrs[13];  // 13 * 8 byte = 104 byte (su sistemi 64-bit)
    r1up_type_id_t type_id;
} r1up_var_raw_ptrs_t;
```

**Utilizzo (teorico):**
```c
r1up_var_raw_ptrs_t raw;
r1up_file_get_var_raw_ptrs(handle, var_id, &raw);

// PROBLEMA: union non consente accesso indexed
// raw.ptrs[raw.type_id].as_uq16_0  // ❌ NON COMPILA
```

**Pro:**
- ✅ Tutti i puntatori typed disponibili

**Contro:**
- ❌ **104 byte**: 13 puntatori (8 byte ciascuno su 64-bit)
- ❌ **Spreco memoria**: 12 puntatori sempre NULL
- ❌ **Union non indicizzabile**: impossibile fare `ptrs[type_id]`
- ❌ Richiede comunque switch per accesso corretto

**Librerie che lo usano:**
- **Nessuna trovata**: approccio teorico senza vantaggi pratici

---

### 3.3 Opzione C: Void* + Metadata (24 byte)

**Struttura:**
```c
typedef struct {
    const void* data;            // 8 byte (puntatore ai dati raw)
    r1up_type_id_t type_id;      // 4 byte (tipo runtime)
    uint8_t element_size;        // 1 byte (1/2/4 byte)
    uint8_t _padding[3];         // 3 byte (allineamento)
    uint64_t count;              // 8 byte (numero elementi array)
} r1up_var_raw_info_t;  // Total: 24 byte
```

**API Function:**
```c
r1up_file_result_t r1up_file_get_var_raw_buffer(
    r1up_file_handle_t handle,
    r1up_variable_id_t var_id,
    r1up_var_raw_info_t* out_info
);
```

**Utilizzo con cast manuale:**
```c
r1up_var_raw_info_t info;
r1up_file_get_var_raw_buffer(handle, R1UP_FILE_VAR_ID_RPM, &info);

switch(info.type_id) {
    case R1UP_FILE_TYPE_ID_UQ16_0: {
        const ag_fixedpoint_uq16_0_t* rpm_array = 
            (const ag_fixedpoint_uq16_0_t*)info.data;  // Manual cast
        float rpm = ag_fixedpoint_uq16_0_to_float(rpm_array[0]);
        break;
    }
}
```

**Utilizzo con C11 _Generic (type-safe):**
```c
// Macro per cast automatico type-safe
#define R1UP_CAST_RAW_BUFFER(info, dest_ptr) \
    _Generic((dest_ptr), \
        const ag_fixedpoint_uq16_0_t**: (info).type_id == R1UP_FILE_TYPE_ID_UQ16_0 ? \
            (*(dest_ptr) = (const ag_fixedpoint_uq16_0_t*)(info).data, 0) : -1, \
        const ag_fixedpoint_q3_28_t**: (info).type_id == R1UP_FILE_TYPE_ID_Q3_28 ? \
            (*(dest_ptr) = (const ag_fixedpoint_q3_28_t*)(info).data, 0) : -1, \
        /* ... altri 11 tipi ... */ \
        default: -1 \
    )

// Utilizzo
const ag_fixedpoint_uq16_0_t* rpm_ptr;
if (R1UP_CAST_RAW_BUFFER(info, &rpm_ptr) == 0) {
    float rpm = ag_fixedpoint_uq16_0_to_float(rpm_ptr[0]);
}
```

**Pro:**
- ✅ Include metadata utili (size, count)
- ✅ Flessibile per array/stream
- ✅ Con _Generic: type-safety a compile-time

**Contro:**
- ❌ **24 byte**: 3× più grande della union
- ❌ Cast manuale error-prone (senza _Generic)
- ❌ _Generic richiede C11 e macro complesse
- ❌ Metadata ridondanti per single-value access

**Librerie che lo usano:**
- **NumPy C API**: `PyArrayObject` con `void* data + dtype + shape`
- **Apache Arrow**: `ArrowArray` con buffer + type metadata
- **HDF5**: `H5T_t` type descriptor + `void* buf`
- **Protocol Buffers C**: `ProtobufCMessage` con descriptor + data

---

### 3.4 Opzione D: C11 _Generic Extension a Tagged Union

**Concetto:** Combinare Union (Opzione A) con _Generic per eliminare switch espliciti.

**Macro per conversione automatica:**
```c
// Macro che seleziona automaticamente la funzione di conversione
#define R1UP_RAW_TO_FLOAT(raw_union) \
    _Generic((raw_union).value.as_uq16_0, \
        ag_fixedpoint_uq16_0_t: ag_fixedpoint_uq16_0_to_float((raw_union).value.as_uq16_0), \
        ag_fixedpoint_uq8_0_t:  ag_fixedpoint_uq8_0_to_float((raw_union).value.as_uq8_0), \
        ag_fixedpoint_q3_28_t:  ag_fixedpoint_q3_28_to_float((raw_union).value.as_q3_28), \
        /* ... altri 10 tipi ... */ \
        default: 0.0f \
    )
```

**Problema critico:**
```c
// ❌ COMPILE ERROR: _Generic non può selezionare dinamicamente il field
// Il tipo da controllare deve essere noto a compile-time
r1up_var_raw_union_t raw;
float val = R1UP_RAW_TO_FLOAT(raw);  // Quale field usare? Tutti hanno type diverso!
```

**Pro:**
- Nessuno pratico per questo caso d'uso

**Contro:**
- ❌ **Non applicabile**: _Generic richiede tipo compile-time
- ❌ Union ha runtime type, incompatibile con _Generic
- ❌ Switch comunque necessario per selezionare field corretto

---

## 4. Benchmark Memoria e Performance

### 4.1 Footprint per Singola Chiamata

| Approccio          | Size Stack | Note           |
| ------------------ | ---------- | -------------- |
| **Tagged Union**   | 8 byte     | Ottimale       |
| Void* + Metadata   | 24 byte    | 3× più grande  |
| Array di Puntatori | 104 byte   | 13× più grande |

### 4.2 Performance Access

| Approccio            | Operazioni                       | Cicli CPU (stimati)      |
| -------------------- | -------------------------------- | ------------------------ |
| Tagged Union         | `switch(type_id)` + field access | 5-10 (branch prediction) |
| Void* + Cast manuale | Cast + dereference               | 2-3                      |
| Void* + _Generic     | _Generic selection + cast        | 5-8 (compile-time)       |

### 4.3 Caso d'Uso Racecapture (28 var × 10,000 samples)

**Tagged Union:**
```
Memory: 28 × 8 byte = 224 byte stack per batch
Total calls: 280,000
```

**Void* + Metadata:**
```
Memory: 28 × 24 byte = 672 byte stack per batch
Total calls: 280,000
Overhead: +448 byte (+200%)
```

---

## 5. Riferimenti a Librerie Professionali

### 5.1 Tagged Union Pattern

**SQLite3 - Mem Structure:**
```c
struct Mem {
    union MemValue {
        double r;       /* Real value */
        i64 i;          /* Integer value */
        char *z;        /* String or BLOB value */
    } u;
    u16 flags;          /* Type tag */
};
```
**Uso:** Database values con tipi eterogenei (NULL/INT/REAL/TEXT/BLOB).

**Lua - TValue:**
```c
typedef struct lua_TValue {
    union Value {
        lua_Number n;
        int b;
        void *p;
        /* ... */
    } value_;
    int tt_;  /* Type tag */
} TValue;
```
**Uso:** Linguaggio dinamico con 8 tipi base + subtypes.

**JSON-C - json_object:**
```c
struct json_object {
    enum json_type o_type;
    union data {
        json_bool c_boolean;
        double c_double;
        int64_t c_int64;
        struct lh_table *c_object;
        struct array_list *c_array;
        struct printbuf *c_string;
    } o;
};
```
**Uso:** Parser JSON con 7 tipi (object/array/string/int/double/bool/null).

### 5.2 Void* + Metadata Pattern

**NumPy C API - PyArrayObject:**
```c
typedef struct PyArrayObject {
    char *data;              /* void* ai dati */
    PyArray_Descr *descr;    /* Type descriptor */
    int nd;                  /* Number of dimensions */
    npy_intp *dimensions;    /* Shape */
    npy_intp *strides;       /* Stride info */
} PyArrayObject;
```
**Uso:** Array multidimensionali con 20+ tipi numerici.

**Apache Arrow - ArrowArray:**
```c
struct ArrowArray {
    const void** buffers;    /* Data buffers */
    int64_t length;
    int64_t null_count;
    /* + ArrowSchema per type info */
};
```
**Uso:** Columnar data interchange con schema complessi.

**HDF5 - H5T_t:**
```c
typedef struct H5T_t {
    H5T_class_t type;        /* Type class */
    size_t size;             /* Size in bytes */
    /* + metadata complessi */
};
```
**Uso:** File scientifici con tipi compositi/custom.

### 5.3 Quando usare quale pattern?

| Pattern              | Quando Usarlo                                                     | Esempio                     |
| -------------------- | ----------------------------------------------------------------- | --------------------------- |
| **Tagged Union**     | • Tipi limitati (< 20)<br>• Single values<br>• RAM critica        | SQLite, Lua, JSON, **R1UP** |
| **Void* + Metadata** | • Tipi infiniti/custom<br>• Array/multi-dim<br>• Interoperabilità | NumPy, Arrow, HDF5          |

---

## 6. Analisi Decisionale per R1UP

### 6.1 Confronto Requirements

| Requisito                | Tagged Union   | Void* + Metadata    |
| ------------------------ | -------------- | ------------------- |
| Embedded RAM constraints | ✅ 8 byte       | ⚠️ 24 byte           |
| 13 tipi fixed-point      | ✅ Ottimale     | ⚠️ Overkill          |
| Single value access      | ✅ Diretto      | ⚠️ Cast necessario   |
| Array support            | ⚠️ Via loop     | ✅ Nativo            |
| Type-safety              | ✅ Compile-time | ⚠️ Solo con _Generic |
| Zero allocations         | ✅ Stack only   | ✅ Stack only        |
| C99 compatibility        | ✅ Standard     | ✅ (senza _Generic)  |

### 6.2 Caso d'Uso Principale: Racecapture Conversion

**Profilo operazione:**
```c
// Conversione batch: 28 variabili × N samples → file racecapture
for (sample in samples) {
    for (var_id in all_vars) {
        raw = get_var_raw(var_id);
        float_val = convert_to_float(raw);
        write_to_racecapture(float_val);
    }
}
```

**Frequenza accesso raw:**
- **Occasionale**: solo durante conversione racecapture
- **Non hot path**: non serve performance estrema
- **Batch processing**: 28 var per sample

**Conclusione:** Tagged union sufficiente, metadata non necessari.

### 6.3 Matrice Decisionale

| Criterio           | Peso | Tagged Union | Void* + Metadata    |
| ------------------ | ---- | ------------ | ------------------- |
| Memoria (embedded) | 30%  | 10/10        | 3/10                |
| Type-safety        | 25%  | 10/10        | 7/10 (con _Generic) |
| Semplicità API     | 20%  | 9/10         | 6/10                |
| Manutenibilità     | 15%  | 9/10         | 7/10                |
| Estensibilità      | 10%  | 7/10         | 10/10               |
| **Score Totale**   |      | **9.2/10**   | **6.2/10**          |

---

## 7. Raccomandazione Finale

### 7.1 Soluzione Raccomandata: **Tagged Union (Opzione A)**

**Rationale:**
1. **Memoria ottimale**: 8 byte vs 24 byte (67% risparmio)
2. **Type-safety nativa**: no cast manuali, no _Generic complessi
3. **Perfetto fit**: 13 tipi fissi, non espandibili → union ideale
4. **Best practice**: pattern usato da SQLite, Lua, JSON-C per casi analoghi
5. **Semplicità**: API pulita, facile da usare e manutenere

**API Proposta:**
```c
typedef struct {
    r1up_type_id_t type_id;
    union {
        ag_fixedpoint_uq8_0_t   as_uq8_0;
        ag_fixedpoint_q7_0_t    as_q7_0;
        ag_fixedpoint_uq16_0_t  as_uq16_0;
        ag_fixedpoint_uq12_4_t  as_uq12_4;
        ag_fixedpoint_uq10_6_t  as_uq10_6;
        ag_fixedpoint_uq6_10_t  as_uq6_10;
        ag_fixedpoint_q15_0_t   as_q15_0;
        ag_fixedpoint_q12_3_t   as_q12_3;
        ag_fixedpoint_q4_11_t   as_q4_11;
        ag_fixedpoint_q3_12_t   as_q3_12;
        ag_fixedpoint_uq32_0_t  as_uq32_0;
        ag_fixedpoint_q31_0_t   as_q31_0;
        ag_fixedpoint_q3_28_t   as_q3_28;
    } value;
} r1up_var_raw_t;

r1up_file_result_t r1up_file_get_var_raw(
    r1up_file_handle_t handle,
    r1up_variable_id_t var_id,
    r1up_var_raw_t* out_raw
);
```

**Esempio d'uso:**
```c
// Lettura raw + conversione manuale
r1up_var_raw_t raw;
if (r1up_file_get_var_raw(handle, R1UP_FILE_VAR_ID_RPM, &raw) == R1UP_FILE_SUCCESS) {
    switch(raw.type_id) {
        case R1UP_FILE_TYPE_ID_UQ16_0:
            printf("RPM raw: %u\n", raw.value.as_uq16_0);
            float rpm = ag_fixedpoint_uq16_0_to_float(raw.value.as_uq16_0);
            break;
        case R1UP_FILE_TYPE_ID_UQ32_0:
            printf("RPM raw: %u\n", raw.value.as_uq32_0);
            float rpm = ag_fixedpoint_uq32_0_to_float(raw.value.as_uq32_0);
            break;
        default:
            return R1UP_FILE_ERROR_UNEXPECTED_TYPE;
    }
}
```

### 7.2 Implementazione Suggerita

**File: r1up_file.h**
```c
/**
 * @brief Raw variable data with runtime type information
 * @details Tagged union containing raw fixed-point value and its type ID.
 *          Size: 8 bytes (4-byte type tag + 4-byte largest value).
 */
typedef struct {
    r1up_type_id_t type_id;  ///< Runtime type identifier from VSF
    union {
        ag_fixedpoint_uq8_0_t   as_uq8_0;
        ag_fixedpoint_q7_0_t    as_q7_0;
        ag_fixedpoint_uq16_0_t  as_uq16_0;
        ag_fixedpoint_uq12_4_t  as_uq12_4;
        ag_fixedpoint_uq10_6_t  as_uq10_6;
        ag_fixedpoint_uq6_10_t  as_uq6_10;
        ag_fixedpoint_q15_0_t   as_q15_0;
        ag_fixedpoint_q12_3_t   as_q12_3;
        ag_fixedpoint_q4_11_t   as_q4_11;
        ag_fixedpoint_q3_12_t   as_q3_12;
        ag_fixedpoint_uq32_0_t  as_uq32_0;
        ag_fixedpoint_q31_0_t   as_q31_0;
        ag_fixedpoint_q3_28_t   as_q3_28;
    } value;
} r1up_var_raw_t;

/**
 * @brief Get raw variable data without float conversion
 * @param[in]  handle   File handle
 * @param[in]  var_id   Variable ID
 * @param[out] out_raw  Output raw value with type tag
 * @return R1UP_FILE_SUCCESS or error code
 */
r1up_file_result_t r1up_file_get_var_raw(
    r1up_file_handle_t handle,
    r1up_variable_id_t var_id,
    r1up_var_raw_t* out_raw
);
```

**File: r1up_file.c (implementazione simile a get_var float):**
```c
r1up_file_result_t r1up_file_get_var_raw(
    r1up_file_handle_t handle,
    r1up_variable_id_t var_id,
    r1up_var_raw_t* out_raw)
{
    // Validation + registry lookup (come r1up_file_get_var)
    r1up_file_t* file = (r1up_file_t*)handle;
    if (!file || !out_raw) return R1UP_FILE_ERROR_INVALID_PARAM;
    
    uint8_t pos = file->registry.id_to_position[var_id];
    if (pos >= file->registry.var_count) return R1UP_FILE_ERROR_INVALID_VAR_ID;
    
    // Get type from VSF
    r1up_type_id_t type_id = file->vsf[pos].type;
    out_raw->type_id = type_id;
    
    // Copy raw data based on runtime type
    const void* src = &file->vsd_read_buffers[pos];
    
    switch(type_id) {
        case R1UP_FILE_TYPE_ID_UQ8_0:
            out_raw->value.as_uq8_0 = *(const ag_fixedpoint_uq8_0_t*)src;
            break;
        case R1UP_FILE_TYPE_ID_Q7_0:
            out_raw->value.as_q7_0 = *(const ag_fixedpoint_q7_0_t*)src;
            break;
        case R1UP_FILE_TYPE_ID_UQ16_0:
            out_raw->value.as_uq16_0 = *(const ag_fixedpoint_uq16_0_t*)src;
            break;
        // ... altri 10 case ...
        default:
            return R1UP_FILE_ERROR_UNKNOWN_TYPE;
    }
    
    return R1UP_FILE_SUCCESS;
}
```

### 7.3 Alternative per Casi Speciali

**Se in futuro servisse metadata aggiuntivi:**
```c
typedef struct {
    r1up_var_raw_t raw;          // 8 byte (tagged union)
    uint64_t sample_count;       // 8 byte
    uint8_t frequency_divisor;   // 1 byte
    uint8_t _padding[7];         // 7 byte
} r1up_var_raw_extended_t;  // 24 byte
```

**Se servisse array access diretto:**
```c
// Alternativa: ritorna puntatore diretto + type
typedef struct {
    const void* data;
    r1up_type_id_t type_id;
    uint8_t element_size;
} r1up_var_raw_ptr_t;  // 16 byte (accettabile per array)
```

---

## 8. Piano di Implementazione

### 8.1 Fase 1: API Definition
- [ ] Definire `r1up_var_raw_t` in `r1up_file.h`
- [ ] Dichiarare `r1up_file_get_var_raw()` 
- [ ] Documentare con Doxygen

### 8.2 Fase 2: Implementation
- [ ] Implementare funzione in `r1up_file.c`
- [ ] Switch su 13 type_id
- [ ] Gestione errori

### 8.3 Fase 3: Testing
- [ ] Unit test per tutti i 13 tipi
- [ ] Test con var_id invalidi
- [ ] Test consistenza con `r1up_file_get_var()` float

### 8.4 Fase 4: Integration
- [ ] Usare in racecapture converter
- [ ] Benchmark performance vs API float
- [ ] Documentazione esempi d'uso

---

## 9. Conclusioni

**Scelta raccomandata:** **Tagged Union (8 byte)**

**Motivi:**
1. ✅ Ottimale per embedded (memoria minima)
2. ✅ Type-safe senza macro complesse
3. ✅ Pattern industriale consolidato (SQLite, Lua)
4. ✅ Perfetto fit per 13 tipi fissi
5. ✅ API pulita e manutenibile

**Da evitare:**
- ❌ Array di puntatori: spreco memoria (104 byte)
- ❌ Void* + metadata: overkill per single values (24 byte)
- ❌ _Generic con union: tecnicamente impossibile

**Prossimi step:**
1. Implementare `r1up_var_raw_t` + `r1up_file_get_var_raw()`
2. Testare con conversione racecapture
3. Valutare se serve anche API array (void* + metadata) per stream processing

---

## 10. Riferimenti

**Documentazione:**
- SQLite Source Code: `vdbe.h` (Mem structure)
- Lua Source: `lobject.h` (TValue definition)
- NumPy C API: `arrayobject.h`
- Apache Arrow Format Spec v15.0

**Best Practices:**
- "Expert C Programming" - Peter van der Linden (Chapter 10: Unions)
- "Embedded C Coding Standard" - BARR Group (Rule 7.2: Tagged Unions)
- "The C Programming Language" 2nd Ed - K&R (Section 6.8: Unions)

**R1UP Project Docs:**
- `R1UP_VARIABLE_STREAM_FORMAT.md`
- `DATA_TYPE.md`
- `VSF_ALEX_GIOV.md`

---

**Document Version:** 1.0  
**Last Updated:** January 29, 2026  
**Status:** Final Recommendation
