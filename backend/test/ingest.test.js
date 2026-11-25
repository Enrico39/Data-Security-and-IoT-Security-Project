/**
 * @file Test dell'elaborazione dei messaggi (event-schema.md §7 e §5.1).
 *
 * Ogni test usa un archivio in memoria e un gateway di prova con chiave
 * generata al momento.
 */

import assert from 'node:assert/strict';
import { beforeEach, describe, it } from 'node:test';

import { ingestMessage } from '../src/ingest.js';
import { MemoryEventStore } from '../src/store/memory-store.js';
import { generateGatewayKeys, makeEvent, signEvent, toMessage } from './helpers.js';

/** Orologio fisso dei test: 25/11/2026 alle 12:00 UTC. */
const NOW = new Date('2026-11-25T12:00:00.000Z');

describe('ingestMessage', () => {
  let deps;
  let gatewayKey;
  let otherGatewayKey;

  beforeEach(() => {
    gatewayKey = generateGatewayKeys();
    otherGatewayKey = generateGatewayKeys();
    deps = {
      store: new MemoryEventStore(),
      now: () => NOW,
      keys: new Map([
        ['gw-a-k1', { gatewayId: 'gw-a', publicKey: gatewayKey.publicKey }],
        ['gw-b-k1', { gatewayId: 'gw-b', publicKey: otherGatewayKey.publicKey }],
      ]),
    };
  });

  /** Firma un evento del gateway gw-a con i campi indicati. */
  const signed = (overrides) => signEvent(makeEvent(overrides), gatewayKey.privateKey, 'gw-a-k1');

  it('accetta, salva e conferma un evento valido', async () => {
    const result = await ingestMessage(toMessage(signed({ seq: 1 })), deps);
    assert.equal(result.status, 'accepted');
    assert.equal(result.ackedUpTo, 1);
    assert.equal(deps.store.events.size, 1);
  });

  it('tratta una ritrasmissione identica come duplicato e riconferma', async () => {
    const message = toMessage(signed({ seq: 1 }));
    await ingestMessage(message, deps);
    const again = await ingestMessage(message, deps);
    assert.equal(again.status, 'duplicate');
    assert.equal(again.ackedUpTo, 1);
    assert.equal(deps.store.events.size, 1);
  });

  it('segnala un conflitto se lo stesso progressivo arriva con contenuto diverso', async () => {
    await ingestMessage(toMessage(signed({ seq: 1 })), deps);
    const other = await ingestMessage(toMessage(signed({ seq: 1, type: 'ACCESS_DENIED', reason: 'PIN_WRONG' })), deps);
    assert.equal(other.status, 'conflict');
    assert.equal(deps.store.events.size, 1);
  });

  it('rileva un buco e conferma solo fino all\'ultimo evento senza buchi', async () => {
    await ingestMessage(toMessage(signed({ seq: 1 })), deps);
    const third = await ingestMessage(toMessage(signed({ seq: 3 })), deps);
    assert.equal(third.status, 'accepted');
    assert.equal(third.ackedUpTo, 1);
    assert.deepEqual(third.gap, { from: 2, to: 2 });

    // Arriva l'evento mancante (per esempio dalla coda offline): la conferma avanza a 3.
    const second = await ingestMessage(toMessage(signed({ seq: 2, online: false })), deps);
    assert.equal(second.ackedUpTo, 3);
    assert.equal(second.gap, undefined);
  });

  it('conferma 0 finché manca il primo evento', async () => {
    const result = await ingestMessage(toMessage(signed({ seq: 2 })), deps);
    assert.equal(result.ackedUpTo, 0);
    assert.deepEqual(result.gap, { from: 1, to: 1 });
  });

  it('rifiuta un evento pubblicato sul topic di un altro gateway', async () => {
    const result = await ingestMessage(toMessage(signed({ seq: 1 }), 'gw-b'), deps);
    assert.equal(result.status, 'rejected');
    assert.equal(result.reason, 'gateway_id diverso da quello del topic');
  });

  it('rifiuta un gateway che firma a nome di un altro con la propria chiave valida', async () => {
    const envelope = signEvent(makeEvent({ gateway_id: 'gw-a' }), otherGatewayKey.privateKey, 'gw-b-k1');
    const result = await ingestMessage(toMessage(envelope), deps);
    assert.equal(result.reason, 'la chiave non appartiene al gateway dichiarato');
  });

  it('rifiuta un evento manomesso dopo la firma', async () => {
    const envelope = signed({ seq: 1, type: 'ACCESS_DENIED', reason: 'OUT_OF_SHIFT' });
    envelope.event.type = 'ACCESS_GRANTED';
    delete envelope.event.reason;
    const result = await ingestMessage(toMessage(envelope), deps);
    assert.equal(result.status, 'rejected');
    assert.equal(result.reason, 'firma non valida');
    assert.equal(deps.store.events.size, 0);
  });

  it('rifiuta eventi nel futuro e accetta eventi offline del passato', async () => {
    const future = await ingestMessage(toMessage(signed({ seq: 1, ts: '2026-11-25T12:10:00.000Z' })), deps);
    assert.equal(future.reason, 'istante dell\'evento nel futuro');
    const old = await ingestMessage(toMessage(signed({ seq: 1, ts: '2026-11-20T08:00:00.000Z', online: false })), deps);
    assert.equal(old.status, 'accepted');
  });

  it('rifiuta messaggi non JSON, troppo grandi o con busta malformata', async () => {
    const topic = 'ac/v1/gw-a/events';
    assert.equal((await ingestMessage({ topic, payload: 'non json' }, deps)).reason, 'JSON non valido');
    assert.equal(
      (await ingestMessage({ topic, payload: Buffer.alloc(9000, 'a') }, deps)).reason,
      'messaggio troppo grande',
    );
    const extra = { ...signed({ seq: 1 }), note: 'campo in più' };
    assert.equal((await ingestMessage(toMessage(extra), deps)).reason, 'busta malformata');
  });

  it('rifiuta topic diversi da quello degli eventi', async () => {
    const result = await ingestMessage({ topic: 'ac/v1/gw-a/policy', payload: '{}' }, deps);
    assert.equal(result.reason, 'topic non previsto');
  });

  it('rifiuta un evento con dati non previsti dallo schema anche se firmato', async () => {
    const result = await ingestMessage(toMessage(signed({ seq: 1, badge_uid: '04A1B2C3' })), deps);
    assert.equal(result.reason, 'evento non valido: campo non previsto: badge_uid');
  });
});
