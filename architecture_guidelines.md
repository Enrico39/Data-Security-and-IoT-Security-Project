# Linee guida architetturali

## Schema dell'architettura

```mermaid
flowchart LR
    U["Operatore"] -->|"Badge, PIN e impronta"| A["Arduino Uno<br/>Lettore NFC, tastierino,<br/>sensore d'impronta e mini display<br/>con rotella e tasto OK"]
    A <-->|"USB seriale locale"| P["Raspberry Pi 1<br/>Verifica credenziali e applica la policy<br/>Firma gli eventi con ECDSA P-256"]
    P -->|"Comando GPIO"| L["LED rosso / verde<br/>Simulazione del varco"]
    P -->|"Eventi firmati via MQTT con mTLS"| B["Raspberry Pi 2 o PC<br/>Broker e backend<br/>Verifica firme e salva gli eventi"]
    B -->|"Hash del lotto di eventi"| C["Smart contract<br/>Registro degli hash"]

    style L fill:#dcfce7,stroke:#15803d,color:#14532d
```

Il LED è rosso quando il varco simulato è bloccato. Se il Pi autorizza l'accesso, diventa verde per cinque secondi e poi torna rosso.

## Flusso utente

| Passo | Cosa fa o vede l'utente | Cosa succede nel prototipo |
| --- | --- | --- |
| 1. Avvicina il badge | Il display mostra «Avvicina il badge». Il LED è rosso. | Il lettore NFC passa l'UID all'Arduino, che lo inoltra al Pi. Il Pi cerca il badge, controlla che sia attivo e avvia una sessione breve associata all'operatore. |
| 2. Inserisce il PIN | Digita sul tastierino, vede `••••` e preme OK. Con la rotella può selezionare «Annulla». | L'Arduino invia al Pi il PIN insieme all'ID della sessione sul collegamento seriale locale. Il Pi verifica il PIN e applica il limite ai tentativi. |
| 3. Appoggia il dito | Il display mostra «Appoggia il dito». | Il Pi indica quale template verificare, usando l'associazione con il badge. Se il sensore supporta il confronto 1:1, confronta l'impronta con quel template. L'Arduino inoltra al Pi l'esito associato alla stessa sessione. |
| 4. Riceve l'esito | Il display mostra «Accesso consentito» e il LED diventa verde, oppure mostra «Accesso negato» e il LED resta rosso. | Il Pi applica la policy locale, controllando ruolo, turno e revoca. Chiude la sessione dopo la decisione e registra l'esito. |
| 5. Il terminale torna pronto | Dopo cinque secondi il LED torna rosso e il display torna a «Avvicina il badge». | Il Pi termina la finestra di accesso simulata e il terminale si prepara al tentativo successivo. |

Se il badge espone soltanto un UID statico, l'UID serve a identificare il badge registrato ed è copiabile. Verificheremo se il badge, il lettore e la libreria consentono una **challenge-response crittografica** per autenticare il badge prima di chiedere il PIN. Se è possibile solo leggere l'UID, nella demo possiamo mostrare che presentarne una copia lascia comunque da superare PIN e impronta.

## Responsabilità hardware

- **Arduino Uno - terminale utente.** Legge NFC e tastierino, comunica con il sensore d'impronta e gestisce display, rotella e tasto OK. Invia gli input al Pi e mostra le istruzioni che riceve.
- **Raspberry Pi 1 - controllore locale.** Verifica il PIN, associa i risultati alla stessa sessione, applica la policy, comanda il LED tramite GPIO e firma gli eventi di audit. I LED sono collegati al Pi perché è il Pi a prendere la decisione.
- **Raspberry Pi 2 o PC - broker e backend.** Riceve gli eventi, verifica le firme, li salva e invia periodicamente allo smart contract l'hash di un lotto di eventi.
- **Smart contract - registro degli hash.** Registra gli hash pubblicati dall'account autorizzato del backend. Chi dispone degli eventi originali può ricalcolare l'hash e confrontarlo con quello registrato.

### Installazione a incasso di riferimento

Il pannello a vista ospita il display e le interfacce per badge, PIN e impronta. Arduino Uno e Raspberry Pi 1 sono alloggiati nella parete, dietro il pannello, con il cavo USB e i connettori interni. Per la demo con PIN fittizi assumiamo che il vano sia accessibile solo a chi si occupa della manutenzione. Con questa ipotesi, la seriale può viaggiare in chiaro.

