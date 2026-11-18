# Pubblicazione di CardTube

Questa guida copre i passaggi che restano da fare **una volta sola**, prima di
pubblicare il repository e il pacchetto sullo store CardputerZero.

L'assemblaggio, la validazione degli asset e i workflow sono già pronti: qui
serve solo inserire i tuoi dati personali e spingere il repository.

---

## 0. Cosa ti serve

| Requisito | Note |
| --- | --- |
| Account GitHub | Il repository sorgente deve essere pubblico e corrispondere a `store.source_repo`. |
| Account store / portale CardputerZero | Da lì ottieni lo `share_code` definitivo. |
| Git | `git --version` deve funzionare. |
| Python 3.10+ | Per `tools/set_identity.py` e `tools/preflight.py`. |

I valori che l'uploader controlla e che **non** sono deducibili dal codice sono
cinque:

| Valore | Esempio | Dove finisce |
| --- | --- | --- |
| Nome e cognome del maintainer | `Mario Rossi` | `APP_MAINTAINER` del `.deb` |
| Email del maintainer | `mario@example.com` | `APP_MAINTAINER` del `.deb` |
| Login GitHub | `mario-rossi` | `store.author.github`, URL del repo |
| Nome autore nello store | `Mario Rossi` | `store.author.display_name` (pubblico) |
| `share_code` | `AB12` | `store.share_code` (lo scegli tu) |

> Lo store rifiuta i manifest che contengono ancora `TODO`, `Your Name`,
> `example.invalid` o un'identità M5Stack. Per questo `tools/preflight.py`
> fallisce finché non hai eseguito il passaggio 1.

### Nome maintainer, display name e share code

Sono tre cose diverse:

- **Nome + email del maintainer** finiscono nel controllo Debian del pacchetto:
  li vedi con `dpkg -I CardTube_0.3.0_rev1_arm64.deb` alla voce `Maintainer:`.
  È un dato tecnico, non la vetrina dello store. Puoi usare nome e cognome
  reali (consigliato, serve per l'identità dell'autore).
- **`display_name`** è il nome pubblico dell'autore mostrato nelle liste e
  nella scheda dell'app nello store (Developer Center: *Author display name*).
  Può essere il tuo nome, un nickname o il nome del progetto: se non ti
  interessa distinguerli, usa lo stesso valore del nome.
- **`share_code`** non viene assegnato dal portale: **lo scegli tu**, di 4
  caratteri `A-Z`/`0-9`, e deve essere unico in tutto lo store. È il codice che
  gli utenti digitano sul dispositivo per saltare direttamente alla tua app.
  Non si può cambiare dopo la prima pubblicazione, quindi sceglilo con calma
  (es. `CTUB`, le iniziali, una parola corta).

---

## 1. Inserisci l'identità

Dalla radice del repository:

```powershell
python tools/set_identity.py `
  --name "Mario Rossi" `
  --email "mario@example.com" `
  --github "mario-rossi" `
  --display-name "Mario Rossi" `
  --share-code "AB12"
```

Su macOS/Linux:

```sh
python3 tools/set_identity.py \
  --name "Mario Rossi" \
  --email "mario@example.com" \
  --github "mario-rossi" \
  --display-name "Mario Rossi" \
  --share-code "AB12"
```

Lo script aggiorna:

- `app-builder.json`: `store.source_repo`, `store.author`, `store.share_code`
- `cmake/cm0-package.cmake`: `set(APP_MAINTAINER "...")`

Se il repository non si chiama `cardtube`, passa anche
`--repo https://github.com/<login>/<nome-repo>`.

Il `share_code` deve essere di **4 caratteri A-Z / 0-9**, unico nello store e
non va più cambiato dopo la prima pubblicazione. Scegli un codice che ti
ricordi: viene usato dagli utenti sul dispositivo per aprire direttamente la
tua app. Se vuoi essere sicuro che sia libero, inseriscilo nel form del
Developer Center: un codice già usato viene rifiutato in fase di submission.

---

## 2. Verifica i metadati

```powershell
python tools/preflight.py
```

Controlla in un colpo solo:

- nessun segnaposto residuo;
- versione e `package_name` allineati a `CMakeLists.txt`;
- `summary` entro 80 caratteri;
- da 1 a 6 screenshot, tutti PNG reali 320x170;
- icona quadrata tra 128 e 512 px;
- `share_code`, categorie, licenza e i sette flag di permessi;
- `APP_MAINTAINER` nel formato `Nome <email>`.

In alternativa, il validatore ufficiale del contratto di release:

```powershell
cmake -P .agents/skills/cardputerzero-package-release/scripts/validate_app_builder.cmake
```

---

## 3. Crea il repository e fai il push

Il repository non è ancora inizializzato: puoi farlo ora. Fallo **dopo** il
passaggio 1: il job `metadata` di GitHub Actions esegue `tools/preflight.py`
in modalità stretta su `main` e sui tag, quindi un push con i segnaposto ancora
presenti fallisce di proposito.

```powershell
git init -b main
git add -A
git commit -m "CardTube 0.3.0"
```

