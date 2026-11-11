# Istruzioni per gli agenti

## Contesto e fonti

Questo repository contiene un progetto universitario congiunto di Sicurezza dei dati e IoT Security, sviluppato da tre persone. Il prototipo controlla un varco simulato con LED rosso/verde, usando badge NFC, PIN e impronta.

Prima di modificare il progetto, leggere `architecture_guidelines.md` e i file del componente interessato. Usare il documento come riferimento architetturale e mantenere codice e documentazione coerenti. In caso di ambiguità, distinguere le decisioni già concordate dalle proposte e dalle capacità hardware ancora da verificare.

Consultare anche `docs/lessons_learned.md` per evidenze, motivazioni e prove derivate dal progetto di riferimento. La sezione delle scelte tecnologiche aperte in `architecture_guidelines.md` è da compilare con il gruppo: i candidati elencati non sono decisioni approvate. Confermare le prove hardware e registrare le decisioni prima di introdurre uno stack.

Il repository è attualmente nella fase di definizione. Ricavare stack, struttura e comandi dai file presenti. Aggiungere dipendenze e componenti quando servono al lavoro richiesto.

## Confini architetturali

- Arduino Uno gestisce lettore NFC, tastierino, sensore d'impronta, mini display, rotella e tasto OK.
- Raspberry Pi Zero W (Pi 1) esegue il servizio locale in Python, verifica le credenziali, applica le policy locali, comanda il LED tramite GPIO e firma gli eventi.
- Il PC ospita Mosquitto e lo stack MERN. Il backend verifica e conserva gli eventi e pubblica gli hash dei lotti.
- Il contratto Solidity, sviluppato con Hardhat, registra gli hash dei lotti tramite un account autorizzato. La decisione di accesso rimane locale al Pi 1.
- Il LED verde rappresenta una finestra di accesso autorizzata di cinque secondi. Gli eventi devono descrivere la simulazione, senza dichiarare un ingresso fisico osservato.

## Hardware e verifiche aperte

Il lettore acquistato è un RC522 MINI da 13,56 MHz, alimentato a 3,3 V e con interfaccia SPI. Il modello del badge e l'hardware del collega devono essere identificati.

Verificare sui modelli effettivi e sulle relative librerie la possibilità di autenticazione challenge-response. La lettura dell'UID e il completamento dell'autenticazione crittografica sono verifiche distinte. Documentare il risultato prima di dichiarare il supporto della challenge o la robustezza del fattore di possesso.

Verificare il supporto del sensore al confronto biometrico 1:1 e alla memorizzazione interna dei template. Confermare interfacce, livelli elettrici e pinout su documentazione del produttore prima di proporre cablaggi. Misurare memoria e tempi delle librerie crittografiche sull'Uno.

## Requisiti di sicurezza

- Legare badge, PIN ed esito biometrico allo stesso utente e alla stessa sessione breve, utilizzabile una sola volta.
- Applicare l'autorizzazione sul Pi, includendo ruolo, turno e revoca. Un'autenticazione riuscita deve comunque passare questi controlli.
- Documentare il perimetro della seriale Arduino-Pi e validare messaggi, fase e ID sessione. Per la demo con PIN fittizi, il collegamento può essere in chiaro se cavo e connettori sono nel perimetro fisico fidato. Se il bus è esposto, usare cifratura autenticata, protezione dal replay e gestione sicura del riavvio, con nonce univoci per chiave e sessioni del canale autenticate.
- Usare primitive e librerie consolidate. Non progettare algoritmi crittografici propri. Documentare formato dei messaggi, gestione delle chiavi e condizioni di rifiuto.
- Conservare il verificatore del PIN con Argon2id e salt. Applicare rate limiting e ridurre la permanenza del PIN in memoria, documentando i limiti della piattaforma.
- Proteggere MQTT con TLS e autenticazione reciproca, verifica dei certificati e permessi per topic. Firmare gli eventi con ECDSA P-256.
- Firmare una rappresentazione deterministica degli eventi. Il backend deve rifiutare firme invalide e gestire ritrasmissioni senza creare duplicati.
- Tenere PIN, campioni e template biometrici fuori da log, MQTT e blockchain. Usare identità pseudonime e dati fittizi negli esempi.
- Rispettare `.gitignore`. Conservare chiavi private, credenziali e dati di enrollment in percorsi locali esclusi da Git. Fornire configurazioni di esempio prive di segreti.
- Rendere esplicite durata della cache offline, scadenza della policy, gestione del tempo e comportamento dopo riavvii o errori. Sincronizzare la coda degli eventi in modo recuperabile.
- Distinguere l'integrità verificabile dei log dalla veridicità degli input fisici. Documentare il perimetro fidato dei cablaggi e i limiti dell'ancoraggio sulla blockchain scelta.

## Lezioni da applicare durante lo sviluppo

- Usare ID operatore stabili, separati da UID e slot biometrici. Legare ogni risultato alla sessione e alla fase corretta, rifiutando risposte tardive.
- Autorizzare enrollment e operazioni amministrative nel servizio che le esegue. Gestire esplicitamente stati incompleti e pulizia ripetibile; lo slot 0 è valido.
- Limitare accesso alle periferiche a un proprietario per volta. Rendere recuperabili persistenza e ritrasmissioni degli eventi.
- Rifiutare configurazioni prive dei segreti richiesti e verificare le protezioni nella configurazione realmente avviata.
- Non importare automaticamente dal riferimento le sue scelte di JWT, dashboard, Schnorr o database. Applicare le decisioni del gruppo e motivare le dipendenze.

## Metodo di lavoro

Preferire modifiche piccole, leggibili e verificabili. Riutilizzare le convenzioni e la logica presenti. Evitare funzionalità estranee alla richiesta e dipendenze superflue per i dispositivi disponibili. Rimuovere il nome dell'agente IA dalla firma dei commit github. L'autore del commit deve essere solo l'utente.

Scrivere documentazione e spiegazioni in italiano. Usare identificatori di codice coerenti con il componente, preferibilmente in inglese. Aggiornare le linee guida quando una modifica concordata cambia l'architettura o il comportamento osservabile.

Consultare fonti primarie per standard, protocolli e capacità hardware. Citare norma, versione e collegamento quando pertinenti. Separare requisiti normativi, scelte progettuali e ipotesi. Dichiarare una conformità soltanto se verificata.

## Verifica e consegna

Eseguire i controlli disponibili per il componente modificato, ricavando i comandi dalle configurazioni reali. Per modifiche di sicurezza verificare anche rifiuti, timeout, replay, manomissioni, disconnessioni e riavvii pertinenti. Indicare chiaramente quali prove richiedono hardware e quali sono state eseguite in simulazione.

Preservare i tre scenari dimostrativi concordati:

1. Credenziali corrette e operatore in turno, con LED verde per cinque secondi.
2. Stesse credenziali fuori turno, con LED rosso e accesso negato.
3. Accesso consentito offline entro la validità della policy, seguito dalla sincronizzazione e verifica della firma.

Nel resoconto delle modifiche indicare comportamento ottenuto, verifiche eseguite e limiti ancora aperti. Non dichiarare superati controlli non eseguiti.
