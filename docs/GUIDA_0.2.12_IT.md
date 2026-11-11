# BassDeamon 0.2.12 — macOS Intel

## KICK EQ

Premi LEARN mentre suona il kick nel sidechain. Il plugin raccoglie circa quattro secondi di segnale e cerca una fondamentale stabile tra 30 e 130 Hz in più colpi. Se la trova, attiva e blocca una campana statica sul basso prima di GLUE. Se appare NO RELIABLE PEAK, l'impostazione precedente resta attiva. FREQUENCY permette la correzione manuale. CUT parte da −1,5 dB, con un massimo di −4 dB e Q fisso a 3. Il centro e il taglio vengono salvati con la sessione. Le sessioni precedenti aprono il nuovo modulo spento.

## Analizzatore PRE e POST

Le viste PRE e POST mostrano un solo spettro a tutta larghezza. Il kick è ambra, il basso blu e la linea chiara mostra la risposta del KICK EQ quando è attivo. La linea del filtro usa una scala di guadagno separata. Le collisioni sono punti sulla curva, con al massimo tre letture principali sotto il grafico. Le letture POST comprendono la variazione rispetto al PRE e possono conservare un punto neutro per una collisione sotto soglia.

RATE aggiorna le curve, le letture e lo scope a 30, 15, 8, 4, 2 o 1 fotogrammi al secondo. La cattura continua tra i fotogrammi, così anche i kick brevi possono essere rappresentati. HOLD continua a bloccare lo scope.

PHASE LEARN + APPLY abilita PHASE solo dopo un risultato affidabile. I due LEARN distinguono il sidechain assente da un risultato incerto, che conserva i parametri correnti.

## LEVEL MATCH e DRY

Attiva LEVEL MATCH e lascia suonare il loop per qualche secondo. Il guadagno del basso trattato si adegua al livello RMS del basso originale entro ±12 dB. Usa DRY per alternare i due segnali, mentre la catena continua a elaborare in background. I controlli di ascolto sono temporanei. Il grafico POST segue l'uscita che stai ascoltando.

## UNMASK Stereo / Mid / Side

STEREO conserva la risposta precedente. MID attenua la componente comune di sinistra e destra, SIDE quella di differenza. Il riferimento kick rimane stereo linked, anche quando il kick è mono. Su un ingresso basso mono, MID equivale a STEREO e SIDE non interviene. Auto Gain di UNMASK agisce sulla componente selezionata. Gli altri moduli e il trim continuano a lavorare come prima. Le sessioni precedenti richiamano STEREO.

I due valori FOCUS Hz delimitano la ricerca di AUTO FOCUS, indipendentemente dall'intervallo visualizzato nel grafico. Il range iniziale è 30–250 Hz. I limiti riguardano il centro di intervento, mentre WIDTH continua a regolare la larghezza della campana. HOLD FOCUS mantiene il centro corrente. Con AUTO FOCUS spento si usa MANUAL FOCUS.

## POST: indice residuo rispetto a PRE

In precedenza PRE e POST venivano normalizzati separatamente: una componente ancora presente poteva apparire con la stessa intensità rossa anche dopo una forte attenuazione. Ora il colore POST è ponderato con il rapporto di ampiezza misurato POST/PRE alla medesima frequenza e nel medesimo canale. Non legge il valore di ATTENUATION o il meter GR: usa i campioni effettivi dell'uscita, includendo Auto Gain, trim e tutti i processori.

A parità di evidenza spettrale, −6 dB effettivi danno un indice residuo circa 0,50, −12 dB circa 0,25 e −20 dB circa 0,10. Il rosso viene mostrato per indici almeno 0,12: un taglio reale di 20 dB in questi casi scende sotto soglia. Se Auto Gain o trim ripristinano il livello, la segnalazione resta coerente con l'uscita realmente misurata. La riga POST mostra anche la variazione rispetto al PRE sulla principale regione condivisa.

Le nuove armoniche condivise che non esistevano nel PRE vengono comunque rilevate nel POST. PRE resta un riferimento indipendente: non perde il rosso quando UNMASK attenua l'uscita. La finestra FFT e la frequenza di visualizzazione introducono un breve ritardo nella lettura; non ci si deve aspettare il cambio dei marcatori nello stesso campione del movimento di un knob.

Questo è un indice comparativo della sovrapposizione residua, non una misura assoluta del mascheramento uditivo. L'assenza di rosso non certifica che due suoni siano sempre distinguibili all'ascolto.

