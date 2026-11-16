# BassDeamon · Backlog DSP

Base dell'audit `1e00665`, ramo `gui/vintage-steel-knobs`. Tutte le voci aperte.

## P0 · Bug riprodotti

- [ ] **Dry → Wet con Duck e Level Match.** Dry disattiva Duck nella catena Wet. Tornando a Wet il detector deve riassestarsi: nel test sintetico con Duck a 10 dB il livello RMS dei primi 200 ms è sceso di 7,81 dB rispetto al regime stabile. Mantenere attiva la catena Wet durante Dry e verificare transizione, guadagno e meter.
- [ ] **Unmask Delta altera il segnale che deve confrontare.** Attivando Delta, il codice disattiva il Duck a monte. Nel test il meter Duck è sceso da 10 a 0 dB. Conservare il Duck durante Delta e verificare che il residuo isoli il contributo di Unmask. Verificare separatamente l'audizione della saturazione.
- [ ] **Soft Tube, cambio oversampling 2×/4× dipendente dalla dimensione del buffer.** A parità di istante di commutazione, le partizioni da 64 e 4096 campioni hanno prodotto una differenza massima di circa 0,047 FS. Effettuare il cambio di rapporto al termine del fade, con gestione dello stato dei filtri. Verificare commutazione in entrambe le direzioni, cambio ripetuto e null test tra partizioni.

## P1 · Metering e CPU

- [ ] **Peak hold di uscita diverso fra mono e stereo.** Il decadimento avviene una volta per canale. Un secondo dopo lo stesso impulso il test ha misurato circa −29 dB in mono e −58 dB in stereo. Usare il massimo fra i canali e applicare il decadimento una volta per campione.
- [ ] **Unmask continua a elaborare anche in OFF.** Dopo il fade di spegnimento saltare il percorso dei filtri audio. Misurare separatamente il costo dei detector e mantenere il lavoro necessario ad Auto Focus o Hold Focus. Verificare riattivazione, silenzio, stereo/Mid/Side e assenza di scatti. Quantificare il risparmio prima di cambiare altre parti del DSP.
- [ ] **Profilo CPU per modulo.** Misurare su Intel e Apple Silicon a 44,1 e 48 kHz con buffer da 64, 128 e 256 campioni. Registrare mediana e p99 del tempo per blocco, ogni modulo da solo, catena completa, moduli OFF e bypass. Verificare l'impatto di Analyzer e Scope aperti e chiusi a più refresh rate.

## P2 · Migliorie audio da validare

- [ ] **Duck più selettivo su Body/Punch.** Nel test Body-only il crossover attuale attenua circa 5,6 dB a 60 Hz, 0,76 dB a 250 Hz e 0,24 dB a 500 Hz. Confrontare un crossover più selettivo con la versione corrente a pari attenuazione a 60 Hz e volume pareggiato. Sui loop reali ascoltare attacco del kick, corpo del basso e naturalezza del rientro. Misurare fase, risposta e CPU prima di decidere.
- [ ] **Qualità della saturazione 2×/4×.** Dopo il fix di commutazione, confrontare aliasing, latenza e CPU alle due modalità con sweep e loop reali. Usare lo stesso gain staging e lo stesso livello d'uscita.

## Verifiche da ripetere prima della build

A ogni modifica del DSP, eseguire le suite di regressione per tutti i moduli e per la catena completa. Prima di distribuire una build, verificare anche in Cubase i controlli, le transizioni e il comportamento con loop reali.

- [ ] **Attack a 0 ms.** Verificare Duck e Unmask a 44,1 e 48 kHz. Confrontare il transiente nella somma kick + basso con Duck a 0 e 1,5 ms, a volume pareggiato. Ascoltare eventuali click sul sub sostenuto e verificare anche Duck e Unmask insieme.

- [ ] **Learn Kick EQ e Learn Phase in Cubase.** Segnalazioni storiche da riverificare a 44,1 kHz con kick reali in sidechain e basso sia stabile sia in sedicesimi con pitch variabile. Kick EQ deve apprendere dal kick. Per Phase, distinguere un mancato rilevamento con segnali stabili da un rifiuto corretto quando non emerge un allineamento affidabile.
- [ ] **Catena completa, misure e monitoraggio.** Provare stati singoli e combinati di Kick EQ, Glue, Phase, Saturation, Duck e Unmask, con AutoMix, Level Match e bypass host. Verificare RMS IN/OUT, picchi, compatibilità mono, latenza dichiarata/misurata e assenza di click. Monitorare silenzio e riavvio del trasporto. Verificare che l'Analyzer PRE/POST commuti solo la vista, rispetti il rate e distingua le collisioni residue dalla curva dell'intervento.
- [ ] **GUI e host.** Verificare in Cubase la nuova skin approvata, l'ordine visivo dei moduli, la leggibilità di Unmask, i meter GR da destra a sinistra e lo Scope a pieno modulo. Provare apertura, chiusura e bypass dell'editor sotto carico.

L'audit di base aveva superato 717 casi di segnale e 24 transizioni. I 90 casi di attenuazione Unmask calibrata avevano uno scarto massimo di circa 0,086 dB. I tre difetti di transizione sopra sono emersi nelle prove mirate aggiuntive.

## Riverifica del 16 novembre 2026

- Dieci suite locali superate. Matrice di segnale 717/717, transizioni generali 24/24, Learn 24/24, attenuazione Unmask 90 casi con scarto massimo 0,086 dB. Duck e Unmask verificati anche con Attack a 0 ms.
- Catena completa con Kick EQ, Glue, Phase, Saturation, Duck, Unmask e Sub Mono attivi sui WAV PML Frail SC e Tantra Kick Loop 03 a 44,1 kHz, con kick ricampionato da 48 kHz. Output stereo finito e identico campione per campione tra buffer 64 e 256, sia con Duck Full-Band sia con il multibanda attuale.
- Restano riproducibili il calo di 7,81 dB tornando da Dry a Wet, Duck disattivato durante Unmask Delta, il cambio di oversampling 4× → 2× dipendente dal buffer (scarto di picco 0,0471 FS) e il decadimento del peak meter diverso fra mono e stereo.
- Verifica eseguita sul DSP standalone. La prova del VST3 e della GUI in Cubase resta da eseguire sulla prossima build.
