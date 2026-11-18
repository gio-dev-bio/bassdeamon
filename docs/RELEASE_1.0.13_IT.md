# BassDeamon 1.0.13

Il percorso del basso passa da Kick EQ, Glue, Phase, Saturation, Duck, Unmask e Sub Mono prima dell'uscita. Il kick sidechain serve da riferimento per i detector e per le viste di analisi. Lo Scope può mostrare visivamente la somma con il kick.

- **Ascolto.** Level Match si aggiorna ogni 32 campioni, a prescindere dal buffer del progetto o dall'apertura dell'editor. Dry riproduce l'ingresso e il Balance Trim agisce sul ramo processato.
- **Duck Low Band.** Il filtro parte dalla modalità selezionata anche sul primo colpo e dopo un salto nel progetto. Auto Mix propone i pesi Body, Punch e Click solo per il crossover legacy che li usa.
- **Phase Learn.** Un risultato affidabile applica ritardo e polarità e azzera la rotazione manuale. Undo ripristina i valori precedenti. Anche la proposta Phase di Auto Mix include questa correzione.
- **Balance e Overlap.** Balance misura il basso processato con le impostazioni correnti, indipendentemente da Dry, Delta e Level Match. Auto Mix misura la sovrapposizione per canale e considera entrambi i canali del kick con un basso mono.
- **Analyzer e Scope.** Un salto nel progetto invalida lo spettro e i marker precedenti. Lo Scope usa minimi e massimi misurati per ogni colonna e conserva l'ultima schermata con Hold anche a trasporto fermo.

Estrai tutto lo ZIP, chiudi Cubase e avvia `Installa_Aggiorna.command`.
