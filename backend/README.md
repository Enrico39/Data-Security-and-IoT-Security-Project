# Backend: ricezione, verifica e archiviazione degli eventi

Il backend Node.js riceve tramite MQTT gli eventi firmati dai gateway, li verifica secondo [`docs/event-schema.md`](../docs/event-schema.md), li archivia in MongoDB e conferma al gateway fino a quale evento non ci sono mancanze. Broker MQTT e database girano in Docker.

## Struttura

```
backend/
├── package.json              dipendenze a versione esatta e comandi npm
├── config/gateways.json      registro delle chiavi pubbliche dei gateway autorizzati
├── src/
│   ├── config.js             lettura del .env; avvio rifiutato se manca un valore
│   ├── event-schema.js       validazione rigida della struttura degli eventi (§2)
│   ├── signature.js          forma canonica RFC 8785 e verifica ECDSA P-256 (§4)
│   ├── ingest.js             controlli di ricezione e archiviazione idempotente (§7)
│   ├── index.js              collegamento a MQTT e MongoDB, pubblicazione delle conferme (§5.1)
│   └── store/
│       ├── mongo-store.js    archivio MongoDB con indice univoco (gateway_id, seq)
│       └── memory-store.js   archivio in memoria per i test
├── test/                     test automatici (node:test)
├── scripts/
│   ├── replay-vectors.js     gateway simulato: pubblica docs/test-vectors sul broker
│   └── check-db.sh           controllo del database con l'utente applicativo
├── docker-compose.yml        Mosquitto e MongoDB
└── infra/                    configurazione di Mosquitto e inizializzazione di MongoDB
```

## Elaborazione di un evento

`ingest.js` applica a ogni messaggio, nell'ordine:

1. topic `ac/v1/<gateway_id>/events` e dimensione massima di 8 KB;
2. JSON valido e struttura della busta e dell'evento: campi non previsti, come un UID del badge, fanno rifiutare il messaggio (minimizzazione dei dati);
3. `gateway_id` dell'evento uguale a quello del topic, indipendentemente dall'ACL del broker;
4. firma: algoritmo `ES256`, chiave nota e appartenente al gateway dichiarato, firma valida sulla forma canonica;
5. istante non successivo all'orologio del backend di oltre 2 minuti; gli eventi passati sono ammessi perché possono provenire dalla coda offline;
6. archiviazione: un evento già presente con lo stesso contenuto è un duplicato e viene riconfermato; lo stesso `(gateway_id, seq)` con contenuto diverso è un conflitto, registrato come errore;
7. calcolo di `acked_up_to`, il numero progressivo fino al quale non mancano eventi, pubblicato come messaggio retained su `ac/v1/<gateway_id>/ack`, e segnalazione dei buchi nella sequenza.

I messaggi sono elaborati uno alla volta nell'ordine di arrivo. La sessione MQTT del backend è persistente: con il backend spento il broker conserva i messaggi QoS 1 e li consegna alla riconnessione. I log sono righe JSON con identificativi tecnici ed esiti, senza lo pseudonimo dell'operatore.

I numeri progressivi di ogni gateway partono da 1.

## Servizi

| Servizio | Immagine | Indirizzo sul PC | Ruolo |
|---|---|---|---|
| Mosquitto | `eclipse-mosquitto:2.1.2-alpine` | `127.0.0.1:1884` | Broker MQTT fra gateway e backend |
| MongoDB | `mongo:8.0.38` | `127.0.0.1:27018` | Archivio degli eventi verificati |

Le versioni delle immagini sono fissate: non si usa `latest`.

## Scelte di sicurezza

