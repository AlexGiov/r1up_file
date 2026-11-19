# File stucture



| Block Identifier  R1UP_FILE_BLCK_ID_LENGTH bytes                                                                           | Lenght of the payload                                                                          | Payload                                    |
| -------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------- | ------------------------------------------ |
| Type:  char[R1UP_FILE_BLCK_ID_LENGTH ] <br />Size: R1UP_FILE_BLCK_ID_LENGTH<br /><br />Value: R1UP_FILE_HEADER_BID         | Type:  uint32_t<br />Size: 4U<br /><br />Content: 4U                                           | [uint32_t] The lengh of all the header     |
| Type:  char[R1UP_FILE_BLCK_ID_LENGTH ] <br />Size: R1UP_FILE_BLCK_ID_LENGTH<br /><br />Value: R1UP_FILE_SAMPLE_RATE_BID    | Type:  uint8_t<br />Size: 1U<br /><br />Content: 1U                                            |                                            |
| Type:  char[R1UP_FILE_BLCK_ID_LENGTH ] <br />Size: R1UP_FILE_BLCK_ID_LENGTH<br /><br />Value: R1UP_FILE_VARS_STRM_FRMT_BID | Type:  Blob<br />Size: Runtime defined<br /><br />Content: the varible stream format structure |                                            |
| R1UP_FILE_SAMPLE_RATE_BID                                                                                                  | [uint32_t] 1U                                                                                  | [uint8_t] Sample Rate                      |
| R1UP_FILE_VARS_STRM_FRMT_BID                                                                                               | [uint32_t] length of payload                                                                   | [blob] the varible stream format structure |
|                                                                                                                            |                                                                                                |                                            |
|                                                                                                                            |                                                                                                |                                            |
|                                                                                                                            |                                                                                                |                                            |



Il file è pensato diviso in tre macro blocchi identificati da R1UP_FILE_HEADER_BID, R1UP_FILE_BODY_BID e R1UP_FILE_FOOTER_BID

If the leng

|     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
|     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |
|     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |

BLOCK ID STRING																Length of payload				Payload			
B0	B1	B2	B3	B4	B5	B6	B7	B8	B9	B10	B11	B12	B13	B14	B15	B16	B17	B18	B19	B20	B21	B22	B23
R	1	U	P	_	F	I	L	E	_	H	E	A	D	E	R	4U				"The lenght of 
the header macro block"			
