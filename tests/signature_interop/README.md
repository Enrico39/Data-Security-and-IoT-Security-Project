# Prova di interoperabilità della firma degli eventi

Questa prova verifica che il gateway (Raspberry Pi, Python) e il backend (Node.js) concordino su come si firmano e si verificano gli eventi di accesso. È la «prima prova di firma» prevista da [`architecture_guidelines.md`](../../architecture_guidelines.md) e usa il formato definito in [`docs/event-schema.md`](../../docs/event-schema.md).

Non richiede hardware, MQTT né database.

## Cosa viene verificato

| Aspetto | Scelta | Riferimento |
|---|---|---|
| Dati firmati | Forma canonica JSON dell'oggetto `event`, codificata in UTF-8 | [RFC 8785](https://www.rfc-editor.org/rfc/rfc8785) |
| Algoritmo | ECDSA su curva P-256 con SHA-256 (`ES256`) | [FIPS 186-5](https://csrc.nist.gov/pubs/fips/186-5/final) |
| Codifica della firma | DER, poi base64 standard | — |
| Nonce della firma | Deterministico (le stesse entrate producono gli stessi file) | [RFC 6979](https://www.rfc-editor.org/rfc/rfc6979) |

## Struttura

```
tests/signature_interop/
├── README.md
├── python/
│   ├── requirements.txt     librerie con versioni fissate
│   └── sign_vectors.py      simula il gateway: genera e firma i casi di prova
└── node/
    ├── package.json         libreria con versione fissata
    ├── package-lock.json    versioni e impronte di integrità installate
    └── verify_vectors.js    simula il backend: verifica i casi di prova

docs/test-vectors/
├── public_key.pem           chiave pubblica del gateway di prova
└── vectors.json             casi di prova con l'esito atteso
```

## Casi di prova

| Caso | Esito atteso | Cosa dimostra |
|---|---|---|
| `valid-granted` | valido | Accesso consentito, in turno, online |
| `valid-denied-out-of-shift` | valido | Accesso negato con motivo `OUT_OF_SHIFT` |
| `valid-granted-offline` | valido | Evento prodotto durante una disconnessione (`online: false`) |
| `invalid-tampered-type` | rifiutato | Un diniego trasformato in accesso dopo la firma viene scoperto |
| `invalid-wrong-key` | rifiutato | Un impostore che dichiara il `kid` del gateway ma firma con un'altra chiave viene scoperto |
| `invalid-malformed-signature` | rifiutato | Una firma non decodificabile produce un rifiuto, non un errore del backend |

Per i casi validi il file contiene anche la forma canonica attesa: il verificatore Node la confronta con la propria, così un eventuale errore si attribuisce subito alla canonicalizzazione o alla firma.

## Come eseguirla

Requisiti: Python 3.11 o successivo, Node.js 22 o successivo.

**Verifica (lato backend).** Basta questa parte per controllare i vettori già presenti nel repository:

```bash
cd tests/signature_interop/node
npm ci --ignore-scripts
npm audit
npm test
```

Risultato atteso: `6/6 casi con l'esito atteso.`

**Rigenerazione dei vettori (lato gateway).** Serve solo se cambiano il formato degli eventi o i casi di prova:

```bash
cd tests/signature_interop/python
python3 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
pip install -r requirements.txt
pip install pip-audit && pip-audit
python sign_vectors.py
```

Alla prima esecuzione lo script crea una chiave privata di prova in `python/secrets/gateway_test.key`. Le esecuzioni successive la riutilizzano e producono file identici. Se la chiave viene cancellata, se ne genera una nuova e cambiano `public_key.pem` e tutte le firme: in quel caso vanno aggiornati entrambi i file in `docs/test-vectors/`.

## Sicurezza

- La chiave privata di prova **non va mai nel repository**: il percorso è escluso dalle regole `secrets/` e `*.key` del `.gitignore`, e il file è creato con permessi `0600`. Chi verifica non ne ha bisogno, perché i vettori contengono già le firme.
- Tutti i dati sono fittizi: pseudonimo dell'operatore e hash della policy sono valori di esempio senza legame con persone o badge reali.
- La chiave dell'impostore usata nel caso `invalid-wrong-key` è ricavata da una stringa pubblica: **non è segreta** ed esiste solo per questa prova.
- Le librerie hanno versioni fissate. Prima di aggiornarle, eseguire `pip-audit` e `npm audit`. `npm ci --ignore-scripts` installa esattamente quanto registrato in `package-lock.json` senza eseguire gli script di installazione dei pacchetti.
- Gli script non aprono connessioni di rete.

## Uso nel progetto

- **Gateway:** `sign_vectors.py` mostra canonicalizzazione e firma da riprodurre nel servizio del Pi. Prima di adottarle vanno misurati i tempi sul Raspberry Pi Zero W e verificata la disponibilità delle librerie.
- **Backend:** la funzione `verifyEnvelope` di `verify_vectors.js` è la base della verifica nel backend, e `vectors.json` diventa un insieme di test automatici.
- **Fuori ambito:** controllo dei duplicati, dei numeri progressivi e del tempo (§7 di `event-schema.md`), che riguardano il backend vero.