Crea su GitHub un repository **vuoto** chiamato `cardtube` (senza README,
senza licenza, senza `.gitignore`) e poi:

```powershell
git remote add origin https://github.com/<login>/cardtube.git
git push -u origin main
```

Se Git non ha un'identità configurata, impostala prima del commit:

```powershell
git config user.name "Mario Rossi"
git config user.email "mario@example.com"
```

---

## 4. Ottieni il pacchetto da GitHub Actions

Il workflow `.github/workflows/build-deb.yml` parte su ogni push verso `main` e
sui tag `v*`. Esegue:

1. **preflight**: `tools/preflight.py` (sui push e sui tag, non sulle PR);
2. **aarch64 .deb**: cross build con `crossbuild-essential-arm64`,
   `cmake --workflow --preset cp0-cross-package`, validazione del manifest,
   ispezione con `dpkg-deb` e upload dell'artifact `cardtube-deb`;
3. **desktop**: build Linux x86-64 con SDL2 come test di regressione;
4. **release** (solo su tag `v*`): allega il `.deb` alla release GitHub.

Scarica il pacchetto da **Actions → build → artifact `cardtube-deb`**
(`CardTube_0.3.0_rev1_arm64.deb`).

Per compilarlo in locale su un host Debian/Ubuntu:

```sh
sudo apt install crossbuild-essential-arm64 ninja-build dpkg-dev python3-pip
pip3 install --break-system-packages "cmake>=3.31"
cmake --workflow --preset cp0-cross-package
```

---

## 5. Prova il pacchetto sul Cardputer Zero

```sh
scp dist/CardTube_0.3.0_rev1_arm64.deb <utente>@<ip>:~/
ssh <utente>@<ip>
sudo apt install --no-install-recommends ./CardTube_0.3.0_rev1_arm64.deb
cardtube
```

Checklist minima sul dispositivo:

- l'app compare in APPLaunch con l'icona CardTube;
- una ricerca restituisce risultati e le miniature si caricano;
- l'audio parte e la UI resta reattiva;
- `V` apre il video a schermo intero e `ESC` torna alla UI;
- i tasti `Tab`/`Shift+Tab` cambiano pagina, `ESC` torna indietro;
- le impostazioni sopravvivono al riavvio.

---

## 6. Pubblica sullo store

La via più semplice è il **Developer Center**: <https://dev.cardputer.cc/>.

1. **Sign in with GitHub** con lo stesso account indicato in
   `store.author.github` (la proprietà del nome pacchetto è legata
   all'account: se pubblichi con un altro login, lo store risponde
   *not the owner of cardtube*).
2. Trascina il `.deb` che hai testato. Il file viene analizzato in locale e
   compare un *preliminary check report*: verifica i percorsi di installazione.
   La policy è che tutto ciò che finisce in una directory condivisa porti il
   nome del pacchetto — CardTube installa solo `/usr/bin/cardtube`,
   `/etc/cardtube.conf`, `/usr/share/cardtube/`,
   `/usr/share/APPLaunch/.../cardtube.*` e `/usr/share/doc/cardtube/`, quindi
   rientra nella regola.
3. Compila la scheda store: i campi possono essere precompilati dalla sezione
   `store` di `app-builder.json`. Controlla in particolare:
   - **App name** univoco nello store;
   - **Author display name** (vedi sopra);
   - **Share code** di 4 caratteri scelto da te;
   - categoria `Media`;
   - permessi: `Network` attivo, tutti gli altri spenti;
   - icona e screenshot (vengono ritagliati automaticamente a 256×256 e
     320×170).
4. Lascia attiva l'opzione *Don't publicly show my email address* se non vuoi
   l'email in chiaro: resta comunque registrata per l'identità dell'autore.
5. **Submit to AppStore**: la submission appare come *under review* e, dopo
   l'approvazione e la ricostruzione dell'indice APT, l'app è nello store.

In alternativa puoi usare la CLI ufficiale `czdev` (login GitHub device-flow,
poi `czdev publish --deb dist/CardTube_0.3.0_rev1_arm64.deb`), che apre una PR
sul repository `CardputerZero/packages`.

Se il nome `cardtube` risultasse già occupato, scegli un nome libero (es.
`cardtube-player`), aggiornalo in `CMakeLists.txt` (`project(...)`), in
`app-builder.json` (`package_name`) e nel nome del file `.deb`.

Il contratto completo è in
`.agents/skills/cardputerzero-package-release/references/release-contract.md`.

---

## 7. Release successive

Per ogni nuova versione:

1. aggiorna `project(cardtube VERSION x.y.z)` in `CMakeLists.txt`;
2. aggiorna `version` in `app-builder.json`;
3. aggiungi una voce in `CHANGELOG.md`;
4. se la UI cambia, rigenera gli screenshot (vedi README, sezione
   «Screenshot per lo store»);
5. esegui `python tools/preflight.py`;
6. commit, poi tag e push:

```powershell
git tag v0.3.1
git push origin main --tags
```

Il tag fa partire la release GitHub con il `.deb` allegato.
