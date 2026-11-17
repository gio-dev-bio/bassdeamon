# Verifiche della prima bozza

## Eseguite nell'ambiente di sviluppo Linux

- GCC 14, C++17; core DSP compilato con `-Wall -Wextra -Wpedantic -Wconversion -Werror`.
- Build CMake del core e CTest.
- Compilazione sintattica di processor, editor e test plugin contro le intestazioni JUCE 9.0.5.
- AddressSanitizer e UndefinedBehaviorSanitizer sul core. LeakSanitizer disabilitato localmente perché l'ambiente usa ptrace; nella CI Linux resta abilitato.

## Contratti coperti dai test

1. Bypass identico bit per bit; il kick da solo non produce uscita sul basso.
2. Sample rate 44,1/48/96/192 kHz; stabilità anche a 8 kHz.
3. Stesso audio con buffer da 1 e 511 campioni.
4. Rapporto L/R conservato con dinamica e filtri attivi.
5. GLUE comprime senza kick e senza BPM; Lock congela i parametri senza fermare il compressore.
6. Ripristino dei valori DSP congelati.
7. Auto Gain: limite, blocco, sospensione sul silenzio e bypass dopo la transizione.
8. UNMASK riduce un tono in conflitto a 75 Hz e preserva un tono a 1 kHz; senza kick è unity.
9. Delta + wet ricostruiscono il segnale in ingresso a UNMASK, entro la precisione float.
10. PHASE ritarda e inverte un impulso; all-pass a guadagno unitario a regime.
11. Learn recupera 2 ms e polarità invertita su un riferimento noto; poi resta fisso e rifiuta il silenzio.

## Verifiche predisposte nella CI macOS

- Build VST3 universale arm64/x86_64.
- Test integrazione JUCE: mono, stereo, sidechain, stato dei parametri.
- Packaging della build di sviluppo.

## Necessarie prima di una beta musicale

- Risposta/impulso e distorsione su più frequenze, livelli, note e impostazioni dinamiche.
- Ascolti con gain matching in Cubase su kick sincopati, code lunghe e bassline a più note.
- Learn su intere frasi e casi in cui la correlazione suggerisce una scelta musicalmente peggiore.
- CPU e dropout sul Mac mini Intel dell'utente e su Apple Silicon.
- Validatore VST3, automazione intensiva, seek/loop/export offline, caricamento/salvataggio in host.
- GUI: verifica visiva, accessibilità, ridimensionamento e design finale.

I test sintetici dimostrano proprietà specifiche del prototipo, non trasparenza audio universale.

## Estensione 0.2.0

Nuovi contratti DSP: focus automatico su risonanza nota, HOLD senza movimento,
richiamo del focus congelato e selettività del focus manuale su due toni distinti.
Test JUCE estesi ad A/B, HOLD e width. Rendering della GUI dal codice reale in CI,
con segnali di test sintetici noti, per controllare la leggibilità e le sovrapposizioni.
Il packaging esegue firma completa e pluginval strictness 5 per ARM/Intel.
La validazione finale in Cubase resta un passaggio separato.
