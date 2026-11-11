# Lezioni dal progetto biometrico di riferimento

## Scopo e attendibilità

Questa analisi riguarda il progetto esterno `biometric_proj-main`, usato come riferimento per il nostro prototipo. Le evidenze derivano dalla lettura del codice, senza esecuzione sull'hardware. Il documento integra e corregge il PDF «Lezioni dal progetto biometrico: errori da evitare». I riferimenti ai sorgenti sono relativi alla radice di quel progetto, che non è una dipendenza del nostro repository.

Le decisioni adottate sono riportate in [architecture_guidelines.md](../architecture_guidelines.md). Le regole di lavoro sono in [AGENTS.md](../AGENTS.md). Le prove sotto sono da eseguire sulla nostra implementazione, non risultati già ottenuti.

## Evidenze e conseguenze per il nostro progetto

| Problema osservato | Evidenza nel progetto di riferimento | Decisione per noi | Come verificarla |
| --- | --- | --- | --- |
| Consegna del token al richiedente sbagliato | `backend/AS608/auth/auth_server.py`, `check_login_status`: può restituire il primo JWT in attesa senza autenticare il richiedente. | Il Pi possiede la sessione del tentativo. Eventuali sessioni web devono ricevere esclusivamente i propri risultati. | Avviare due sessioni e verificare che nessuna recuperi o consumi il risultato dell'altra. |
| Autenticazione senza autorizzazione amministrativa | `backend/routes.py`, `token_required`, creazione e cancellazione dipendenti: il token e l'esistenza dell'utente non impongono un ruolo admin. | Applicare i permessi nel servizio che esegue l'operazione e definire una matrice ruoli-operazioni. | Chiamare le API direttamente come utente ordinario e verificare il rifiuto delle operazioni amministrative. |
| Enrollment non autorizzato | `auth_server.py`, `start_enrollment_window` e `cancel_enrollment`: manca il controllo di autorizzazione. La presenza del JWT nel frontend non lo sostituisce. | Enrollment amministrativo autenticato, con sessione breve e accesso esclusivo al sensore. | Provare avvio e annullamento senza permessi e due richieste concorrenti. La registrazione fisica richiede comunque un campione o una compromissione del sensore. |
| Identità legata allo slot biometrico | Il JWT usa come `sub` l'ID del sensore; `routes.py` cerca il dipendente per `fingerprint_id`. | Usare un ID operatore stabile, distinto da badge e slot del sensore. | Riutilizzare uno slot per un altro utente e verificare che credenziali precedenti non assumano la nuova identità. |
| Segreti predefiniti e configurazione dispersa | `auth_server.py`, `routes.py` e `frontend/src/api.ts`: segreti di fallback e indirizzi scritti nel codice. | Configurazione documentata e segreti locali esclusi da Git. I servizi che li richiedono rifiutano l'avvio se mancano. | Avviare con configurazione incompleta e controllare il repository senza stampare i segreti nei log. |
| TLS non attivo nell'avvio fornito | `auth_server.py`: `ssl_context` commentato; frontend con URL HTTP. | Verificare mTLS e permessi MQTT nella configurazione effettivamente eseguita. | Tentare connessioni con CA errata, certificato non autorizzato e topic vietato. |
| Cifratura delle chiavi con master key sullo stesso disco | `backend/AS608/auth/key_crypto.py`: `master.key` locale accanto all'applicazione. | Definire posizione, accessi e ciclo di vita delle chiavi. Dichiarare il limite rispetto alla copia completa del dispositivo. | Verificare quali file permettono di recuperare una chiave e che nessuno sia versionato. |
| Fiducia nella risposta biometrica | `finger_module.py`: il software firma dopo il match; il collegamento UART non autentica crittograficamente ogni risposta. | Esplicitare il perimetro fidato dei cablaggi. La cifratura Arduino-Pi protegge quel collegamento, non l'intero percorso dal dito. | Simulare un esito tardivo o appartenente a un'altra sessione e rifiutarlo. Documentare separatamente gli attacchi fisici esclusi dalla demo. |
| Enrollment con stati parziali e risultato ambiguo | `finger_module.py`, `enroll_fingerprint`, e chiamante in `auth_server.py`: alcuni errori restituiscono `None`; il template è salvato prima della registrazione remota della chiave. | Stato esplicito in corso/attivo/fallito. Attivare l'utente solo dopo le associazioni richieste, con recupero o pulizia dei dati parziali. | Interrompere ogni fase e riprovare senza generare identità attive incomplete o duplicati. |
| Cancellazione incompleta e slot zero | `routes.py`, `delete_dipendente`: `if fingerprint_id` salta lo slot 0; il dipendente viene eliminato anche se la pulizia remota fallisce. | Revocare l'accesso e tracciare separatamente la pulizia, che deve essere ripetibile. Trattare 0 come valore valido. | Revocare lo slot 0 e simulare sensore irraggiungibile; verificare rifiuto dell'accesso e recupero della pulizia. |
| Dati sensibili nei log | `finger_as608_schnorr.py` stampa template in esadecimale; `routes.py` stampa porzioni di header/token. La variante `finger_module.py` differisce. | Log strutturati senza PIN, template, campioni o token. | Ispezionare i log dei percorsi di successo, errore, enrollment e debug. |
| Implementazione crittografica personalizzata | `schnorr.py`: schema Schnorr proprio, parsing e controlli non equivalenti a BIP-340. | Usare librerie consolidate per le funzioni operative e motivare algoritmo, formato e contesto firmato. | Verificare firma valida, messaggio alterato, chiave errata, encoding invalido e contesto errato. |
| Stato condiviso e persistenza fragile | `auth_server.py`: risultati globali e file JSON riscritti; lock di processo. | Una sola sessione attiva per terminale e un solo proprietario delle periferiche. Usare persistenza recuperabile e deduplicazione degli eventi. | Testare concorrenza, timeout, annullamento, riavvio e ritrasmissione. |
| Documentazione e avvio divergenti | Due flussi biometrici simili; README con istruzioni e riferimenti incompleti; dipendenze senza versioni. | Un percorso documentato di avvio per componente e dipendenze riproducibili. | Un collega prova l'avvio da un ambiente pulito e registra i prerequisiti effettivi. |

