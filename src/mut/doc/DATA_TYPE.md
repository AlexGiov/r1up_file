Per identificare il tipo di dato della variabile si va a definire un ID univoco per ogni tipo di dato.

L'ID permette di capire il tipo di dato presente nel file a run time perché è salvato nella struttura VSD.

Dal tipo di dato poi possiamo ricavare la dimensione e quindi leggerlo correttamente nel file.

Dal tipo di dato possiamo anche capire la giusta modalita di lettura

Ecco un esempio di associazione tipo di dato - ID



```c
R1UP_FILE_UINT16	uint16_t
R1UP_FILE_INT8		int8_t
RUP_FILE_UQ12_4		ag_fixedpoint_uq12_4_t
    
```

Ovviamente se un tipo non è standard C da qualche parte ci deve essere la definizioine di tipo

```
typedef uint16_t ag_fixedpoint_uq12_4_t;
```

Nota nella tabella attuale è indicato solo uq12_4_t perche ag_fixedpoint_ è aggiunto durante l'espansione. Inoltre cè una colona ridondande che specifica i dati base ma che alla luce di quanto detto adesso non sarebbe più necessario

Potremmo quindi definire la nuova tabella come



```
#define R1UP_FILE_VARS_STREAM_FORMAT_TABLE(VSF_ENTRY)	\
VSF_ENTRY(  RPM_ID,       	R1UP_FILE_UQ16_0,  ag_fixedpoint_uq16_0, 	 1U,     rpm               )  \
  VSF_ENTRY(  EXAUSTEMP_ID,    R1UP_FILE_UQ12_4,  ag_fixedpoint_uq12_4,  	1U,     exaust               )  \
  VSF_ENTRY(  FLUIDTEMP_ID,    R1UP_FILE_UIQ10_6,  ag_fixedpoint_uq10_6, 	 10U,     coolant	)  \
```

e poi una tabella di associazione ID tipo di dato

```
#define R1UP_FILE_INNER_DATA_TYPE_ID_TABLE(IDT_TYPE_ENTRY) \
  IDT_TYPE_ENTRY(R1UP_FILE_UQ16_0, 0U)           \
  IDT_TYPE_ENTRY(R1UP_FILE_UQ12_4, 1U)          \
  IDT_TYPE_ENTRY(R1UP_FILE_UIQ10_6, 2U)          \


```

