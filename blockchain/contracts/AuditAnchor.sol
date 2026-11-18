// SPDX-License-Identifier: MIT
pragma solidity 0.8.34;

import {AccessControl} from "@openzeppelin/contracts/access/AccessControl.sol";

/**
 * @title AuditAnchor
 * @notice Registro on-chain dei lotti di eventi di accesso del varco.
 *
 * Il backend raccoglie gli eventi firmati dai gateway, ne verifica le firme,
 * li salva nel proprio database e, a intervalli, li raggruppa in lotti. Per
 * ogni lotto calcola un'impronta (radice di Merkle o hash del lotto) e la
 * registra qui. Chi possiede gli eventi originali può ricalcolare l'impronta e
 * confrontarla con quella registrata: se il database viene alterato dopo la
 * registrazione, la differenza è evidente.
 *
 * Sulla blockchain non finisce alcun dato personale: solo impronte da 32 byte,
 * intervalli temporali e numero di eventi (principio "dati off-chain, prove
 * on-chain", docs/event-schema.md §1 e §8).
 *
 * @dev Scelte di sicurezza:
 * - Solo gli account con ruolo PUBLISHER_ROLE possono registrare lotti
 *   (pattern Access Restriction). La gestione dei ruoli è delegata ad
 *   AccessControl di OpenZeppelin invece di essere riscritta.
 * - I lotti hanno identificativi consecutivi a partire da 0. Un lotto non può
 *   essere saltato né registrato due volte: chi confronta il registro con il
 *   database si accorge sia di un lotto mancante sia di un troncamento.
 * - Ogni lotto è scritto una sola volta e non esiste alcuna funzione per
 *   modificarlo o cancellarlo.
 * - Tutti i controlli sugli argomenti precedono la scrittura (pattern Guard
 *   Check). Il contratto non riceve né trasferisce ether e non chiama altri
 *   contratti, quindi non è esposto a rientranza.
 */
