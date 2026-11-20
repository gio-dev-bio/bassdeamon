# BassDeamon 1.0.15

Duck reagisce anche ai kick a livello più basso. In Full Band, Attack a 0 ms applica la riduzione appena il detector supera la soglia. Nel test a 44,1 kHz un kick con picco intorno a −35 dBFS ha raggiunto i 18 dB impostati.

In Low Band la riduzione è più profonda sotto il cutoff e sfuma verso le frequenze alte. Con Amount a 18 dB e cutoff a 110 Hz il test misura 16,3 dB di riduzione a 55 Hz e un intervento trascurabile a 2,2 kHz. Al cutoff la riduzione a pieno intervento è di 9 dB.

Unmask usa il livello del sidechain per valutare la presenza di un kick debole. In Auto Focus il taglio segue la sovrapposizione rimasta dopo Duck. La priorità è la fondamentale appresa. Le armoniche ricevono un intervento più leggero, proporzionato alla sovrapposizione.

Auto Mix misura anche la componente Mid o Side selezionata per Unmask. La proposta di Kick EQ continua a usare entrambi i canali del basso.

Estrai tutto lo ZIP, chiudi Cubase e avvia `Installa_Aggiorna.command`. Le istruzioni per l'avvio da Terminale sono in `INSTALL.txt`.
