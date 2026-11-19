# BassDeamon 1.0.14

GLUE calcola la soglia adattiva dal livello RMS del basso in ingresso al modulo e la aggiorna anche durante un fade-in. Mantiene il riferimento nelle pause brevi e lo ricalcola con il segnale al rientro da una pausa prolungata o dopo un salto nel progetto.

LOCK ADAPTATION conserva i parametri appresi. Il meter rappresenta il guadagno applicato da GLUE anche durante la transizione del suo interruttore.

In modalità dB, Auto Focus dà priorità alla frequenza esatta appresa quando kick e basso si sovrappongono ancora dopo Duck. Una seconda campana stretta può attenuare l'armonica condivisa che interferisce di più. L'intervento ordinario sulle armoniche arriva a 1,25 dB e sale gradualmente fino a 4 dB se la sovrapposizione è marcata. Con la fondamentale già libera, Auto Focus può seguire la banda residua. Il limite sulle altre bande basse è di 6 dB. Il controllo manuale consente di scegliere frequenza, larghezza e attenuazione.

Learn e Auto Mix valutano i periodi del kick anche quando una sua armonica è più forte della fondamentale. Auto Gain di Unmask sfuma dopo la fine della sovrapposizione, con un breve mantenimento per coprire gli intervalli tra i colpi. Con Attenuation a zero il makeup sfuma subito.

In Auto Focus, Attenuation indica il taglio massimo disponibile. La profondità aumenta progressivamente con la sovrapposizione e segue il corpo del kick, iniziando a rilasciare mentre il colpo si spegne.

Auto Mix stima l'intervento di GLUE sui passaggi in cui il basso suona. Dosa Duck in base alla sovrapposizione e usa la coda del kick e l'ingresso della nota successiva per proporre il rilascio di Duck e Unmask. Propone un taglio statico leggero solo se la fondamentale richiede ancora spazio dopo Duck e misura Unmask sulla catena risultante. Soft Tube è una scelta timbrica controllata da chi mixa.

ROTATION raggiunge −180° e +180° alla frequenza REF. Il knob Advanced e il valore nel pannello principale controllano lo stesso angolo. Il passaggio attraverso zero usa una breve dissolvenza e LEGACY AP conserva il suo controllo di frequenza. Il richiamo di un progetto o di uno stato A/B annulla le richieste Phase Learn della configurazione precedente.

Estrai tutto lo ZIP, chiudi Cubase e avvia `Installa_Aggiorna.command`.
