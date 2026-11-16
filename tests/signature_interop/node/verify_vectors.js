/**
 * @file Verificatore dei vettori di prova per la firma degli eventi (lato backend).
 *
 * Simula la parte del backend Node.js che riceve un evento dal gateway e ne
 * verifica la firma prima di salvarlo. Legge i file prodotti dallo script
 * Python `tests/signature_interop/python/sign_vectors.py`:
 *
 * - `docs/test-vectors/public_key.pem`: chiave pubblica del gateway di prova;
 * - `docs/test-vectors/vectors.json`: casi di prova con l'esito atteso.
 *
 * Per ogni caso ricalcola la forma canonica dell'evento (RFC 8785), verifica la
 * firma ECDSA P-256 / SHA-256 in codifica DER e confronta l'esito con quello
 * atteso. Se tutti i casi coincidono, Python e Node sono interoperabili.
 *
 * Uso: `npm test` (oppure `node verify_vectors.js`).
 * Codice di uscita: 0 se tutti i casi danno l'esito atteso, 1 altrimenti.
 *
 * Lo script non apre connessioni di rete e non scrive file.
 */

import { createPublicKey, verify } from 'node:crypto';
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

import canonicalize from 'canonicalize';

/** Cartella di questo script. */
const SCRIPT_DIR = dirname(fileURLToPath(import.meta.url));

/** Cartella dei vettori di prova (tests/signature_interop/node -> radice -> docs). */
const VECTORS_DIR = join(SCRIPT_DIR, '..', '..', '..', 'docs', 'test-vectors');

/** Unico algoritmo accettato. Un valore diverso nella busta è un rifiuto. */
const EXPECTED_ALG = 'ES256';

/**
 * Base64 standard con padding obbligatorio. `Buffer.from(x, 'base64')` è
 * permissivo e ignora i caratteri non validi: controllare prima il formato
 * evita di verificare dati diversi da quelli ricevuti.
 */
const BASE64_PATTERN = /^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/;

/**
 * Calcola la forma canonica di un evento secondo RFC 8785 (JCS).
 *
 * È la stessa trasformazione che il gateway applica prima di firmare: ordina
 * i campi, elimina gli spazi e fissa la rappresentazione di numeri e stringhe.
 *
 * @param {object} event - L'oggetto `event` della busta, senza la firma.
 * @returns {string} La forma canonica, da codificare in UTF-8 prima della verifica.
 */
function canonicalForm(event) {
  return canonicalize(event);
}

/**
 * Verifica la busta di un evento con la chiave pubblica del gateway.
 *
 * Applica, nell'ordine, i controlli del §7 di event-schema.md che riguardano
 * la firma: struttura della busta, algoritmo, identificativo della chiave,
 * formato base64, firma sulla forma canonica. Qualsiasi errore, anche una
 * firma malformata che fa sollevare un'eccezione alla libreria, produce un
 * rifiuto e non un arresto del programma.
 *
 * @param {object} envelope - Busta `{ event, sig: { alg, kid, value } }`.
 * @param {import('node:crypto').KeyObject} publicKey - Chiave pubblica del gateway.
 * @param {string} expectedKid - Identificativo della chiave registrata per il gateway.
 * @returns {{ valid: boolean, reason: string }} Esito e motivo, utile nei log.
 */
function verifyEnvelope(envelope, publicKey, expectedKid) {
  const event = envelope?.event;
  const sig = envelope?.sig;
  if (typeof event !== 'object' || event === null || typeof sig !== 'object' || sig === null) {
    return { valid: false, reason: 'busta malformata' };
  }
  if (sig.alg !== EXPECTED_ALG) {
    return { valid: false, reason: `algoritmo non ammesso: ${sig.alg}` };
  }
  if (sig.kid !== expectedKid) {
    return { valid: false, reason: `chiave sconosciuta: ${sig.kid}` };
  }
  if (typeof sig.value !== 'string' || !BASE64_PATTERN.test(sig.value)) {
    return { valid: false, reason: 'firma non in base64' };
  }

  const data = Buffer.from(canonicalForm(event), 'utf8');
  const signature = Buffer.from(sig.value, 'base64');
  try {
    // dsaEncoding 'der' è il formato prodotto dalla libreria Python
    // `cryptography`. Con 'ieee-p1363' Node si aspetterebbe r||s a 64 byte.
    const ok = verify('sha256', data, { key: publicKey, dsaEncoding: 'der' }, signature);
    return ok ? { valid: true, reason: 'firma valida' } : { valid: false, reason: 'firma non valida' };
  } catch (err) {
    return { valid: false, reason: `firma non decodificabile (${err.code ?? err.message})` };
  }
}

/**
 * Esegue tutti i casi di prova e stampa un riepilogo.
 *
 * Per i casi validi controlla anche che la forma canonica calcolata da Node
 * coincida carattere per carattere con quella prodotta da Python: se la firma
 * fallisse, questo confronto indicherebbe subito se il problema sta nella
 * canonicalizzazione o nella firma.
 *
 * @returns {number} Codice di uscita: 0 se tutto coincide, 1 altrimenti.
 */
function main() {
  const document = JSON.parse(readFileSync(join(VECTORS_DIR, 'vectors.json'), 'utf8'));
  const publicKey = createPublicKey(readFileSync(join(VECTORS_DIR, document.public_key_file), 'utf8'));

  if (publicKey.asymmetricKeyType !== 'ec' || publicKey.asymmetricKeyDetails?.namedCurve !== 'prime256v1') {
    console.error('ERRORE: la chiave pubblica non è una chiave EC P-256');
    return 1;
  }

  let failures = 0;
  for (const vector of document.vectors) {
    const problems = [];

    if (vector.canonical !== null && canonicalForm(vector.envelope.event) !== vector.canonical) {
      problems.push('forma canonica diversa da quella di Python');
    }

    const result = verifyEnvelope(vector.envelope, publicKey, document.kid);
    const expectedValid = vector.expected === 'valid';
    if (result.valid !== expectedValid) {
      problems.push(`atteso ${vector.expected}, ottenuto ${result.valid ? 'valid' : 'invalid'}`);
    }

    const status = problems.length === 0 ? 'OK      ' : 'FALLITO ';
    console.log(`${status}${vector.id.padEnd(30)} ${result.reason}`);
    for (const problem of problems) console.log(`         -> ${problem}`);
    if (problems.length > 0) failures += 1;
  }

  const total = document.vectors.length;
  console.log(`\n${total - failures}/${total} casi con l'esito atteso.`);
  return failures === 0 ? 0 : 1;
}

process.exitCode = main();
