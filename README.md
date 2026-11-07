# BassDeamon 0.1.0 — prima bozza DSP/VST3

Plugin audio originale per uniformare il basso e gestirne la relazione con il kick.
**Prototipo da misurare e ascoltare: non una release audio definitiva.**

## Routing

Inserire sul **basso**, inviare il **kick al sidechain esterno**.

`Bass mono/stereo → GLUE → PHASE opzionale → UNMASK → Output`

Il kick è solo un riferimento e non viene mai sommato all'uscita.
Stereo linking dei rilevatori e dei guadagni; calcolo interno DSP in double.

## Implementato nella bozza

| Modulo | Comportamento attuale |
|---|---|
| GLUE | Compressore RMS feed-forward, soft knee, soglia/ratio/attack/release adattivi, Amount e blocco dei parametri. Il basso guida la compressione; onset kick e BPM contribuiscono al release. |
| PHASE | Bypass iniziale, polarità, ritardo causale 0–5 ms con interpolazione Lagrange di terzo ordine, all-pass del primo ordine opzionale. |
| Learn + Lock | Quattro secondi di analisi low-pass; ricerca di polarità e ritardo su 21 candidati (passo 0,25 ms). A fine analisi il risultato resta fisso e modificabile. Segnale insufficiente: nessuna modifica. |
| UNMASK | Sei rilevatori di sovrapposizione a 40/55/75/105/145/200 Hz; tagli parametrici minimum phase smussati, massimo 4 dB per banda a Amount 100%. Delta prima di AGC/trim. |
| Auto Gain | Tre compensazioni indipendenti con on/off e Lock, misura energetica stereo, sospensione sul silenzio, limite ±3 dB per modulo e +6 dB complessivi. Adattamento lento (5 s; 12 s per UNMASK). Off iniziale. |
| Stato | Parametri con ID stabili, schema versionato, salvataggio dei parametri GLUE congelati e dei guadagni AGC appresi. |
| Interfaccia | Console di controllo provvisoria, colori vintage, meter GR/AGC/peak. Grafica finale con demone DJ ancora da realizzare. |

La correlazione Learn è una **euristica**, non la garanzia di un mix migliore.
Analizza la frase dopo GLUE; non apprende la rotazione all-pass, che resta manuale.
Non abilita automaticamente PHASE: confrontare il risultato con ACTIVE.
Il ritardo è intenzionale: latenza algoritmica dichiarata **0 campioni**; nessun anticipo o lookahead.
Il controllo all-pass cambia fase in funzione della frequenza, non di un angolo costante.

## Build macOS (Intel e Apple Silicon)

Richiede Xcode con Command Line Tools, CMake ≥3.22 e accesso GitHub per scaricare JUCE.
JUCE **9.0.5**, fissato al commit `d60b7a78030661574a615b586d9325ba7a7fa7dd`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release --parallel 3
ctest --test-dir build -C Release --output-on-failure
```

Output: `build/BassDeamon_artefacts/Release/VST3/BassDeamon.vst3`.
Per un checkout JUCE locale: `-DBASSDEAMON_JUCE_PATH=/percorso/JUCE`.
La build di sviluppo non include firma Developer ID, notarizzazione o installer.
AU e standalone non sono inclusi in questa versione.

La workflow GitHub Actions compila il VST3 universale e pubblica uno ZIP negli artifact
**solo dopo il superamento della build e dei test**. Non equivale alla validazione in Cubase.

## Test DSP senza JUCE

```sh
cmake -S . -B build-core -DBASSDEAMON_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

Oppure: `g++ -std=c++17 -O2 -ISource tests/DspTests.cpp -o dsp_tests`.

I test verificano identità a moduli bypassati, isolamento sidechain, stereo linking,
indipendenza dal buffer, compressione senza kick/BPM, AGC e lock, selettività UNMASK,
ricostruzione Delta, ritardo/polarità/all-pass, Learn e stabilità a sample rate diversi.
Il target JUCE `plugin_tests` verifica layout, routing e roundtrip dello stato.

## Limiti concreti della 0.1

- UNMASK usa bande fisse: tracking della fondamentale e Lock frequenza non sono implementati.
- GLUE è un primo modello adattivo, da tarare per preservare accenti e articolazione su bassline reali.
- Learn confronta correlazione low-end, non loudness della somma, salienza del kick o qualità percettiva.
- Il mono usato dal Learn può annullarsi su stereo fuori fase: in quel caso ignorare il risultato o usare materiale coerente.
- La transizione fra ritardi usa smoothing e può produrre un breve cambio di pitch; usare Learn su loop di prova.
- La misura AGC è RMS non ponderata. La metrica percettiva e l'effetto sullo spazio lasciato al kick devono essere validati.
- Nessun limiter: monitorare il peak meter e lasciare headroom.
- Oversampling e linear phase non sono esposti finché non esistono implementazione e misure a sostegno.
- CPU, artefatti, transienti, validazione VST3 e prove in Cubase/macOS sono ancora da completare.

## Documentazione e dipendenze

- [Obiettivi di prodotto originali](docs/PRODUCT.md)
- [Architettura e criteri di qualità](docs/ARCHITECTURE.md)
- [Verifiche di questa bozza](docs/VALIDATION.md)
- [JUCE 9.0.5](https://github.com/juce-framework/JUCE/tree/9.0.5) e relativa licenza nel repository JUCE; nessun codice o asset KickUncover.

La scelta della licenza di distribuzione BassDeamon/JUCE resta da definire prima di distribuire il prodotto.
