# BassDeamon

Plugin audio per rendere il basso stabile e musicale in relazione al kick.

## Stato

Specifiche di prodotto iniziali. Per lo stato corrente della bozza DSP/VST3 consultare il README nella radice.
La qualità audio sarà verificata attraverso misure e confronti di ascolto.

## Destinazione

- macOS Intel e Apple Silicon.
- VST3 per Cubase; AU come possibile formato aggiuntivo.
- Implementazione originale in C++ con JUCE e CMake; versione e licenze delle dipendenze da fissare prima dell'integrazione.
- Inserimento sul basso; kick ricevuto esclusivamente come riferimento sidechain.
- Il plugin elabora il basso, non la traccia del kick.

## Catena prevista

GLUE → PHASE opzionale → UNMASK → OUTPUT

Ogni modulo ha bypass e Auto Gain Compensation indipendenti.

### GLUE

Compressione del basso per uniformare le note senza cancellarne articolazione e accenti.
Il basso guida il rilevatore; kick e tempo del progetto forniscono il contesto ritmico.
Attack, release, threshold, ratio e knee automatici, con Amount e blocco dell'adattamento.
Non deve diventare un ducking comandato da ogni colpo di kick.

### PHASE

Controllo opzionale di polarità, ritardo e rotazione della fase.
Disattivato per impostazione iniziale. Analisi e confronto con il kick di riferimento.
Nessuna promessa di allineamento universalmente corretto per tutte le note.
Le modifiche devono essere esplicite, stabilizzabili e verificabili all'ascolto.

### UNMASK

Attenuazione dinamica selettiva del basso nelle zone di conflitto con il kick.
Analisi del basso dopo GLUE e PHASE.
Tracking stabile della fondamentale, possibilità di blocco e ascolto Delta.
Minimum phase come candidato iniziale e linear phase come alternativa da confrontare.

### QUALITÀ

Oversampling Off / 2× / 4× / 8× come candidati da misurare, non come garanzia di qualità.
Latenza comunicata all'host e compensazione interna dei percorsi.
Nessuna saturazione o limitazione obbligatoria.

## Documentazione

- [Architettura e criteri di verifica](ARCHITECTURE.md)

## Sviluppo

1. Prototipo DSP misurabile e strumenti di verifica.
2. Integrazione JUCE/VST3, sidechain, trasporto e salvataggio dei parametri.
3. GUI originale semplice, ispirata all'immediatezza del riferimento KickUncover.
4. Confronti su materiale musicale, verifiche in Cubase e build macOS universale.

Il codice, il marchio e le risorse grafiche di KickUncover non fanno parte del progetto.
