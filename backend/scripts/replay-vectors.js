/**
 * @file Simulatore minimo del gateway: pubblica i vettori di prova sul broker.
 *
 * Si connette a Mosquitto con le credenziali del gateway di prova e pubblica,
 * nell'ordine, tutte le buste di docs/test-vectors/vectors.json sul topic
 * degli eventi del gateway. Poi attende la conferma del backend sul topic
 * di conferma e la stampa.
 *
 * Esito atteso con il backend avviato: i tre eventi validi (seq 1, 2, 3) sono
 * salvati, i tre non validi sono rifiutati, la conferma vale 3. Rilanciando lo
 * script, i tre eventi validi risultano duplicati e la conferma resta 3.
 *
 * Uso, dalla cartella backend/: `npm run replay`
 */

import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';

import mqtt from 'mqtt';

/** Tempo massimo di attesa della conferma, in millisecondi. */
const ACK_TIMEOUT_MS = 5000;

/**
 * Pubblica i vettori e attende la conferma.
 * @returns {Promise<void>}
 */
async function main() {
  for (const name of ['MQTT_URL', 'MQTT_GATEWAY_USER', 'MQTT_GATEWAY_PASSWORD']) {
    if (!process.env[name]) throw new Error(`manca ${name} nel file .env`);
  }
  const vectorsFile = fileURLToPath(new URL('../../docs/test-vectors/vectors.json', import.meta.url));
  const { gateway_id: gatewayId, vectors } = JSON.parse(readFileSync(vectorsFile, 'utf8'));
  if (gatewayId !== process.env.MQTT_GATEWAY_USER) {
    throw new Error(`i vettori sono del gateway ${gatewayId}, ma MQTT_GATEWAY_USER è ${process.env.MQTT_GATEWAY_USER}`);
  }

  const client = await mqtt.connectAsync(process.env.MQTT_URL, {
    username: process.env.MQTT_GATEWAY_USER,
    password: process.env.MQTT_GATEWAY_PASSWORD,
    clientId: `${gatewayId}-replay`,
    protocolVersion: 5,
  });

  // Ci si iscrive alla conferma prima di pubblicare, per non perderla.
  const ackTopic = `ac/v1/${gatewayId}/ack`;
  const acks = [];
  client.on('message', (topic, payload) => {
    if (topic === ackTopic) acks.push(JSON.parse(payload.toString('utf8')));
  });
  await client.subscribeAsync(ackTopic, { qos: 1 });

  for (const vector of vectors) {
    await client.publishAsync(`ac/v1/${gatewayId}/events`, JSON.stringify(vector.envelope), { qos: 1 });
    console.log(`pubblicato ${vector.id.padEnd(30)} (atteso: ${vector.expected})`);
  }

  await new Promise((resolve) => setTimeout(resolve, ACK_TIMEOUT_MS));
  const last = acks.at(-1);
  console.log(last ? `ultima conferma ricevuta: acked_up_to = ${last.acked_up_to}` : 'nessuna conferma ricevuta');
  await client.endAsync();
}

main().catch((err) => {
  console.error(`errore: ${err.message}`);
  process.exit(1);
});
