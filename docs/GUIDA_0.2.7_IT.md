# BassDeamon 0.2.7 — macOS Intel

## Collisioni PRE e POST

PRE confronta il basso prima della catena con il kick sidechain; POST confronta l'uscita realmente elaborata con lo stesso kick. Seleziona PRE / POST sopra il grafico. Il rosso segue soltanto i picchi spettrali condivisi; una corsia rossa di 5 pixel sul fondo aiuta a leggere anche frequenze strette. CLASH indica il numero di regioni rilevate e il testo sotto il grafico indica la frequenza del picco più forte. FROM / TO restano valori liberi in Hz, digitabili e trascinabili. Il conteggio riguarda l'intero spettro analizzato, anche fuori dalla finestra visualizzata.

Il rilevatore è stato rivisto per i kick brevi: analizza finestre a passi fissi di circa 10 ms durante il consumo dei campioni, anziché controllare soltanto l'ultimo istante del refresh grafico. Non scarta più le collisioni perché kick e basso hanno livelli diversi. Le armoniche deboli possono essere rilevate indipendentemente dalla fondamentale: il test con Soft Tube acceso genera realmente la componente condivisa a 100 Hz, assente nel PRE e presente nel POST.

La conferma combina picchi della FFT lunga e presenza temporale nella FFT breve. Usa finestre Blackman-Harris a quattro termini per limitare i lobi laterali, confronto nello stesso canale stereo e campioni realmente contemporanei. Non somma L/R prima di analizzare: un basso in controfase non sparisce. Picchi sotto -110 dBFS o oltre 60 dB sotto il massimo del proprio segnale vengono esclusi; il supporto sfuma nei 12 dB sotto ciascun picco locale. La finestra breve fornisce evidenza temporale anche quando una parziale debole non è separabile dalla fondamentale; la FFT lunga deve comunque confermare un picco condiviso. I casi con sorgenti alternate, canali separati, sidechain assente e residui molto deboli sono verificati automaticamente.

Il rosso conserva brevemente gli eventi recenti (circa 0,3 secondi), per rendere leggibili i transienti; non è un'indicazione campione per campione. Un cambio di nota senza supporto condiviso corrente cancella il vecchio evento. Ogni aggiornamento consuma soltanto i campioni già presenti al suo avvio: un render offline che produce continuamente nuovi campioni non può bloccare l’interfaccia inseguendo la coda. Le interruzioni della coda azzerano la FFT; se non arrivano callback per oltre 500 ms, il grafico viene svuotato. Un sidechain silenzioso viene indicato come KICK SC SILENT.

FFT lunga: 16384 campioni; passo 2,93 Hz a 48 kHz, 5,86 Hz a 96 kHz; finestra 341/171 ms. FFT breve: 8192 campioni, 171/85 ms. Il passo dei bin non equivale alla capacità di separare due sinusoidi vicinissime. All'apertura servono 16384 campioni prima di mostrare collisioni; compare COLLECTING AUDIO. Le due curve e il nastro non rappresentano bande EQ fisse.

Il rilevatore misura sovrapposizione spettrale significativa e contemporanea: è un aiuto al mix, non una certificazione di mascheramento uditivo. Il mascheramento percepito dipende anche dal contenuto musicale e dall'ascolto. Abbassare l'intero POST non implica che le frequenze condivise siano scomparse: confrontare anche la curva e ascoltare. Il detector non modifica il DSP audio di UNMASK.

## Undo e redo

UNDO annulla l'ultima modifica ai parametri; REDO la ripristina. Una rotazione continua è una modifica unica, anche se produce molti valori intermedi. I starting point e le azioni di ascolto che cambiano più parametri sono raggruppati. Una nuova modifica dopo UNDO elimina il ramo REDO. I pulsanti si disabilitano quando la relativa cronologia è vuota. Il risultato applicato da Learn è annullabile.

La cronologia resta nell'istanza quando chiudi e riapri l'editor. Viene azzerata quando richiami uno stato di sessione o cambi A/B: non attraversa stati caricati e non viene salvata nel progetto. Riguarda i parametri, non la memoria interna adattiva dei processori. L'automazione resta gestita dall'host; per annullare registrazioni o operazioni della DAW usa l'undo di Cubase.

## Misurazione del carico

DSP CPU mostra la durata misurata del callback audio divisa per la durata del blocco attuale, con media smussata. Esempio: 1 ms di lavoro su un blocco che dura 10 ms = 10%. Include il DSP, i meter e la scrittura dei campioni nella coda dell'analizzatore. Non è la percentuale CPU totale del Mac e non è direttamente confrontabile con la barra globale di Cubase.

ANALYZER mostra separatamente i millisecondi medi impiegati per l'analisi sul thread dell'interfaccia. OVR conta i callback che hanno superato il proprio budget temporale; non equivale al conteggio dei dropout hardware. Il contatore viene azzerato quando il plugin viene preparato nuovamente. Il timer del callback non alloca memoria; i test controllano le allocazioni audio anche con analizzatore abilitato.

## Verifiche incluse

La matrice automatica usa 44,1 / 48 / 96 kHz, aggiornamenti GUI a 17 / 33 / 90 ms, livelli del basso a -12 / -30 / -48 dBFS e kick da -30 a +10 dB rispetto al basso. Comprende transienti di 20 e 70 ms, armoniche, kick con discesa di frequenza e cinque profili negativi. I CSV e i risultati generati dalla build sono inclusi nello ZIP. I test dell'interfaccia fanno passare audio vero nel processor e verificano i pixel rossi PRE e POST, oltre a gesture, preset, azzeramento della cronologia e calibrazione del misuratore CPU.

La validazione automatica include routing mono/stereo e sidechain, stato e migrazione, bypass, assenza di allocazioni C++ nel callback, DSP e segnali, firma ad-hoc e caricamento del VST3 Intel in pluginval. Non equivale a una prova dentro Cubase sul tuo Mac.

## Installazione e controlli precedenti

Chiudi Cubase e segui INSTALL.txt per sostituire il bundle con la stessa identità VST3. Build Intel x86_64, firma ad-hoc, non notarizzata. Attiva il sidechain del plugin e inviagli il kick. Se non arriva kick, non può esserci un confronto.

Attack resta in millisecondi; in SYNC soltanto Release segue le note e i BPM. GLUE AUTO conserva il comportamento adattivo. Lo scope cattura un quarto musicale completo, HOLD ferma la vista, SUM K+B è solo visivo. Soft Tube conserva l'oversampling fisso 4×. Learn richiede quattro secondi di audio e attraversa i loop brevi; PHASE resta spento se era spento. Nessuna modifica nascosta alla profondità di UNMASK.
