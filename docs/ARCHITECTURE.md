# Architettura preliminare

Questo documento specifica il comportamento desiderato, non funzionalità già implementate.

## Routing

Ingresso basso mono/stereo → GLUE → PHASE opzionale → UNMASK → trim di uscita.
Ingresso sidechain kick mono/stereo → analisi ritmica e spettrale.
Il kick non viene sommato all'uscita.
I due canali stereo del basso condividono il controllo dinamico per preservare l'immagine stereo.

## GLUE automatico

- Analizzare energia media, picchi e variabilità del basso.
- Usare onset e intervalli reali del kick insieme al BPM dell'host.
- Adattare threshold, ratio, knee, attack e release con limiti e transizioni regolari.
- Non ricavare tutta la temporizzazione dal solo BPM.
- Preservare gli accenti; Amount stabilisce l'intensità desiderata.
- Separare il blocco dell'adattamento dei parametri dal normale movimento dell'inviluppo.
- Senza sidechain: compressione basata sul basso e sul tempo host.
- Senza BPM valido: temporizzazione in millisecondi basata sull'audio, senza inventare un tempo.
- La compressione non garantisce un tetto assoluto ai picchi.
- Lookahead eventuale da validare su transienti, distorsione e latenza.

## PHASE

- Bypass iniziale.
- Polarità, ritardo fine e rotazione mediante filtri all-pass come controlli distinti.
- Un anticipo rispetto all'ingresso richiede un budget di latenza: nessuna lettura di campioni futuri.
- Latenza algoritmica e ritardo intenzionale sono grandezze distinte; la compensazione host non deve annullare il ritardo musicale scelto.
- Un angolo fisso a tutte le frequenze non è una promessa del controllo all-pass.
- Non inseguire automaticamente ogni nota.
- Un suggerimento automatico è un'estensione successiva, con confidenza e possibilità di blocco.
- Valutare la somma con il kick sull'intera frase, non solo su un singolo colpo.

## UNMASK

- Analizzare il basso al proprio ingresso e il kick temporalmente corrispondente.
- Applicare tagli limitati al conflitto effettivo.
- Stabilizzare frequenza e guadagno per evitare salti e modulazioni eccessive.
- Rendere disponibile il Delta prima della compensazione di livello.
- Confrontare minimum phase e linear phase con lo stesso obiettivo di attenuazione.
- Linear phase implica latenza e possibili artefatti di ringing; per un filtro variabile nel tempo non promettere invarianza assoluta della fase.
- Verificare ordine dei moduli e comportamento su note diverse.

## Auto Gain Compensation per modulo

Ogni modulo dispone di:
- attivazione indipendente;
- misura input/output su finestre temporali corrispondenti;
- guadagno applicato visibile;
- blocco del solo guadagno di compensazione;
- limiti di boost/cut e velocità di adattamento;
- sospensione dell'apprendimento durante silenzio o segnale insufficiente.

Principi:
- La misura è stereo-linked e riferita al basso, non alla somma kick/basso.
- La metrica finale (energia, ponderazione percettiva o combinazione) va scelta con prove sul low-end.
- Nessun recupero istantaneo della gain reduction.
- L'AGC di UNMASK deve preservare lo spazio creato: apprendimento lento su una frase rappresentativa e possibilità di blocco.
- Il bypass del modulo esclude anche la sua compensazione mantenendo la latenza appropriata.
- La misura del modulo successivo include il guadagno applicato dal precedente.
- Prevedere limiti complessivi contro l'accumulo dei boost.
- PHASE deve poter mantenere guadagno unitario; niente normalizzazione dei picchi che annulli l'effetto cercato.
- Il confronto globale a volume equivalente è distinto dai tre AGC.
- Nessuna compensazione garantisce assenza di clipping; mantenere headroom e indicazione dei picchi.

## Qualità e prestazioni

- Oversampling selezionabile soltanto dopo verifica del beneficio e implementazione corretta.
- Trattare coerentemente il ritardo del sidechain e del percorso audio.
- Compensare dry/wet e bypass internamente.
- Cambi di qualità con transizioni sicure e latenza correttamente riportata all'host.
- Non cambiare silenziosamente l'algoritmo durante l'export.
- Nessuna allocazione, I/O, rete o lock bloccante nel callback audio.
- Gestire denormal, silenzio, reset, seek e cambi di sample rate.
- Preset e automazioni con identificatori stabili; salvataggio dello stato versionato.

## Criteri di verifica prima di dichiarare la qualità

1. Identità a moduli disattivati, compensando il ritardo dichiarato.
2. Risposta in frequenza, fase e impulso con filtri statici.
3. Ringing, transienti e distorsione con filtri dinamici.
4. Comportamento su sinusoidi basse, note sostenute e transienti.
5. Indipendenza dal buffer size e temporizzazione coerente.
6. AGC: silenzi, ripartenze, segnali bassissimi, cambi di livello e boost cumulativo.
7. Sidechain assente, kick sincopato, cambi di BPM e trasporto.
8. Ascolti a volume equivalente su bassline reali con note e accenti diversi.
9. CPU e latenza misurate sul Mac Intel dell'utente e su Apple Silicon.
10. Validazione VST3 e prove reali in Cubase prima della distribuzione.

## Da determinare sperimentalmente

Range definitivi dei parametri, metrica AGC, budget di latenza, famiglia di filtri,
fattore di oversampling predefinito e specifiche quantitative di trasparenza.
