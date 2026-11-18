/**
 * @file Test del contratto AuditAnchor.
 *
 * Ogni test parte da un registro appena distribuito sulla rete simulata di
 * Hardhat (fixture), con tre account:
 * - admin: amministra i ruoli;
 * - publisher: il backend, l'unico autorizzato a registrare lotti;
 * - outsider: un account qualsiasi, senza ruoli.
 *
 * I test coprono le tre proprietà richieste dalle linee guida del gruppo
 * (deposito autorizzato, rifiuto delle scritture non autorizzate, gestione
 * dei duplicati) più la validazione degli argomenti e la gestione dei ruoli.
 */

import { expect } from "chai";
import { network } from "hardhat";

const { ethers, networkHelpers } = await network.create();

/** Impronta di esempio: SHA-256 di una stringa fittizia, come farebbe il backend. */
const ROOT_A = ethers.sha256(ethers.toUtf8Bytes("lotto-fittizio-0"));
const ROOT_B = ethers.sha256(ethers.toUtf8Bytes("lotto-fittizio-1"));

/** Intervallo temporale di esempio, in secondi Unix (16/11/2026). */
const FIRST = 1_794_816_000n;
const LAST = FIRST + 600n;

/**
 * Distribuisce un registro nuovo e restituisce contratto e account.
 * Usata con loadFixture: Hardhat ripristina lo stato salvato invece di
 * ridistribuire il contratto a ogni test.
 */
async function deployFixture() {
  const [admin, publisher, outsider] = await ethers.getSigners();
  const anchor = await ethers.deployContract("AuditAnchor", [admin.address, publisher.address]);
  const publisherRole = await anchor.PUBLISHER_ROLE();
  const adminRole = await anchor.DEFAULT_ADMIN_ROLE();
  return { anchor, admin, publisher, outsider, publisherRole, adminRole };
}

