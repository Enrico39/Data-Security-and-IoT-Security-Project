/**
 * @file Test della verifica della firma sui vettori prodotti dal gateway Python.
 *
 * Usa docs/test-vectors/ e il registro config/gateways.json: lo stesso
 * percorso che il backend segue in esecuzione. Se un collega rigenera i
 * vettori con una nuova chiave, il test continua a funzionare purché anche
 * public_key.pem sia aggiornato.
 */

import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { describe, it } from 'node:test';
import { fileURLToPath } from 'node:url';

import { validateEvent } from '../src/event-schema.js';
import { loadGatewayKeys, verifyEnvelopeSignature } from '../src/signature.js';
import { generateGatewayKeys, makeEvent, signEvent } from './helpers.js';

const registryFile = fileURLToPath(new URL('../config/gateways.json', import.meta.url));
const vectorsFile = fileURLToPath(new URL('../../docs/test-vectors/vectors.json', import.meta.url));

describe('verifica della firma', () => {
  const keys = loadGatewayKeys(registryFile);
  const document = JSON.parse(readFileSync(vectorsFile, 'utf8'));

  for (const vector of document.vectors) {
    it(`vettore ${vector.id}: esito ${vector.expected}`, () => {
      // I vettori validi devono superare anche la validazione dello schema.
      if (vector.expected === 'valid') assert.equal(validateEvent(vector.envelope.event).ok, true);
      const result = verifyEnvelopeSignature(vector.envelope, keys);
      assert.equal(result.ok, vector.expected === 'valid', result.reason);
    });
  }

  it('rifiuta la chiave valida di un gateway usata per firmare a nome di un altro', () => {
    const { privateKey, publicKey } = generateGatewayKeys();
    const registry = new Map([['gw-a-k1', { gatewayId: 'gw-a', publicKey }]]);
    const envelope = signEvent(makeEvent({ gateway_id: 'gw-b' }), privateKey, 'gw-a-k1');
    assert.deepEqual(verifyEnvelopeSignature(envelope, registry), {
      ok: false,
      reason: 'la chiave non appartiene al gateway dichiarato',
    });
  });

  it('rifiuta algoritmi diversi da ES256 e firme non in base64', () => {
    const { privateKey, publicKey } = generateGatewayKeys();
    const registry = new Map([['gw-a-k1', { gatewayId: 'gw-a', publicKey }]]);
    const envelope = signEvent(makeEvent(), privateKey, 'gw-a-k1');
    assert.equal(verifyEnvelopeSignature({ ...envelope, sig: { ...envelope.sig, alg: 'none' } }, registry).ok, false);
    assert.equal(verifyEnvelopeSignature({ ...envelope, sig: { ...envelope.sig, value: '***' } }, registry).ok, false);
  });
});