L'incasso da solo non garantisce il perimetro fidato. Se rimuovere il frontale dà accesso al cavo o al bus del sensore, quei collegamenti vanno valutati separatamente. Il Pi controlla comunque fase e ID di sessione di ogni messaggio.

## Comunicazioni e cifratura

| Collegamento | Cosa viene mandato | Come viene protetto |
| --- | --- | --- |
| Periferiche ↔ Arduino | UID dal lettore, tasti premuti, comandi ed esito del sensore, istruzioni per il display. | I cablaggi interni rientrano nel perimetro fidato della demo. Un eventuale canale protetto Arduino-Pi copre la sola seriale USB. Il bus del sensore ha un perimetro di fiducia separato. |
| Arduino ↔ Pi 1 | UID, PIN, richiesta di verifica di un template, risultato biometrico e istruzioni per il display. | Per la demo con dati fittizi, la seriale può rientrare nel perimetro fidato se schede, cavo e connettori sono protetti e l'accesso fisico al bus è fuori dal modello di minaccia. Il Pi valida tipo, fase e ID sessione dei messaggi. Se il collegamento è esposto a lettura o iniezione, usare un canale autenticato e cifrato, con protezione contro il replay. |
| Pi 1 ↔ broker MQTT | Eventi di audit con ID pseudonimo, esito, orario, numero progressivo e versione della policy. | TLS 1.3 con mTLS cifra il traffico e autentica sia il gateway sia il broker tramite certificati. Il broker limita il gateway al topic assegnato. Il Pi firma inoltre gli eventi con ECDSA P-256, così la firma resta verificabile anche dopo il salvataggio. Riferimento: RFC 9846. |
| Backend → smart contract | Hash del lotto di eventi. | Il backend verifica le firme prima di calcolare l'hash e usa il proprio account autorizzato per registrarlo. Gli eventi completi restano nel database. |

## Gestione dei dati e significato degli eventi

Il PIN è presente in memoria durante l'inserimento e la verifica. Sul Pi si conserva il verificatore derivato con **Argon2id e salt**. Il template dell'impronta rimane nel sensore se il modulo scelto supporta il flusso descritto. PIN, immagini e template restano fuori dai log, da MQTT e dalla blockchain.

Nel registro si prevedono eventi come `ACCESS_GRANTED`, `ACCESS_DENIED` ed eventualmente `SIMULATED_UNLOCK_ENDED`. Il verde indica che il Pi ha autorizzato lo sblocco simulato. Il log documenta quella decisione e la fine della finestra, senza attribuire all'utente un passaggio fisico.

## Funzionamento offline

Se cade il Wi-Fi, il Pi continua a verificare badge, PIN e impronta e a comandare il LED finché la policy locale è valida. Conserva gli eventi firmati in una coda locale e li sincronizza quando torna la rete. Una revoca fatta sul backend durante la disconnessione arriva al Pi alla riconnessione. La scadenza della policy limita il tempo per cui il gateway può usare le informazioni già in suo possesso.

## Prove dimostrative

Le tre prove usano lo stesso operatore.

1. **In turno, credenziali corrette.** Il LED passa da rosso a verde per cinque secondi e compare `ACCESS_GRANTED`.
2. **Fuori turno, stesse credenziali.** Il LED resta rosso e compare `ACCESS_DENIED` con motivo «fuori turno». Si vede la differenza fra identità verificata e accesso autorizzato.
3. **In turno, Wi-Fi spento.** Il LED diventa verde e l'evento resta sul Pi. Quando si riattiva la rete, lo stesso evento arriva al backend e la sua firma viene verificata.

## Decisioni consolidate dalle lezioni del progetto precedente

Le motivazioni, le evidenze nel codice di riferimento e le prove da eseguire sono raccolte in [docs/lessons_learned.md](docs/lessons_learned.md).

