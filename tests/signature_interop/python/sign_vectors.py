"""Generatore dei vettori di prova per la firma degli eventi (lato gateway).

Questo script simula il Raspberry Pi del varco: costruisce alcuni eventi di
accesso conformi a ``docs/event-schema.md``, li firma con ECDSA P-256 e scrive
i risultati in ``docs/test-vectors/``. Il verificatore Node.js in
``tests/signature_interop/node`` legge gli stessi file e deve ottenere, per
ogni caso, l'esito atteso. Se ci riesce, Python e Node concordano su:

* forma canonica dei dati firmati (JSON Canonicalization Scheme, RFC 8785);
* algoritmo di firma (ECDSA su curva P-256 con SHA-256, FIPS 186-5);
* codifica della firma (DER, poi base64 standard).

File prodotti:

* ``docs/test-vectors/public_key.pem``: chiave pubblica del gateway di prova.
  È pubblica per definizione e va nel repository.
* ``docs/test-vectors/vectors.json``: i casi di prova con l'esito atteso.

La chiave **privata** del gateway di prova viene salvata in
``secrets/gateway_test.key``, accanto a questo script. Il percorso è escluso da
Git (regole ``secrets/`` e ``*.key`` del ``.gitignore``) e il file è creato con
permessi 0600. Non serve a chi verifica: i vettori contengono già le firme.

Uso::

    python sign_vectors.py

Rieseguire lo script riutilizza la stessa chiave e produce file identici,
perché le firme sono deterministiche (RFC 6979).

Lo script non apre connessioni di rete e scrive solo nei due percorsi sopra.
Tutti i dati sono fittizi.
"""

from __future__ import annotations

import base64
import copy
import hashlib
import json
import os
import sys
from pathlib import Path

import rfc8785
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec

# ---------------------------------------------------------------------------
# Percorsi
# ---------------------------------------------------------------------------

#: Cartella che contiene questo script.
SCRIPT_DIR = Path(__file__).resolve().parent

#: Radice del repository (tests/signature_interop/python -> radice).
REPO_ROOT = SCRIPT_DIR.parents[2]

#: Dove si salva la chiave privata di prova (ignorata da Git).
PRIVATE_KEY_PATH = SCRIPT_DIR / "secrets" / "gateway_test.key"

#: Cartella pubblica dei vettori di prova.
VECTORS_DIR = REPO_ROOT / "docs" / "test-vectors"

# ---------------------------------------------------------------------------
# Costanti del protocollo (da docs/event-schema.md)
# ---------------------------------------------------------------------------

#: Contesto di protocollo incluso nei dati firmati (§2 di event-schema.md).
#: Impedisce di riutilizzare una firma prodotta per un altro tipo di messaggio.
PROTOCOL_CONTEXT = "unisa-ac/event/v1"

#: Versione dello schema degli eventi.
SCHEMA_VERSION = 1

#: Identificativi del gateway e della sua chiave di prova.
GATEWAY_ID = "gw-test-01"
KEY_ID = "gw-test-01-k1"

#: Pseudonimo fittizio di un operatore. Nel sistema reale è
#: HMAC-SHA256(K_pseud, UID del badge); qui basta un valore esadecimale
#: di 64 caratteri senza alcun legame con badge reali.
DEMO_SUBJECT = hashlib.sha256(b"operatore-fittizio-001").hexdigest()

#: Hash fittizio del documento di policy applicato.
DEMO_POLICY_HASH = hashlib.sha256(b"policy-fittizia-v7").hexdigest()


# ---------------------------------------------------------------------------
# Chiavi
# ---------------------------------------------------------------------------

def load_or_create_private_key(path: Path) -> ec.EllipticCurvePrivateKey:
    """Carica la chiave privata di prova oppure, se manca, la genera.

    Riutilizzare la stessa chiave rende i vettori stabili fra un'esecuzione
    e l'altra: rigenerandola cambierebbero chiave pubblica e firme.

    Il file viene creato con permessi 0600 (lettura e scrittura solo per il
    proprietario) già al momento dell'apertura, così non esiste nemmeno un
    istante in cui sia leggibile da altri utenti.

    Args:
        path: percorso del file PEM (PKCS#8) della chiave privata.

    Returns:
        La chiave privata ECDSA su curva P-256 (SECP256R1).

    Raises:
        ValueError: se il file esiste ma non contiene una chiave P-256.
    """
    if path.exists():
        key = serialization.load_pem_private_key(path.read_bytes(), password=None)
        if not isinstance(key, ec.EllipticCurvePrivateKey) or not isinstance(
            key.curve, ec.SECP256R1
        ):
            raise ValueError(f"{path} non contiene una chiave privata P-256")
        return key

    key = ec.generate_private_key(ec.SECP256R1())
    pem = key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.PKCS8,
        # Chiave di sola prova, protetta da permessi e .gitignore. Una chiave
        # reale del gateway richiede invece una protezione da definire
        # (punto aperto 1 di event-schema.md).
        encryption_algorithm=serialization.NoEncryption(),
    )
    path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "wb") as f:
        f.write(pem)
    return key


