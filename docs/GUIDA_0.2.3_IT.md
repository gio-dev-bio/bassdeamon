# BassDeamon 0.2.3 — prova in Cubase

Chiudi Cubase e sostituisci BassDeamon.vst3 in `/Library/Audio/Plug-Ins/VST3/` con il bundle incluso. È una build di sviluppo firmata ad-hoc, non notarizzata. Le istruzioni in INSTALL.txt restano applicabili.

Inserisci il plugin sul basso e invia il kick al sidechain. Il kick non entra nell'uscita audio. La catena è GLUE → PHASE opzionale → SOFT TUBE opzionale → UNMASK → trim.

## Grafici

- PRE è l'ingresso del basso prima dell'intera catena; POST è l'uscita reale, compresi trim, POWER e le modalità di ascolto attive. La scala dei due grafici è la stessa; il kick è il riferimento ambra, il basso è chiaro, il possibile masking è evidenziato in rosso.
- Il rosso indica un'euristica di sovrapposizione con energia sufficiente e livelli vicini nella finestra di analisi. Non misura un conflitto necessariamente udibile e non rappresenta il taglio applicato da UNMASK.
- FFT di 4096 campioni: finestra di 85,3 ms a 48 kHz; lo spettro usa le potenze di entrambi i canali. La vista arriva a 5 kHz, ma UNMASK continua a elaborare le sue bande basse.
- Lo scope mostra il canale L; SUM K+B visualizza la somma con il riferimento kick, senza modificare il routing audio. I due segnali condividono una scala di ampiezza. Il campionamento grafico non è una vista campione per campione dei transienti più rapidi.
- L'acquisizione per i nuovi grafici si ferma chiudendo l'editor. La FFT viene eseguita nel thread dell'interfaccia; in caso di coda piena vengono scartati dati grafici, non bloccato l'audio.

## Soft Tube

Parte spento, anche caricando stati delle versioni precedenti. Attivalo e aumenta gradualmente Amount confrontando POWER a volume simile.

- DRIVE: intensità della curva morbida leggermente asimmetrica, 0–18 dB. A 0 dB il circuito matematico rimane una saturazione: per il bypass usa ON/OFF o Amount a zero con Auto Gain disattivato.
- AMOUNT: contributo parallelo aggiunto al basso originale.
- SOURCE LP: passa-basso della sorgente, 60–500 Hz. Non segue le note; la saturazione genera armoniche in base al segnale che entra.
- COLOUR: modifica gradualmente asimmetria e filtro del ramo aggiunto.
- AUTO GAIN / GAIN LOCK: compensazione energetica lenta del modulo, con limite +/-3 dB e budget positivo condiviso. Non è un limiter e non garantisce lo stesso volume percepito.
- ADDED ONLY: manda il solo residuo aggiunto verso UNMASK e uscita. Il residuo può contenere fondamentale/intermodulazione, non esclusivamente armoniche. L'AGC del Tube viene disattivato durante questo ascolto.

Oversampling FIR fisso 4x nel ramo Tube: riduce l'aliasing, non lo elimina universalmente. Il ramo originale non riceve latenza aggiuntiva; il ramo aggiunto ha 12 campioni di ritardo FIR, oltre alla fase dei filtri. Il sub della somma non è garantito identico al dry. Non è un modello fisico di una valvola.

## Risorse e limiti

Primo benchmark nativo sul runner Intel i7-8700B, 48 kHz stereo/buffer 128, GLUE e UNMASK attivi, PHASE spento, altri AGC spenti: circa 3,93% di un core con Tube spento, 7,39% con Tube attivo, 7,67% con Tube e suo AGC. Mac ARM M1 virtuale: 1,78%, 3,01%, 3,84%. Sono tempi del motore su macchine condivise, senza GUI/Cubase; non stimano con precisione il Mac dell'utente. Sono stati osservati outlier temporali. Il vecchio 2,9% appartiene a una prova diversa e non è un confronto controllato.

La nuova coda audio/grafica usa 196624 byte di memoria fissa per istanza; questo non è il consumo RAM totale del plugin. Memoria del processo host e costo del rendering GUI non sono ancora quantificati.

Usa una copia del progetto e verifica salvataggio/riapertura, export offline, mono/stereo, buffer piccoli e più istanze. I test sintetici non sostituiscono l'ascolto su stem reali. Nessun limiter è aggiunto; controlla il peak e riduci il trim se necessario. M/S non implementato.
