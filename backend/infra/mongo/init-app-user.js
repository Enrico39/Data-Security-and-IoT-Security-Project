// Crea l'utente applicativo del backend, con permessi solo sul proprio database
// (principio del minimo privilegio). MongoDB esegue questo script una sola volta,
// al primo avvio, quando il volume dei dati è vuoto. Le credenziali arrivano dalle
// variabili d'ambiente impostate in docker-compose.yml a partire dal file .env.

const dbName = process.env.MONGO_APP_DB;
const user = process.env.MONGO_APP_USER;
const pwd = process.env.MONGO_APP_PASSWORD;

if (!dbName || !user || !pwd) {
  throw new Error("Variabili MONGO_APP_DB, MONGO_APP_USER o MONGO_APP_PASSWORD mancanti");
}

db.getSiblingDB(dbName).createUser({
  user: user,
  pwd: pwd,
  roles: [{ role: "readWrite", db: dbName }],
  mechanisms: ["SCRAM-SHA-256"],
});

print(`Utente applicativo "${user}" creato sul database "${dbName}"`);
