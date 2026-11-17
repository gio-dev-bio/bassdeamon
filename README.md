# BassDeamon 0.2.2

Novità: POWER termina su bypass digitale esatto dopo 20 ms; PHASE usa cambi di delay a due tap fissi e Learn rifiuta corrispondenze deboli, ambigue o instabili. Dettagli e compromessi in [RELEASE_0.2.2.md](docs/RELEASE_0.2.2.md).

VST3 originale per lavorare sulla relazione kick/basso. Inserire sul **basso** e inviare il **kick al sidechain**. Il kick non viene sommato all'uscita.

`Bass mono/stereo → GLUE → PHASE opzionale → UNMASK → Output trim`

## Novità della 0.2

- GUI JUCE nativa e ridimensionabile: console vintage, demone DJ vettoriale, manopole, VU GR, indicazione sidechain e livelli reali.
- Grafico a **12 bande** per l'energia di basso/kick e risposta EQ calcolata dai filtri correnti. Non è uno spettrogramma FFT.
- UNMASK con focus automatico della zona di sovrapposizione, isteresi e movimento lento; HOLD congela la frequenza mentre i tagli restano dinamici.
- Focus manuale 30–250 Hz, larghezza, attack 1–50 ms, release 30–500 ms, budget di taglio 3/6/9/12 dB e ascolto Delta.
- Confronto A/B completo, quattro starting point, bypass globale, tooltip e peak hold con indicazione di clipping.
- Stato v2: richiamo dei parametri congelati GLUE, AGC e focus; lettura dei vecchi stati v1.
- Firma ad-hoc applicata dopo la generazione del manifest; controllo firma e pluginval su ARM/Intel prima del packaging.

## Controlli e comportamento

| Modulo | Funzione |
|---|---|
| GLUE | Compressione RMS feed-forward stereo-linked, soft knee e parametri adattivi. Amount e Lock Adaptation. Il basso guida la dinamica; onset kick e BPM forniscono contesto al release. |
| PHASE | Off iniziale. Learn analizza 4 s, cerca polarità e ritardo su 21 candidati 0–5 ms e mantiene il risultato fisso. Polarità, ritardo causale Lagrange e all-pass manuale. |
| UNMASK | 12 rilevatori di sovrapposizione 30–370 Hz, focus 30–250 Hz, tagli minimum phase con pesi normalizzati e limite al budget comandato. |
| Auto Gain | Tre AGC indipendenti con Lock, misura energetica stereo, sospensione sul silenzio, ±3 dB per modulo e massimo +6 dB complessivi. Tempi lenti 5 s / 12 s per UNMASK. Off iniziale. |
| A/B | Due memorie di confronto mantenute finché l'istanza resta caricata, anche chiudendo la GUI. Nel progetto viene salvata l'impostazione attiva, non entrambe le memorie A/B. |
| Starting points | Clean Low End, Afro / Round & Steady, Punch / Make Room, Sub / Gentle Control. Non correggono automaticamente ogni mix e lasciano PHASE off. |

**Il focus automatico segue una risonanza condivisa: non è il riconoscimento della fondamentale o della nota.** La correlazione PHASE non garantisce una scelta musicalmente migliore. Confrontare sempre la frase intera. L'all-pass non ruota tutte le frequenze dello stesso angolo.

Latenza algoritmica dichiarata: **0 campioni**. Il ritardo PHASE è un offset musicale intenzionale. Nessun anticipo, lookahead, limiter o saturazione obbligatoria.

## Installazione macOS

Estrarre lo ZIP e sostituire `BassDeamon.vst3` in `/Library/Audio/Plug-Ins/VST3/` a Cubase chiuso. Istruzioni complete nel file `INSTALL.txt`. Questa build di sviluppo è firmata ad-hoc e non notarizzata da Apple; se Gatekeeper la blocca, seguire l'autorizzazione mirata indicata nel file.

## Build e verifiche