describe("AuditAnchor", function () {
  describe("Distribuzione", function () {
    it("assegna i ruoli iniziali e parte dal lotto 0", async function () {
      const { anchor, admin, publisher, publisherRole, adminRole } =
        await networkHelpers.loadFixture(deployFixture);

      expect(await anchor.hasRole(adminRole, admin.address)).to.equal(true);
      expect(await anchor.hasRole(publisherRole, publisher.address)).to.equal(true);
      expect(await anchor.hasRole(publisherRole, admin.address)).to.equal(false);
      expect(await anchor.nextBatchId()).to.equal(0n);
    });

    it("rifiuta l'indirizzo zero come amministratore o publisher", async function () {
      const [admin] = await ethers.getSigners();
      const factory = await ethers.getContractFactory("AuditAnchor");

      await expect(factory.deploy(ethers.ZeroAddress, admin.address))
        .to.be.revertedWithCustomError(factory, "ZeroAddress");
      await expect(factory.deploy(admin.address, ethers.ZeroAddress))
        .to.be.revertedWithCustomError(factory, "ZeroAddress");
    });
  });

  describe("Registrazione autorizzata", function () {
    it("registra un lotto, emette l'evento e conserva i dati", async function () {
      const { anchor, publisher } = await networkHelpers.loadFixture(deployFixture);

      await expect(anchor.connect(publisher).anchorBatch(0, ROOT_A, FIRST, LAST, 42))
        .to.emit(anchor, "BatchAnchored")
        .withArgs(0n, ROOT_A, FIRST, LAST, 42n, publisher.address);

      const batch = await anchor.getBatch(0);
      expect(batch.root).to.equal(ROOT_A);
      expect(batch.firstEventTime).to.equal(FIRST);
      expect(batch.lastEventTime).to.equal(LAST);
      expect(batch.eventCount).to.equal(42n);
      expect(batch.anchoredAt).to.equal(BigInt(await networkHelpers.time.latest()));
      expect(await anchor.nextBatchId()).to.equal(1n);
    });

    it("registra lotti consecutivi e li ritrova per impronta", async function () {
      const { anchor, publisher } = await networkHelpers.loadFixture(deployFixture);

      await anchor.connect(publisher).anchorBatch(0, ROOT_A, FIRST, LAST, 1);
      await anchor.connect(publisher).anchorBatch(1, ROOT_B, LAST, LAST, 1);

      expect(await anchor.nextBatchId()).to.equal(2n);
      expect(await anchor.findRoot(ROOT_B)).to.deep.equal([true, 1n]);
      expect(await anchor.findRoot(ethers.ZeroHash)).to.deep.equal([false, 0n]);
    });
  });

  describe("Scritture non autorizzate", function () {
    it("rifiuta la registrazione da un account senza ruolo", async function () {
      const { anchor, outsider, publisherRole } = await networkHelpers.loadFixture(deployFixture);

      await expect(anchor.connect(outsider).anchorBatch(0, ROOT_A, FIRST, LAST, 1))
        .to.be.revertedWithCustomError(anchor, "AccessControlUnauthorizedAccount")
        .withArgs(outsider.address, publisherRole);
    });

    it("rifiuta la registrazione anche dall'amministratore", async function () {
      const { anchor, admin, publisherRole } = await networkHelpers.loadFixture(deployFixture);

      // Separazione dei compiti: chi gestisce i ruoli non scrive i lotti.
      await expect(anchor.connect(admin).anchorBatch(0, ROOT_A, FIRST, LAST, 1))
        .to.be.revertedWithCustomError(anchor, "AccessControlUnauthorizedAccount")
        .withArgs(admin.address, publisherRole);
    });

    it("rifiuta un publisher revocato (sostituzione della chiave del backend)", async function () {
      const { anchor, admin, publisher, outsider, publisherRole } =
        await networkHelpers.loadFixture(deployFixture);

      await anchor.connect(admin).revokeRole(publisherRole, publisher.address);
      await anchor.connect(admin).grantRole(publisherRole, outsider.address);

      await expect(anchor.connect(publisher).anchorBatch(0, ROOT_A, FIRST, LAST, 1))
        .to.be.revertedWithCustomError(anchor, "AccessControlUnauthorizedAccount");
      await expect(anchor.connect(outsider).anchorBatch(0, ROOT_A, FIRST, LAST, 1))
        .to.emit(anchor, "BatchAnchored");
    });

    it("impedisce a un account senza ruolo di assegnarsi il ruolo di publisher", async function () {
      const { anchor, outsider, publisherRole, adminRole } =
        await networkHelpers.loadFixture(deployFixture);

      await expect(anchor.connect(outsider).grantRole(publisherRole, outsider.address))
        .to.be.revertedWithCustomError(anchor, "AccessControlUnauthorizedAccount")
        .withArgs(outsider.address, adminRole);
    });
  });

  describe("Duplicati e ordine dei lotti", function () {
    it("rifiuta di registrare due volte lo stesso lotto", async function () {
      const { anchor, publisher } = await networkHelpers.loadFixture(deployFixture);

      await anchor.connect(publisher).anchorBatch(0, ROOT_A, FIRST, LAST, 1);
      // Caso tipico: il backend ritenta dopo un errore di rete.
      await expect(anchor.connect(publisher).anchorBatch(0, ROOT_A, FIRST, LAST, 1))
        .to.be.revertedWithCustomError(anchor, "UnexpectedBatchId")
        .withArgs(1n, 0n);
    });

    it("rifiuta la stessa impronta in un lotto successivo", async function () {
      const { anchor, publisher } = await networkHelpers.loadFixture(deployFixture);

      await anchor.connect(publisher).anchorBatch(0, ROOT_A, FIRST, LAST, 1);
      await expect(anchor.connect(publisher).anchorBatch(1, ROOT_A, FIRST, LAST, 1))
        .to.be.revertedWithCustomError(anchor, "RootAlreadyAnchored")
        .withArgs(ROOT_A, 0n);
    });

    it("rifiuta un lotto che ne salta un altro", async function () {
      const { anchor, publisher } = await networkHelpers.loadFixture(deployFixture);

      await expect(anchor.connect(publisher).anchorBatch(1, ROOT_A, FIRST, LAST, 1))
        .to.be.revertedWithCustomError(anchor, "UnexpectedBatchId")
        .withArgs(0n, 1n);
    });
  });

  describe("Validazione degli argomenti", function () {
    it("rifiuta un'impronta nulla", async function () {
      const { anchor, publisher } = await networkHelpers.loadFixture(deployFixture);

      await expect(anchor.connect(publisher).anchorBatch(0, ethers.ZeroHash, FIRST, LAST, 1))
        .to.be.revertedWithCustomError(anchor, "ZeroRoot");
    });

    it("rifiuta un lotto vuoto", async function () {
      const { anchor, publisher } = await networkHelpers.loadFixture(deployFixture);

      await expect(anchor.connect(publisher).anchorBatch(0, ROOT_A, FIRST, LAST, 0))
        .to.be.revertedWithCustomError(anchor, "EmptyBatch");
    });

    it("rifiuta un intervallo temporale invertito", async function () {
      const { anchor, publisher } = await networkHelpers.loadFixture(deployFixture);

      await expect(anchor.connect(publisher).anchorBatch(0, ROOT_A, LAST, FIRST, 1))
        .to.be.revertedWithCustomError(anchor, "InvalidTimeRange")
        .withArgs(LAST, FIRST);
    });

    it("segnala un lotto inesistente in lettura", async function () {
      const { anchor } = await networkHelpers.loadFixture(deployFixture);

      await expect(anchor.getBatch(0))
        .to.be.revertedWithCustomError(anchor, "BatchNotFound")
        .withArgs(0n);
    });
  });
});
