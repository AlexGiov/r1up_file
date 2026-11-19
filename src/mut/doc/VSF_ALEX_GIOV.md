La lettura del file presenta ancora problemi per risolvere è necessario definire in dettagli le specifiche.

La lettura del file deve basarsi esclusivamente sulle informazione note in modo pregresso sulle seguenti due tabelle. Tutto il resto deve essere ricavato runtime estraendo le informazioni leggendo il file. 

Nominare la tabella 



| Varible id         | Valore (2byte) |
| ------------------ | -------------- |
| SIG_ID_RPM         | 0x11           |
| SIG_ID_TEMPERATURE | 0x15           |
| SIG_ID_SPEED       | 0x02           |

e sulla tabella del tipo di variabile

| Tipo   | Valore (2byte) |
| ------ | -------------- |
| UQ12.4 | 0x12           |
| Q12.3  | 0x11           |
| UQ32.0 | 0x03           |

ID variabile ha lo scopo di identificare la variabile con un nome leggile, mentre id del dato ha lo scopo di leggere la giusta quantità di dati dal file e di convertile correttamente.

Nell'header del file è memorizzato un array di struttyre nominate variable stream format con i seguenti campio

```c
typedef struct {
	uint16_t id;	   //!< variable id
	uint16_t type;  //!< variable type
	uint16_t sz;	   //!< size in byte of the type
	uint16_t fd;	   //!< frequency divisor
} r1up_file_vsf_t;
```

se nel file sono memorizzate N variabile nell'header del file l'array di struttre variable stram format presenta N elementi corrispondenti che permette di decodificare lo stream di variabili memorizzate nel file.

Le funzioni di intergaccia per ottenere le varaibili avranno come argomenti l'ID variabile e restituiranno un valore float della variable convertita. Molto importante che la conversione dipende dal tipo specificato

```c
swtich(type)
    case UQ12_4:
  		ag_fixedpoint_uq12_4_to_float(buffer)

```

