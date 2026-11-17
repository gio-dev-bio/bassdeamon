# BassDeamon 0.2.8 — macOS Intel

## Grafico PRE / POST

PRE e POST cambiano soltanto la vista del basso. Non commutano ascolti, bypass, gain o processori. La scelta adesso è uno stato grafico locale, salvato nella sessione senza inviare gesture o modifiche di parametri alla DAW. Il vecchio parametro host Spectrum signal rimane riservato per compatibilità: le nuove viste non lo usano per comandare l'audio.

Il grafico è più alto. Basso PRE chiaro / POST verde, sempre disegnato sopra il kick ambra tratteggiato. Le collisioni hanno una frequenza in Hz, un riferimento centrale sottile, un segmento rosso sulla curva e un intervallo preciso sulla corsia inferiore. Il riquadro dell'etichetta non indica la larghezza della collisione. Fino a quattro etichette sono mostrate contemporaneamente per evitare sovrapposizioni; il conteggio comprende tutte le regioni visibili nell'intervallo FROM / TO. BASS SILENT e KICK SC SILENT distinguono l'assenza delle sorgenti.

Il rilevatore conserva il metodo della 0.2.7: FFT Blackman-Harris lunga 16384 / breve 8192, confronto per canale e presenza temporale, eventi recenti per circa 0,3 secondi. Le frequenze riportate sono stime spettrali; il passo dei bin non è la risoluzione garantita tra due toni. È un indicatore di sovrapposizione significativa, non una certificazione del mascheramento uditivo.

## UNMASK: attenuazione in dB

Attiva **dB CONTROL**. La manopola grande **ATTENUATION** imposta liberamente 0–24 dB, anche digitando il valore. In questa modalità il valore comanda il guadagno al centro di una singola campana dinamica, al FOCUS; WIDTH regola la larghezza. AUTO FOCUS segue il segnale, oppure disattivalo e scegli MANUAL FOCUS. Attack resta in ms; solo Release segue le note con SYNC.

Il valore è la massima attenuazione al focus quando il detector è pienamente attivo. La riduzione effettiva dipende dalla presenza contemporanea di kick e basso e dai tempi Attack/Release, e compare come **GR**. Non è un attenuatore statico sempre acceso: senza kick non scava il basso. Fuori dal centro della campana la riduzione diminuisce.

La misura è **prima di AUTO GAIN**. Per valutare direttamente −6 / −12 dB disattiva AUTO GAIN del modulo. Se è acceso, il makeup può aumentare il livello generale e far sembrare più morbido il risultato. Il riquadro AGC indica il guadagno aggiunto: non è la gain reduction. DELTA ascolta il segnale rimosso prima di makeup e trim.

Con dB CONTROL spento resta il precedente algoritmo DEPTH in percentuale. Le sessioni fino alla 0.2.7 si aprono in quella modalità per conservarne il suono; accendi dB CONTROL per usare il nuovo controllo calibrato. I vecchi budget rimangono compatibili con le sessioni e le automazioni salvate, ma non limitano il nuovo controllo in dB. Le istanze nuove e i nuovi starting point usano dB CONTROL.

## Verifiche

Il test di calibrazione usa 40 / 55 / 75 / 110 / 220 Hz, 44,1 / 48 / 96 kHz e attenuazioni 0 / 3,5 / 6 / 12 / 18 / 24 dB, con Auto Gain spento, focus manuale e segnali stabili che attivano il detector. Controlla anche unità senza kick, valori finiti e assenza di allocazioni C++ nei callback. Non equivale a garantire la stessa attenuazione su ogni nota transitoria o con Auto Gain acceso.

Un null test del processor con moduli e Auto Gain attivi confronta l'uscita con 360 click PRE/POST contro un'istanza identica senza click: 184.320 campioni confrontati bit per bit. Verificati anche salvataggio della vista, richiamo di sessioni precedenti, undo/redo e rendering delle collisioni da campioni effettivi del processore. Risultati di calibrazione, suite e validazione VST3 sono inclusi nello ZIP.

## Installazione

Chiudi Cubase, sostituisci il bundle seguendo INSTALL.txt, riapri e controlla v0.2.8. Build macOS Intel x86_64, firma ad-hoc. Undo/redo, scope a un quarto, CPU del callback e tempo dell'analizzatore separati restano disponibili. Le viste PRE/POST non sono un confronto audio A/B: per quello usa i pulsanti A/B o il POWER interno.
