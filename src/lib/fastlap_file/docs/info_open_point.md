# Assunzioni fatte per il file flt

## unità di misura e colonne vuote

Nel file la successione, grandezze unità di misura sembra non tornare.

Nel file è specificato quanto segue

> Time,	Speed,	GPS_Latitude,	GPS_Longitude,	gforce(Y),	gforce(X),	gforce(z),	LeanAngle(),	altitude,	brake,	throttle,	DriftAngle,	Gear,	RPM
> "sec",	"km/h",	"dd",			"dd",			g,			g,			g,			dd,				dd,			m,		bar,		%,			dd,		n
> 0.007,	265.99,	43.96187236,	12.68396213,	0.0,		0.0,		0.0,		0.0

Si sceglie di procedere con questa assegnazione:

> Time,	Speed,	GPS_Latitude,	GPS_Longitude,	gforce(Y),	gforce(X),	gforce(z),	LeanAngle(),	altitude,	brake,	throttle,	DriftAngle,	Gear,	RPM
> "sec",	"km/h",	"dd",			"dd",			g,			g,			g,			dd,				m,			bar,	%,			dd,			n		rpm
> 0.007,	265.99,	43.96187236,	12.68396213,	0.0,		0.0,		0.0,		0.0



Metterei una define che permetta di gestire le colonne vuote. Per le colonne che non hanno un corrispettivo su Race1UP tramite la define o una configurazione si sceglie se non metterle, azzerarle (secondo la unità di misura), o lasciarle vuote (c'`e la virgola di separazione che identifica la colonna ma non il valore.)

## assi accelerazioni

Si copia x in x e y in y e z in z direttamente da come è race1up con la stessa unita di misura e segno.

## vendor id

Si mette tramite una define, verrà assegnato da fastlap in un secondo momento.

## unità di misura della velocità

Consideriamo km/h