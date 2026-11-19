# Da fare

La seguente funzione 

```
void* r1up_file_get_payload(r1up_file_handle_t handle, const char* id_string);
```

Non ha la firma classica usata per tutto il file e la segunete funzione ha la firma giusta, verificare dove è usata e modificarla.

```
r1up_file_result_t r1up_file_get_block_payload_ptr(r1up_file_handle_t handle, const char* id_string, void* pl_pntr,
												   uint32_t* pl_sz);
```

