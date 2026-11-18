# Changelog

Tutte le modifiche rilevanti di CardTube sono annotate in questo file.
Il formato segue [Keep a Changelog](https://keepachangelog.com/it/1.1.0/) e il
progetto usa il [versionamento semantico](https://semver.org/lang/it/).

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
