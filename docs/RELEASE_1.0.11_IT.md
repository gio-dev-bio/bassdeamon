# BassDeamon 1.0.11

## Scope

Stereo mostra due somme separate Kick + Bass, L e R. Phase A/B seleziona Mono per il confronto di fase. Selezionando Stereo si esce da Phase A/B e si torna alle due tracce.

## Spettro

RATE va da 1 a 60 fps. Le nuove istanze partono da 30 fps e i progetti conservano il valore salvato. Le FFT lavorano in background e l'interfaccia riceve solo misure già calcolate. La traccia scorre tra due misure senza interpolare il contatore, le bande o i valori in dB.

## Bande di conflitto

Lo spettro mostra il range scelto. Il contatore misura le bande da 30 a 250 Hz dentro quel range.

Un marker richiede sovrapposizione spettrale e presenza simultanea di basso e kick sullo stesso canale. Il rilevatore misura una finestra FFT di 8192 campioni con peso maggiore sul corpo del kick con passo di 20 ms e conferma la banda con una FFT di 16384 campioni. A 44,1 kHz le finestre durano circa 186 e 372 ms. La misura usa livelli spettrali dBFS, con peso che sale tra −90 e −66 dBFS per la sorgente più debole. Il peso relativo parte da zero quando il basso è 24 dB sotto il kick e raggiunge il massimo a parità. Le due misure confluiscono in un punteggio tra 0 e 1, con soglia a 0,12.

Picchi distanti al massimo 12 Hz, o tre bin se più larghi, vengono raggruppati. Le bande separate vengono contate tutte. I marker all'interno di un refresh provengono da una singola istantanea. PRE usa il basso in ingresso, POST quello processato. Il confronto in dB della banda principale usa le misure del kick e del basso nello stesso intervallo e canale. Il rilevatore misura la sovrapposizione, la sua rilevanza in ascolto dipende dal mix.

## Installazione

Estrai tutto lo ZIP, chiudi Cubase e avvia Installa_Aggiorna.command. Se Finder blocca l'apertura, apri Terminale, digita /bin/bash seguito da uno spazio, trascina il file .command nella finestra e premi Invio.
