# TODO
Refactoring a seguito dell'intoroduzione della nuova lettura del file legacy ReRace 


Pensare a questo:

Aggiungere alla configurazione il tipo di dato del PAYLOAD?
[] Riportare tutti i readme delle librerie dismesse qui dentro.
[] ATTENZIONE ID troppo lunghi nella tabella potrebbero convertire in stringhe che hanno i prim 16 caratteri uguali ma sono id diversi.

# File specs

[] il body deve contenere solo BODY_ID e lo stream dei dati
[] La formattazione dello stream delle variabili deve essere nell'header che una volta chiuso consente l'accesso alla scrittura del BODY secondo la formattazione specificata.
[] le variabili al primo blocco sono tutte scritte. il campione zero dal quale parte il conto della frequenza.

## scrittura

I file blocks sono definiti nella file di configurazione della libreria.

Se un utente deve aggiungere un nuovo blocco è consigliabile farlo aggiungendolo alla R1UP_FILE_BLOCKS_TABLE

Un blocco può anche essere aggiunto a run time ma nel caso della scrittura non ha molto senso, in quanto chi scrive il codice ed ha bisogno di un blocco dedicato puo farlo aggiungendolo alla R1UP_FILE_BLOCKS_TABLE. In tal modo è anche più semplice anche andarlo a rileggere semplicemente accedendolo tramite l'enum

## lettura

Sono letti tutti i blocchi e messi in una lista.

Può succedere che vi sia missmatch tra R1UP_FILE_BLOCKS_TABLE e i blocchi nel file. In tal caso si puo cercare un blocco particolare passando direttamente una id_strin

## Data e Time
Deve gestire epoch ed anche il tempo UTC, verificare come arriva lo stream del tempo GPS.
Sarebbe opportuno anche una libreria per gestire date e tempo.
Il funzionamento intelligente potrebbe essere:
usare __DATE__ e __TIME__ che sicuramente costituiscono un tempo, ma attenzione che vanno espansi come macro UTC altrimenti ti metto l'ora di sistema
gcc -DBUILD_TIME="\"$(date -u +%H:%M:%S)\"" -DBUILD_DATE="\"$(date -u +'%b %d %Y')\"" mio_programma.c


## TODO subito
nel converter prendere il numero di campioni in un altro modo:
	/* Step 3: Get total sample count */
	uint64_t total_samples = 0;
	const uint16_t* rpm_data = r1up_file_get_rpm(r1up_handle, &total_samples);
	if (rpm_data == NULL || total_samples == 0) {
		result = R1UP_TO_RC_ERROR_R1UP_READ;
		goto cleanup;
	}

 [] Mettere un campo nell'header del nuovo file R1UP che indica la timezone impostata dall'utente, o verificare se è possibile 
 ricavarla direttamente dal GPS, vedere chat gpt dove quando analizzavi time.h c'erano anche delle librerie rmc per parsare i dati gps credo. NOTA INTERESSANTE il fuso orario lo potresti anche trovare facendo la differenza tra UTC e __BUILDTIME___
 dovresti salvare l'utc a tempo di compiazione, questo non so se è facile da fare.