#!/usr/bin/env bash
# Controllo del contenuto del database con l'utente applicativo del backend.
#
# Stampa il numero di eventi salvati e il loro elenco (senza lo pseudonimo
# dell'operatore), gli indici, lo stato delle conferme e verifica che l'utente
# applicativo NON possa leggere il database di amministrazione (minimo privilegio).
# Le credenziali sono lette da .env e passate al container senza comparire a schermo.
#
# Uso, dalla cartella backend/ con i servizi avviati: bash scripts/check-db.sh
set -euo pipefail
cd "$(dirname "$0")/.."
set -a; . ./.env; set +a
docker exec -i -e MONGO_APP_DB -e MONGO_APP_USER -e MONGO_APP_PASSWORD ac-mongo sh -c '
  cat > /tmp/verifica.js &&
  mongosh --quiet "mongodb://127.0.0.1:27017/$MONGO_APP_DB" \
    -u "$MONGO_APP_USER" -p "$MONGO_APP_PASSWORD" --authenticationDatabase "$MONGO_APP_DB" \
    /tmp/verifica.js; status=$?; rm -f /tmp/verifica.js; exit $status' <<'EOF'
const d = db.getSiblingDB(process.env.MONGO_APP_DB);
print("eventi salvati:", d.events.countDocuments());
d.events.find().sort({ gateway_id: 1, seq: 1 }).forEach((e) =>
  print("  ", e.gateway_id, "seq", e.seq, e.envelope.event.type, "ricevuto", e.received_at.toISOString()));
print("indici:", d.events.getIndexes().map((i) => i.name).join(", "));
d.gateway_state.find({}, { _id: 0, gateway_id: 1, acked_up_to: 1 }).forEach((s) => print("stato:", JSON.stringify(s)));
try {
  db.getSiblingDB("admin").getCollectionNames();
  print("accesso al database admin: CONSENTITO (problema!)");
} catch (err) {
  print("accesso al database admin: negato (corretto)");
}
EOF