- **Sessioni.** Il Pi gestisce una sessione breve per tentativo con ID, utente, fase e scadenza. Accetta ogni fattore solo nella fase prevista. Annullamento, timeout e decisione invalidano la sessione; risultati tardivi o di un'altra sessione vengono rifiutati. Un solo flusso alla volta possiede le periferiche del terminale.
- **Identità.** L'ID operatore è stabile e separato dall'UID del badge e dallo slot biometrico. La riassegnazione di uno slot non trasferisce vecchie credenziali al nuovo utente.
- **Amministrazione ed enrollment.** Le operazioni amministrative richiedono autorizzazione sul servizio che le esegue. L'enrollment ha stati espliciti e attiva la credenziale solo dopo la conferma delle associazioni richieste. Errori parziali devono poter essere recuperati o ripuliti.
- **Revoca.** Lo stato di accesso e la pulizia fisica dei template sono distinti. Una pulizia fallita resta tracciata e ripetibile. Il funzionamento offline rispetta la scadenza della policy locale e la finestra dichiarata di mancata conoscenza delle revoche.
- **Audit.** Gli eventi hanno un identificativo univoco, una rappresentazione deterministica e una firma. Backend e coda persistente gestiscono riavvii e ritrasmissioni senza duplicare l'evento. Registrare separatamente ricezione, verifica e anchoring confermato.
- **Fiducia.** Documentare protezione delle chiavi, accesso ai cablaggi e limiti del gateway compromesso. La firma attesta il gateway; il significato fisico dipende dai suoi input. Per l'anchoring dichiarare chi controlla la rete e quali eventi sono già coperti da un deposito verificato.

## Scelte tecnologiche aperte - riunione del gruppo

**Stato: da compilare con tutti e tre i componenti.** Riunione prevista per l'11 novembre 2026. Questa sezione guida le decisioni e non assegna automaticamente uno stack.

### Come usare questa sezione

1. Confermare hardware disponibile, tempo e richieste dei docenti, incluso l'eventuale obbligo di implementare una primitiva crittografica.
2. Scegliere piattaforma blockchain e linguaggio del contratto insieme.
3. Definire protocolli, gestione delle identità e dati firmati prima di selezionare le librerie.
4. Provare sensore e badge. Valutare la cifratura sull'Uno se la seriale è fuori dal perimetro fidato.
5. Scegliere backend, storage, interfaccia e strumenti di verifica in base alle integrazioni necessarie.

Per ogni scelta registrare motivazione, riferimento al corso, prova minima, responsabile e stato. Un'opzione rimane aperta finché manca la prova che ne conferma la fattibilità. Le versioni saranno fissate dopo la scelta e prima dell'implementazione integrata.

Le linee guida precedenti costituiscono la base attuale: MQTT con mTLS, firme ECDSA P-256 e Argon2id per il PIN. La riunione deve confermare queste scelte e completarne i dettagli; eventuali cambiamenti vanno motivati e riportati anche nelle sezioni precedenti e in AGENTS.md.

### Priorità per la prima demo

| Priorità | Cosa decidere o verificare |
| --- | --- |
| **Essenziale** | Inventario hardware, lettura del badge e confronto 1:1, stack minimo sul Pi e sul backend, firma degli eventi, mTLS MQTT, protezione del PIN, piattaforma del contratto e rete per eseguirlo. |
| **Da definire durante l'implementazione** | Formato dei messaggi, versioni delle librerie, QoS, frequenza dei lotti, parametri Argon2id e gestione dei certificati. |
| **Dipende dal perimetro fisico** | Cifratura autenticata della seriale Arduino-Pi. La decisione dipende da chi può leggere o modificare il traffico sul cavo. |
| **Verifica aperta** | Challenge crittografica del badge. L'UID basta a identificare il badge nella prima demo; la challenge serve a dimostrare il possesso crittografico. |

### Opzioni facoltative

