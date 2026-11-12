# BassDeamon · Auto Mix

Specifica approvata. Il pannello e il motore Auto Mix sono da implementare.

## Flusso iniziale

- Inserire BassDeamon sul basso e collegare il kick al sidechain.
- Aprire **Auto Mix** dal pulsante con la faccia del demone, vicino a **Power**.
- Selezionare **4, 8 o 16 battute**. Il valore iniziale è **8**.
- Analizzare il passaggio una volta e mostrare le sole impostazioni proposte che cambiano. I moduli lasciati come sono possono essere indicati in una riga compatta. Dopo la conferma, i parametri si possono regolare a mano.
- Se manca il kick del sidechain, segnalarlo. Se i dati di un modulo sono insufficienti, mantenere i valori precedenti e indicare il motivo.

## Ascolto e conferma

- **A · Dry** riproduce il basso all'ingresso del plugin.
- **B · Auto Mix** riproduce la proposta.
- **Level Match** è attivo di default. Misura l'RMS del basso sullo stesso passaggio per A e B, attenua la versione più forte e mostra su quale lato interviene, con il valore in dB.
- Il kick rimane il riferimento allo stesso livello. La compensazione appartiene all'ascolto A/B. L'**Output Trim** conserva il valore impostato.
- **Apply** conferma la proposta in un'unica operazione Undo. **Cancel** ripristina le impostazioni precedenti all'apertura.
- Misurare l'overlap kick/bass prima e dopo per accompagnare il confronto.

## Mono Check

Durante l'analisi iniziale, misurare il basso in ingresso senza il kick del sidechain. Presentare un solo referto di compatibilità mono, indipendente dai pulsanti di ascolto. Mostrare l'esito, la banda coinvolta, la perdita sul beat più critico e la sua posizione nel passaggio. Indicare il numero di beat con avviso quando il problema è intermittente. Valutare la banda intera e il sub, con 120 Hz come riferimento iniziale.

Riutilizzare le soglie del Mono Check: **Low Loss** sotto 4,5 dB di perdita, **Caution** da 4,5 dB, **High Loss** da 8 dB e **No Signal** con segnale insufficiente. Descrivere come cancellazione tra canali solo un caso supportato dalla correlazione negativa. Il referto è informativo.

Il calcolo è in `Source/StereoPhaseAnalysis.h`. La cattura e il referto nel pannello fanno parte dell'integrazione di Auto Mix.

## Proposte per modulo

| Modulo | Parametri e criteri |
|---|---|
| **01 Kick EQ** | Frequenza e taglio statico sul basso quando la fondamentale del kick è stabile. |
| **02 Glue** | Quantità, attacco e rilascio in base a dinamica e ritmo. |
| **03 Phase** | Ritardo e polarità quando l'intervento migliora con coerenza più colpi consecutivi. |
| **04 Saturation** | Drive e Amount contenuti quando le armoniche aiutano il basso a emergere. |
| **05 Unmask** | Frequenza, larghezza, attenuazione e timing in base alla sovrapposizione. Il modulo continua a reagire al kick con i parametri scelti. |

## Barra Output

Un'unica barra in fondo alla console raggruppa **Kick SC**, **Sub Mono**, **Output Trim**, meter **IN / OUT** sulla stessa scala dBFS RMS e picco in uscita. Il confronto RMS del percorso corrente indica **Matched** entro 0,2 dB, **Different** oltre la tolleranza e **No Signal** quando entrambi i livelli sono a −60 dBFS RMS o meno. **Kick SC** è un indicatore di presenza del sidechain. **Sub Mono** parte spento, con frequenza iniziale regolabile di 120 Hz, e agisce prima del Trim. La misura **Sub Fold Loss** si trova nello Scope.

## Anteprime

- [Console completa Raw e pannello Auto Mix](https://chatgpt.com/library?file_id=libfile_756915b30a7881919a0728f98e8cdc06&account_id=personal)
- [Pannello Auto Mix e pulsante con demone](https://chatgpt.com/library?file_id=libfile_4e705b99bed0819184c9e951fa954fdd&account_id=personal)
- [Confronto dei temi Classic e Raw](https://chatgpt.com/library?file_id=libfile_d4824555c6ac8191a33fd57104383936&account_id=personal)
- [Scope Stereo e Mono](https://chatgpt.com/library?file_id=libfile_8dcd37dd4c108191b291e358a4ef1686&account_id=personal)
- [Avvisi del Mono Check](https://chatgpt.com/library?file_id=libfile_0d7b0c7e88048191956b9e2897e3cb36&account_id=personal)
- [Referto Mono Check in Auto Mix](https://chatgpt.com/library?file_id=libfile_535348e05ef88191bee5942002c6cc55&account_id=personal)
