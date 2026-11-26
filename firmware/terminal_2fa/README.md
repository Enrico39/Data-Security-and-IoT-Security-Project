# Terminale badge e PIN

Aprire `terminal_2fa.ino` in Arduino IDE, selezionare Arduino Uno e installare MFRC522 di Miguel Balboa (compilato con 1.4.12 e Arduino AVR Boards 1.8.8). Il driver OLED nella stessa cartella proviene dallo sketch di banco. Usare il [cablaggio del 26 novembre](../../docs/hardware/current_wiring.md). Il display maschera subito ogni cifra. Il PIN dimostrativo ha quattro cifre, compresi gli eventuali zeri iniziali. Con * si cancella l'ultima cifra, a campo vuoto si annulla. Con # si inviano quattro cifre. Una quinta cifra annulla il tentativo.

## Cosa succede al varco

L’Uno gestisce già badge, tastierino, display e scadenza della sessione. Le righe che descrivono decisioni, LED e audit indicano il comportamento previsto del servizio Pi. Il limite ai tentativi è una proposta per la demo.

| Situazione | Esperienza dell’operatore | Esito previsto |
|---|---|---|
| Accesso in turno | Su `AVVICINA BADGE` presenta la carta. Dopo `INSERISCI PIN` digita quattro cifre e preme `#`. | Con credenziali valide, ruolo ammesso e turno attivo, compare `ACCESSO CONSENTITO`. Il Pi accende il verde per cinque secondi, poi torna al rosso. L’evento registra la finestra autorizzata del varco simulato. |
| Stesse credenziali fuori turno o ruolo insufficiente | Inserisce il PIN corretto e vede `ACCESSO NEGATO`. | Il LED resta rosso. Il Pi registra il motivo della decisione. Il PIN corretto azzera gli errori PIN consecutivi anche in questo caso. |
| Carta incompatibile o challenge Crypto1 fallita | Vede `BADGE NON ACCETTATO`. Può ripresentare la carta al tentativo successivo. | La sessione si chiude prima della richiesta PIN. |
| Challenge riuscita, badge sconosciuto o revocato | Vede `ACCESSO NEGATO` prima di digitare il PIN. | Il Pi rifiuta il badge e mantiene il LED rosso. |
| Primo o secondo PIN errato | Vede `ACCESSO NEGATO`. Per riprovare deve ripresentare il badge. | Il Pi conta un errore per operatore e chiude la sessione. |
| Terzo PIN errato consecutivo | Per i successivi 60 secondi ripresentare il badge produce un diniego prima del PIN. | Il blocco proposto riguarda quel solo operatore. Alla scadenza può iniziare una nuova sessione. |
| PC o broker offline, Pi disponibile | Con policy locale ancora valida, il flusso badge e PIN prosegue. | Il Pi applica i controlli locali. Se autorizza, apre la finestra verde e accoda l’evento firmato. Al ritorno della rete il backend riceve e verifica l’evento prima del successivo ancoraggio del lotto. Con policy scaduta o tempo inaffidabile, l’accesso è negato. |
| Pi assente o collegamento interrotto | All’avvio resta `ATTESA PI`. Se la challenge è già riuscita, allo scadere della sessione compare `TEMPO SCADUTO`. | L’Uno ignora un esito arrivato dopo la scadenza. Se la connessione cade dopo l’invio del PIN, il Pi potrebbe aver già registrato la decisione. |

### Correggere, annullare e riprovare

- Durante la digitazione, ogni pressione di `*` cancella l’ultima cifra. A campo vuoto, `*` mostra `ANNULLATO` e chiude la sessione.
- Con meno di quattro cifre, `#` lascia aperto il campo. Con quattro cifre, invia il PIN una sola volta. Dopo la conferma il tastierino attende la decisione. Una quinta cifra prima della conferma mostra `TROPPE CIFRE` e chiude il tentativo.
- I 30 secondi decorrono dal successo della challenge e comprendono digitazione e attesa della risposta. Una correzione non riavvia il timer. Un annullamento o una scadenza prima della verifica non consuma un tentativo PIN.
- La schermata finale dura cinque secondi. Poi il terminale attende che il Pi prepari una nuova sessione prima di chiedere il badge.

## Protocollo della prima versione

USB interna fidata, 9600 baud, 8N1, righe ASCII con solo LF. Il Pi deve avere accesso esclusivo alla porta. Per una prova manuale con dati fittizi usare il monitor seriale con terminatore «Nuova riga». La porta trasporta il PIN in chiaro. Disattivare la registrazione del traffico. Il firmware azzera il proprio buffer PIN dopo invio o chiusura. Le copie transitorie nei buffer seriali hanno un ciclo di vita distinto.

