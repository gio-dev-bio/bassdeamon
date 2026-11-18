# BassDeamon 1.0.9

La GUI principale mantiene materiali, disposizione e proporzioni della 1.0.8 approvata. Le aggiunte sono contenute nei pannelli Advanced e Auto Mix.

## Collisioni

PRE e POST usano lo stesso criterio di interferenza con il kick: supporto spettrale, presenza simultanea, livello assoluto e rapporto basso/kick. Una sovrapposizione non è una misura psicoacustica universale di mascheramento. Il peso relativo scende progressivamente tra 0 e -24 dB di basso rispetto al kick; il gate assoluto è graduale tra -90 e -66 dBFS. Questi sono criteri operativi del rilevatore, non livelli ideali per un mix. Le finestre brevi condividono la selezione temporale dei tratti in cui il kick è attivo; lo spettro principale resta quello del segnale completo. Gli attacchi non attenuati e le armoniche residue significative possono rimanere visibili. Il colore ambra distingue le segnalazioni moderate da quelle forti.

## Auto Focus e curva

Auto Focus privilegia la zona della fondamentale acquisita, sempre entro il range scelto. Prima acquisizione con evidenza stabile; cambi successivi con margine, persistenza e ulteriore colpo di kick oppure materiale continuo persistente. La curva usa i coefficienti reali. Il mantenimento del picco viene rinnovato quando il focus cambia, evitando di mostrare una vecchia campana su una nuova frequenza.

## Phase

AUTO REF acquisisce un riferimento affidabile dopo Auto Mix o Learn Kick EQ. L’acquisizione per lo Scope non modifica i parametri audio; la priorità della fondamentale in Auto Focus viene proposta da Auto Mix e confermata con APPLY, oppure acquisita con Learn Kick EQ. È disponibile in Phase Advanced. Il riferimento dello Scope è separato dal riferimento fisico del filtro: l'acquisizione non cambia da sola il suono. La lettura e l'immissione dei gradi vengono convertite alle nuove coordinate. Una scelta manuale disattiva AUTO REF. Riferimenti e scelta automatica sono salvati nello stato.

## Balance

Auto Mix misura il basso processato con le impostazioni catturate e lo confronta con il kick in due bande, con estremi regolabili. Le energie stereo sono misurate per canale, senza cancellazione mono. Viene mostrato anche il picco campionato della somma virtuale K+B: non è il true peak del master.

Non esiste un target universale implicito. Analizzare un equilibrio preferito e usare SAVE TARGET per ottenere consigli nelle analisi successive, con gli stessi estremi di banda. Il suggerimento è limitato a +/-3 dB per prova. Senza target si può effettuare una prova manuale con il trim.

LISTEN TRIM ascolta temporaneamente la regolazione senza cambiare il parametro salvato e senza modificare il kick. APPLY TRIM applica la scelta con Undo; RESET TRIM torna a 0 dB. Il Balance Trim è separato da Output Trim, opera dopo Auto Gain e Level Match, rimane fisso e viene salvato nel progetto. Non è applicato all'ascolto Delta.

## Installazione

Estrai tutto lo ZIP, chiudi Cubase e apri Installa_Aggiorna.command. Digita la password amministratore quando sudo la richiede.

Lo script prepara la copia, sostituisce il bundle in /Library/Audio/Plug-Ins/VST3 e rimuove la quarantena dal plugin installato.
