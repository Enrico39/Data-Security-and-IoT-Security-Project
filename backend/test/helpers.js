/**
 * @file Funzioni di supporto per i test: chiavi e eventi firmati di prova.
 *
 * Le chiavi sono generate in memoria a ogni esecuzione e non vengono mai
 * salvate. Gli eventi rispettano docs/event-schema.md e sono firmati come
 * farebbe il gateway (forma canonica RFC 8785, ECDSA P-256, DER, base64).
 */

import { createHash, generateKeyPairSync, sign } from 'node:crypto';

import { canonicalBytes } from '../src/signature.js';

/** Pseudonimo e hash di policy fittizi, senza legame con persone reali. */
export const DEMO_SUBJECT = createHash('sha256').update('operatore-fittizio-test').digest('hex');
export const DEMO_POLICY_HASH = createHash('sha256').update('policy-fittizia-test').digest('hex');

/**
 * Genera una coppia di chiavi P-256 per un gateway di prova.
 * @returns {{ privateKey: import('node:crypto').KeyObject, publicKey: import('node:crypto').KeyObject }}
 */
export function generateGatewayKeys() {
  return generateKeyPairSync('ec', { namedCurve: 'prime256v1' });
}

/**
 * Costruisce un evento di accesso valido, con eventuali campi sostituiti.
 * @param {object} [overrides] - Campi da aggiungere o sostituire.
 * @returns {object}
 */
export function makeEvent(overrides = {}) {
  return {
    ctx: 'unisa-ac/event/v1',
    v: 1,
    gateway_id: 'gw-a',
    terminal_id: 'term-01',
    seq: 1,
    ts: '2026-11-25T10:00:00.000Z',
    type: 'ACCESS_GRANTED',
    subject: DEMO_SUBJECT,
    factors: { badge: 'ok', pin: 'ok' },
    policy_version: 7,
    policy_hash: DEMO_POLICY_HASH,
    online: true,
    ...overrides,
  };
}

/**
 * Firma un evento e lo inserisce nella busta prevista dallo schema.
 * @param {object} event
 * @param {import('node:crypto').KeyObject} privateKey
 * @param {string} kid
 * @returns {{ event: object, sig: { alg: string, kid: string, value: string } }}
 */
export function signEvent(event, privateKey, kid) {
  const signature = sign('sha256', canonicalBytes(event), { key: privateKey, dsaEncoding: 'der' });
  return { event, sig: { alg: 'ES256', kid, value: signature.toString('base64') } };
}

/**
 * Prepara un messaggio MQTT come arriverebbe dal broker.
 * @param {object} envelope
 * @param {string} [gatewayId] - Gateway del topic; di default quello dell'evento.
 * @returns {{ topic: string, payload: Buffer }}
 */
export function toMessage(envelope, gatewayId = envelope.event.gateway_id) {
  return { topic: `ac/v1/${gatewayId}/events`, payload: Buffer.from(JSON.stringify(envelope)) };
}
