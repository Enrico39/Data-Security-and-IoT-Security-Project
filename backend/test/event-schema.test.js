/**
 * @file Test della validazione strutturale degli eventi (event-schema.md §2).
 */

import assert from 'node:assert/strict';
import { describe, it } from 'node:test';

import { validateEvent } from '../src/event-schema.js';
import { makeEvent } from './helpers.js';

describe('validateEvent', () => {
  it('accetta un accesso consentito completo', () => {
    assert.deepEqual(validateEvent(makeEvent()), { ok: true });
  });

  it('accetta un diniego con motivo e un evento di sistema senza operatore', () => {
    assert.equal(validateEvent(makeEvent({ type: 'ACCESS_DENIED', reason: 'OUT_OF_SHIFT' })).ok, true);
    const started = makeEvent({ type: 'GATEWAY_STARTED' });
    delete started.subject;
    delete started.factors;
    assert.equal(validateEvent(started).ok, true);
  });

  it('rifiuta campi non previsti, per esempio dati non minimizzati', () => {
    const result = validateEvent(makeEvent({ badge_uid: '04A1B2C3' }));
    assert.deepEqual(result, { ok: false, reason: 'campo non previsto: badge_uid' });
  });

  it('rifiuta un evento senza campi obbligatori', () => {
    const event = makeEvent();
    delete event.policy_hash;
    assert.equal(validateEvent(event).ok, false);
  });

  it('rifiuta contesto di protocollo e versione errati', () => {
    assert.equal(validateEvent(makeEvent({ ctx: 'altro-protocollo' })).ok, false);
    assert.equal(validateEvent(makeEvent({ v: 2 })).ok, false);
  });

  it('rifiuta numeri progressivi non interi o minori di 1', () => {
    for (const seq of [0, -1, 1.5, '1', Number.MAX_SAFE_INTEGER + 1]) {
      assert.equal(validateEvent(makeEvent({ seq })).ok, false, `seq ${seq}`);
    }
  });

  it('rifiuta istanti malformati o inesistenti', () => {
    for (const ts of ['2026-11-25T10:00:00Z', '2026-02-31T10:00:00.000Z', 'ieri']) {
      assert.equal(validateEvent(makeEvent({ ts })).ok, false, ts);
    }
  });

  it('richiede il motivo solo per i dinieghi', () => {
    assert.equal(validateEvent(makeEvent({ type: 'ACCESS_DENIED' })).ok, false);
    assert.equal(validateEvent(makeEvent({ type: 'ACCESS_DENIED', reason: 'INVENTATO' })).ok, false);
    assert.equal(validateEvent(makeEvent({ reason: 'OUT_OF_SHIFT' })).ok, false);
  });

  it('richiede tutti e tre i fattori con esiti ammessi', () => {
    assert.equal(validateEvent(makeEvent({ factors: { badge: 'ok', pin: 'ok' } })).ok, false);
    assert.equal(validateEvent(makeEvent({ factors: { badge: 'ok', pin: 'ok', bio: 'forse' } })).ok, false);
  });

  it('rifiuta pseudonimi che non sono 64 cifre esadecimali', () => {
    assert.equal(validateEvent(makeEvent({ subject: 'mario.rossi' })).ok, false);
  });

  it('non ammette dati dell\'operatore negli eventi di sistema', () => {
    const started = makeEvent({ type: 'GATEWAY_STARTED' });
    delete started.factors;
    assert.equal(validateEvent(started).ok, false);
  });
});
