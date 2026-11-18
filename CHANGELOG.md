# Changelog

Tutte le modifiche rilevanti di CardTube sono annotate in questo file.
Il formato segue [Keep a Changelog](https://keepachangelog.com/it/1.1.0/) e il
progetto usa il [versionamento semantico](https://semver.org/lang/it/).

## [0.4.2] - 2026-11-18

### Corretto

- Tastiera integrata: le lettere erano sbagliate (`qrstuvw...` invece di
  `qwerty...`, `abcde...` invece di `asdfg...`) perché i codici evdev seguono la
  posizione fisica dei tasti e non l'alfabeto. Ora le tre file usano tabelle
  esplicite (`qwertyuiop`, `asdfghjkl`, `zxcvbnm`) con Shift corretto.
- Audio Bluetooth: il log di `mpv` era sempre vuoto perché `--no-terminal`
  disattivava anche stdout/stderr. Ora `mpv` scrive sempre in
  `/tmp/cardtube/mpv.log` tramite `--log-file`.
- Audio Bluetooth: `mpv` riceve prima `pulse/<sink>` e solo come ripiego
  `pipewire/<sink>`; dopo il caricamento lo stream viene spostato sul sink
  scelto non appena compare, con più tentativi. Il sink viene anche smutato e,
  se è a volume zero, riportato al 60%.
- Diagnostica: CardTube scrive un log persistente in
  `/tmp/cardtube/cardtube.log` (utile quando l'app è avviata da APPLaunch) con
  sink disponibili, esito dei comandi `pactl`/`bluetoothctl` e proprietà audio
  effettive di `mpv`.

## [0.4.1] - 2026-11-18

### Aggiunto

- Interfaccia bilingue: inglese e italiano selezionabili dalla pagina Settings
  (`language=en/it` nel file di configurazione). Il cambio è immediato e
  ricostruisce le schermate senza riavviare l'app.
- La finestra di aiuto e i messaggi di stato sono tradotti.
- Uscita audio selezionabile dalla pagina Settings: cassa interna oppure cuffie
  e speaker Bluetooth già accoppiati (`audio_output` nel file di
  configurazione). Il cambio sposta subito lo stream di `mpv` sul nuovo sink
  PipeWire/PulseAudio e i dispositivi Bluetooth vengono connessi da CardTube.
- Tasti multimediali del layer Fn della tastiera integrata (play/pausa,
  precedente, successivo, mute, volume) attivi in ogni pagina.
- Self-test della mappa tastiera all'avvio, con esito nei log.
- La versione mostrata dalla UI ora deriva da CMake (`APP_VERSION`).

### Corretto

- Simboli della tastiera integrata CardputerZero: CardTube ora usa la mappa
  ufficiale M5Stack del layer Sym. La tastiera `tca8418` emette codici vendor
  non standard (26 = `!`, 93 = `?`, ...) che prima venivano interpretati come
  tasti US, producendo simboli sbagliati o mancanti. La mappa si applica solo
  alla tastiera integrata, non a quelle USB esterne.
- Rimossa la voce **Keyboard** dalle Impostazioni: l'hardware ha un'unica
  serigrafia fisica e la scelta US/IT non cambiava i tasti reali.
- Audio Bluetooth: `mpv` riceve ora esplicitamente il sink scelto tramite
  `audio-device` a ogni riproduzione, oltre a `pactl set-default-sink`; viene
  anche rimosso il mute del sink. Questo evita che WirePlumber riporti lo
  stream sul dispositivo precedente.

## [0.3.0] - 2026-11-18

### Aggiunto

- Riproduzione video a schermo intero tramite `mpv` sul framebuffer, con
  fallback DRM/GPU, pausa del refresh LVGL durante la visione e ritorno
  automatico alla UI.
- Demone audio `mpv` persistente (`--no-video --idle`) pilotato via socket
  Unix: la UI resta reattiva durante la riproduzione.
- Ricerca YouTube con `yt-dlp --flat-playlist`, risultati con titolo, canale,
  durata e miniatura.
- Miniatura a 320x170-friendly (`mqdefault`, fallback `default`) scaricata con
  `curl` e messa in cache in `<runtime>/thumbs/`.
- Coda di riproduzione con aggiunta, rimozione, svuotamento e avanzamento
  automatico a fine traccia.
- Pagina Settings con tema, modalità video, autoplay, volume, qualità,
  controllo dipendenze e aggiornamento di `yt-dlp`.
- Screenshot PNG catturabili dalla UI e modalità demo per generare gli
  screenshot 320x170 dello store.
- Pacchetto Debian `arm64` con voce APPLaunch, icona e conffile (il servizio
  systemd è opzionale e disattivato per default).
- Workflow GitHub Actions che compila in cross, valida il manifest e pubblica
  il `.deb` come artifact (e come release GitHub sui tag `v*`).
- `tools/set_identity.py` e `tools/preflight.py` per preparare e verificare i
  metadati di pubblicazione.

### Note

- Prima release pubblica: `dist/CardTube_0.3.0_rev1_arm64.deb`.