| Direzione | Riga | Quando |
|---|---|---|
| Uno → Pi | `READY` oppure `ERROR RFID` | Avvio dopo reset. READY viene ripetuto al termine della schermata di esito. |
| Pi → Uno | `ARM id` | Dopo READY. Il Pi genera un ID fresco di 16 cifre esadecimali maiuscole per ogni tentativo, anche dopo riavvio. |
| Uno → Pi | `ARMED id` | Lettore pronto, in attesa di una carta. |
| Uno → Pi | `BADGE_OK id uid` | Solo dopo autenticazione Crypto1 del settore 1. UID di 4, 7 o 10 byte in esadecimale maiuscolo, due cifre per byte. Inizia la finestra complessiva di 30 secondi. |
| Uno → Pi | `BADGE_FAIL id` | Carta selezionata incompatibile o autenticazione fallita. Chiudere la sessione. |
| Pi → Uno | `ASK id` oppure `DENY id` | Il Pi associa il badge a un operatore attivo oppure rifiuta. |
| Uno → Pi | `PIN id cifre` | Una sola trasmissione su conferma. |
| Pi → Uno | `ALLOW id` oppure `DENY id` | Dopo PIN. Il Pi verifica credenziali, tentativi e policy, consuma la sessione e gestisce il LED. |
| Entrambe | `CANCEL id` | Chiusura del tentativo corrente. |
| Uno → Pi | `TIMEOUT id` | Termine dei 30 secondi dal badge, compresa l'attesa della decisione. |

Esempio da banco: dopo READY inviare `ARM 0000000000000001`, presentare la carta, attendere BADGE_OK e inviare `ASK 0000000000000001`. Digitare quattro cifre fittizie e #. Dopo la riga PIN inviare `DENY 0000000000000001`. Per provare il solo display di consenso ripetere dopo READY con un nuovo ID e ALLOW. Un comando manuale simula il Pi.

Il parser ignora righe fuori fase, con ID errato, caratteri di controllo o oltre 31 byte. Dopo un esito il display lo mantiene cinque secondi. Il Pi deve invalidare ogni sessione su reset, scadenza o perdita del collegamento. Alla riconnessione deve azzerare l'Uno e attendere READY. L'Uno non rileva autonomamente una disconnessione USB con alimentazione separata. Il protocollo si affida al Pi per l'unicità degli ID e al perimetro fisico per l'autenticità dei messaggi. Una modifica che produce un altro comando valido sul bus fidato può essere accettata.

La demo usa la chiave A pubblica di fabbrica sul blocco 4 dei supporti Classic 1K. Il firmware non scrive sul badge. Il Pi deve accettare soltanto BADGE_OK nella fase badge e scartare il precedente messaggio BADGE basato sul solo UID. Un errore RF può causare BADGE_FAIL anche con un badge registrato. I tre scenari di policy, la finestra LED, la firma e l'ancoraggio richiedono gli altri componenti. Il collaudo fisico integrato rimane da eseguire.

## Politica proposta per il servizio Pi

Una sessione permette una sola verifica del PIN. Al primo e al secondo errore il Pi risponde DENY e per riprovare serve ripresentare il badge. Al terzo PIN errato consecutivo per lo stesso operatore il Pi blocca nuove verifiche per 60 secondi. Durante il blocco risponde DENY già a BADGE_OK. Alla scadenza riparte da zero. Anche una verifica PIN riuscita azzera il contatore, compreso il caso fuori turno. Il primo e il secondo errore restano conteggiati tra sessioni. Un blocco temporaneo limita il disagio causato da errori o blocchi provocati da terzi.

Il Pi dovrà salvare in modo atomico contatore, scadenza e consumo della sessione. Cambiare badge associato allo stesso operatore, ripresentare il badge o riavviare il servizio conserva il limite. A tempo incerto si mantiene il blocco fino a una verifica affidabile. Correzioni, annullamenti, timeout prima della verifica, UID sconosciuti, errori RF e duplicati non incrementano il contatore. Un errore già verificato dal Pi conta anche se la risposta si perde. Sul display compare ACCESSO NEGATO; il motivo specifico rimane nell'audit pseudonimo del Pi. Tre errori e 60 secondi sono parametri proposti per il banco. Riferimento metodologico: [OWASP Authentication Cheat Sheet, Login Throttling](https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html#login-throttling).