## Letture spettrali

PRE e POST cambiano soltanto la vista del basso. Non commutano ascolto, bypass, gain o processori. La scelta viene salvata nella sessione senza modifiche ai parametri audio. Le vecchie viste BOTH e SPLIT si aprono in POST.

FROM e TO permettono di zoomare sulle basse frequenze. Il punto indica una frequenza stimata, non la larghezza del taglio. BASS − KICK e POST − PRE confrontano livelli della stessa finestra. UNMASK GR indica la riduzione del modulo. Il grafico usa le finestre FFT lunga e breve, la conferma temporale e il confronto per canale già presenti.

## UNMASK: attenuazione in dB

Attiva **dB CONTROL**. La manopola grande **ATTENUATION** imposta liberamente 0–24 dB, anche digitando il valore. In questa modalità il valore comanda il guadagno al centro di una singola campana dinamica, al FOCUS; WIDTH regola la larghezza. AUTO FOCUS segue il segnale, oppure disattivalo e scegli MANUAL FOCUS. Attack resta in ms; solo Release segue le note con SYNC.

Il valore è la massima attenuazione al focus quando il detector è pienamente attivo. La riduzione effettiva dipende dalla presenza contemporanea di kick e basso e dai tempi Attack/Release, e compare come **GR**. Non è un attenuatore statico sempre acceso: senza kick non scava il basso. Fuori dal centro della campana la riduzione diminuisce.

La misura è **prima di AUTO GAIN**. Per valutare direttamente −6 / −12 dB disattiva AUTO GAIN del modulo. Se è acceso, il makeup può aumentare il livello generale e far sembrare più morbido il risultato. Il riquadro AGC indica il guadagno aggiunto: non è la gain reduction. DELTA ascolta il segnale rimosso prima di makeup e trim.

Con dB CONTROL spento resta il precedente algoritmo DEPTH in percentuale. Le sessioni fino alla 0.2.7 si aprono in quella modalità per conservarne il suono; accendi dB CONTROL per usare il nuovo controllo calibrato. I vecchi budget rimangono compatibili con le sessioni e le automazioni salvate, ma non limitano il nuovo controllo in dB. Le istanze nuove e i nuovi starting point usano dB CONTROL.

## Verifiche

Il test di calibrazione usa 40 / 55 / 75 / 110 / 220 Hz, 44,1 / 48 / 96 kHz e attenuazioni 0 / 3,5 / 6 / 12 / 18 / 24 dB, con Auto Gain spento, focus manuale e segnali stabili che attivano il detector. Controlla anche unità senza kick, valori finiti e assenza di allocazioni C++ nei callback. Non equivale a garantire la stessa attenuazione su ogni nota transitoria o con Auto Gain acceso.

Un null test del processor con moduli e Auto Gain attivi confronta l'uscita con cambi RATE e 360 click PRE/POST contro un'istanza identica senza click: 184.320 campioni confrontati bit per bit. Verificati anche salvataggio della vista, richiamo di sessioni precedenti, undo/redo e rendering delle collisioni da campioni effettivi del processore. Risultati di calibrazione, suite e validazione VST3 sono inclusi nello ZIP.

## Installazione

Chiudi Cubase, sostituisci il bundle seguendo INSTALL.txt, riapri e controlla v0.2.12. Build macOS Intel x86_64, firma ad-hoc. Undo/redo, scope a un quarto, CPU del callback e tempo dell'analizzatore separati restano disponibili. Le viste PRE/POST non sono un confronto audio A/B: per quello usa i pulsanti A/B o il POWER interno.

## Test aggiuntivi 0.2.10

60 combinazioni con UNMASK reale: 44,1/48/96 kHz, attenuazioni 0/6/12/20/24 dB e quattro velocità. Verificato il rapporto residuo POST, il mantenimento del PRE, gli impulsi di 20 ms, la frequenza di aggiornamento effettiva e la scadenza dei vecchi marcatori. Un caso con taglio 12 dB compensato da trim +12 dB deve restare rosso, per escludere un'implementazione che spenga il rosso leggendo soltanto il knob. La matrice precedente verifica ancora le armoniche generate realmente dal Tube. Il test GUI usa audio elaborato a −20 dB e 4 fps: PRE rosso, POST sotto soglia dopo l'assestamento.
