# Autenticazione di settore sul badge MIFARE Classic 1K

Prova diagnostica con la libreria MFRC522, distinta dalla lettura UID.
Usare soltanto il badge di prova del progetto. Mantenere il cablaggio con
i tre partitori già impiegato nelle prove SPI e UID.

## Esecuzione

1. Aprire `test_rc522_auth.ino` nell'Arduino IDE, selezionare Uno e la porta.
2. Caricare lo sketch; monitor seriale a **9600 baud**.
3. Avvicinare un solo badge e tenerlo fermo fino a `Test terminato`.
4. RESET ripete la prova; non modificare il cablaggio alimentato.

Lo sketch richiede il tipo PICC classificato come MIFARE 1K. Esegue
`PCD_Authenticate` con **chiave A del settore 1**, usando l'indirizzo del
blocco 4. Non invoca letture o scritture di memoria applicativa, non cambia
chiavi e non scrive il sector trailer. Le sole chiavi contenute nel sorgente
sono valori pubblici di prova: `FF FF FF FF FF FF` e il valore diverso
`00 FF FF FF FF FF`. Non inserire in questo file chiavi reali.

## Sequenza ed esiti

| Passo | Prova | Esito atteso |
| --- | --- | --- |
| 1 | Chiave A di fabbrica | `STATUS_OK` |
| 2 | Chiave A diversa | Autenticazione non riuscita |
| 3 | Di nuovo chiave A di fabbrica | `STATUS_OK` |

Solo la sequenza completa produce `PASS FUNZIONALE`. Fra le prove vengono
disattivati Crypto1 e il campo RF, poi il badge viene riselezionato. Lo sketch
controlla versione del lettore, UID e tipo; una carta diversa viene rifiutata.
Il controllo UID tiene coerente la prova ma non autentica da solo l'identità.
Il campo RF viene infine spento. Non occorre allontanare il badge fra i passi.

Se il primo passo fallisce, la chiave di fabbrica potrebbe non essere quella
configurata, oppure potrebbe esserci un problema RF o SPI. Nessuna ricerca
di chiavi viene eseguita. Un timeout nel passo negativo, preso da solo, non
dimostra il rifiuto crittografico: può dipendere anche dalla comunicazione.
Il terzo passo verifica il recupero della sessione, ma non elimina ogni
possibile errore intermittente. Ripetere il test con collegamenti stabili.

## Cosa dimostra e cosa resta aperto

NXP descrive per MIFARE Classic EV1 1K un'autenticazione mutua a tre passi,
con due chiavi per settore. Qui il protocollo viene eseguito dal chip e dalla
libreria tramite `PCD_Authenticate`: non viene costruita una challenge
applicativa propria e non vengono misurati nonce o qualità della casualità.
Il tipo PICC osservato non identifica con certezza il modello NXP EV1:
la documentazione è il riferimento del protocollo da verificare, non una
certificazione del badge reale o del lettore con versione `0x82`.

Un esito positivo verifica la compatibilità funzionale dell'autenticazione
di settore. **Non dimostra un fattore di possesso robusto:** la chiave di
fabbrica è pubblica e la sicurezza di Crypto1 è compromessa. Cambiare la
chiave di fabbrica non risolve le debolezze dell'algoritmo. La libreria
MFRC522 usata non implementa l'autenticazione AES/3DES dei DESFire.

Il test non verifica resistenza a replay, emulazione, clonazione o relay,
né integra badge, PIN e impronta nella sessione gestita dal Pi. Nessuna
decisione di accesso viene presa. Non attivare questo test come autenticazione
definitiva del progetto sulla base del solo `PASS FUNZIONALE`.

## Challenge e cambio chiavi: risultati della valutazione

Crypto1 usa chiavi di 48 bit e presenta attacchi pubblicati al cifrario e al
protocollo; nelle versioni studiate sono state sfruttate anche debolezze del
generatore dei nonce. Non abbiamo misurato il generatore dei due supporti o
eseguito attacchi: applicabilità e condizioni dei singoli attacchi restano
distinte dal test funzionale.

