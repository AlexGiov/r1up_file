# Memory Abstraction Layer (MAL) 🧠

Un layer di astrazione per la gestione della memoria che fornisce portabilità tra C standard e FreeRTOS, con funzionalità avanzate di debugging e monitoraggio.

## 📁 Struttura dei File

- **`ag_mal.h`** - API pubblica del Memory Abstraction Layer
- **`ag_mal.c`** - Implementazione delle funzioni MAL
- **`ag_mal_cfg.h`** - Configurazione e parametri del sistema

## ✨ Caratteristiche Principali

### 🔄 **Portabilità**
- Supporto seamless tra `malloc/free` (C standard) e `pvPortMalloc/vPortFree` (FreeRTOS)
- Cambio di backend tramite semplice configurazione

### 🐛 **Debugging Avanzato**
- Tracking delle allocazioni con informazioni su file/riga
- Rilevamento automatico dei memory leak
- Protezione da buffer overflow con guard bytes
- Rilevamento doppia deallocazione

### 📊 **Monitoraggio**
- Statistiche dettagliate sull'uso della memoria
- Tracking del picco di utilizzo
- Contatori di allocazioni/deallocazioni
- Report dettagliati per debugging

### 🔒 **Thread Safety**
- Supporto completo per ambienti multi-thread
- Mutex automatici in FreeRTOS
- Facilmente estendibile per pthread in C standard

## 🚀 Guida Rapida

### 1. **Configurazione**

Modifica `ag_mal_cfg.h` per le tue esigenze:

```c
// Seleziona il backend
#define AG_MAL_BACKEND AG_MAL_BACKEND_STD_C     // C standard
// #define AG_MAL_BACKEND AG_MAL_BACKEND_FREERTOS  // FreeRTOS

// Abilita funzionalità
#define AG_MAL_ENABLE_TRACKING     1  // Tracking allocazioni
#define AG_MAL_ENABLE_STATISTICS   1  // Statistiche memoria
#define AG_MAL_ENABLE_THREAD_SAFE  1  // Thread safety
#define AG_MAL_ENABLE_BOUNDS_CHECK 1  // Rilevamento overflow
```

### 2. **Inizializzazione**

```c
#include "ag_mal.h"

int main(void) {
    // Inizializza il MAL
    ag_mal_result_t result = ag_mal_init();
    if (result != AG_MAL_OK) {
        printf("Errore inizializzazione MAL: %d\n", result);
        return -1;
    }
    
    // Il tuo codice qui...
    
    // Verifica leak prima della chiusura
    if (ag_mal_check_leaks() > 0) {
        printf("⚠️  Memory leak rilevati!\n");
        ag_mal_print_report();
    }
    
    // Deinitializza
    ag_mal_deinit();
    return 0;
}
```

### 3. **Utilizzo Base**

```c
// Allocazione
char *buffer = AG_MALLOC(1024);
if (buffer == NULL) {
    printf("Allocazione fallita!\n");
    return -1;
}

// Utilizzo
strcpy(buffer, "Hello MAL!");
printf("Data: %s\n", buffer);

// Deallocazione
AG_FREE(buffer);
```

### 4. **Funzioni Avanzate**

```c
// Calloc (allocazione + azzeramento)
int *array = (int*)ag_mal_calloc(10, sizeof(int));

// Realloc
buffer = (char*)ag_mal_realloc(buffer, 2048);

// Statistiche
ag_mal_stats_t stats;
ag_mal_get_stats(&stats);
printf("Memoria allocata: %zu bytes\n", stats.total_allocated);
printf("Picco utilizzo: %zu bytes\n", stats.peak_allocated);
printf("Allocazioni attive: %zu\n", stats.num_allocations);
```

## 📊 API Reference

### Funzioni di Allocazione

| Funzione | Descrizione |
|----------|-------------|
| `ag_mal_alloc(size)` | Alloca `size` bytes |
| `ag_mal_free(ptr)` | Libera memoria precedentemente allocata |
| `ag_mal_calloc(num, size)` | Alloca e azzera `num * size` bytes |
| `ag_mal_realloc(ptr, new_size)` | Ridimensiona blocco di memoria |

### Funzioni di Monitoraggio