contract AuditAnchor is AccessControl {
    /// @notice Ruolo dell'account del backend autorizzato a registrare i lotti.
    bytes32 public constant PUBLISHER_ROLE = keccak256("PUBLISHER_ROLE");

    /**
     * @notice Dati registrati per un lotto.
     * @param root Impronta del lotto (radice di Merkle o hash), mai zero.
     * @param firstEventTime Istante del primo evento del lotto, in secondi Unix.
     * @param lastEventTime Istante dell'ultimo evento del lotto, in secondi Unix.
     * @param eventCount Numero di eventi inclusi nel lotto, almeno uno.
     * @param anchoredAt Istante del blocco in cui il lotto è stato registrato.
     */
    struct Batch {
        bytes32 root;
        uint64 firstEventTime;
        uint64 lastEventTime;
        uint32 eventCount;
        uint64 anchoredAt;
    }

    /// @notice Identificativo che deve avere il prossimo lotto registrato.
    /// Coincide con il numero di lotti già registrati.
    uint64 public nextBatchId;

    /// @dev Lotti registrati, indicizzati per identificativo.
    mapping(uint64 batchId => Batch) private _batches;

    /// @dev Identificativo del lotto + 1 per ogni impronta già registrata.
    /// Lo 0 indica "impronta mai registrata"; per questo si somma 1.
    mapping(bytes32 root => uint64) private _batchIdPlusOneByRoot;

    /**
     * @notice Emesso quando un lotto viene registrato.
     * @param batchId Identificativo del lotto.
     * @param root Impronta del lotto.
     * @param firstEventTime Istante del primo evento, in secondi Unix.
     * @param lastEventTime Istante dell'ultimo evento, in secondi Unix.
     * @param eventCount Numero di eventi nel lotto.
     * @param publisher Account che ha registrato il lotto.
     */
    event BatchAnchored(
        uint64 indexed batchId,
        bytes32 indexed root,
        uint64 firstEventTime,
        uint64 lastEventTime,
        uint32 eventCount,
        address indexed publisher
    );

    /// @notice L'identificativo non è quello atteso (lotto saltato o ripetuto).
    error UnexpectedBatchId(uint64 expected, uint64 provided);

    /// @notice L'impronta è zero, valore non ammesso.
    error ZeroRoot();

    /// @notice L'impronta è già stata registrata per un altro lotto.
    error RootAlreadyAnchored(bytes32 root, uint64 existingBatchId);

    /// @notice Il lotto non contiene eventi.
    error EmptyBatch();

    /// @notice L'intervallo temporale è invertito.
    error InvalidTimeRange(uint64 firstEventTime, uint64 lastEventTime);

    /// @notice Il lotto richiesto non esiste.
    error BatchNotFound(uint64 batchId);

    /// @notice Un indirizzo obbligatorio è l'indirizzo zero.
    error ZeroAddress();

    /**
     * @notice Crea il registro e assegna i ruoli iniziali.
     * @param admin Account che amministra i ruoli (concede e revoca
     *        PUBLISHER_ROLE, per esempio quando si sostituisce la chiave del
     *        backend).
     * @param publisher Account del backend autorizzato a registrare i lotti.
     */
    constructor(address admin, address publisher) {
        if (admin == address(0) || publisher == address(0)) revert ZeroAddress();
        _grantRole(DEFAULT_ADMIN_ROLE, admin);
        _grantRole(PUBLISHER_ROLE, publisher);
    }

    /**
     * @notice Registra l'impronta di un nuovo lotto di eventi.
     * @dev L'identificativo deve essere esattamente nextBatchId: impedisce di
     *      saltare lotti e di registrarne uno due volte, anche quando il
     *      backend ritenta una transazione dopo un errore di rete.
     * @param batchId Identificativo del lotto, uguale a nextBatchId.
     * @param root Impronta del lotto, diversa da zero e mai registrata prima.
     * @param firstEventTime Istante del primo evento, in secondi Unix.
     * @param lastEventTime Istante dell'ultimo evento, in secondi Unix,
     *        non precedente a firstEventTime.
     * @param eventCount Numero di eventi del lotto, almeno uno.
     */
    function anchorBatch(
        uint64 batchId,
        bytes32 root,
        uint64 firstEventTime,
        uint64 lastEventTime,
        uint32 eventCount
    ) external onlyRole(PUBLISHER_ROLE) {
        if (batchId != nextBatchId) revert UnexpectedBatchId(nextBatchId, batchId);
        if (root == bytes32(0)) revert ZeroRoot();
        uint64 existing = _batchIdPlusOneByRoot[root];
        if (existing != 0) revert RootAlreadyAnchored(root, existing - 1);
        if (eventCount == 0) revert EmptyBatch();
        if (firstEventTime > lastEventTime) revert InvalidTimeRange(firstEventTime, lastEventTime);

        _batches[batchId] = Batch({
            root: root,
            firstEventTime: firstEventTime,
            lastEventTime: lastEventTime,
            eventCount: eventCount,
            anchoredAt: uint64(block.timestamp)
        });
        _batchIdPlusOneByRoot[root] = batchId + 1;
        nextBatchId = batchId + 1;

        emit BatchAnchored(batchId, root, firstEventTime, lastEventTime, eventCount, msg.sender);
    }

    /**
     * @notice Restituisce i dati di un lotto registrato.
     * @param batchId Identificativo del lotto.
     * @return Il lotto, se esiste.
     */
    function getBatch(uint64 batchId) external view returns (Batch memory) {
        if (batchId >= nextBatchId) revert BatchNotFound(batchId);
        return _batches[batchId];
    }

    /**
     * @notice Indica se un'impronta è registrata e in quale lotto.
     * @param root Impronta da cercare.
     * @return found Vero se l'impronta è registrata.
     * @return batchId Identificativo del lotto (0 se found è falso).
     */
    function findRoot(bytes32 root) external view returns (bool found, uint64 batchId) {
        uint64 stored = _batchIdPlusOneByRoot[root];
        if (stored == 0) return (false, 0);
        return (true, stored - 1);
    }
}
