ok, perfetto.
Premessa. Il body del file contiene i dati di una sessione in pista, quindi in un circuito. Potrebbe anche contenere i dati di una traccia aperta come una prova speciale rally ma per adesso trascuriamo questo aspetto.
Le funzioni di accesso ai dati del file adesso restituiscono l'interi dati, ad esempio se sono richiesti gli RPM sono restituiti tutti gli rpm dall'inizio alla fine dell'acqusizione.
Si tratta della funzione seguente:
r1up_file_result_t r1up_file_get_var_float(r1up_file_handle_t handle, r1up_variable_id_t var_id,
                                           ag_fixedpoint_float_t* out_float, uint32_t buffer_size)
oppure di questa:
r1up_file_result_t r1up_file_stream_body_next_row(r1up_file_handle_t handle, ag_fixedpoint_float_t* out_values,
                                                  uint32_t array_size)
che permette una lettura senza allocare grande quantità di memoria.

Dobbiamo realizzare una versione della funzione che permette di ricavare i dati raggruppati per giro.