C++17, JUCE **9.0.5**, commit `d60b7a78030661574a615b586d9325ba7a7fa7dd`, CMake ≥3.22 e Xcode Command Line Tools.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release --parallel 3
ctest --test-dir build -C Release --output-on-failure
```

Output: `build/BassDeamon_artefacts/Release/VST3/BassDeamon.vst3`.
Per un checkout JUCE locale: `-DBASSDEAMON_JUCE_PATH=/percorso/JUCE`.
Dopo tutti i passaggi che modificano il bundle, firmare il bundle completo con `codesign --force --sign - --timestamp=none` e verificarlo con `codesign --verify --strict`.

```sh
cmake -S . -B build-core -DBASSDEAMON_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

La CI verifica DSP, sanitizers, stato/A-B, rendering della vera GUI, firma e pluginval strictness 5 sulle due architetture. Il PNG di anteprima usa segnali sintetici noti, elaborati dal motore reale. Non è una misura della qualità musicale.

## Limiti e lavoro audio ancora necessario

- Analisi a bande, senza FFT e senza pitch tracking; linear phase e oversampling non implementati.
- AGC RMS non ponderato: la trasparenza percettiva e lo spazio lasciato al kick richiedono ascolti.
- I cambi di delay PHASE sfumano tra due tap fissi per 20 ms: evitano il pitch glide del tap mobile, ma possono creare una breve variazione di ampiezza/comb filtering. Le variazioni rapide sono accorpate fino al termine del fade corrente.
- Stereo fuori fase può annullare il mono usato dal Learn: ignorare un risultato non affidabile.
- I nuovi filtri UNMASK possono cambiare il suono dei preset 0.1: confrontare su una copia del progetto.
- Firma ad-hoc, non Developer ID/notarizzazione. AU e standalone non distribuiti.
- Validazione finale su Cubase/macOS Sonoma dell'utente, misure di distorsione/transienti/CPU e ascolti su bassline reali ancora necessari.

## Documentazione

[Obiettivi di prodotto](docs/PRODUCT.md) · [Architettura](docs/ARCHITECTURE.md) · [Verifiche](docs/VALIDATION.md)

Nessun codice o asset KickUncover. Licenze JUCE/BassDeamon da definire prima della distribuzione commerciale; vedere la licenza del framework JUCE.


## Benchmark e ottimizzazioni 0.2.1

`tests/Benchmark.cpp` confronta il motore corrente con la reference congelata 0.2.0
(`tests/baselines/Dsp020.h`, commit e6d48b7). Esegue misure stereo a 44,1/48/96 kHz,
buffer 64/128/256/512, tre ripetizioni di 0,6 s e warmup di 30 blocchi. Riporta media,
p50/p95/p99/max, quota di un core e conteggio di `new/new[]` nel callback.
Questi tempi comprendono la schedulazione di un runner condiviso: non sono garanzie hard realtime o misure del Mac dell'utente.

```sh
c++ -std=c++17 -O3 -DNDEBUG -ISource tests/Benchmark.cpp -o dsp-benchmark
./dsp-benchmark bench-results
python3 benchmarks/analyze.py bench-results
```

L'analisi richiede numpy/scipy. Genera metriche THD+N su toni coerenti, silenzio,
code, automazioni e confronto numerico. I segnali variabili non sono interpretati
come toni stazionari; le modulazioni intenzionali di PHASE non sono chiamate distorsione inattesa.
La CI ripete il benchmark su macOS ARM e Intel **nativi**, separatamente dalla validazione VST3.

Ottimizzazioni: meno operazioni log/pow/trig ridondanti, all-pass con coefficienti
in cache, learner fermo quando non analizza, coefficienti EQ calcolati una sola
volta per i due canali e curva GUI con filtri precomputati.
La larghezza EQ viene smussata su 20 ms: è un cambiamento voluto durante le
automazioni, da confrontare separatamente dalle ottimizzazioni numericamente trasparenti.