| Funzione | Descrizione |
|----------|-------------|
| `ag_mal_get_stats(stats)` | Ottiene statistiche memoria |
| `ag_mal_check_leaks()` | Controlla memory leak (ritorna numero leak) |
| `ag_mal_print_report()` | Stampa report dettagliato |
| `ag_mal_validate_all()` | Valida tutte le allocazioni |

### Funzioni di Gestione

| Funzione | Descrizione |
|----------|-------------|
| `ag_mal_init()` | Inizializza il MAL |
| `ag_mal_deinit()` | Deinitializza il MAL |

### Codici di Ritorno

| Codice | Significato |
|--------|-------------|
| `AG_MAL_OK` | Operazione completata con successo |
| `AG_MAL_ERROR_NULL_POINTER` | Puntatore NULL non valido |
| `AG_MAL_ERROR_INVALID_SIZE` | Dimensione non valida |
| `AG_MAL_ERROR_OUT_OF_MEMORY` | Memoria esaurita |
| `AG_MAL_ERROR_CORRUPTION` | Corruzione memoria rilevata |
| `AG_MAL_ERROR_NOT_FOUND` | Blocco memoria non trovato |
| `AG_MAL_ERROR_LIMIT_EXCEEDED` | Limite superato |

## 🔧 Configurazione Avanzata

### Parametri Configurabili

```c
// In ag_mal_cfg.h
#define AG_MAL_MAX_TRACKED_ALLOCS 256        // Max allocazioni tracciabili
#define AG_MAL_GUARD_SIZE         4          // Bytes di guardia
#define AG_MAL_GUARD_PATTERN      0xDEADBEEF // Pattern per rilevamento overflow
```

### Macro di Compatibilità

Per facilitare la migrazione da codice esistente:

```c
#define AG_MALLOC(size)       ag_mal_alloc(size)
#define AG_FREE(ptr)          ag_mal_free(ptr)
#define AG_REALLOC(ptr, size) ag_mal_realloc(ptr, size)
#define AG_CALLOC(num, size)  ag_mal_calloc(num, size)
```

## 🧪 Testing

Il progetto include una suite completa di test CMocka che verifica:

- ✅ Funzionalità di allocazione base
- ✅ Tracking e statistiche memoria
- ✅ Rilevamento memory leak
- ✅ Protezione buffer overflow
- ✅ Thread safety
- ✅ Gestione errori
- ✅ Stress test con allocazioni multiple

### Esecuzione Test

```bash
# Compila e esegui i test
make test

# Output atteso:
# =================================================================
#   Memory Abstraction Layer (MAL) Unit Test Suite
# =================================================================
#   All MAL tests PASSED! ✓
```

## 🛡️ Sicurezza e Robustezza

### Protezioni Implementate

1. **Buffer Overflow Detection**: Guard bytes prima e dopo ogni allocazione
2. **Double Free Protection**: Rilevamento tentativi di doppia deallocazione  
3. **Invalid Pointer Detection**: Controllo validità puntatori
4. **Memory Corruption Detection**: Validazione integrità guard bytes
5. **Thread Safety**: Mutex automatici per accesso concorrente

### Best Practices

```c
// ✅ CORRETTO
void *ptr = AG_MALLOC(size);
if (ptr != NULL) {
    // Usa il puntatore
    AG_FREE(ptr);
    ptr = NULL; // Buona pratica
}

// ❌ EVITARE
AG_FREE(ptr);
AG_FREE(ptr); // Doppia deallocazione - rilevata dal MAL!
```

## 🔄 Migrazione da Codice Esistente

### Da malloc/free Standard

```c
// Prima
char *buffer = malloc(1024);
free(buffer);

// Dopo  
char *buffer = AG_MALLOC(1024);  
AG_FREE(buffer);
```

### Da FreeRTOS

```c
// Prima
char *buffer = pvPortMalloc(1024);
vPortFree(buffer);

// Dopo (stessa sintassi, cambio solo configurazione)
char *buffer = AG_MALLOC(1024);
AG_FREE(buffer);
```

## 📈 Monitoraggio in Produzione

### Controlli Periodici

