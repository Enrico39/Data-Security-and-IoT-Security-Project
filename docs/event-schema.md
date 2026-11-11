# Formato degli eventi e interfaccia gateway ↔ backend

**Stato:** bozza da approvare (v0.2, allineata alle decisioni della riunione dell'11 novembre 2026)
**Ambito:** definisce l'unico punto di contatto fra la parte IoT (Arduino, Raspberry Pi 1) e la parte Data Security (backend, blockchain, DApp). Tutto ciò che sta prima della pubblicazione MQTT è responsabilità del gateway; tutto ciò che sta dopo è responsabilità del backend.

**Componenti coinvolti** (da `architecture_guidelines.md`):

| Lato | Piattaforma | Ruolo in questo documento |
|---|---|---|
| Gateway | Raspberry Pi Zero W, servizio in Python, SQLite locale | Firma e pubblica gli eventi, conserva la coda offline, riceve la policy |
| Broker | Mosquitto sul PC, mTLS, ACL per topic | Trasporto |
| Backend | Node.js + Express, MQTT.js, MongoDB | Verifica, deduplica, archivia, conferma, forma i lotti |
| Registro | Contratto Solidity su rete Hardhat locale | Ancoraggio dei lotti e delle versioni di policy |

---

## 1. Principi

1. **Minimizzazione.** Un evento non contiene mai: PIN, template o immagini dell'impronta, UID del badge in chiaro, nome dell'operatore.
2. **Pseudonimizzazione.** L'operatore è identificato da uno pseudonimo calcolato con una funzione a chiave (§3). Solo il titolare del trattamento, che possiede la chiave e l'anagrafica dei badge, può ricondurlo a una persona.
3. **Firma alla fonte.** Ogni evento è firmato dal gateway nel momento in cui prende la decisione; la firma resta verificabile anche dopo il salvataggio nel database.
4. **Idempotenza.** Il backend deve poter ricevere lo stesso evento più volte (ritrasmissioni, coda offline) senza duplicarlo.
5. **Nulla di personale on-chain.** Sulla blockchain vanno solo radici di Merkle dei lotti di eventi e hash delle versioni della policy.

---

## 2. Struttura dell'evento

L'evento viaggia come JSON UTF-8, in una busta che separa il contenuto dalla firma:

```json
{
  "event": {
    "ctx": "unisa-ac/event/v1",
    "v": 1,
    "gateway_id": "gw-01",
    "terminal_id": "term-01",
    "seq": 1042,
    "ts": "2026-11-10T20:41:07.512Z",
    "type": "ACCESS_DENIED",
    "subject": "b3f1c9e0a7d4...e21f",
    "factors": { "badge": "ok", "pin": "ok", "bio": "ok" },
    "reason": "OUT_OF_SHIFT",
    "policy_version": 7,
    "policy_hash": "9c2e...41ab",
    "online": true
  },
  "sig": {
    "alg": "ES256",
    "kid": "gw-01-2026-01",
    "value": "MEUCIQ...base64"
  }
}
```

### Campi di `event`

| Campo | Tipo | Obbligatorio | Descrizione |
|---|---|---|---|
| `ctx` | stringa | sì | Contesto di protocollo, fisso: `unisa-ac/event/v1`. Essendo dentro i dati firmati, impedisce che una firma del gateway prodotta per un altro tipo di messaggio (per esempio una conferma o un documento di policy) venga riusata come firma di un evento. |
| `v` | intero | sì | Versione di questo schema. Oggi `1`. |
| `gateway_id` | stringa | sì | Identificativo del Raspberry Pi che decide. Deve coincidere con il topic MQTT (§5) e con il CN del certificato mTLS. |
| `terminal_id` | stringa | sì | Terminale Arduino da cui proviene il tentativo. |
| `seq` | intero ≥ 0 | sì | Numero progressivo **per gateway**, strettamente crescente, **persistito in SQLite sul Pi** (non si azzera al riavvio). Insieme a `gateway_id` identifica l'evento in modo univoco: l'ID evento è la coppia `(gateway_id, seq)`. |
| `ts` | stringa | sì | Istante della decisione, RFC 3339 in UTC con millisecondi. |
| `type` | enum | sì | Vedi §2.1. |
| `subject` | stringa esadecimale | sì per gli eventi di accesso | Pseudonimo dell'operatore (§3). Assente solo negli eventi di sistema. Se il badge non è registrato si usa lo pseudonimo calcolato sull'UID letto, così i tentativi ripetuti con un badge sconosciuto restano correlabili senza esporre l'UID. |
| `factors` | oggetto | sì per gli eventi di accesso | Esito di ciascun fattore: `ok`, `fail`, `skipped` (non richiesto perché un fattore precedente è fallito). |
| `reason` | enum | sì se `ACCESS_DENIED` | Vedi §2.2. |
| `policy_version` | intero | sì | Versione della policy applicata nella decisione. |
| `policy_hash` | stringa esadecimale | sì | SHA-256 del documento di policy applicato (§6). Permette all'auditor di verificare on-chain che la policy usata fosse quella registrata. |
| `online` | booleano | sì | `false` se la decisione è stata presa mentre il gateway era disconnesso dal broker. |

### 2.1 Tipi di evento

| `type` | Quando |
|---|---|
| `ACCESS_GRANTED` | Tutti i fattori verificati e policy soddisfatta: LED verde. |
| `ACCESS_DENIED` | Un fattore fallito o policy non soddisfatta: LED rosso. |
| `SIMULATED_UNLOCK_ENDED` | Fine della finestra di sblocco simulato (5 s). |
| `GATEWAY_STARTED` | Avvio del gateway (utile a rilevare riavvii anomali). |
| `POLICY_APPLIED` | Il gateway ha installato una nuova versione della policy. |

### 2.2 Motivi di diniego (`reason`)

`BADGE_UNKNOWN`, `BADGE_REVOKED`, `PIN_WRONG`, `PIN_LOCKED` (superato il limite di tentativi), `BIO_MISMATCH`, `OUT_OF_SHIFT`, `ROLE_NOT_ALLOWED`, `POLICY_EXPIRED`, `TIMEOUT`, `CANCELLED` (l'utente ha scelto «Annulla»).

---

## 3. Pseudonimo dell'operatore

```
subject = hex( HMAC-SHA256( K_pseud, badge_uid ) )
```

- `K_pseud` è una chiave di 32 byte generata dal titolare, installata sul gateway in fase di provisioning e conservata dal backend. Non compare mai negli eventi né nei log.
- Senza `K_pseud`, lo pseudonimo non è invertibile né calcolabile a partire da un UID: questo distingue la pseudonimizzazione a chiave da un semplice hash dell'UID, che sarebbe ricalcolabile da chiunque conosca o enumeri gli UID.
- La corrispondenza pseudonimo → persona è tenuta solo nell'anagrafica del titolare, separata dal registro degli eventi.

---

## 4. Firma

- **Algoritmo:** ECDSA su curva P-256 con SHA-256 (`ES256`).
- **Dati firmati:** la forma canonica di `event` secondo **JSON Canonicalization Scheme ([RFC 8785](https://www.rfc-editor.org/rfc/rfc8785))**, codificata in UTF-8. Si firma il solo oggetto `event`, mai la busta.
- **Codifica della firma:** **DER** (sequenza ASN.1 di `r` e `s`, 70–72 byte), poi base64 standard. DER è il formato predefinito sia della libreria `cryptography` in Python sia del modulo `crypto` di Node.js, come previsto dalla prima prova di firma nelle linee guida.
- **`kid`:** identifica la chiave pubblica del gateway. Il backend mantiene l'elenco `kid → chiave pubblica`; una rotazione di chiave introduce un nuovo `kid` senza invalidare gli eventi già firmati.

La canonicalizzazione è necessaria perché due serializzazioni JSON dello stesso oggetto (ordine dei campi, spazi) produrrebbero firme diverse. Per restare dentro il sottoinsieme in cui RFC 8785 è banale da rispettare, gli eventi contengono solo stringhe, interi, booleani e oggetti: **nessun numero decimale**.

### 4.1 Librerie

| Lato | Canonicalizzazione | Firma / verifica |
|---|---|---|
| Pi (Python) | [`rfc8785`](https://pypi.org/project/rfc8785/) | `cryptography`: `private_key.sign(data, ec.ECDSA(hashes.SHA256()))` |
| Backend (Node.js) | [`canonicalize`](https://www.npmjs.com/package/canonicalize) | `crypto.verify('sha256', data, { key, dsaEncoding: 'der' }, signature)` |

Le versioni vanno fissate dopo la prima prova di interoperabilità.

### 4.2 Vettori di prova

Il gruppo mantiene in `docs/test-vectors/` alcuni eventi di esempio con chiave pubblica, forma canonica attesa e firma, prodotti dal Pi. Il backend li usa nei test automatici. Devono comprendere almeno: un evento valido, lo stesso evento con un campo alterato (firma da rifiutare), una firma di un'altra chiave, una firma malformata.

---

## 5. Trasporto MQTT

| Topic | Chi pubblica | Chi sottoscrive | QoS | Retain |
|---|---|---|---|---|
| `ac/v1/{gateway_id}/events` | gateway | backend | 1 | no |
| `ac/v1/{gateway_id}/ack` | backend | gateway | 1 | sì |
| `ac/v1/{gateway_id}/policy` | backend | gateway | 1 | sì |

- **ACL del broker:** ogni gateway può pubblicare solo su `ac/v1/{proprio id}/events` e sottoscrivere solo i propri `ack` e `policy`. Il backend ha un account distinto.
- **Canale:** TLS 1.3 con autenticazione mutua; il CN del certificato del gateway coincide con `gateway_id`.

### 5.1 Conferma e coda offline

Il PUBACK di MQTT conferma solo che il **broker** ha ricevuto il messaggio, non che il backend lo abbia verificato e salvato. Per questo:

1. il gateway conserva ogni evento nella propria coda locale persistente fino a conferma;
2. dopo averlo verificato e salvato, il backend pubblica su `ack` il messaggio `{"acked_up_to": <seq>}`, cioè il numero progressivo più alto fino al quale ha ricevuto **tutti** gli eventi senza buchi;
3. il gateway cancella dalla coda gli eventi con `seq ≤ acked_up_to`;
4. alla riconnessione il gateway ritrasmette, in ordine di `seq`, tutto ciò che è ancora in coda.

---

## 6. Policy e revoche (backend → gateway)

La policy è un documento JSON firmato dal backend con una chiave dedicata (`ES256`, stessa codifica del §4) e pubblicato come messaggio retained su `ac/v1/{gateway_id}/policy`:

```json
{
  "policy": {
    "version": 7,
    "issued_at": "2026-11-10T08:00:00Z",
    "expires_at": "2026-11-11T08:00:00Z",
    "subjects": {
      "b3f1c9e0a7d4...e21f": { "role": "technician", "active": true }
    },
    "rules": [
      { "role": "technician", "terminal_id": "term-01",
        "shifts": [ { "days": [1,2,3,4,5], "from": "08:00", "to": "16:00" } ] }
    ]
  },
  "sig": { "alg": "ES256", "kid": "policy-2026-01", "value": "..." }
}
```

- **Revoca:** si imposta `active: false` (oppure si rimuove lo pseudonimo) e si emette una nuova versione. Ogni revoca incrementa `version`.
- **Scadenza:** oltre `expires_at` il gateway nega tutti gli accessi con `POLICY_EXPIRED`. È questo che limita per quanto tempo un gateway disconnesso può continuare ad applicare una policy vecchia, ignorando revoche avvenute nel frattempo.
- **Verifica sul gateway:** firma valida, `version` maggiore di quella installata, `expires_at` nel futuro. Altrimenti il documento viene scartato.
- **Ancoraggio:** per ogni versione il backend registra `(version, SHA-256 della forma canonica di policy)` nel contratto `PolicyRegistry`. Il gateway non deve parlare con la blockchain.

---

## 7. Validazione lato backend

Per ogni evento ricevuto, nell'ordine:

1. **Schema:** campi obbligatori presenti e dei tipi previsti; `v` supportata.
2. **Mittente:** `gateway_id` nell'evento uguale a quello del topic.
3. **Firma:** `kid` noto e associato a quel `gateway_id`; firma valida sulla forma canonica.
4. **Duplicati:** se `(gateway_id, seq)` è già presente con lo stesso contenuto, si scarta senza errore e si riconferma l'ack; se è presente con contenuto **diverso**, è un'anomalia grave da segnalare.
5. **Buchi:** se `seq` salta dei valori, l'evento si accetta ma il buco viene segnalato finché non arrivano i mancanti.
6. **Tempo:** `ts` non può essere nel futuro oltre una tolleranza (es. 2 minuti). Gli eventi `online: false` possono essere vecchi.
7. **Policy:** `(policy_version, policy_hash)` deve corrispondere a una versione emessa.

Gli eventi che superano i controlli vengono salvati integralmente (busta compresa) in MongoDB e accodati al lotto corrente. La collezione degli eventi ha un **indice univoco** su `(event.gateway_id, event.seq)`: un inserimento duplicato fallisce a livello di database, e il backend lo tratta come il caso 4 dei controlli. Per ogni evento il backend registra separatamente tre istanti: **ricezione**, **verifica** e **ancoraggio confermato** (transazione inclusa in un blocco). Finché l'ancoraggio non è confermato, l'evento è protetto solo dalla firma del gateway: questa finestra va dichiarata nella documentazione.

---

## 8. Lotti e ancoraggio on-chain

*Proposta: le linee guida confermano l'ancoraggio di un hash per lotto; l'albero di Merkle compare fra le opzioni facoltative ed è proposto come parte della componente di sicurezza dei dati.*

- Il backend chiude un lotto ogni *N* eventi o ogni *T* minuti, quale delle due condizioni si verifica prima.
- Con gli eventi del lotto costruisce un **albero di Merkle** con separazione di dominio fra foglie e nodi interni, come in RFC 6962:
  - foglia: `SHA-256(0x00 || canonical(busta))`
  - nodo interno: `SHA-256(0x01 || sinistro || destro)`
- La radice viene registrata nel contratto `AuditAnchor` insieme a numero del lotto e intervallo temporale.
- Per un singolo evento, il backend fornisce la **prova di inclusione** (gli hash fratelli lungo il cammino). Chiunque abbia l'evento e la prova può ricalcolare la radice e confrontarla con quella on-chain, senza vedere gli altri eventi del lotto.

---

## 9. Punti da decidere insieme

Già decisi nella riunione dell'11 novembre: servizio del gateway in Python, backend Node.js + Express con MongoDB, MQTT con mTLS e QoS 1, conferma applicativa e deduplicazione per ID, prima prova di firma in DER.

Restano aperti:

1. **Conservazione delle chiavi sul Pi:** chiave di firma ECDSA e `K_pseud` su file con permessi ristretti, oppure su un secure element (es. ATECC608)? Il limite va dichiarato.
2. **Enrollment:** badge, PIN (verificatore Argon2id) e slot biometrici si registrano localmente sul Pi; al backend arrivano solo pseudonimo e ruolo?
3. **Più terminali:** uno o più terminali per gateway nella demo?
4. **Parametri:** dimensione e durata dei lotti (*N*, *T*), validità della policy, tolleranza sull'orario.
5. **Prestazioni sul Pi Zero W:** tempi di firma ECDSA, canonicalizzazione e Argon2id da misurare sull'hardware reale.
6. **Albero di Merkle:** adozione per i lotti (§8).
