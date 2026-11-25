/**
 * @file Archivio degli eventi in memoria, usato nei test automatici.
 *
 * Implementa la stessa interfaccia di MongoEventStore, così la logica di
 * ingest.js si prova senza database. Non va usato in esecuzione: i dati si
 * perdono alla chiusura del processo.
 */

/**
 * Evento da archiviare.
 * @typedef {object} EventRecord
 * @property {string} gatewayId - Gateway che ha prodotto l'evento.
 * @property {number} seq - Numero progressivo dell'evento per quel gateway.
 * @property {object} envelope - Busta completa ricevuta (evento e firma).
 * @property {string} eventSha256 - SHA-256 della forma canonica dell'evento.
 * @property {Date} receivedAt - Istante di ricezione e verifica.
 */

/**
 * Interfaccia comune degli archivi.
 * @typedef {object} EventStore
 * @property {(record: EventRecord) => Promise<'accepted'|'duplicate'|'conflict'>} insertEvent
 *   Salva un evento; se (gatewayId, seq) esiste già, confronta il contenuto.
 * @property {(gatewayId: string) => Promise<number>} advanceContiguous
 *   Restituisce il massimo n tale che gli eventi 1..n del gateway siano tutti
 *   presenti (0 se manca il primo).
 */

/** @implements {EventStore} */
export class MemoryEventStore {
  constructor() {
    /** @type {Map<string, EventRecord>} */
    this.events = new Map();
    /** @type {Map<string, number>} */
    this.contiguous = new Map();
  }

  /**
   * @param {EventRecord} record
   * @returns {Promise<'accepted'|'duplicate'|'conflict'>}
   */
  async insertEvent(record) {
    const id = `${record.gatewayId}:${record.seq}`;
    const existing = this.events.get(id);
    if (existing) return existing.eventSha256 === record.eventSha256 ? 'duplicate' : 'conflict';
    this.events.set(id, structuredClone(record));
    return 'accepted';
  }

  /**
   * @param {string} gatewayId
   * @returns {Promise<number>}
   */
  async advanceContiguous(gatewayId) {
    let upTo = this.contiguous.get(gatewayId) ?? 0;
    while (this.events.has(`${gatewayId}:${upTo + 1}`)) upTo += 1;
    this.contiguous.set(gatewayId, upTo);
    return upTo;
  }
}
