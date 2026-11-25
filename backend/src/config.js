/**
 * @file Lettura e controllo della configurazione dalle variabili d'ambiente.
 *
 * Le variabili arrivano dal file .env (escluso da Git) tramite l'opzione
 * `--env-file` di Node.js. Se manca un valore obbligatorio il backend si
 * rifiuta di partire: nessun valore predefinito per credenziali o indirizzi,
 * per non avviare per errore un servizio configurato male.
 */

import { fileURLToPath } from 'node:url';

/** Variabili obbligatorie. */
const REQUIRED = [
  'MONGO_APP_DB',
  'MONGO_APP_USER',
  'MONGO_APP_PASSWORD',
  'MQTT_URL',
  'MQTT_BACKEND_USER',
  'MQTT_BACKEND_PASSWORD',
];

/**
 * Configurazione del backend.
 * @typedef {object} BackendConfig
 * @property {{ uri: string, dbName: string }} mongo
 * @property {{ url: string, username: string, password: string }} mqtt
 * @property {string} gatewayKeysFile - Percorso del registro delle chiavi dei gateway.
 * @property {number} maxClockSkewMs - Anticipo massimo ammesso per l'istante di un evento.
 */

/**
 * Costruisce la configurazione a partire dalle variabili d'ambiente.
 *
 * @param {NodeJS.ProcessEnv} env - Variabili d'ambiente (di norma process.env).
 * @returns {BackendConfig}
 * @throws {Error} Se manca una variabile obbligatoria. Il messaggio indica
 *         solo il nome della variabile, mai un valore.
 */
export function loadConfig(env) {
  const missing = REQUIRED.filter((name) => !env[name]);
  if (missing.length > 0) {
    throw new Error(`Configurazione incompleta, mancano nel file .env: ${missing.join(', ')}`);
  }

  // Le credenziali possono contenere caratteri speciali: vanno codificate
  // nell'URI. L'utente applicativo è definito nel database dell'applicazione.
  const user = encodeURIComponent(env.MONGO_APP_USER);
  const password = encodeURIComponent(env.MONGO_APP_PASSWORD);
  const host = env.MONGO_HOST ?? '127.0.0.1:27018';
  const uri = `mongodb://${user}:${password}@${host}/${env.MONGO_APP_DB}?authSource=${env.MONGO_APP_DB}`;

  return {
    mongo: { uri, dbName: env.MONGO_APP_DB },
    mqtt: {
      url: env.MQTT_URL,
      username: env.MQTT_BACKEND_USER,
      password: env.MQTT_BACKEND_PASSWORD,
    },
    gatewayKeysFile: env.GATEWAY_KEYS_FILE
      ?? fileURLToPath(new URL('../config/gateways.json', import.meta.url)),
    maxClockSkewMs: 2 * 60 * 1000,
  };
}