```c
void memory_health_check(void) {
    ag_mal_stats_t stats;
    ag_mal_get_stats(&stats);
    
    // Controlla utilizzo memoria
    if (stats.total_allocated > MEMORY_WARNING_THRESHOLD) {
        log_warning("Alto utilizzo memoria: %zu bytes", stats.total_allocated);
    }
    
    // Controlla leak
    size_t leaks = ag_mal_check_leaks();
    if (leaks > 0) {
        log_error("Memory leak rilevati: %zu", leaks);
        ag_mal_print_report();
    }
    
    // Valida integrità
    if (ag_mal_validate_all() != AG_MAL_OK) {
        log_critical("Corruzione memoria rilevata!");
    }
}
```

## ✅ Verifica Deallocazione Completa

Il MAL offre diversi metodi per verificare che tutta la memoria allocata sia stata correttamente deallocata:

### 🔍 **Metodo 1: Check Rapido con ag_mal_check_leaks()**

```c
int main(void) {
    ag_mal_init();
    
    // Il tuo codice con allocazioni/deallocazioni
    char *buffer1 = AG_MALLOC(100);
    char *buffer2 = AG_MALLOC(200);
    
    AG_FREE(buffer1);
    AG_FREE(buffer2);
    
    // Verifica finale prima della chiusura
    size_t active_allocations = ag_mal_check_leaks();
    if (active_allocations == 0) {
        printf("✅ Tutta la memoria è stata deallocata correttamente!\n");
    } else {
        printf("❌ %zu allocazioni ancora attive!\n", active_allocations);
        ag_mal_print_report(); // Stampa dettagli sui leak
    }
    
    ag_mal_deinit();
    return 0;
}
```

### 📊 **Metodo 2: Verifica con Statistiche Dettagliate**

```c
bool verify_complete_deallocation(void) {
    ag_mal_stats_t stats;
    ag_mal_get_stats(&stats);
    
    printf("=== Verifica Deallocazione ===\n");
    printf("Memoria ancora allocata: %zu bytes\n", stats.total_allocated);
    printf("Allocazioni attive: %zu\n", stats.num_allocations);
    printf("Totale allocazioni: %zu\n", stats.total_allocations);
    printf("Totale deallocazioni: %zu\n", stats.total_deallocations);
    
    bool all_deallocated = (stats.total_allocated == 0 && 
                           stats.num_allocations == 0 &&
                           stats.total_allocations == stats.total_deallocations);
    
    if (all_deallocated) {
        printf("✅ Bilancio perfetto: tutta la memoria deallocata!\n");
    } else {
        printf("❌ Memoria non completamente deallocata!\n");
        ag_mal_print_report();
    }
    
    return all_deallocated;
}
```

### 🏃‍♂️ **Metodo 3: Verifica Automatica in Teardown**

```c
// Setup automatico con verifica leak
static int smart_setup(void** state) {
    ag_mal_init();
    return 0;
}

static int smart_teardown(void** state) {
    // Verifica automatica leak prima del cleanup
    size_t leaks = ag_mal_check_leaks();
    
    if (leaks > 0) {
        printf("⚠️  ATTENZIONE: %zu memory leak rilevati!\n", leaks);
        ag_mal_print_report();
        
        // Opzionale: fai fallire il test se ci sono leak
        // fail_msg("Memory leak detected in teardown");
    }
    
    ag_mal_deinit();
    return 0;
}
```

### 🔧 **Metodo 4: Hook di Cleanup Personalizzato**

```c
// Macro per verifica automatica
#define MAL_CLEANUP_AND_VERIFY() do { \
    size_t leaks = ag_mal_check_leaks(); \
    if (leaks > 0) { \
        fprintf(stderr, "❌ LEAK DETECTED: %zu active allocations at %s:%d\n", \
                leaks, __FILE__, __LINE__); \
        ag_mal_print_report(); \
        exit(EXIT_FAILURE); \
    } \
    ag_mal_deinit(); \
} while(0)

// Utilizzo
int main(void) {
    ag_mal_init();
    
    // Il tuo codice...
    
    // Cleanup con verifica automatica
    MAL_CLEANUP_AND_VERIFY();
    return 0;
}
```

### 🧪 **Metodo 5: Verifica in Unit Test**