def public_key_pem(key: ec.EllipticCurvePrivateKey) -> str:
    """Restituisce la chiave pubblica corrispondente in formato PEM (SPKI).

    Args:
        key: chiave privata del gateway.

    Returns:
        Il testo PEM che inizia con ``-----BEGIN PUBLIC KEY-----``.
    """
    return (
        key.public_key()
        .public_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PublicFormat.SubjectPublicKeyInfo,
        )
        .decode("ascii")
    )


# ---------------------------------------------------------------------------
# Canonicalizzazione e firma
# ---------------------------------------------------------------------------

def canonical_bytes(event: dict) -> bytes:
    """Calcola la forma canonica di un evento secondo RFC 8785.

    Due serializzazioni JSON dello stesso oggetto possono differire per
    ordine dei campi e spazi; la firma però è calcolata sui byte. La forma
    canonica fissa un'unica sequenza di byte per ogni oggetto, così chi firma
    e chi verifica lavorano sugli stessi dati.

    Args:
        event: l'oggetto ``event`` (senza la busta e senza la firma).

    Returns:
        La forma canonica codificata in UTF-8.
    """
    return rfc8785.dumps(event)


def sign_event(event: dict, key: ec.EllipticCurvePrivateKey, kid: str) -> dict:
    """Firma un evento e lo inserisce nella busta prevista dallo schema.

    Si firma solo l'oggetto ``event`` in forma canonica, mai la busta
    (§4 di event-schema.md). La libreria restituisce la firma in DER.

    Args:
        event: evento da firmare.
        key: chiave privata del firmatario.
        kid: identificativo della chiave, da inserire nella busta.

    Returns:
        La busta ``{"event": ..., "sig": {"alg", "kid", "value"}}``.
    """
    # deterministic_signing=True applica RFC 6979: il nonce k è derivato
    # dalla chiave e dal messaggio invece che dal generatore casuale. Così
    # rieseguire lo script produce file identici, e la sicurezza non dipende
    # dalla qualità del generatore casuale nel momento della firma.
    signature_der = key.sign(
        canonical_bytes(event),
        ec.ECDSA(hashes.SHA256(), deterministic_signing=True),
    )
    return {
        "event": event,
        "sig": {
            "alg": "ES256",
            "kid": kid,
            "value": base64.b64encode(signature_der).decode("ascii"),
        },
    }


def verify_envelope(envelope: dict, public_key: ec.EllipticCurvePublicKey) -> bool:
    """Verifica una busta con la chiave pubblica data (controllo interno).

    Serve a controllare, prima di scrivere i file, che i casi marcati come
    validi lo siano davvero anche per Python.

    Args:
        envelope: busta con evento e firma.
        public_key: chiave pubblica del gateway.

    Returns:
        ``True`` se la firma è valida, ``False`` altrimenti.
    """
    try:
        signature = base64.b64decode(envelope["sig"]["value"], validate=True)
        public_key.verify(
            signature, canonical_bytes(envelope["event"]), ec.ECDSA(hashes.SHA256())
        )
        return True
    except (InvalidSignature, ValueError, KeyError):
        return False


# ---------------------------------------------------------------------------
# Eventi di esempio
# ---------------------------------------------------------------------------

def make_event(seq: int, ts: str, event_type: str, **extra) -> dict:
    """Costruisce un evento di accesso fittizio conforme a event-schema.md.

    I campi sono inseriti volutamente in un ordine diverso da quello
    alfabetico: così la prova verifica davvero che entrambi i lati
    riordinino i campi allo stesso modo.

    Args:
        seq: numero progressivo del gateway.
        ts: istante della decisione, RFC 3339 in UTC.
        event_type: ``ACCESS_GRANTED`` o ``ACCESS_DENIED``.
        **extra: campi aggiuntivi o sostitutivi (es. ``reason``, ``online``).

    Returns:
        Il dizionario dell'evento.
    """
    event = {
        "type": event_type,
        "seq": seq,
        "ctx": PROTOCOL_CONTEXT,
        "gateway_id": GATEWAY_ID,
        "v": SCHEMA_VERSION,
        "terminal_id": "term-01",
        "ts": ts,
        "subject": DEMO_SUBJECT,
        "factors": {"pin": "ok", "badge": "ok"},
        "policy_version": 7,
        "policy_hash": DEMO_POLICY_HASH,
        "online": True,
    }
    event.update(extra)
    return event


