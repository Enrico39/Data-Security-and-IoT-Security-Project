/**
 * @file Ricezione di un evento dal broker: controlli e archiviazione.
 *
 * Implementa la validazione lato backend di docs/event-schema.md (§7):
 *
 *  1. dimensione e sintassi JSON del messaggio;
 *  2. struttura della busta e dell'evento;
 *  3. coerenza fra gateway del topic e gateway dichiarato nell'evento;
 *  4. firma;
 *  5. istante non nel futuro oltre la tolleranza;
 *  6. archiviazione idempotente: un duplicato identico è accettato senza
 *     effetti, un duplicato con contenuto diverso è un'anomalia;
 *  7. calcolo del numero progressivo fino al quale non mancano eventi,
 *     da confermare al gateway (§5.1).
 *
 * Il modulo non conosce MQTT né MongoDB: riceve topic e messaggio grezzi e
 * usa un archivio astratto. Questo permette di provarlo con un archivio in
 * memoria e di riusarlo con qualsiasi trasporto.
 */

import { validateEvent } from './event-schema.js';
import { canonicalSha256, verifyEnvelopeSignature } from './signature.js';

/** Topic degli eventi: ac/v1/<gateway_id>/events (§5). */
const EVENTS_TOPIC = /^ac\/v1\/([^/+#]+)\/events$/;

/** Campi ammessi nella busta e nella firma. */
const ENVELOPE_FIELDS = ['event', 'sig'];
const SIG_FIELDS = ['alg', 'kid', 'value'];

/**
 * Esito dell'elaborazione di un messaggio.
 * @typedef {object} IngestResult
 * @property {'accepted'|'duplicate'|'rejected'|'conflict'} status
 *   - accepted: evento nuovo, verificato e salvato;
 *   - duplicate: ritrasmissione identica di un evento già salvato;
 *   - rejected: messaggio scartato (vedi reason);
 *   - conflict: stesso (gateway_id, seq) di un evento già salvato ma
 *     contenuto diverso; possibile malfunzionamento o compromissione.
 * @property {string} reason - Motivo, adatto ai log (mai dati dell'operatore).
 * @property {string} [gatewayId] - Gateway del topic, se riconosciuto.
 * @property {number} [seq] - Numero progressivo dell'evento, se leggibile.
 * @property {number} [ackedUpTo] - Progressivo da confermare al gateway.
 * @property {{ from: number, to: number }} [gap] - Eventi mancanti prima di questo.
 */

/**
 * Controlla che un oggetto abbia esattamente i campi indicati.
 * @param {unknown} value
 * @param {string[]} fields
 * @returns {boolean}
 */
function hasExactFields(value, fields) {
  if (typeof value !== 'object' || value === null || Array.isArray(value)) return false;
  const keys = Object.keys(value);
  return keys.length === fields.length && fields.every((field) => keys.includes(field));
}

/**
 * Elabora un messaggio ricevuto sul topic degli eventi.
 *
 * @param {{ topic: string, payload: Buffer | string }} message - Messaggio MQTT grezzo.
 * @param {object} deps - Dipendenze.
 * @param {Map<string, import('./signature.js').GatewayKey>} deps.keys - Registro delle chiavi.
 * @param {import('./store/memory-store.js').EventStore} deps.store - Archivio degli eventi.
 * @param {() => Date} [deps.now] - Orologio, sostituibile nei test.
 * @param {number} [deps.maxClockSkewMs] - Anticipo massimo ammesso rispetto all'orologio del backend.
 * @param {number} [deps.maxMessageBytes] - Dimensione massima del messaggio.
 * @returns {Promise<IngestResult>}
 */
export async function ingestMessage(message, deps) {
  const {
    keys,
    store,
    now = () => new Date(),
    maxClockSkewMs = 2 * 60 * 1000,
    maxMessageBytes = 8 * 1024,
  } = deps;

  const topicMatch = EVENTS_TOPIC.exec(message.topic);
  if (!topicMatch) return { status: 'rejected', reason: 'topic non previsto' };
  const gatewayId = topicMatch[1];

  // 1. Dimensione e JSON. Il limite evita di allocare memoria per messaggi
  //    enormi inviati per esaurire le risorse del backend.
  const payload = Buffer.isBuffer(message.payload) ? message.payload : Buffer.from(message.payload);
  if (payload.length > maxMessageBytes) return { status: 'rejected', reason: 'messaggio troppo grande', gatewayId };
  let envelope;
  try {
    envelope = JSON.parse(payload.toString('utf8'));
  } catch {
    return { status: 'rejected', reason: 'JSON non valido', gatewayId };
  }

  // 2. Struttura.
  if (!hasExactFields(envelope, ENVELOPE_FIELDS) || !hasExactFields(envelope.sig, SIG_FIELDS)) {
    return { status: 'rejected', reason: 'busta malformata', gatewayId };
  }
  const schema = validateEvent(envelope.event);
  if (!schema.ok) return { status: 'rejected', reason: `evento non valido: ${schema.reason}`, gatewayId };
  const { event } = envelope;

  // 3. Il gateway può pubblicare solo a proprio nome. L'ACL del broker impone
  //    già il topic; questo controllo è indipendente (difesa in profondità).
  if (event.gateway_id !== gatewayId) {
    return { status: 'rejected', reason: 'gateway_id diverso da quello del topic', gatewayId, seq: event.seq };
  }

  // 4. Firma.
  const signature = verifyEnvelopeSignature(envelope, keys);
  if (!signature.ok) return { status: 'rejected', reason: signature.reason, gatewayId, seq: event.seq };

  // 5. Un evento non può provenire dal futuro. Gli eventi passati sono ammessi:
  //    quelli prodotti offline arrivano anche molto dopo (online: false).
  const receivedAt = now();
  if (Date.parse(event.ts) - receivedAt.getTime() > maxClockSkewMs) {
    return { status: 'rejected', reason: 'istante dell\'evento nel futuro', gatewayId, seq: event.seq };
  }

  // 6. Archiviazione idempotente.
  const outcome = await store.insertEvent({
    gatewayId,
    seq: event.seq,
    envelope,
    eventSha256: canonicalSha256(event),
    receivedAt,
  });

  // 7. Progressivo confermabile e buchi. Si ricalcola anche per i duplicati:
  //    il gateway ritrasmette proprio quando non ha ricevuto la conferma.
  const ackedUpTo = await store.advanceContiguous(gatewayId);
  /** @type {IngestResult} */
  const result = {
    status: outcome,
    reason: {
      accepted: 'evento verificato e salvato',
      duplicate: 'ritrasmissione di un evento già salvato',
      conflict: 'stesso numero progressivo di un evento già salvato, contenuto diverso',
    }[outcome],
    gatewayId,
    seq: event.seq,
    ackedUpTo,
  };
  if (outcome === 'accepted' && event.seq > ackedUpTo + 1) {
    result.gap = { from: ackedUpTo + 1, to: event.seq - 1 };
  }
  return result;
}
