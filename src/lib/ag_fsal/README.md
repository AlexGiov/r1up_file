# File System Abstraction Layer (FSAL) 🗂️

[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](https://github.com/race1up/fsal)
[![Backend](https://img.shields.io/badge/backend-configurable-green.svg)](#backend-selection)
[![Tests](https://img.shields.io/badge/tests-17%2F17%20passing-brightgreen.svg)](#testing)
[![Features](https://img.shields.io/badge/buffering-adaptive-orange.svg)](#read-buffering-e-ottimizzazione-performance)

Una libreria di astrazione per file system altamente performante e portabile, progettata per funzionare seamlessly tra ambienti desktop ed embedded.

## 🚀 Caratteristiche Principali

- **🔧 Multi-Backend**: Supporto per Standard C Library, FatFS e implementazioni custom
- **⚡ Zero Overhead**: Selezione backend a compile-time senza costi runtime
- **🚀 Read Buffering**: Sistema di buffering avanzato con modalità adattiva e read-ahead intelligente
- **🛡️ Error Handling**: Codici di errore dettagliati e descrizioni user-friendly
- **📊 Debug & Statistics**: Supporto opzionale per statistiche e diagnostica
- **🔐 Thread Safety**: Sincronizzazione configurabile per applicazioni multi-thread
- **💾 Embedded Optimized**: Memory footprint minimo e utilizzo stack ottimizzato

## 🏗️ Architettura

### Backend Supportati

| Backend | ID | Descrizione | Caso d'uso |
|---------|----|-----------|------------|
| **Standard C Library** | 0 | Implementazione POSIX completa | Desktop, server |
| **FatFS** | 1 | Ottimizzato per sistemi embedded | SD cards, eMMC, flash memory |
| **Custom** | 2 | Implementazione definita dall'utente | Casi specializzati |

### Struttura API

```c
// Core Functions
fsal_result_t fsal_init(void);
fsal_result_t fsal_deinit(void);

// File Operations  
fsal_result_t fsal_open(const char* path, fsal_mode_t mode, fsal_handle_t* handle);
fsal_result_t fsal_close(fsal_handle_t* handle);
fsal_result_t fsal_read(void* buffer, size_t size, size_t count, fsal_handle_t* handle, size_t* bytes_read);
fsal_result_t fsal_write(const void* buffer, size_t size, size_t count, fsal_handle_t* handle, size_t* bytes_written);

// Positioning
fsal_result_t fsal_seek(fsal_handle_t* handle, long offset, int origin);
fsal_result_t fsal_tell(fsal_handle_t* handle, long* position);
fsal_result_t fsal_flush(fsal_handle_t* handle);

// Performance Optimization
fsal_result_t fsal_enable_buffering(fsal_handle_t* handle, fsal_buffer_mode_t mode);

// Utility
fsal_result_t fsal_stat(const char* path, fsal_file_info_t* info);
fsal_result_t fsal_exists(const char* path, bool* exists);
const char* fsal_get_error_string(fsal_result_t result);
```

## 📚 Guida all'Uso

### 1. Configurazione

Modifica `ag_fsal_cfg.h` per configurare la libreria:

```c
// Backend selection (0=StdC, 1=FatFS, 2=Custom)
#define FSAL_CFG_BACKEND 0

// Feature configuration
#define FSAL_CFG_DEBUG_ENABLED 1
#define FSAL_CFG_PARAM_VALIDATION_ENABLED 1
#define FSAL_CFG_STATISTICS_ENABLED 1

// Memory limits
#define FSAL_CFG_MAX_OPEN_FILES 16U
#define FSAL_CFG_MAX_PATH_LENGTH 260U
#define FSAL_CFG_BUFFER_SIZE 1024U

// Read buffering configuration (NEW!)
#define FSAL_CFG_READ_BUFFERING_ENABLED 1
#define FSAL_CFG_READ_BUFFER_SIZE 4096U
#define FSAL_CFG_READAHEAD_THRESHOLD 3U
#define FSAL_CFG_READAHEAD_MULTIPLIER 4U
#define FSAL_CFG_SEQUENTIAL_TOLERANCE 0U
```

### 2. Inizializzazione

```c
#include "ag_fsal.h"

int main() {
    // Inizializza la libreria
    fsal_result_t result = fsal_init();
    if (result != FSAL_SUCCESS) {
        printf("Errore inizializzazione: %s\n", fsal_get_error_string(result));
        return -1;
    }
    
    // ... usa la libreria ...
    
    // Cleanup finale
    fsal_deinit();
    return 0;
}
```

### 3. Operazioni su File

#### Scrittura File

```c
fsal_handle_t handle;
fsal_result_t result;

// Apri file per scrittura
result = fsal_open("data.txt", FSAL_MODE_WRITE, &handle);
if (result == FSAL_SUCCESS) {
    const char* data = "Hello, FSAL!";
    size_t bytes_written;
    
    // Scrivi dati
    result = fsal_write(data, 1, strlen(data), &handle, &bytes_written);
    if (result == FSAL_SUCCESS) {
        printf("Scritti %zu bytes\n", bytes_written);
    }
    
    // Chiudi file
    fsal_close(&handle);
}
```

#### Lettura File

```c
fsal_handle_t handle;
char buffer[256];
size_t bytes_read;

// Apri file per lettura
if (fsal_open("data.txt", FSAL_MODE_READ, &handle) == FSAL_SUCCESS) {
    // Leggi dati
    if (fsal_read(buffer, 1, sizeof(buffer)-1, &handle, &bytes_read) == FSAL_SUCCESS) {
        buffer[bytes_read] = '\0';
        printf("Letti %zu bytes: %s\n", bytes_read, buffer);
    }
    
    fsal_close(&handle);
}
```

#### Informazioni File

```c
fsal_file_info_t info;
bool exists;

// Controlla esistenza
if (fsal_exists("data.txt", &exists) == FSAL_SUCCESS) {
    if (exists) {
        // Ottieni statistiche dettagliate
        if (fsal_stat("data.txt", &info) == FSAL_SUCCESS) {
            printf("File size: %zu bytes\n", info.size);
            printf("Is directory: %s\n", info.is_directory ? "Yes" : "No");
            printf("Is readonly: %s\n", info.is_readonly ? "Yes" : "No");
        }
    }
}
```

### 4. Read Buffering e Ottimizzazione Performance

FSAL supporta un sistema di buffering avanzato per ottimizzare la lettura di file, particolarmente utile quando si eseguono molte piccole letture consecutive.

#### Modalità di Buffering

```c
typedef enum {
    FSAL_BUFFER_NONE,     // Buffering disabilitato (I/O diretto)
    FSAL_BUFFER_FULL,     // Buffering sempre attivo con read-ahead
    FSAL_BUFFER_ADAPTIVE  // Buffering adattivo basato su pattern di accesso
} fsal_buffer_mode_t;
```

#### Buffering per Letture Sequenziali

Ideale per la lettura di file di grandi dimensioni in piccoli blocchi:

```c
fsal_handle_t handle;
char buffer[64];
size_t bytes_read;

// Apri file
fsal_open("large_data.bin", FSAL_MODE_READ, &handle);

// Abilita buffering in modalità FULL con read-ahead aggressivo
fsal_enable_buffering(&handle, FSAL_BUFFER_FULL);

// Leggi il file in piccoli blocchi - il buffer interno velocizza le operazioni
for (int i = 0; i < 100; i++) {
    if (fsal_read(buffer, 1, sizeof(buffer), &handle, &bytes_read) != FSAL_SUCCESS) {
        break;
    }
    // Processa dati...
}

fsal_close(&handle);  // Il buffer viene deallocato automaticamente
```

**Performance**: Con buffering abilitato, le piccole letture sequential vengono servite dal buffer interno (default 4KB), riducendo drasticamente le chiamate I/O al backend sottostante.

#### Buffering Adattivo per Pattern Misti

La modalità `FSAL_BUFFER_ADAPTIVE` analizza il pattern di accesso e attiva automaticamente il read-ahead solo quando rileva accessi sequenziali:

```c
fsal_handle_t handle;
char data[16];
size_t bytes_read;

fsal_open("config.dat", FSAL_MODE_READ, &handle);

// Buffering adattivo - attivato solo per accessi sequenziali
fsal_enable_buffering(&handle, FSAL_BUFFER_ADAPTIVE);

// Letture sequenziali - il buffer si attiva automaticamente
// dopo FSAL_CFG_READAHEAD_THRESHOLD letture consecutive
for (int i = 0; i < 10; i++) {
    fsal_read(data, 1, sizeof(data), &handle, &bytes_read);
}

// Seek - il buffer viene invalidato automaticamente
fsal_seek(&handle, 1024, SEEK_SET);

// Letture random - nessun overhead di read-ahead
fsal_read(data, 1, sizeof(data), &handle, &bytes_read);

fsal_close(&handle);
```

**Quando usare ADAPTIVE**: Pattern di accesso imprevedibili dove si alternano letture sequenziali e random. Il sistema evita read-ahead inutili per accessi casuali.

#### Controllo Dinamico del Buffering

È possibile abilitare e disabilitare il buffering in qualsiasi momento:

```c
fsal_handle_t handle;
char buffer[256];

fsal_open("stream.dat", FSAL_MODE_READ, &handle);

// Fase 1: Letture sequenziali con buffering
fsal_enable_buffering(&handle, FSAL_BUFFER_FULL);
fsal_read(buffer, 1, 256, &handle, NULL);

// Fase 2: Operazione che richiede I/O diretto (es. memory-mapped)
fsal_enable_buffering(&handle, FSAL_BUFFER_NONE);
fsal_read(buffer, 1, 256, &handle, NULL);

// Fase 3: Riabilita buffering adattivo
fsal_enable_buffering(&handle, FSAL_BUFFER_ADAPTIVE);
fsal_read(buffer, 1, 256, &handle, NULL);

fsal_close(&handle);
```

#### Considerazioni su Memoria e Performance

| Parametro | Valore Default | Descrizione |
|-----------|----------------|-------------|
| `FSAL_CFG_READ_BUFFER_SIZE` | 4096 bytes | Dimensione buffer per file. Più grande = meno I/O, più memoria |
| `FSAL_CFG_READAHEAD_THRESHOLD` | 3 letture | Letture sequenziali necessarie per attivare read-ahead in ADAPTIVE mode |
| `FSAL_CFG_READAHEAD_MULTIPLIER` | 4x | Moltiplicatore read-ahead: buffer_size × multiplier bytes letti in anticipo |

**Ottimizzazione memoria**: Con 16 file aperti e buffer da 4KB, l'overhead massimo è 64KB solo se tutti i file hanno buffering abilitato. Il buffer viene allocato dinamicamente solo quando richiesto.

**Ottimizzazione I/O**: Un `READAHEAD_MULTIPLIER` di 4x significa che ogni miss del buffer legge 16KB (4096 × 4), riducendo le chiamate I/O dell'80-95% per accessi sequenziali.

#### Invalidazione Automatica del Buffer

Il buffer viene automaticamente invalidato per garantire coerenza dei dati in queste situazioni:

- **Dopo `fsal_seek()`**: Il buffer potrebbe contenere dati di una posizione diversa
- **Prima di `fsal_write()`**: Evita inconsistenze tra buffer di lettura e dati scritti
- **Su cambio modalità**: Passando da FULL/ADAPTIVE a NONE

Questo comportamento è trasparente e non richiede intervento dell'utente.

#### Esempio Completo: Log Parser Ottimizzato

```c
// Parser di file di log con buffering adattivo
fsal_handle_t log_file;
char line[256];
size_t bytes_read;
int lines_parsed = 0;

if (fsal_open("system.log", FSAL_MODE_READ, &log_file) == FSAL_SUCCESS) {
    // Abilita buffering adattivo - ottimale per file di log
    fsal_enable_buffering(&log_file, FSAL_BUFFER_ADAPTIVE);
    
    // Lettura riga per riga - il buffer interno accelera l'I/O
    while (fsal_read(line, 1, sizeof(line), &log_file, &bytes_read) == FSAL_SUCCESS 
           && bytes_read > 0) {
        // Processa la riga
        parse_log_line(line, bytes_read);
        lines_parsed++;
    }
    
    printf("Parsed %d lines with optimized buffering\n", lines_parsed);
    fsal_close(&log_file);
}
```

### 5. Gestione Errori

```c
fsal_result_t result = fsal_open("missing.txt", FSAL_MODE_READ, &handle);
if (result != FSAL_SUCCESS) {
    printf("Errore: %s\n", fsal_get_error_string(result));
    
    switch (result) {
        case FSAL_ERROR_NOT_FOUND:
            // File non trovato
            break;
        case FSAL_ERROR_ACCESS_DENIED:
            // Permessi negati
            break;
        case FSAL_ERROR_TOO_MANY_OPEN:
            // Troppi file aperti
            break;
        // ... altri errori
    }
}
```

## 🎯 Punti di Estensione Identificati

### 1. Nuove Funzionalità File System

#### **Directory Operations** 
- `fsal_mkdir()` - Creazione directory
- `fsal_rmdir()` - Rimozione directory
- `fsal_list_dir()` - Listing contenuto directory
- `fsal_get_current_dir()` - Directory corrente

#### **File Metadata Advanced**
- `fsal_rename()` - Rinomina file/directory
- `fsal_remove()` - Cancellazione file
- `fsal_get_size()` - Dimensione file veloce
- `fsal_set_attributes()` - Modifica attributi file

#### **Advanced I/O Operations**
- `fsal_copy()` - Copia file efficiente
- `fsal_move()` - Spostamento file
- `fsal_append_string()` - Append semplificato
- `fsal_read_all()` - Lettura completa file
- ✅ **`fsal_enable_buffering()`** - **IMPLEMENTATO!** Controllo buffering con modalità adattiva

### 2. Estensioni Backend

#### **Nuovi Backend**
- **Memory Backend**: File system in memoria per testing/cache
- **Compressed Backend**: Supporto file compressi (ZIP, GZIP)
- **Network Backend**: Integrazione con cloud storage (S3, FTP)
- **Encrypted Backend**: File system criptato

#### **Miglioramenti Backend Esistenti**
- Supporto Unicode per nomi file
- Metadati estesi (ACL, extended attributes)
- Journaling per consistenza dati

### 3. Performance & Scalability

#### **Read Buffering** ✅ **IMPLEMENTATO!**
- ✅ Buffer lineare con read-ahead adattivo
- ✅ Tre modalità: NONE (direct I/O), FULL (always buffer), ADAPTIVE (pattern-based)
- ✅ Statistiche di accesso (sequential vs random reads)
- ✅ Invalidazione automatica su seek/write
- ✅ Configurazione flessibile (dimensione buffer, threshold, multiplier)
- ✅ Zero overhead quando disabilitato via compile-time

#### **I/O Asincrono**
```c
// Esempio API futura
fsal_result_t fsal_read_async(fsal_handle_t* handle, void* buffer, 
                              size_t size, fsal_callback_t callback);
fsal_result_t fsal_wait_async(fsal_async_handle_t* async_handle);
```

#### **Connection Pooling**
- Pool di handle per backend custom
- Riutilizzo connessioni per performance

#### **Cache Layer**
- ✅ **Read buffering implementato** - cache intelligente per letture ripetute
- Write-back caching configurabile
- Memory-mapped I/O per file grandi

### 4. Monitoring & Diagnostics

#### **Statistiche Avanzate**
- Performance metrics per operazione
- Heat map di accesso file
- Bandwidth monitoring

#### **Logging & Tracing**
- Integration con syslog
- Structured logging (JSON)
- Distributed tracing support

## 🚀 Pronto per la Prossima Fase!

### Raccomandazioni per Nuove Funzionalità

1. **📋 Mantenere API Consistency**
   - Seguire il pattern `fsal_<operation>()`
   - Utilizzare i tipi `fsal_result_t` esistenti
   - Documentazione Doxygen completa

2. **🔄 Supporto Multi-Backend**
   - Implementare per tutti i backend supportati
   - Graceful degradation se funzionalità non disponibile
   - Testing su tutti i backend

3. **🛡️ Error Handling Robusto**
   - Utilizzare codici `fsal_result_t` esistenti
   - Aggiungere nuovi codici se necessario
   - Messaggi di errore user-friendly

4. **⚙️ Configurabilità**
   - Opzioni in `ag_fsal_cfg.h` per nuove features
   - Default values sensati
   - Validazione compile-time

5. **🧪 Testing Completo**
   - Unit test per ogni path di codice
   - Integration test multi-backend
   - Performance benchmarks

### Idee per Funzionalità Future

#### **High Priority** 🔴
- **Directory operations** (mkdir, rmdir, list)
- **File manipulation** (copy, move, rename, delete)
- **Extended file info** (permissions, timestamps avanzati)
- ✅ **Read buffering** - **COMPLETATO!**

#### **Medium Priority** 🟡
- **Batch operations** (operazioni multi-file)
- **Memory backend** per testing
- **Async I/O support**
- **Write buffering** (complementare al read buffering esistente)

#### **Low Priority** 🟢
- **Compression support**
- **Network/cloud backends**
- **Advanced caching**

## 📖 Esempi Avanzati

### Custom Backend Implementation

```c
#if (FSAL_CFG_BACKEND == 2)

fsal_result_t my_custom_open(const char* path, fsal_mode_t mode, void** handle) {
    // Implementazione custom
    return FSAL_SUCCESS;
}

fsal_result_t my_custom_close(void* handle) {
    // Implementazione custom
    return FSAL_SUCCESS;
}

// ... altre funzioni ...

fsal_custom_interface_t my_interface = {
    .open = my_custom_open,
    .close = my_custom_close,
    .read = my_custom_read,
    .write = my_custom_write,
    .seek = my_custom_seek,
    .tell = my_custom_tell,
    .flush = my_custom_flush,
    .stat = my_custom_stat
};

int main() {
    // Imposta interfaccia custom prima di init
    fsal_set_custom_interface(&my_interface);
    fsal_init();
    
    // ... usa la libreria ...
}

#endif
```

### Statistiche Runtime

```c
#if FSAL_CFG_STATISTICS_ENABLED

void print_file_stats(fsal_handle_t* handle) {
    size_t bytes_read, bytes_written;
    uint32_t operations;
    
    if (fsal_get_stats(handle, &bytes_read, &bytes_written, &operations) == FSAL_SUCCESS) {
        printf("File Statistics:\n");
        printf("  Bytes read: %zu\n", bytes_read);
        printf("  Bytes written: %zu\n", bytes_written);
        printf("  Total operations: %u\n", operations);
    }
}

#endif
```

## 🧪 Testing

La libreria include una suite completa di unit test con CMocka:

```bash
# Compilazione test
make build

# Esecuzione test
make test

# Code coverage
make gcov
```

### Test Coverage
- ✅ **17/17 test** passano con successo
- ✅ **Inizializzazione/deinizializzazione** libreria
- ✅ **Operazioni file base** (open, read, write, close)
- ✅ **Positioning** (seek, tell)
- ✅ **Metadata** (stat, exists)
- ✅ **Error handling** e validazione parametri
- ✅ **Gestione memoria** corretta
- ✅ **Read buffering** (sequential, adaptive, invalidation, enable/disable)

## 📄 Licenza

Copyright (c) 2025, Race1Up Project. All rights reserved.

## 🤝 Contribuzioni

Per contribuire al progetto:

1. Fork del repository
2. Crea feature branch (`git checkout -b feature/amazing-feature`)
3. Implementa modifiche con test completi
4. Commit (`git commit -m 'Add amazing feature'`)
5. Push (`git push origin feature/amazing-feature`)
6. Apri Pull Request

---

**Autore**: Alex Giov  
**Data**: September 28, 2025  
**Versione**: 1.0.0

*Ready to take your file operations to the next level!* 🚀