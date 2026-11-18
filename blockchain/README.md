# Registro on-chain dei lotti di eventi

Progetto Hardhat con il contratto Solidity `AuditAnchor`, che registra sulla blockchain l'impronta di ogni lotto di eventi di accesso verificati dal backend. Chi possiede gli eventi originali può ricalcolare l'impronta di un lotto e confrontarla con quella registrata: un'alterazione del database successiva alla registrazione diventa evidente.

Il formato degli eventi e la formazione dei lotti sono descritti in [`docs/event-schema.md`](../docs/event-schema.md) (§8).

## Contenuto

```
blockchain/
├── contracts/AuditAnchor.sol   contratto del registro
├── test/AuditAnchor.ts         test automatici (Mocha + ethers)
├── hardhat.config.ts           configurazione di Hardhat
├── package.json                dipendenze
└── package-lock.json           versioni e impronte di integrità installate
```

## Il contratto in breve

| Funzione | Chi può chiamarla | Cosa fa |
|---|---|---|
| `anchorBatch(batchId, root, firstEventTime, lastEventTime, eventCount)` | solo `PUBLISHER_ROLE` (account del backend) | Registra l'impronta di un nuovo lotto ed emette `BatchAnchored` |
| `getBatch(batchId)` | chiunque | Restituisce i dati di un lotto registrato |
| `findRoot(root)` | chiunque | Indica se un'impronta è registrata e in quale lotto |
| `grantRole` / `revokeRole` | solo `DEFAULT_ADMIN_ROLE` | Concede o revoca il ruolo di publisher, per esempio alla sostituzione della chiave del backend |

Proprietà di sicurezza:

- **Scrittura riservata.** Solo il publisher registra lotti; l'amministratore gestisce i ruoli ma non scrive lotti (separazione dei compiti). Ruoli gestiti da `AccessControl` di OpenZeppelin.
- **Lotti consecutivi.** Ogni lotto deve avere identificativo `nextBatchId`: non si può saltare un lotto né registrarlo due volte, anche quando il backend ritenta una transazione.
- **Impronte uniche e immutabili.** Un'impronta si registra una sola volta e non esistono funzioni di modifica o cancellazione.
- **Controlli prima della scrittura.** Impronta diversa da zero, almeno un evento, intervallo temporale coerente.
- **Superficie ridotta.** Il contratto non riceve ether e non chiama altri contratti, quindi non è esposto a rientranza.
- **Nessun dato personale on-chain.** Solo impronte da 32 byte, istanti e numero di eventi.

Limite da dichiarare: il registro garantisce l'integrità dei lotti **dopo** la registrazione. Gli eventi non ancora inclusi in un lotto registrato sono protetti solo dalla firma del gateway, e sulla rete locale di sviluppo il registro è controllato dal gruppo stesso.

## Comandi

Requisiti: Node.js 22 o successivo.

```bash
cd blockchain
npm ci
npm audit --omit=dev
npx hardhat test
```

Risultato atteso: `found 0 vulnerabilities` e `15 passing`.

`npm ci` installa esattamente le versioni registrate in `package-lock.json`. La prima esecuzione dei test scarica il compilatore Solidity indicato in `hardhat.config.ts`.

## Dipendenze e vulnerabilità note

Versioni di riferimento: Hardhat 3.22.0, Solidity 0.8.34, OpenZeppelin Contracts 5.6.1 (versione esatta).

Regola adottata, in linea con NIST SP 800-218 (SSDF), pratica RV.2:

- le dipendenze che entrano nel contratto distribuito devono avere **zero vulnerabilità note**: `npm audit --omit=dev` deve restituire `found 0 vulnerabilities`;
- le segnalazioni sugli strumenti di sviluppo sono ammesse solo se valutate e registrate nella tabella seguente;
- `npm audit` va ripetuto a ogni aggiornamento delle dipendenze; quando Hardhat adotta versioni corrette, si aggiorna e si rimuove la riga.

Valutazione del 18/11/2026: `npm audit` riporta 16 segnalazioni (13 alte, 3 moderate), tutte nelle dipendenze di sviluppo e riconducibili a tre avvisi.

| Pacchetto | Avviso | Gravità | Uso nel progetto | Valutazione |
|---|---|---|---|---|
| `zod` 3.25.76 | [GHSA-mrhx-c7xc-m3w5](https://github.com/advisories/GHSA-mrhx-c7xc-m3w5): esaurimento di memoria nella validazione di collezioni molto grandi | alta | Validazione di `hardhat.config.ts` da parte di Hardhat | Input scritto dal gruppo, solo sul PC di sviluppo. Effetto massimo: arresto dello strumento. La correzione richiede `zod` 4, non supportata da Hardhat 3: forzarla rischierebbe una validazione della configurazione silenziosamente errata. **Accettato.** |
| `glob` 13.x | [GHSA-3g3m-2fjr-6pw4](https://github.com/advisories/GHSA-3g3m-2fjr-6pw4): attraversamento del filesystem dopo il timeout | moderata | Ricerca dei file di test da parte di Mocha | Pattern definiti dal gruppo, nessun input esterno. **Accettato.** |
| `minimatch` 10.x | [GHSA-g3hv-75wg-m249](https://github.com/advisories/GHSA-g3hv-75wg-m249): denial of service con pattern costruiti ad arte | moderata | Come sopra, tramite Mocha | Come sopra. **Accettato.** |

La correzione proposta da `npm audit fix --force` consiste nel tornare a Hardhat 2 e non è stata applicata.

Lo script di installazione di `esbuild` (dipendenza di sviluppo) non è autorizzato: compilazione e test funzionano senza.

## Configurazione

`hardhat.config.ts` è quello generato dal modello ufficiale. La rete `sepolia` legge URL e chiave privata con `configVariable`, cioè da variabili d'ambiente o dal keystore cifrato di Hardhat: **nessuna chiave è scritta nel file**. Per la demo si usa la rete simulata locale.

I file per assistenti IA generati da `hardhat --init` (`AGENTS.md`, `CLAUDE.md`, `.agents/`, `.claude/`) sono esclusi dal `.gitignore` di questa cartella: nel repository vale l'`AGENTS.md` del gruppo.