La challenge-response è già parte dell'autenticazione del badge. Il badge
non offre allo sketch la possibilità di sostituire Crypto1 con AES, e
`PCD_Authenticate` non seleziona algoritmi alternativi. Una challenge
applicativa aggiuntiva o un nonce migliore non eliminano le altre debolezze
di Crypto1. Non è previsto sviluppare un protocollo crittografico proprio.

**Le chiavi di fabbrica sono invece modificabili**, nel rispetto dei permessi
del sector trailer. Per il settore 1 di MIFARE Classic 1K il trailer è il
blocco 7. Questa possibilità deriva dal datasheet, **non è stata ancora
provata sui due supporti** e non è implementata dallo sketch corrente.

Una futura procedura richiede una decisione su chiavi A/B e permessi, verifica
della codifica ridondante dei bit di accesso e gestione degli errori di
scrittura. Un formato errato può bloccare irreversibilmente il settore;
non si può promettere un recupero in ogni caso. Key A non si legge dal
trailer per farne un backup; Key B può risultare leggibile in alcune
configurazioni e in tal caso non va trattata come chiave di autenticazione
segreta. Questi dettagli vanno verificati prima della personalizzazione.

La proposta discussa è usare chiavi casuali distinte per badge/settore e
conservare i segreti fuori da Git, definendo anche rotazione e recupero delle
chiavi note. Non è una decisione del gruppo né un'operazione già eseguita.
La prova minima successiva, se la personalizzazione viene adottata, è nuova
chiave accettata e vecchia chiave di fabbrica non più accettata, con controllo
del settore e dei permessi. Questa miglioria elimina la chiave pubblica come
credenziale, ma non risolve le debolezze di Crypto1.

L'impiego nella demo e l'eventuale passaggio a un sistema con autenticazione
moderna restano aperti. La matrice riepilogativa è nelle
[linee guida architetturali](../../architecture_guidelines.md#stato-della-challenge-e-delle-chiavi-del-badge).

## Fonti

- [NXP MIFARE Classic EV1 1K, MF1S50YYX_V1, rev. 3.2, 23 maggio 2018](https://www.nxp.com/docs/en/data-sheet/MF1S50YYX_V1.pdf),
  sezioni su autenticazione e organizzazione della memoria.
- [MFRC522: sezione Security](https://github.com/miguelbalboa/rfid#security).
- [Esempio ufficiale della libreria](https://github.com/miguelbalboa/rfid/blob/master/examples/ReadAndWrite/ReadAndWrite.ino):
  riferimento per `PCD_Authenticate` e chiusura di Crypto1. Lo sketch di questa
  cartella non esegue le operazioni di scrittura presenti in quell'esempio.
- [Radboud University, ricerche MIFARE Classic del 2008](https://www.sos.cs.ru.nl/applications/rfid/main.html):
  fonte degli attacchi pubblicati; non un esito di prove del gruppo.

## Esito hardware comunicato — 18 novembre 2026

L'utente ha eseguito lo sketch e comunicato la stessa sequenza positiva su
entrambi i supporti disponibili, chiamati «badge» e «card»:

| Passo | Esito osservato |
| --- | --- |
| Chiave A di fabbrica, settore 1 | `Success` |
| Chiave A diversa | `Timeout in communication` |
| Di nuovo chiave A di fabbrica | `Success` |

Il test ha prodotto `PASS FUNZIONALE`. È verificata sui due supporti la
compatibilità con l'autenticazione MIFARE Classic tramite Crypto1; il timeout
negativo rimane un mancato completamento dell'autenticazione, non una prova
isolata della causa del fallimento. Non sono stati misurati gli scambi RF,
la casualità, la resistenza a replay/clonazione o la stabilità a lungo termine.
Non è stata eseguita una modifica delle chiavi e il flusso non è integrato
nel servizio del Pi. Non sono riportati UID reali.

Verificata la compilazione C++ locale con warning trattati come errori e
periferiche simulate. Controllati sequenza completa, fallimento della prima
chiave, accettazione anomala della chiave diversa, mancato recupero, cambio
UID, mancata riselezione, tipo incompatibile e perdita della comunicazione.
Le risposte Crypto1 sono simulate: questi controlli verificano il flusso dello
sketch, non l'implementazione crittografica né il protocollo radio.