- **Solo localhost.** Le porte sono pubblicate su `127.0.0.1`: i servizi non sono raggiungibili da altri computer della rete. Le porte 1884 e 27018 evitano conflitti con altre istanze locali di Mosquitto o MongoDB.
- **Nessun accesso anonimo a MQTT.** Ogni client ha utente e password.
- **Permessi per topic (ACL).** Il backend legge gli eventi di tutti i gateway e scrive conferme e policy. Ogni gateway usa il proprio `gateway_id` come nome utente e può pubblicare solo su `ac/v1/<gateway_id>/events` e leggere solo le proprie conferme e la propria policy.
- **Minimo privilegio su MongoDB.** L'utente amministratore serve solo all'inizializzazione; il backend usa un utente con ruolo `readWrite` limitato al database `access_audit`, autenticato con SCRAM-SHA-256.
- **Segreti fuori dal repository.** Le password stanno in `.env` (permessi `0600`) e gli hash MQTT in `infra/mosquitto/passwd` (proprietario: utente del broker, permessi `0600`). Entrambi sono esclusi da Git. Se una variabile manca, Docker Compose rifiuta di avviare i servizi.
- **Dati nei volumi Docker**, non nella cartella del progetto.
- **Nessun avvio automatico** (`restart: "no"`).

Il canale MQTT è per ora in chiaro, limitato a localhost. TLS 1.3 con autenticazione reciproca (mTLS) è previsto in una fase successiva.

## Prima configurazione

Requisiti: Docker con Docker Compose e Node.js 22 o successivo. Tutti i comandi si eseguono dalla cartella `backend/`.

**1. Creare `.env` con password casuali** a partire da `.env.example`:

```bash
gen() { openssl rand -base64 48 | tr -dc 'A-Za-z0-9' | head -c 32; }
umask 077
sed -e "s/^MONGO_ROOT_PASSWORD=.*/MONGO_ROOT_PASSWORD=$(gen)/" \
    -e "s/^MONGO_APP_PASSWORD=.*/MONGO_APP_PASSWORD=$(gen)/" \
    -e "s/^MQTT_BACKEND_PASSWORD=.*/MQTT_BACKEND_PASSWORD=$(gen)/" \
    -e "s/^MQTT_GATEWAY_PASSWORD=.*/MQTT_GATEWAY_PASSWORD=$(gen)/" \
    .env.example > .env
umask 022
```

**2. Generare il file delle password del broker** dagli stessi valori:

```bash
(
  set -a; . ./.env; set +a
  docker run --rm --entrypoint sh \
    -e MQTT_BACKEND_USER -e MQTT_BACKEND_PASSWORD \
    -e MQTT_GATEWAY_USER -e MQTT_GATEWAY_PASSWORD \
    -v "$PWD/infra/mosquitto:/m" \
    eclipse-mosquitto:2.1.2-alpine -c '
      rm -f /m/passwd &&
      mosquitto_passwd -H sha512-pbkdf2 -c -b /m/passwd "$MQTT_BACKEND_USER" "$MQTT_BACKEND_PASSWORD" &&
      mosquitto_passwd -H sha512-pbkdf2 -b /m/passwd "$MQTT_GATEWAY_USER" "$MQTT_GATEWAY_PASSWORD" &&
      chown mosquitto:mosquitto /m/passwd && chmod 0600 /m/passwd'
)
```

Le password sono lette in una sotto-shell e passate al container come variabili d'ambiente, senza comparire nella cronologia dei comandi.

**3. Avviare e controllare:**

```bash
docker compose config --quiet && echo "configurazione valida"
docker compose up -d
docker compose ps
```

`docker compose config` va usato con `--quiet`: senza, stampa la configurazione completa con le password.

**4. Installare le dipendenze e lanciare i test:**

```bash
npm ci --ignore-scripts
npm audit
npm test
```

Risultato atteso: `found 0 vulnerabilities` e tutti i test superati.

**5. Avviare il backend** (il terminale resta occupato; si ferma con Ctrl+C):

```bash
npm start
```

**6. Prova completa**, in un secondo terminale:

```bash
npm run replay
bash scripts/check-db.sh
```

**Spegnimento** a fine sessione: Ctrl+C sul backend, poi `docker compose stop`. I dati restano nei volumi; `docker compose down -v` li cancella.

## Chiavi dei gateway