| Opzione | Quando aggiunge valore |
| --- | --- |
| **Testnet pubblica** | Serve a provare il deployment su una rete esterna e a mostrare la transazione in un explorer pubblico. Per dimostrare registrazione e verifica dei lotti basta una rete di sviluppo locale. Nel repository dei colleghi non risultano blockchain, contratti o testnet. [Reti di sviluppo Ethereum](https://ethereum.org/developers/docs/development-networks/) |
| **Merkle tree** | Serve a verificare l'inclusione di un singolo evento senza recuperare tutto il lotto. Per verificare il lotto intero basta il suo hash. |
| **Dashboard React** | Serve se volete esplorare l'audit e mostrare gli esiti in un'interfaccia web. Per la prima integrazione bastano il LED e un comando o una pagina che mostri gli eventi verificati. |
| **Docker** | Può semplificare l'avvio riproducibile dei servizi sul PC. |
| **Secure element** | Protegge la chiave privata dall'estrazione quando questo rischio rientra negli obiettivi della demo. |
| **JWT** | È un'opzione per le sessioni dell'interfaccia web se quella funzionalità viene sviluppata. |

### A. Blockchain e smart contract

Nei materiali del corso compaiono Ethereum/Solidity e Hyperledger Fabric/Go. Negli appunti della lezione 14 la parte Fabric/Go è indicata come integrazione dalle slide, con svolgimento a lezione da confermare. Portare alla riunione gli esempi effettivamente usati dal gruppo.

Solidity è un linguaggio per contratti EVM. Go può essere usato per chaincode Fabric e anche per servizi esterni: il suo ruolo va specificato. Scegliere una piattaforma per la prima versione del progetto.

| Argomento | Domanda da risolvere | Percorsi da discutere | Decisione del gruppo |
| --- | --- | --- | --- |
| Piattaforma e linguaggio | Su quale registro eseguiamo la logica? | Ethereum/EVM + Solidity; Hyperledger Fabric + chaincode Go. Confrontare familiarità, configurazione e risorse del PC/server. | Da compilare |
| Rete della demo | Chi controlla il registro e quali garanzie vogliamo dimostrare? | Rete di sviluppo EVM locale; eventuale testnet successiva; rete Fabric di prova se si sceglie Fabric. Distinguere sviluppo locale da registro indipendente. | Da compilare |
| Funzioni del contratto | Quali dati registra e chi può scriverli? | Hash del lotto, ID lotto e autorizzazione del publisher. Definire duplicati, amministratore e cambio del publisher. | Da compilare |
| Formazione del lotto | Come colleghiamo eventi e hash depositato? | Ordinamento e serializzazione deterministici; hash dell'intero lotto o Merkle root se serve provare singoli eventi. Definire frequenza o dimensione del lotto. | Da compilare |
| Integrazione e conferma | Come il backend sa che il deposito è riuscito? | Client della piattaforma scelta, ricevuta/esito verificato, ritentativi e recupero dopo riavvio. | Da compilare |
| Sviluppo e test | Come compiliamo, testiamo e distribuiamo? | EVM: esempi Remix del corso e possibile automazione con Hardhat; Fabric: esempi e strumenti chaincode del corso. Fissare versioni e comando di avvio. | Da compilare |

Fonti tecniche: [linguaggi Ethereum](https://ethereum.org/developers/docs/smart-contracts/languages/), [reti di sviluppo Ethereum](https://ethereum.org/developers/docs/development-networks/), [chaincode Fabric](https://hyperledger-fabric.readthedocs.io/en/latest/chaincode_lifecycle.html).

### B. Crittografia e gestione delle chiavi

Compilare una scelta per ogni uso. La curva della firma dei log, quella usata da TLS e quella prevista dalla blockchain possono essere diverse. Firma, cifratura, hash e derivazione del PIN svolgono funzioni distinte.

| Uso | Cosa occorre decidere | Base attuale e punti aperti | Decisione del gruppo |
| --- | --- | --- | --- |
| Firma degli eventi | Algoritmo, curva, libreria, encoding della firma e byte esatti firmati. | ECDSA P-256 come base. Confermare la libreria sul Pi e il verificatore sul backend; includere contesto di protocollo e ID gateway nei dati firmati. | Da compilare |
| Seriale Arduino-Pi | Il cavo e i connettori saranno nel perimetro fidato? | Con seriale interna protetta e PIN di prova, la demo può usare il collegamento in chiaro. Se il cavo è esposto, definire cifratura autenticata, chiavi, nonce e protezione dal replay e provarli sull'Uno. | Da compilare |
| MQTT con mTLS | Versione TLS, certificati, algoritmo delle chiavi, CA e rinnovo/revoca. | TLS 1.3 previsto. Distinguere autenticazione con certificato, accordo di chiavi e cifratura della sessione. Confermare compatibilità broker/client. | Da compilare |
| PIN | Libreria, parametri di derivazione e politica dei tentativi. | Argon2id con salt sul Pi. Misurare memoria e latenza sull'hardware disponibile; definire contatori, blocco e persistenza al riavvio. | Da compilare |
| Hash degli eventi e dei lotti | Funzione di hash, encoding, ordine e concatenazione. | Da specificare. SHA-256 è un candidato per i dati applicativi; se si usa EVM distinguere Keccak-256 da SHA3-256 nelle operazioni della piattaforma. | Da compilare |
| Firma della transazione blockchain | Chi firma il deposito e dove conserva la chiave? | Account backend della piattaforma scelta. In EVM la firma dell'account usa secp256k1, distinta dalla P-256 dei log; per Fabric seguire identità e policy della rete scelta. | Da compilare |
| Ciclo di vita delle chiavi | Come vengono generate, installate, protette, sostituite e revocate? | Distinguere chiave seriale, firma gateway, TLS/CA e publisher blockchain. Decidere se protezione software è sufficiente alla demo e dichiararne i limiti. | Da compilare |

Per ciascun algoritmo scelto aggiungere riferimento ufficiale e libreria/versione verificata. La citazione di un algoritmo o una RFC non definisce da sola un protocollo completo.

### C. Hardware, comunicazione e applicazione

| Argomento | Domande per il gruppo | Vincoli o alternative da valutare | Decisione del gruppo |
| --- | --- | --- | --- |
| Inventario hardware | Quali modelli e quantità abbiamo? | Pi e RAM disponibili, Uno, RC522, badge, sensore biometrico, modello del display/rotella, LED e collegamenti. | Da compilare |
| Badge | Leggiamo l'UID o possiamo autenticare il badge? | Identificare chip, lettore e libreria; prova di lettura separata dalla prova challenge-response. | Da compilare |
| Biometria | Possiamo fare confronto 1:1 e conservare i template nel sensore? | Confermare sul modello reale acquisizione, slot, cancellazione ed esito; un solo proprietario della periferica. | Da compilare |
| Firmware | Come sviluppiamo e compiliamo per l'Uno? | C/C++ con ambiente Arduino; scegliere IDE/toolchain e librerie, eventualmente PlatformIO se utile al gruppo. | Da compilare |
| Servizio locale sul Pi | Quale linguaggio gestisce seriale, sessioni, policy e LED? | Python è un candidato coerente col riferimento; valutare Go se conosciuto e supportato dalle librerie necessarie. | Da compilare |
| Messaggi seriali | Come riconosciamo messaggi completi, malformati o fuori fase? | Framing, dimensione massima, encoding, versione, ID sessione, timeout, ACK ed errori. Scegliere un formato sostenibile sull'Uno. | Da compilare |
| MQTT | Quale broker/client e quali regole di consegna usiamo? | Mosquitto come candidato; scegliere versione protocollo, topic, ACL, QoS e persistenza. Deduplicare a livello applicativo. | Da compilare |
| Backend | Quale linguaggio/framework riceve, verifica e consulta gli eventi? | Valutare Python/Flask del riferimento o strumenti già noti al gruppo. Definire API e responsabilità prima del framework. | Da compilare |
| Storage | Dove conserviamo operatori, policy, coda offline e audit? | SQLite è un candidato per la demo. Distinguere dati sul Pi e backend, vincoli univoci, transazioni, backup e protezione a riposo. | Da compilare |
| Policy e offline | Come rappresentiamo ruolo, turno, revoca e scadenza? | Regole locali esplicite o strumento studiato nel corso se motivato; decidere aggiornamento autenticato, versione e gestione del tempo non affidabile. | Da compilare |
| Dashboard e amministrazione | Quali pagine servono e come autentichiamo gli amministratori? | UI essenziale o React se utile e conosciuto. Ruoli applicati dalle API; scegliere sessioni/cookie o token e relative protezioni. | Da compilare |

### D. Prove, avvio e organizzazione

| Argomento | Risultato da concordare | Decisione del gruppo |
| --- | --- | --- |
| Validazione | Strumenti di test per linguaggi scelti; prove su sessioni, permessi, replay, firme, revoca, riavvii, offline e contratto. Distinguere test simulati e hardware. | Da compilare |
| Avvio riproducibile | Versioni delle dipendenze, configurazione di esempio e comandi per ciascun dispositivo. Valutare script, servizi o container in base all'hardware. | Da compilare |
| Dati e privacy | Utenti fittizi, eventuali impronte reali, cancellazione e conservazione, contenuto dei log. | Da compilare |
| Responsabilità | Referente hardware/protocollo; referente identità/policy; referente backend/audit/blockchain. Revisione incrociata e integrazione condivisa. | Da compilare |

### Scheda da compilare per ogni scelta

- **Argomento e stato:** da discutere / da provare / confermato.
- **Scelta e versione:** da compilare.
- **Motivo e compromesso accettato:** da compilare.
- **Lezione, slide o esempio del corso:** da compilare.
- **Documentazione primaria:** da compilare.
- **Prova minima e risultato atteso:** da compilare.
- **Responsabile e revisore:** da compilare.
- **Esito della prova e limiti rimasti:** da compilare.

### Esito della riunione

- **Partecipanti:** da compilare.
- **Decisioni confermate:** da compilare.
- **Decisioni rinviate e prove necessarie:** da compilare.
- **Attività assegnate e prossima verifica:** da compilare.
