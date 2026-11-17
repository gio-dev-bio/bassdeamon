# BassDeamon 0.2.2 — guida alla prova in Cubase

## Cosa cambia

- **POWER:** sfuma l'intera catena verso il segnale originale in 20 ms, poi raggiunge un bypass digitale esatto. Mantiene parametri e valori appresi; l'adattamento resta sospeso mentre POWER è spento. Il bypass dell'host resta diretto e può produrre un click se la forma d'onda elaborata e quella originale non coincidono nel punto di cambio.
- **PHASE / DELAY:** i cambi sfumano tra due ritardi fissi. Evitano lo scivolamento di pitch del ritardo mobile, ma durante il passaggio possono modificare brevemente ampiezza e risposta in frequenza. I comandi molto rapidi vengono accorpati fino alla fine della sfumatura in corso.
- **PHASE / POLARITY:** transizione con termine preciso a 20 ms. Passare tra due polarità opposte comporta comunque una cancellazione temporanea.
- **LEARN:** mantiene le impostazioni manuali quando il risultato è debole, ambiguo o cambia durante l'analisi. Il valore `r` è una correlazione, non un giudizio sulla qualità del mix.

## Installazione

Chiudi Cubase, estrai lo ZIP e sostituisci `BassDeamon.vst3` in `/Library/Audio/Plug-Ins/VST3/`. Segui `INSTALL.txt` incluso se macOS blocca questa build firmata ad-hoc. La versione è ancora di sviluppo, non notarizzata.

Inserisci il plugin sul **basso** e invia il **kick al sidechain**. PHASE parte spento. Il kick viene usato come riferimento e non viene sommato all'uscita.

## Significato dei messaggi Learn

| Messaggio | Significato e comportamento |
| --- | --- |
| MANUAL / READY | Puoi usare i controlli manuali o avviare un'analisi. |
| ANALYSING 4 s... | Suona una frase completa con kick e basso per quattro secondi. |
| LOCKED / r … | Il risultato supera i controlli ed è mantenuto fisso. PHASE non si attiva da solo: confronta acceso/spento. |
| WEAK / NO SIGNAL | Segnale insufficiente o correlazione debole; nessuna nuova correzione viene applicata. |
| AMBIGUOUS MATCH | Più ritardi/polarità risultano simili: rimangono i valori precedenti. Può succedere con segnali molto periodici. |
| MOVING MATCH | Le due metà della frase suggeriscono allineamenti differenti: rimangono i valori precedenti. |

Il Learn è intenzionalmente prudente. Un rifiuto non significa che il plugin sia guasto. Con kick e basso differenti o un sub molto puro, il controllo manuale e l'ascolto possono essere più appropriati.

## Prova consigliata

1. Apri una copia del progetto. Parti da PHASE spento e confronta POWER acceso/spento a volume simile.
2. Prova GLUE e UNMASK separatamente, con bassi corti e note sostenute. Ascolta anche DELTA per capire cosa UNMASK rimuove: questa funzione era già presente.
3. Attiva PHASE solo quando serve. Prova prima la polarità e poi piccoli ritardi; controlla il risultato su tutta la frase, non su un solo colpo.
4. Usa il Learn sul passaggio rappresentativo. Se rifiuta il risultato, conserva il controllo manuale.
5. Confronta la riproduzione con un export offline; salva, chiudi e riapri il progetto per verificare il richiamo. Prova anche più istanze e buffer piccoli sul tuo Mac Intel.

## Cosa resta da verificare con audio reale

Servono un WAV del **kick** e uno del **basso**, esportati separatamente, senza normalizzazione e con lo stesso punto di inizio; idealmente 20–30 secondi con note diverse e qualche pausa. Indica sample rate e BPM. Con questi file si possono misurare pumping, sovrapposizione spettrale, comportamento del Learn e differenze di livello su materiale rappresentativo.

Non sono stati forniti questi stem per la presente iterazione. I test sintetici e la validazione del plugin non sostituiscono l'ascolto nel progetto. Nessun limiter o oversampling è attivo.