`config/gateways.json` elenca i gateway autorizzati e, per ciascuno, gli identificativi (`kid`) e i file delle chiavi **pubbliche**. Più chiavi per gateway permettono la rotazione. Il gateway di prova `gw-test-01` usa `docs/test-vectors/public_key.pem`, generata da `tests/signature_interop`: se i vettori vengono rigenerati con una nuova chiave, i test e il backend usano automaticamente quella nuova.

## Verifica dei controlli di accesso MQTT

Prove eseguite il 24/11/2026 con i client MQTT del container:

| Prova | Esito atteso | Esito |
|---|---|---|
| Connessione anonima | Rifiutata | `Connection Refused: not authorised` |
| Password errata | Rifiutata | `Connection Refused: not authorised` |
| Gateway che pubblica sul proprio topic | Ricevuta dal backend | Ricevuta |
| Gateway che pubblica sul topic di un altro gateway | Scartata dal broker | `Publish failed: Not authorized` (MQTT 5); nessun messaggio ricevuto dal backend |

Con MQTT 3.1.1 il broker scarta in silenzio le pubblicazioni non autorizzate e il client non riceve errori. Per questo il backend verifica comunque che il `gateway_id` dell'evento coincida con quello del topic (§7 di `event-schema.md`): il controllo dell'ACL e quello del backend sono indipendenti.

## Prova completa con i servizi reali

Eseguita il 25/11/2026 con Mosquitto 2.1.2, MongoDB 8.0.38 e i vettori di `docs/test-vectors`, pubblicati con le credenziali del gateway di prova:

| Verifica | Esito |
|---|---|
| Tre eventi validi (seq 1, 2, 3) | Verificati, salvati e confermati: `acked_up_to = 3` |
| Evento manomesso dopo la firma | Rifiutato: firma non valida |
| Evento firmato con una chiave estranea | Rifiutato: firma non valida |
| Firma malformata | Rifiutata, senza interruzione del servizio |
| Seconda pubblicazione degli stessi vettori | Tre duplicati riconosciuti, nessun documento aggiunto, conferma invariata |
| Contenuto del database | 3 eventi, indice `gateway_seq_unique`, stato `acked_up_to: 3` |
| Utente applicativo verso il database `admin` | Accesso negato |

## Limiti noti

| Punto | Situazione | Motivazione e azione |
|---|---|---|
| Algoritmo degli hash MQTT | `mosquitto_passwd` 2.1 dichiara Argon2id come predefinito, ma l'immagine ufficiale `2.1.2-alpine` non lo supporta: con `-H argon2id` restituisce `Unable to hash password` e, senza `-H`, ripiega senza avvisi su PBKDF2-SHA512 con 1000 iterazioni. Si usa quindi `-H sha512-pbkdf2` in modo esplicito. | Le iterazioni proteggono soprattutto password scelte da persone. Qui le password sono casuali da 32 caratteri alfanumerici (circa 190 bit), non indovinabili per tentativi. Rigenerare con Argon2id quando l'immagine ufficiale lo supporterà. |
| Permessi del file `acl` | Mosquitto avvisa che il file è leggibile da tutti e non appartiene all'utente `mosquitto`, e che versioni future lo rifiuteranno. | Il file non contiene segreti ed è versionato. La sistemazione dei permessi di tutti i file del broker è prevista con l'introduzione di mTLS. |
| Controllo della policy | Il controllo di `policy_version` e `policy_hash` contro le versioni emesse (§7, punto 7) non è ancora attivo: la distribuzione firmata della policy non è implementata. | Da aggiungere insieme alla pubblicazione della policy e al contratto `PolicyRegistry`. |
| Compatibilità di MongoDB con il kernel | Le note di rilascio di MongoDB 8.0 segnalano crash all'avvio con i kernel Linux dalla 6.19 alla 7.0.13. Il kernel WSL in uso è il 6.18. | Verificare `uname -r` dopo gli aggiornamenti di WSL. |
