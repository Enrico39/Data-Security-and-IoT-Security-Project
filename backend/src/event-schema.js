/**
 * @file Validazione strutturale degli eventi del gateway.
 *
 * Applica il formato definito in docs/event-schema.md (§2): campi ammessi,
 * campi obbligatori, tipi e valori consentiti. La validazione è volutamente
 * rigida: un campo sconosciuto o di tipo errato fa rifiutare l'evento, così
 * nel registro di audit finiscono solo eventi dal significato univoco.
 *
 * La validazione precede la verifica della firma: rifiutare subito i messaggi
 * malformati evita di dedicare calcolo crittografico a dati senza senso.
 */

/** Contesto di protocollo atteso nei dati firmati (event-schema.md §2). */
export const PROTOCOL_CONTEXT = 'unisa-ac/event/v1';

/** Versione dello schema supportata. */
export const SCHEMA_VERSION = 1;

/** Tipi di evento ammessi (§2.1). */
export const EVENT_TYPES = Object.freeze([
  'ACCESS_GRANTED',
  'ACCESS_DENIED',
  'SIMULATED_UNLOCK_ENDED',
  'GATEWAY_STARTED',
  'POLICY_APPLIED',
]);

/** Motivi di diniego ammessi (§2.2). */
export const DENIAL_REASONS = Object.freeze([
  'BADGE_UNKNOWN',
  'BADGE_REVOKED',
  'PIN_WRONG',
  'PIN_LOCKED',
  'BIO_MISMATCH',
  'OUT_OF_SHIFT',
  'ROLE_NOT_ALLOWED',
  'POLICY_EXPIRED',
  'TIMEOUT',
  'CANCELLED',
]);

/** Esiti ammessi per ciascun fattore di autenticazione. */
const FACTOR_OUTCOMES = new Set(['ok', 'fail', 'skipped']);

/** Fattori previsti nell'oggetto `factors`. */
const FACTOR_NAMES = ['badge', 'pin', 'bio'];

/** Tutti i campi che possono comparire in un evento. */
const ALLOWED_FIELDS = new Set([
  'ctx', 'v', 'gateway_id', 'terminal_id', 'seq', 'ts', 'type',
  'subject', 'factors', 'reason', 'policy_version', 'policy_hash', 'online',
]);

/** Campi obbligatori per qualunque tipo di evento. */
const COMMON_REQUIRED = [
  'ctx', 'v', 'gateway_id', 'terminal_id', 'seq', 'ts', 'type',
  'policy_version', 'policy_hash', 'online',
];

/** Identificativi di gateway e terminale: minuscole, cifre e trattini. */
const ID_PATTERN = /^[a-z0-9][a-z0-9-]{0,31}$/;

/** Valore esadecimale di 32 byte (pseudonimo, hash della policy). */
const HEX32_PATTERN = /^[0-9a-f]{64}$/;

/** Istante RFC 3339 in UTC con millisecondi, come richiesto dallo schema. */
const TS_PATTERN = /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z$/;

/**
 * Esito di una validazione.
 * @typedef {{ ok: true } | { ok: false, reason: string }} ValidationResult
 */

/**
 * Crea un esito negativo con il motivo indicato.
 * @param {string} reason - Descrizione breve del problema, adatta ai log.
 * @returns {ValidationResult}
 */
function fail(reason) {
  return { ok: false, reason };
}

/**
 * Indica se un valore è un oggetto semplice (non null, non array).
 * @param {unknown} value
 * @returns {boolean}
 */