## Precisazioni rispetto al documento iniziale

- La cancellazione del dipendente viene controllata a ogni richiesta nel backend: se l'utente manca, il token viene respinto. Rimangono da gestire revoca di sessione e riuso dello slot biometrico.
- Separare firmatario e verificatore migliora l'isolamento. Non corregge automaticamente consegna dei token, controlli dei ruoli o enrollment aperto.
- Un secure element può impedire l'estrazione della chiave; se il software compromesso può chiedere firme, non dimostra da solo la presenza di un'impronta valida. La password del sensore non rende automaticamente autentiche tutte le risposte UART.
- Cookie HttpOnly richiedono anche Secure e protezione CSRF adeguata. Un WebSocket o un ID casuale richiedono comunque associazione e autorizzazione della sessione. JWT e firma asimmetrica sono scelte da motivare per l'interfaccia amministrativa.
- In BIP-340 il tagged hash è `SHA256(SHA256(tag) || SHA256(tag) || messaggio)` e lo scalare della firma soddisfa `0 <= s < n`. Lo schema del progetto di riferimento usa una rappresentazione diversa. Il suo nonce HMAC personalizzato non equivale automaticamente a una costruzione standard verificata.
- La verifica dei punti richiede anche parsing canonico e intervalli validi, oltre all'equazione della curva. La frase «Schnorr corretto con pochi accorgimenti» non costituisce una validazione completa.
- Una catena di hash richiede un riferimento esterno affidabile per rilevare anche alcuni rollback o troncamenti. L'anchoring protegge i lotti depositati; definire la finestra degli eventi ancora non depositati. Una blockchain locale amministrata dal gruppo offre garanzie diverse da un registro indipendente.
- Nel nostro prototipo usare operatori fittizi e minimizzare i dati raccolti. Se si acquisiscono impronte reali, valutarne trattamento, conservazione e rischi. Gli obblighi GDPR dipendono dal trattamento concreto; l'art. 32 richiede misure adeguate al rischio e l'art. 35 disciplina la DPIA.

## Elementi da recuperare

- Integrazione con hardware reale e gestione guidata dell'enrollment.
- Gestione di timeout e annullamento, da adattare alla nostra macchina a stati.
- Driver con `load_template` e `compare_buffers`, utile per verificare la fattibilità del confronto 1:1 sul nostro sensore.
- Challenge casuali, con scadenza, legame con l'utente e uso singolo come principi del protocollo.
- Dimostrazione completa e interfaccia comprensibile, mantenendo i controlli nel backend e nel Pi.

## Riferimenti di verifica

- [BIP-340](https://github.com/bitcoin/bips/blob/master/bip-0340.mediawiki).
- [OWASP Session Management](https://cheatsheetseries.owasp.org/cheatsheets/Session_Management_Cheat_Sheet.html).
- [GDPR, testo ufficiale](https://eur-lex.europa.eu/eli/reg/2016/679/ojv).
