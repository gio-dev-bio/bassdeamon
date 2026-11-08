# BassDeamon 0.2.3 — modifiche locali da validare su Mac

Base: `gio-dev-bio/bassdeamon`, ramo `codex/first-dsp-prototype`, commit `73cacb42e61279d706cb1c1556ce06c00073f29a`.

Questo pacchetto contiene una patch e i file modificati. **Non contiene un VST3 compilato e non è una release pronta da installare.** La pubblicazione sul ramo di sviluppo privato è stata autorizzata esplicitamente ed eseguita. Il candidato audio 9c348324c082bcea11d21ab29148130739c2d738 ha superato build universale, test JUCE, sanitizers e pluginval ARM/Intel (run 42514115749), oltre ai segnali nativi su entrambe le architetture (run 42514115721). Questa revisione corregge la cattura grafica deterministica; il motore audio è invariato.

## Implementazione

- Soft Tube opzionale, inizialmente spento, prima di UNMASK: ramo parallelo sulle basse, curva tanh leggermente asimmetrica, Drive 0–18 dB, Amount, SOURCE LP 60–500 Hz, COLOUR, ON/OFF, Auto Gain e Gain Lock, ascolto del ramo aggiunto. Oversampling FIR fisso 4x nel solo ramo aggiunto. La curva non modella un circuito fisico.
- Il ramo originale non riceve latenza aggiuntiva. Il ramo aggiunto ha 12 campioni di ritardo dai filtri FIR, oltre alla fase dei filtri di selezione. La sua somma può cambiare livello/fase anche in basso: non si dichiara che il sub risultante rimanga identico. Il residuo può conservare energia alla fondamentale e prodotti di intermodulazione.
- PRE e POST misurati prima dell'intera catena e sull'uscita effettiva, incluso trim, POWER e modalità di ascolto. Kick come riferimento separato; il kick non viene sommato all'audio in uscita.
- FFT 4096 campioni, finestra Hann, potenze dei canali stereo mediate; visualizzazione 25 Hz–5 kHz con stessa scala. Non è il vecchio grafico di energia a 12 bande. Le frequenze alte sono solo analizzate: UNMASK continua a operare nelle sue bande basse.
- Rosso: euristica di possibile sovrapposizione, con soglia di energia e vicinanza dei livelli, identica per PRE e POST. Non rappresenta il taglio di UNMASK né certifica un mascheramento udibile. La misura usa una finestra temporale (85,3 ms a 48 kHz), non una coincidenza istantanea perfetta.
- Scope del canale sinistro e somma kick+basso esclusivamente visiva. I canali in opposizione non scompaiono dallo spettro, che usa potenze stereo.
- Trasporto a coda SPSC senza attese nel thread audio; memoria fissa di 196624 byte. FFT nel thread dell'interfaccia, nessuna FFT nel callback audio; acquisizione disattivata con editor chiuso. Se la coda si riempie vengono scartati dati di analisi, mai bloccato l'audio.
- Stato schema v3, lettura di v1/v2 con Soft Tube spento per gli stati precedenti. Identità VST invariata. M/S non implementato.

## Verifiche eseguite localmente

- Compilazione ed esecuzione dei test DSP e della suite estesa: **717 casi, zero fallimenti**.
- Confronti stereo con la 0.2.2 a 44,1/48/96 kHz e buffer 17/128/511: Soft Tube spento conserva i campioni in modo esatto nelle condizioni provate.
- Soft Tube attivo: indipendenza dalla partizione del buffer, isolamento dei canali, bypass assestato, POWER e commutazioni/parametri estremi. Nessuna allocazione C++ osservata nei callback misurati.
- Controlli numerici dell'analizzatore: stereo in opposizione di polarità, POST a +6,02 dB quando l'uscita raddoppia, silenzio senza rosso, coda piena e ordinamento produttore/consumatore su 200000 frame.
- Nuovi test sotto AddressSanitizer e UBSan: passati. LeakSanitizer disabilitato perché incompatibile con ptrace in questo ambiente; non è una verifica delle perdite di memoria dell'app completa.
- Controllo di compilazione `-fsyntax-only` di processore, editor e test JUCE con la revisione JUCE 9.0.5 fissata nel progetto: passato su Linux. Non equivale al collegamento di un'applicazione, all'esecuzione dei test JUCE o al rendering della GUI.
- Sinusoide runtime a 50 Hz: seconda armonica circa -41,02 dBc, terza circa -42,17 dBc alle impostazioni iniziali. Questa fixture non certifica la musicalità, il comportamento con tutti i bassi o l'assenza di aliasing.

I numerosi confronti per campione e per frame presenti nel log non sono altrettanti scenari indipendenti di test.

## Verifiche ancora da eseguire

- Build macOS universale, firma, test processore JUCE (incluso analizzatore acceso/spento), validazione pluginval e rendering dell'interfaccia.
- Misure CPU/RAM nel processo host, interfaccia aperta/chiusa e più istanze; benchmark Intel/ARM nativi. I tempi Linux allegati non stimano il carico del Mac dell'utente.
- Misure estese di aliasing/intermodulazione e transienti del nuovo Soft Tube, ascolto critico, prova su Cubase/Sonoma e materiale reale.

La patch aggiorna il workflow esistente e aggiunge test nativi; le modifiche restano nel ramo di sviluppo, senza merge in main. Per istruzioni e limiti aggiornati leggere GUIDA_0.2.3_IT.md.

## Applicazione

Dal repository al commit base indicato: `git apply --check BassDeamon-0.2.3-review.patch`, poi `git apply BassDeamon-0.2.3-review.patch` su un ramo di lavoro. Gli strumenti locali usati per costruire la patch non vengono inclusi. Il pacchetto non modifica da solo alcun progetto Cubase.