def build_vectors(
    gateway_key: ec.EllipticCurvePrivateKey,
) -> list[dict]:
    """Prepara l'elenco dei casi di prova con il relativo esito atteso.

    Casi validi: accesso consentito, accesso negato fuori turno, accesso
    consentito offline. Casi da rifiutare: evento manomesso dopo la firma,
    evento firmato da una chiave sconosciuta, firma malformata.

    Args:
        gateway_key: chiave privata del gateway di prova.

    Returns:
        Lista di dizionari ``{id, description, expected, canonical, envelope}``.
        ``canonical`` è la forma canonica attesa, presente solo per i casi
        validi, così il verificatore può confrontarla con la propria.
    """
    granted = make_event(1, "2026-11-16T08:15:02.120Z", "ACCESS_GRANTED")
    denied = make_event(
        2,
        "2026-11-16T20:41:07.512Z",
        "ACCESS_DENIED",
        reason="OUT_OF_SHIFT",
    )
    offline = make_event(3, "2026-11-16T09:02:44.003Z", "ACCESS_GRANTED", online=False)

    vectors = []
    for vid, description, event in (
        ("valid-granted", "Accesso consentito, in turno, online.", granted),
        ("valid-denied-out-of-shift", "Accesso negato fuori turno.", denied),
        ("valid-granted-offline", "Accesso consentito durante una disconnessione.", offline),
    ):
        vectors.append(
            {
                "id": vid,
                "description": description,
                "expected": "valid",
                "canonical": canonical_bytes(event).decode("utf-8"),
                "envelope": sign_event(event, gateway_key, KEY_ID),
            }
        )

    # Manomissione: si firma il diniego, poi lo si trasforma in un accesso
    # consentito senza rifirmare. La firma non corrisponde più ai dati.
    tampered = sign_event(denied, gateway_key, KEY_ID)
    tampered = copy.deepcopy(tampered)
    tampered["event"]["type"] = "ACCESS_GRANTED"
    del tampered["event"]["reason"]
    vectors.append(
        {
            "id": "invalid-tampered-type",
            "description": "Diniego trasformato in accesso dopo la firma.",
            "expected": "invalid",
            "canonical": None,
            "envelope": tampered,
        }
    )

    # Firma di un gateway estraneo. Il kid dichiarato è quello legittimo,
    # per simulare un impostore che si spaccia per il gateway. La chiave è
    # derivata da una stringa pubblica, così il vettore resta identico fra
    # un'esecuzione e l'altra: proprio per questo NON è una chiave segreta e
    # va usata solo qui.
    rogue_scalar = int.from_bytes(
        hashlib.sha256(b"chiave-estranea-solo-per-test").digest(), "big"
    )
    rogue_key = ec.derive_private_key(rogue_scalar, ec.SECP256R1())
    vectors.append(
        {
            "id": "invalid-wrong-key",
            "description": "Evento firmato da una chiave diversa da quella registrata.",
            "expected": "invalid",
            "canonical": None,
            "envelope": sign_event(granted, rogue_key, KEY_ID),
        }
    )

    # Firma malformata: base64 valido ma il contenuto non è una struttura DER.
    malformed = sign_event(granted, gateway_key, KEY_ID)
    malformed = copy.deepcopy(malformed)
    malformed["sig"]["value"] = base64.b64encode(b"\x30\x05not-der").decode("ascii")
    vectors.append(
        {
            "id": "invalid-malformed-signature",
            "description": "Firma che non è una struttura DER valida.",
            "expected": "invalid",
            "canonical": None,
            "envelope": malformed,
        }
    )
    return vectors


# ---------------------------------------------------------------------------
# Programma principale
# ---------------------------------------------------------------------------

def main() -> int:
    """Genera la chiave (se serve), i vettori e il controllo interno.

    Returns:
        0 se i file sono stati scritti e il controllo interno è riuscito,
        1 altrimenti.
    """
    key = load_or_create_private_key(PRIVATE_KEY_PATH)
    vectors = build_vectors(key)

    # Controllo interno: Python deve dare l'esito atteso su ogni caso,
    # altrimenti non ha senso pubblicare i vettori.
    public_key = key.public_key()
    for vector in vectors:
        is_valid = verify_envelope(vector["envelope"], public_key)
        if is_valid != (vector["expected"] == "valid"):
            print(f"ERRORE: il caso {vector['id']} non dà l'esito atteso in Python")
            return 1

    VECTORS_DIR.mkdir(parents=True, exist_ok=True)
    (VECTORS_DIR / "public_key.pem").write_text(public_key_pem(key), encoding="ascii")
    document = {
        "description": (
            "Vettori di prova per la firma degli eventi del gateway "
            "(docs/event-schema.md, v0.2). Dati fittizi."
        ),
        "alg": "ES256",
        "signature_encoding": "DER, base64 standard",
        "canonicalization": "RFC 8785 (JCS), UTF-8",
        "gateway_id": GATEWAY_ID,
        "kid": KEY_ID,
        "public_key_file": "public_key.pem",
        "vectors": vectors,
    }
    (VECTORS_DIR / "vectors.json").write_text(
        json.dumps(document, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )

    print(f"Chiave privata di prova: {PRIVATE_KEY_PATH} (esclusa da Git)")
    print(f"Scritti {len(vectors)} vettori in {VECTORS_DIR / 'vectors.json'}")
    for vector in vectors:
        print(f"  OK  {vector['id']:<30} atteso: {vector['expected']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
