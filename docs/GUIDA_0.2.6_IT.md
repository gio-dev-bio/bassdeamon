# BassDeamon 0.2.6 — macOS Intel

## Novità

- PRE e POST selezionano un solo spettro del basso; il kick resta come riferimento. PRE è il basso prima della catena, POST è l’uscita effettiva. La selezione non modifica l’audio.
- Il rosso è un piccolo nastro locale sulla curva del basso, solo dove il contenuto condiviso è significativo. Il criterio è identico per PRE e POST: stesso canale stereo, presenza contemporanea, entrambi sopra -60 dBFS e non oltre 18 dB sotto il proprio picco spettrale, distanza di livello inferiore a 12 dB. Il rosso non riempie più il grafico fino al fondo. Non è una misura assoluta del mascheramento percettivo né l’indicazione dei tagli EQ.
- FROM / TO: limiti continui in Hz, digitabili e trascinabili, fino a 20 kHz (o Nyquist se inferiore). Impostare FROM minore di TO; la finestra minima visualizzata è 20 Hz. L’intervallo effettivo è scritto sotto il grafico. Nessun preset a blocchi. Selezione e limiti sono salvati nella sessione.
- GLUE e UNMASK: in SYNC l’Attack rimane regolabile in millisecondi; solo il Release segue le note e i BPM del progetto. GLUE AUTO conserva l’adattamento automatico. PHASE e Soft Tube non hanno Attack/Release esposti.
- Le sessioni precedenti mantengono gli ID dei parametri; i vecchi parametri Attack Note restano riservati per compatibilità ma non controllano più l’Attack. Una sessione SYNC precedente userà il valore Attack in ms salvato: verificare il risultato all’apertura.

# BassDeamon 0.2.6 — prova in Cubase

Build di sviluppo macOS universale, firma ad-hoc, ancora da verificare nel progetto reale.
Installazione: chiudere Cubase, sostituire BassDeamon.vst3 nella cartella VST3 come indicato in INSTALL.txt, riaprire Cubase. Il bundle mantiene la stessa identità VST3.

## Analizzatore e scope

PRE grigio o POST verde, uno alla volta, con kick ambra e bande di collisione rosse. Scala lineare, limiti FROM / TO liberi in Hz. FFT Hann 16384: passo 2,93 Hz a 48 kHz e 5,86 Hz a 96 kHz; la finestra dura rispettivamente 341 e 171 ms. Questo è il passo dei bin, non una garanzia di separazione tra toni distanti un solo bin. Potenze stereo mediate per lo spettro, analisi per canale per il conflitto. Eliminato lo smoothing aggiuntivo in dB che trascinava le curve. All’avvio si attende una finestra completa. Un’interruzione della coda azzera l’analisi, evitando spettri spurii da campioni discontinui. POST include tutta la catena e il trim.

Scope del canale sinistro: una cattura completa di 1/4 musicale, allineata alla posizione PPQ del progetto, senza scorrimento continuo. A 120 BPM dura 500 ms, a 60 BPM 1000 ms. Mostra minimi e massimi per colonna, preservando i transienti. SUM K+B mostra o nasconde il grafico della somma; quando è spento non viene sostituito da un altro tracciato. La somma è esclusivamente visiva. HOLD ferma la cattura mostrata e non l'audio. Quando manca la posizione host usa un clock locale. Se manca anche il BPM usa 120 BPM, indicato FALLBACK. Un loop più corto di 1/4 può impedire una nuova cattura completa.

## Attack e Release

GLUE: AUTO conserva il comportamento adattivo; MS permette valori manuali; SYNC mantiene Attack in ms e usa una nota per il Release. UNMASK: Attack sempre in ms, Release in ms oppure in note con SYNC. Valori: 1/64, 1/32, 1/16, 1/8, 1/4, 1/2, 1/1, 1/8 terzinato e 1/8 puntato. 1/4 indica un quarto, 1/1 quattro quarti. Il tempo risultante viene mostrato in millisecondi. I cambi di BPM dell'host aggiornano i tempi. Senza un tempo host valido si usano 120 BPM. Le sessioni precedenti mantengono GLUE AUTO e UNMASK MS.

## Soft Tube e ascolti

ADDED ONLY attiva il generatore Tube e disattiva DELTA UNMASK. DELTA disattiva a sua volta ADDED ONLY; spegnere Tube esce dall'ascolto isolato. POWER deve essere acceso. Il display ADDED indica il livello RMS del residuo generato prima di UNMASK/trim. A livelli d'ingresso bassi l'effetto è volutamente sottile; per verificarlo, usare Drive 18 dB e Amount 100%, poi tornare a valori musicali. Amount zero produce residuo zero. Oversampling 4× fisso; nessuna modifica alla curva di saturazione della 0.2.3.

## PHASE Learn

Immettere basso nel canale principale e kick nel sidechain, poi LEARN. L'analisi richiede quattro secondi d'audio e ora prosegue attraverso i cicli brevi del trasporto. Il display mostra progresso e risultato. Un risultato affidabile imposta delay e polarità; PHASE rimane spento se era spento: accenderlo per ascoltare. Nessun risultato affidabile significa che le impostazioni vengono conservate, non che il plugin debba forzare un allineamento. All-pass resta manuale. Non è un ottimizzatore universale della qualità sonora.

## UNMASK: cosa aspettarsi

Il comportamento audio UNMASK rimane quello precedente. BUDGET 9 dB limita la somma dei tagli comandati alle bande: non garantisce un'attenuazione di 9 dB sulla fondamentale. Con toni continui kick/basso coincidenti, AGC spento, budget 9 dB: Depth 40% circa 1,3–1,4 dB, Depth 100% circa 3,2–3,6 dB nei casi 55/75/110/220 Hz misurati. Non sono valori garantiti su musica reale. La lettura BAND GR mostra il taglio più grande su una singola banda. Per confrontare la profondità reale disattivare Auto Gain, verificare KICK SC ACTIVE e alternare UNMASK ON/OFF a pari livello; DELTA permette di ascoltare quanto viene tolto.

Questa versione corregge monitoraggio, feedback e timing; non introduce un aumento nascosto della profondità di UNMASK. Le misure su segnali sintetici non sostituiscono ascolto, controllo CPU e stabilità nel Cubase dell'utente.