function isPlainObject(value) {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

/**
 * Controlla che un istante sia nel formato atteso e corrisponda a una data reale.
 * Il solo pattern accetterebbe per esempio il 31 febbraio.
 * @param {string} ts
 * @returns {boolean}
 */
function isValidTimestamp(ts) {
  if (!TS_PATTERN.test(ts)) return false;
  const parsed = new Date(ts);
  return !Number.isNaN(parsed.getTime()) && parsed.toISOString() === ts;
}

/**
 * Valida l'oggetto `factors` degli eventi di accesso.
 * @param {unknown} factors
 * @returns {ValidationResult}
 */
function validateFactors(factors) {
  if (!isPlainObject(factors)) return fail('factors non è un oggetto');
  const keys = Object.keys(factors);
  if (keys.length !== FACTOR_NAMES.length || !FACTOR_NAMES.every((name) => keys.includes(name))) {
    return fail('factors deve contenere esattamente badge, pin e bio');
  }
  for (const name of FACTOR_NAMES) {
    if (!FACTOR_OUTCOMES.has(factors[name])) return fail(`esito non valido per il fattore ${name}`);
  }
  return { ok: true };
}

/**
 * Valida un evento secondo docs/event-schema.md (§2).
 *
 * Regole per tipo:
 * - ACCESS_GRANTED e ACCESS_DENIED richiedono `subject` e `factors`;
 *   `reason` è obbligatorio solo per ACCESS_DENIED e vietato altrimenti;
 * - SIMULATED_UNLOCK_ENDED può indicare `subject`, ma non `factors` né `reason`;
 * - GATEWAY_STARTED e POLICY_APPLIED non riguardano un operatore e non
 *   ammettono `subject`, `factors` né `reason`.
 *
 * @param {unknown} event - L'oggetto `event` estratto dalla busta.
 * @returns {ValidationResult} Esito, con il motivo del rifiuto se negativo.
 */
export function validateEvent(event) {
  if (!isPlainObject(event)) return fail('event non è un oggetto');

  for (const key of Object.keys(event)) {
    if (!ALLOWED_FIELDS.has(key)) return fail(`campo non previsto: ${key}`);
  }
  for (const key of COMMON_REQUIRED) {
    if (!(key in event)) return fail(`campo obbligatorio mancante: ${key}`);
  }

  if (event.ctx !== PROTOCOL_CONTEXT) return fail('contesto di protocollo non valido');
  if (event.v !== SCHEMA_VERSION) return fail('versione dello schema non supportata');
  if (typeof event.gateway_id !== 'string' || !ID_PATTERN.test(event.gateway_id)) {
    return fail('gateway_id non valido');
  }
  if (typeof event.terminal_id !== 'string' || !ID_PATTERN.test(event.terminal_id)) {
    return fail('terminal_id non valido');
  }
  if (!Number.isSafeInteger(event.seq) || event.seq < 1) {
    return fail('seq deve essere un intero maggiore o uguale a 1');
  }
  if (typeof event.ts !== 'string' || !isValidTimestamp(event.ts)) return fail('ts non valido');
  if (!EVENT_TYPES.includes(event.type)) return fail('type non previsto');
  if (!Number.isSafeInteger(event.policy_version) || event.policy_version < 0) {
    return fail('policy_version non valida');
  }
  if (typeof event.policy_hash !== 'string' || !HEX32_PATTERN.test(event.policy_hash)) {
    return fail('policy_hash non valido');
  }
  if (typeof event.online !== 'boolean') return fail('online deve essere booleano');

  const isAccess = event.type === 'ACCESS_GRANTED' || event.type === 'ACCESS_DENIED';
  const isSystem = event.type === 'GATEWAY_STARTED' || event.type === 'POLICY_APPLIED';

  if ('subject' in event) {
    if (isSystem) return fail('subject non ammesso per gli eventi di sistema');
    if (typeof event.subject !== 'string' || !HEX32_PATTERN.test(event.subject)) {
      return fail('subject non valido');
    }
  } else if (isAccess) {
    return fail('subject obbligatorio per gli eventi di accesso');
  }

  if (isAccess) {
    const factors = validateFactors(event.factors);
    if (!factors.ok) return factors;
  } else if ('factors' in event) {
    return fail('factors ammesso solo per gli eventi di accesso');
  }

  if (event.type === 'ACCESS_DENIED') {
    if (!DENIAL_REASONS.includes(event.reason)) return fail('reason mancante o non previsto');
  } else if ('reason' in event) {
    return fail('reason ammesso solo per ACCESS_DENIED');
  }

  return { ok: true };
}
