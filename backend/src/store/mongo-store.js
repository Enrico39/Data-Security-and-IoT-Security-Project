/**
 * @file Archivio degli eventi su MongoDB.
 *
 * Collezioni:
 * - `events`: un documento per evento verificato, con la busta completa così
 *   come ricevuta (la firma resta verificabile in seguito) e gli istanti di
 *   ricezione e di ancoraggio. Un indice univoco su (gateway_id, seq) rende
 *   impossibili i duplicati anche in caso di elaborazioni concorrenti.
 * - `gateway_state`: per ogni gateway, il progressivo fino al quale non
 *   mancano eventi, cioè il valore confermato al gateway.
 */

import { MongoClient } from 'mongodb';

/** Codice di errore di MongoDB per violazione di un indice univoco. */
const DUPLICATE_KEY = 11000;

/** @implements {import('./memory-store.js').EventStore} */
export class MongoEventStore {
  /**
   * @param {import('mongodb').Db} db - Database dell'applicazione.
   */
  constructor(db) {
    this.events = db.collection('events');
    this.state = db.collection('gateway_state');
  }

  /**
   * Si connette a MongoDB, crea gli indici se mancano e restituisce l'archivio.
   *
   * @param {string} uri - URI di connessione con le credenziali dell'utente applicativo.
   * @param {string} dbName - Nome del database.
   * @returns {Promise<{ store: MongoEventStore, client: MongoClient }>}
   */
  static async connect(uri, dbName) {
    const client = new MongoClient(uri, { serverSelectionTimeoutMS: 5000 });
    await client.connect();
    const store = new MongoEventStore(client.db(dbName));
    await store.events.createIndex(
      { gateway_id: 1, seq: 1 },
      { unique: true, name: 'gateway_seq_unique' },
    );
    await store.state.createIndex({ gateway_id: 1 }, { unique: true, name: 'gateway_unique' });
    return { store, client };
  }

  /**
   * Salva un evento. Se (gateway_id, seq) esiste già, confronta l'impronta
   * dell'evento: uguale → ritrasmissione, diversa → conflitto.
   *
   * @param {import('./memory-store.js').EventRecord} record
   * @returns {Promise<'accepted'|'duplicate'|'conflict'>}
   */
  async insertEvent(record) {
    try {
      await this.events.insertOne({
        gateway_id: record.gatewayId,
        seq: record.seq,
        envelope: record.envelope,
        event_sha256: record.eventSha256,
        received_at: record.receivedAt,
        // Valorizzato quando il lotto che contiene l'evento è registrato on-chain.
        anchored_at: null,
      });
      return 'accepted';
    } catch (err) {
      if (err?.code !== DUPLICATE_KEY) throw err;
      const existing = await this.events.findOne(
        { gateway_id: record.gatewayId, seq: record.seq },
        { projection: { event_sha256: 1 } },
      );
      return existing?.event_sha256 === record.eventSha256 ? 'duplicate' : 'conflict';
    }
  }

  /**
   * Avanza e restituisce il progressivo fino al quale non mancano eventi.
   *
   * @param {string} gatewayId
   * @returns {Promise<number>}
   */
  async advanceContiguous(gatewayId) {
    const state = await this.state.findOne({ gateway_id: gatewayId });
    let upTo = state?.acked_up_to ?? 0;
    while (await this.events.countDocuments({ gateway_id: gatewayId, seq: upTo + 1 }, { limit: 1 })) {
      upTo += 1;
    }
    await this.state.updateOne(
      { gateway_id: gatewayId },
      { $set: { acked_up_to: upTo, updated_at: new Date() } },
      { upsert: true },
    );
    return upTo;
  }
}