```c
// Test specifico per verificare deallocazione completa
static void Test_Complete_Deallocation_Verification(void** state) {
    // Alloca varia memoria
    void *ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = ag_mal_alloc((i + 1) * 64);
        assert_non_null(ptrs[i]);
    }
    
    // Verifica che ci siano allocazioni attive
    assert_true(ag_mal_check_leaks() == 10);
    
    // Dealloca tutto
    for (int i = 0; i < 10; i++) {
        ag_mal_free(ptrs[i]);
    }
    
    // Verifica deallocazione completa
    assert_int_equal(ag_mal_check_leaks(), 0);
    
    ag_mal_stats_t stats;
    ag_mal_get_stats(&stats);
    assert_int_equal(stats.total_allocated, 0);
    assert_int_equal(stats.num_allocations, 0);
}
```

### 📈 **Metodo 6: Monitoraggio Continuo (Production)**

```c
// Timer periodico per controllo memoria
void periodic_memory_check(void) {
    static time_t last_check = 0;
    time_t now = time(NULL);
    
    // Controlla ogni 30 secondi
    if (now - last_check >= 30) {
        size_t leaks = ag_mal_check_leaks();
        
        if (leaks > 0) {
            // Log warning ma non fermare l'applicazione
            log_warning("Memory leak detection: %zu active allocations", leaks);
            
            // In debug mode, stampa report dettagliato
            #ifdef DEBUG
            ag_mal_print_report();
            #endif
        }
        
        last_check = now;
    }
}
```

### ⚡ **Best Practice per Verifica**

```c
// 1. Verifica all'uscita dell'applicazione
void application_exit(void) {
    printf("Shutting down application...\n");
    
    // Verifica memoria prima della chiusura
    if (ag_mal_check_leaks() == 0) {
        printf("✅ Clean shutdown: no memory leaks\n");
    } else {
        printf("❌ Memory leaks detected during shutdown!\n");
        ag_mal_print_report();
    }
    
    ag_mal_deinit();
}

// 2. Verifica per moduli specifici
bool module_cleanup_verify(const char* module_name) {
    size_t leaks_before = ag_mal_check_leaks();
    
    // Esegui cleanup del modulo
    module_cleanup();
    
    size_t leaks_after = ag_mal_check_leaks();
    
    if (leaks_after < leaks_before) {
        printf("✅ Module '%s' cleaned up successfully\n", module_name);
        return true;
    } else {
        printf("❌ Module '%s' may have memory leaks\n", module_name);
        return false;
    }
}

// 3. Verifica con assertion per debug builds
#ifdef DEBUG
#define ASSERT_NO_LEAKS() do { \
    size_t leaks = ag_mal_check_leaks(); \
    if (leaks > 0) { \
        ag_mal_print_report(); \
        assert(leaks == 0 && "Memory leaks detected!"); \
    } \
} while(0)
#else
#define ASSERT_NO_LEAKS() do { } while(0)
#endif
```

### 🎯 **Riassunto Metodi di Verifica**

| Metodo | Quando Usare | Vantaggi |
|--------|--------------|----------|
| `ag_mal_check_leaks()` | Fine programma, test | Rapido, semplice |
| Statistiche dettagliate | Debug approfondito | Informazioni complete |
| Teardown automatico | Unit test | Verifica automatica |
| Hook personalizzato | Applicazioni critiche | Controllo totale |
| Monitoraggio continuo | Produzione | Rilevamento precoce |

**💡 Tip**: In produzione, usa `ag_mal_check_leaks()` prima della chiusura dell'applicazione. In sviluppo, abilita `ag_mal_print_report()` per dettagli completi sui leak!

## 🤝 Supporto e Contributi

Per domande, bug report o miglioramenti:

1. Verifica che la configurazione in `ag_mal_cfg.h` sia corretta
2. Esegui i test per verificare l'installazione: `make test`
3. Abilita debug per diagnostica: `#define AG_MAL_DEBUG_ENABLED 1`
4. Usa `ag_mal_print_report()` per informazioni dettagliate

## 📄 Licenza

Copyright (c) 2025, AlexGiov.
All Rights Reserved.

---

**Buon coding con MAL! 🚀**