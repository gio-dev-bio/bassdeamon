# Verifiche candidato 0.2.4

Stato: sorgenti aggiornati; nessun nuovo VST3 macOS disponibile o dichiarato pronto.

Verifiche locali Linux con g++ C++17:

- Contratti DSP completi: passati (bypass, fase, AGC, unmasking, partizionamento, silenzio, automazione).
- Segnali estesi: 717 casi, zero errori.
- NewFeatureTests: 446404 controlli passati; zero allocazioni C++ nei callback misurati. Coda grafici 262160 byte; questo non misura tutta la RAM del plugin.
- UsabilityTests: conversione note/BPM, cambi tempo, fallback 120 BPM, scope 1/4 a 44,1/48/96 kHz e 60/120/173 BPM, HOLD e conservazione dei transienti passati.
- Learn con continuità dell'analisi attraverso reset della storia audio: recuperati 2 ms e polarità invertita su segnale noto; analisi completata.
- Tube ADDED ONLY: residuo presente, ricostruzione wet = dry + added con errore circa -149,6 dB relativo nel test sintetico. Curva Tube invariata rispetto alla 0.2.3.
- UNMASK: AGC spento, toni coincidenti 55/75/110/220 Hz, budget 9 dB, focus manuale. Depth 40%: attenuazione 1,29–1,44 dB. Depth 100%: 3,22–3,60 dB. Confermata la risposta delicata; non è stata aumentata di nascosto.
- UsabilityTests anche con AddressSanitizer/UBSan: passati, leak detector disabilitato nel container. Nessuna affermazione di verifica completa delle perdite di memoria.

La compilazione GUI, GuiInteractionTests, acquisizione screenshot e validazione VST3 della 0.2.4 sono ancora da eseguire. I test GUI verificano pulsanti/attachment reali, audio di ADDED ONLY, tempo host e Learn con cicli di trasporto di 250 ms; la loro presenza nel sorgente non equivale a un esito positivo.

GitHub Actions, commit 53727132308fb71fb838e67b9f9ee4a7ada05a44:

- Build: https://github.com/gio-dev-bio/bassdeamon/actions/runs/42524898868
- Segnali/bypass: https://github.com/gio-dev-bio/bassdeamon/actions/runs/42524898850

Entrambi terminati in failure prima dell'avvio degli step, senza log di compilazione disponibili. La causa precisa non è esposta dalle risposte accessibili: serve il messaggio Annotations del run. Non è stato attribuito il problema al codice o a quote/fatturazione senza evidenza.

Il VST3 0.2.3 precedentemente distribuito non contiene queste modifiche. Nessun merge in main.
