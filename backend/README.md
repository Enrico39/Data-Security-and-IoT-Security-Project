# Backend: ricezione, verifica e archiviazione degli eventi

Questa cartella conterrà il backend Node.js che riceve gli eventi firmati dai gateway tramite MQTT, li verifica secondo [`docs/event-schema.md`](../docs/event-schema.md), li archivia in MongoDB e li conferma al gateway. Per ora contiene l'**ambiente di sviluppo**: broker MQTT e database in Docker.

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

Requisiti: Docker e Docker Compose. Tutti i comandi si eseguono dalla cartella `backend/`.

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

**Spegnimento** a fine sessione: `docker compose stop`. I dati restano nei volumi; `docker compose down -v` li cancella.

## Verifica dei controlli di accesso MQTT

Prove eseguite il 24/11/2026 con i client MQTT del container:

| Prova | Esito atteso | Esito |
|---|---|---|
| Connessione anonima | Rifiutata | `Connection Refused: not authorised` |
| Password errata | Rifiutata | `Connection Refused: not authorised` |
| Gateway che pubblica sul proprio topic | Ricevuta dal backend | Ricevuta |
| Gateway che pubblica sul topic di un altro gateway | Scartata dal broker | `Publish failed: Not authorized` (MQTT 5); nessun messaggio ricevuto dal backend |

Con MQTT 3.1.1 il broker scarta in silenzio le pubblicazioni non autorizzate e il client non riceve errori. Per questo il backend verifica comunque che il `gateway_id` dell'evento coincida con quello del topic (§7 di `event-schema.md`): il controllo dell'ACL e quello del backend sono indipendenti.

## Limiti noti

| Punto | Situazione | Motivazione e azione |
|---|---|---|
| Algoritmo degli hash MQTT | `mosquitto_passwd` 2.1 dichiara Argon2id come predefinito, ma l'immagine ufficiale `2.1.2-alpine` non lo supporta: con `-H argon2id` restituisce `Unable to hash password` e, senza `-H`, ripiega senza avvisi su PBKDF2-SHA512 con 1000 iterazioni. Si usa quindi `-H sha512-pbkdf2` in modo esplicito. | Le iterazioni proteggono soprattutto password scelte da persone. Qui le password sono casuali da 32 caratteri alfanumerici (circa 190 bit), non indovinabili per tentativi. Rigenerare con Argon2id quando l'immagine ufficiale lo supporterà. |
| Permessi del file `acl` | Mosquitto avvisa che il file è leggibile da tutti e non appartiene all'utente `mosquitto`, e che versioni future lo rifiuteranno. | Il file non contiene segreti ed è versionato. La sistemazione dei permessi di tutti i file del broker è prevista con l'introduzione di mTLS. |
| Compatibilità di MongoDB con il kernel | Le note di rilascio di MongoDB 8.0 segnalano crash all'avvio con i kernel Linux dalla 6.19 alla 7.0.13. Il kernel WSL in uso è il 6.18. | Verificare `uname -r` dopo gli aggiornamenti di WSL. |
