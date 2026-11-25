/**
 * @file Avvio del backend: collega broker MQTT, verifica e archivio.
 *
 * Il backend si iscrive al topic degli eventi di tutti i gateway, elabora ogni
 * messaggio con ingest.js e pubblica la conferma `acked_up_to` sul topic di
 * conferma del gateway (docs/event-schema.md §5.1). I messaggi sono elaborati
 * uno alla volta, nell'ordine di arrivo, così il calcolo del progressivo
 * confermabile non subisce interferenze.
 *
 * Avvio: `npm start` dalla cartella backend/, con Mosquitto e MongoDB attivi.
 */

import mqtt from 'mqtt';

import { loadConfig } from './config.js';
import { ingestMessage } from './ingest.js';
import { loadGatewayKeys } from './signature.js';
import { MongoEventStore } from './store/mongo-store.js';

/** Topic a cui il backend si iscrive: eventi di qualunque gateway. */
const EVENTS_SUBSCRIPTION = 'ac/v1/+/events';

/**
 * Scrive una riga di log in formato JSON.
 * I log contengono identificativi tecnici ed esiti, mai lo pseudonimo
 * dell'operatore né altri dati dell'evento.
 * @param {string} level - info, warn o error.
 * @param {string} msg - Descrizione breve.
 * @param {object} [fields] - Campi aggiuntivi.
 */
function log(level, msg, fields = {}) {
  console.log(JSON.stringify({ time: new Date().toISOString(), level, msg, ...fields }));
}

/**
 * Pubblica la conferma per un gateway come messaggio retained, così un
 * gateway che si riconnette la riceve subito.
 * @param {import('mqtt').MqttClient} client
 * @param {string} gatewayId
 * @param {number} ackedUpTo
 * @returns {Promise<void>}
 */
async function publishAck(client, gatewayId, ackedUpTo) {
  await client.publishAsync(
    `ac/v1/${gatewayId}/ack`,
    JSON.stringify({ acked_up_to: ackedUpTo }),
    { qos: 1, retain: true },
  );
}

/**
 * Avvia il backend e resta in attesa dei messaggi.
 * @returns {Promise<void>}
 */
async function main() {
  const config = loadConfig(process.env);
  const keys = loadGatewayKeys(config.gatewayKeysFile);
  const { store, client: mongoClient } = await MongoEventStore.connect(config.mongo.uri, config.mongo.dbName);
  log('info', 'connesso a MongoDB', { db: config.mongo.dbName, gateway_keys: keys.size });

  const client = await mqtt.connectAsync(config.mqtt.url, {
    username: config.mqtt.username,
    password: config.mqtt.password,
    clientId: 'ac-backend',
    protocolVersion: 5,
    // Sessione persistente: se il backend è spento, il broker conserva i
    // messaggi QoS 1 e li consegna alla riconnessione.
    clean: false,
    properties: { sessionExpiryInterval: 7 * 24 * 3600 },
  });
  await client.subscribeAsync(EVENTS_SUBSCRIPTION, { qos: 1 });
  log('info', 'connesso al broker MQTT', { subscription: EVENTS_SUBSCRIPTION });

  // Coda di elaborazione: ogni messaggio attende la fine del precedente.
  let queue = Promise.resolve();
  client.on('message', (topic, payload) => {
    queue = queue.then(async () => {
      try {
        const result = await ingestMessage({ topic, payload }, {
          keys,
          store,
          maxClockSkewMs: config.maxClockSkewMs,
        });
        const level = { accepted: 'info', duplicate: 'info', rejected: 'warn', conflict: 'error' }[result.status];
        log(level, result.reason, {
          status: result.status,
          gateway_id: result.gatewayId,
          seq: result.seq,
          acked_up_to: result.ackedUpTo,
          gap: result.gap,
        });
        if (result.ackedUpTo !== undefined) await publishAck(client, result.gatewayId, result.ackedUpTo);
      } catch (err) {
        log('error', 'errore durante l\'elaborazione', { topic, error: err.message });
      }
    });
  });

  client.on('error', (err) => log('error', 'errore MQTT', { error: err.message }));

  /** Chiude le connessioni in modo ordinato. */
  const shutdown = async () => {
    log('info', 'arresto in corso');
    await queue;
    await client.endAsync();
    await mongoClient.close();
    process.exit(0);
  };
  process.on('SIGINT', shutdown);
  process.on('SIGTERM', shutdown);
}

main().catch((err) => {
  log('error', 'avvio non riuscito', { error: err.message });
  process.exit(1);
});
