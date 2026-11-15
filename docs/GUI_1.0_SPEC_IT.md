# BassDeamon GUI 1.0

Decisioni approvate per la GUI e per il percorso audio della prossima versione. Gli esempi di valori nelle anteprime sono illustrativi. Questa specifica prevale sulle anteprime e sulle guide precedenti quando descrivono diversamente il layout o l'ordine dei moduli.

## Percorso audio

**Bass In → 01 Kick EQ → 02 Glue → 03 Phase → 04 Saturation → 05 Duck → 06 Unmask → Bass Out**

- Il kick entra dal sidechain come riferimento di analisi e controllo. La somma Kick + Bass appartiene allo Scope.
- Duck lavora sul basso già compresso e saturato. L'obiettivo è liberare attacco, punta e corpo del kick con un intervento naturale.
- Unmask lavora per ultimo sulle sovrapposizioni residue. I processori di uscita eventualmente attivi appartengono alla sezione Output.
- La riga **Signal Path** mostra l'ordine DSP. I pannelli principali sono disposti da sinistra a destra **Glue, Unmask, Duck**. I numeri dei moduli seguono l'ordine DSP.
- Verificare l'ordine eseguibile prima della build. Nel sorgente in lavorazione esaminato il guadagno Duck veniva applicato dopo Unmask.

## Vista principale

- Logo gotico **BASS ✝ DEAMON** in avorio con croce in rame e firma **Engineered by Guy_BioRg**. Etichette in inglese.
- **Skin** sceglie fra **Classic** e **Raw**. Il tema Raw riprende una console analogica vissuta, con selettori metallici e meter a lancetta intera, dello stesso stile per Glue, Unmask e Duck.
- Testata con preset, Power, Undo/Redo, A/B globale, CPU DSP, due barre **IN RMS / OUT RMS** in dBFS con scala e finestra di misura comuni. Il kick sidechain è escluso dalla misura RMS del basso.
- **Analyzer / Scope** condividono il display a tutta larghezza. Kick EQ e Phase in fasce compatte. Saturation nella fascia inferiore. Un solo indicatore generale di presenza del kick in sidechain.

## Analyzer e Scope

- **PRE / POST** cambia la sorgente della sola visualizzazione. Il kick di riferimento è lo stesso e il commutatore non cambia l'audio. Range in Hz liberamente regolabile, refresh configurabile con lettura stabile e Hold.
- Evidenziare solo le collisioni significative con marker stretti e frequenza leggibile. Mostrare in PRE la differenza **Bass − Kick** in dB. In POST calcolare la differenza sul basso processato, mostrare le collisioni residue e rendere visibile l'attenuazione dinamica di Unmask. Curva Kick EQ distinta e in legenda.
- Scope della somma finale **Kick + Bass post processo**, con Stereo e Mono selezionabili. Mono Check sul basso processato con avvisi colorati per gravità e lettura full-range e sub. Riferimento sub iniziale 120 Hz, modificabile.

## Controlli dei moduli

| Modulo | Vista principale | Dettagli |
|---|---|---|
| 01 Kick EQ | On/Off, Learn, frequenza e Cut | Learn una tantum dal kick in sidechain. Campana sottrattiva statica sul basso, Q fisso 3, taglio leggero con massimo indicativo 3–4 dB. Dry e Level Match disponibili. |
| 02 Glue | Auto/Manual, Amount, Attack, Release, Auto Gain e meter GR | Auto adatta i tempi. Auto Gain compensa il livello. Includere Lock Adaptation e mantenere accessibili i controlli timing. |
| 03 Phase | On/Off, A/B locale, Advanced | Learn + Apply, delay, polarità, all-pass, frequenza di riferimento della rotazione e indicazione dell'intervento. Se il Learn è incerto, mantenere le impostazioni. |
| 04 Saturation | On/Off accanto al numero, Drive, Mix, Delta, Oversampling e Auto Gain | Saturazione morbida. Delta consente l'ascolto della differenza introdotta dal modulo. |
| 05 Duck | On/Off, Amount, meter GR, Advanced | Duck guidato dal kick. Controlli e analisi Click/Punch/Body previsti in Advanced e nella proposta Auto Mix. |
| 06 Unmask | On/Off, attenuazione impostabile in dB, Width, Auto Focus, Delta, meter GR, Advanced | Focus manuale/automatico, Stereo/Mid/Side e tempi. Auto Gain e Auto Focus attivi di default dove previsti. |

**Convenzione per i tempi**. In Glue, Duck e Unmask l'Attack è sempre in millisecondi. Solo Release/Decay può passare da MS a SYNC, con divisione musicale e tempo del progetto. In Glue il selettore dei tempi sta nella vista principale. In Duck e Unmask sta in Advanced.

## Auto Mix

- Pulsante con icona del demone. Analisi iniziale una tantum su 4, 8 o 16 battute, con 8 come valore iniziale, per preparare la proposta. Da includere anche i parametri Duck.
- A/B a volume pareggiato, con riferimento kick invariato. Apply conferma la proposta in un'operazione Undo. Con dati insufficienti, mantenere le impostazioni interessate e indicarne il motivo.
- Referto di compatibilità mono sul basso, full-range e sub, con frequenza sub configurabile e avvisi per gravità. Il referto è indipendente dal confronto A/B.
- Per il flusso dettagliato di cattura, ascolto e applicazione vedere [Auto Mix](AUTO_MIX_SPEC_IT.md).

## Verifica prima della build

Controllare ordine DSP, Learn Kick EQ e Phase a 44,1 kHz con loop reali di kick e basso, risposta con arpeggi, confronto a volume pareggiato, collisioni residue PRE/POST, Scope Stereo/Mono, timing in Sync, Delta e Oversampling, Undo/Redo, CPU e latenza riportata a Cubase. Il mockup è il riferimento grafico. La verifica DSP richiede audio e misurazioni.
