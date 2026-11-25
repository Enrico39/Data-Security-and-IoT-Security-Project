/**
 * @file Canonicalizzazione e verifica della firma degli eventi.
 *
 * Il gateway firma la forma canonica dell'oggetto `event` (RFC 8785) con
 * ECDSA P-256 / SHA-256 e codifica la firma in DER, poi in base64
 * (docs/event-schema.md §4). L'interoperabilità con l'implementazione
 * Python del gateway è provata da tests/signature_interop.
 */

import { createHash, createPublicKey, verify } from 'node:crypto';
import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';

import canonicalize from 'canonicalize';

/** Unico algoritmo accettato nella busta. */
export const SIGNATURE_ALG = 'ES256';

/**
 * Base64 standard con padding obbligatorio. `Buffer.from(x, 'base64')`
 * ignora i caratteri non validi: il controllo preventivo evita di verificare
 * dati diversi da quelli ricevuti.
 */
const BASE64_PATTERN = /^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/;

/**
 * Restituisce la forma canonica di un oggetto in UTF-8 (RFC 8785).
 * @param {object} value
 * @returns {Buffer}
 */
export function canonicalBytes(value) {
  return Buffer.from(canonicalize(value), 'utf8');
}

/**
 * Calcola lo SHA-256 esadecimale della forma canonica di un oggetto.
 * Serve a confrontare due eventi senza dipendere dall'ordine dei campi.
 * @param {object} value
 * @returns {string}
 */
export function canonicalSha256(value) {
  return createHash('sha256').update(canonicalBytes(value)).digest('hex');
}

/**
 * Voce del registro delle chiavi: chiave pubblica e gateway a cui appartiene.
 * @typedef {{ gatewayId: string, publicKey: import('node:crypto').KeyObject }} GatewayKey
 */

/**
 * Carica il registro delle chiavi pubbliche dei gateway.
 *
 * Il file JSON elenca, per ogni gateway, gli identificativi delle chiavi
 * (`kid`) e il percorso del file PEM, relativo al file di configurazione.
 * Più chiavi per gateway consentono la rotazione senza invalidare gli eventi
 * firmati con la chiave precedente (§4).
 *
 * @param {string} registryPath - Percorso del file JSON del registro.
 * @returns {Map<string, GatewayKey>} Mappa kid → chiave e gateway.
 * @throws {Error} Se il file è malformato, un kid è duplicato o una chiave
 *         non è di tipo EC P-256.
 */
export function loadGatewayKeys(registryPath) {
  const registry = JSON.parse(readFileSync(registryPath, 'utf8'));
  const baseDir = dirname(resolve(registryPath));
  const keys = new Map();

  if (!Array.isArray(registry.gateways)) throw new Error('registro chiavi: manca l\'elenco gateways');
  for (const gateway of registry.gateways) {
    for (const entry of gateway.keys ?? []) {
      if (keys.has(entry.kid)) throw new Error(`registro chiavi: kid duplicato ${entry.kid}`);
      const publicKey = createPublicKey(readFileSync(resolve(baseDir, entry.public_key_file), 'utf8'));
      if (publicKey.asymmetricKeyType !== 'ec' || publicKey.asymmetricKeyDetails?.namedCurve !== 'prime256v1') {
        throw new Error(`registro chiavi: la chiave ${entry.kid} non è EC P-256`);
      }
      keys.set(entry.kid, { gatewayId: gateway.gateway_id, publicKey });
    }
  }
  return keys;
}

/**
 * Verifica la firma di una busta già validata nella struttura dell'evento.
 *
 * Controlla, nell'ordine: algoritmo, chiave nota, appartenenza della chiave
 * al gateway dichiarato nell'evento, formato base64, firma sulla forma
 * canonica. Una firma non decodificabile produce un rifiuto, mai un'eccezione
 * non gestita.
 *
 * @param {{ event: object, sig: { alg: string, kid: string, value: string } }} envelope
 * @param {Map<string, GatewayKey>} keys - Registro delle chiavi.
 * @returns {{ ok: true } | { ok: false, reason: string }}
 */
export function verifyEnvelopeSignature(envelope, keys) {
  const { event, sig } = envelope;
  if (sig.alg !== SIGNATURE_ALG) return { ok: false, reason: 'algoritmo di firma non ammesso' };

  const keyEntry = keys.get(sig.kid);
  if (!keyEntry) return { ok: false, reason: 'chiave sconosciuta' };
  // Una chiave valida di un altro gateway non basta: ogni gateway può firmare
  // solo eventi a proprio nome.
  if (keyEntry.gatewayId !== event.gateway_id) {
    return { ok: false, reason: 'la chiave non appartiene al gateway dichiarato' };
  }
  if (typeof sig.value !== 'string' || !BASE64_PATTERN.test(sig.value)) {
    return { ok: false, reason: 'firma non in base64' };
  }

  try {
    const valid = verify(
      'sha256',
      canonicalBytes(event),
      { key: keyEntry.publicKey, dsaEncoding: 'der' },
      Buffer.from(sig.value, 'base64'),
    );
    return valid ? { ok: true } : { ok: false, reason: 'firma non valida' };
  } catch {
    return { ok: false, reason: 'firma non decodificabile' };
  }
}
