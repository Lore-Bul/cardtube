/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "i18n.h"

#include <cctype>
#include <cstring>

namespace i18n {
namespace {

struct Entry {
    const char* english;
    const char* italian;
};

// Keys are the English source strings used by the UI. Keeping the English text
// as the key means a missing entry simply falls back to English.
constexpr Entry kEntries[] = {
    // Pages and navigation
    {"Search", "Cerca"},
    {"Results", "Risultati"},
    {"Player", "Player"},
    {"Queue", "Coda"},
    {"Settings", "Impostazioni"},

    // Page hints
    {"Type a search and press Enter", "Scrivi una ricerca e premi Invio"},
    {"UP/DOWN select - Enter plays - Q queues - V video",
     "SU/GIÙ scegli - Invio riproduce - Q in coda - V video"},
    {"SPACE pause - LEFT/RIGHT seek - N next - V video",
     "SPAZIO pausa - SINISTRA/DESTRA cerca - N avanti - V video"},
    {"UP/DOWN select - Enter plays - D removes - C clears",
     "SU/GIÙ scegli - Invio riproduce - D rimuove - C svuota"},
    {"UP/DOWN select - LEFT/RIGHT changes - Enter confirms",
     "SU/GIÙ scegli - SINISTRA/DESTRA cambia - Invio conferma"},

    // Settings
    {"Theme", "Tema"},
    {"Language", "Lingua"},
    {"Keyboard", "Tastiera"},
    {"Video mode", "Modalità video"},
    {"Autoplay next", "Riproduci dopo"},
    {"Quality", "Qualità"},
    {"Dependencies", "Dipendenze"},
    {"Update yt-dlp", "Aggiorna yt-dlp"},
    {"Quit CardTube", "Esci da CardTube"},
    {"Dark", "Scuro"},
    {"Light", "Chiaro"},
    {"On", "Sì"},
    {"Off", "No"},
    {"Press Enter", "Premi Invio"},
    {"Ready", "Pronto"},
    {"Missing tools", "Strumenti mancanti"},
    {"Best", "Migliore"},

    // Search and queue status
    {"Nothing is playing yet", "Niente in riproduzione"},
    {"Search already in progress", "Ricerca già in corso"},
    {"Type something to search", "Scrivi qualcosa da cercare"},
    {"Searching YouTube for ", "Ricerca su YouTube di "},
    {"Search failed: ", "Ricerca fallita: "},
    {" results - Enter plays, Q queues, V video",
     " risultati - Invio riproduce, Q in coda, V video"},
    {"Already in the queue: ", "Già in coda: "},
    {"Queued: ", "In coda: "},
    {"No result selected - run a search first", "Nessun risultato - avvia una ricerca"},
    {"No result selected", "Nessun risultato selezionato"},
    {"Nothing to play in the queue", "Niente da riprodurre in coda"},
    {"The queue is empty", "La coda è vuota"},
    {"Removed: ", "Rimosso: "},
    {"The queue is already empty", "La coda è già vuota"},
    {"Queue cleared", "Coda svuotata"},
    {"End of the queue", "Fine della coda"},

    // Playback status
    {"Restarted the current track", "Brano riavviato"},
    {"No previous track", "Nessun brano precedente"},
    {"Invalid track", "Brano non valido"},
    {"A track is already loading, try again in a moment",
     "Brano in caricamento, riprova tra poco"},
    {"Loading ", "Caricamento "},
    {"mpv is not installed or failed to start", "mpv non è installato o non si avvia"},
    {"unable to open the audio stream (see /tmp/cardtube/mpv.log)",
     "impossibile aprire l'audio (vedi /tmp/cardtube/mpv.log)"},
    {"Playing - Space pause, N next, V video",
     "In riproduzione - Spazio pausa, N avanti, V video"},
    {"Nothing to watch", "Niente da guardare"},
    {"A video is already starting or playing", "Un video è già in avvio"},
    {"mpv is not installed - video playback needs mpv",
     "mpv non è installato - serve per il video"},
    {"Starting video... (ESC closes)", "Avvio video... (ESC chiude)"},
    {"Video cancelled", "Video annullato"},
    {"Nothing is playing", "Niente in riproduzione"},
    {"Paused", "In pausa"},
    {"Playing", "In riproduzione"},
    {"Stopped", "Fermato"},
    {"Volume ", "Volume "},
    {"Seek -10s", "-10 secondi"},
    {"Seek +10s", "+10 secondi"},
    {"Playback stopped", "Riproduzione fermata"},
    {"Nothing is playing - pick a result first",
     "Niente in riproduzione - scegli un risultato"},

    // Settings feedback
    {"Video mode on - playback opens the video",
     "Video attivo - la riproduzione apre il video"},
    {"Video mode off - audio only", "Video spento - solo audio"},
    {"Autoplay next: on", "Riproduci dopo: sì"},
    {"Autoplay next: off", "Riproduci dopo: no"},
    {"Quality: ", "Qualità: "},
    {"Checking yt-dlp, mpv and curl...", "Controllo yt-dlp, mpv e curl..."},
    {" - install them with apt or pip", " - installali con apt o pip"},
    {"Updating yt-dlp, this can take a minute...",
     "Aggiorno yt-dlp, può volerci un minuto..."},
    {"Reading versions...", "Lettura versioni..."},
    {"not installed", "non installato"},
    {"Language: ", "Lingua: "},
    {"Keyboard layout: ", "Tastiera: "},

    // Video session
    {"Video finished - Space resumes audio", "Video terminato - Spazio riprende l'audio"},
    {"Video closed", "Video chiuso"},
    {"Cancelling video...", "Annullo il video..."},
    {"Video failed: ", "Video non riuscito: "},
    {"Playback ended early - press N to try the next track",
     "Riproduzione finita presto - premi N per il prossimo"},
    {"Finished", "Terminato"},
    {"Ready - type a search, Tab switches pages",
     "Pronto - scrivi una ricerca, Tab cambia pagina"},
    {" - see Settings > Dependencies", " - vedi Impostazioni > Dipendenze"},

    // Search screen
    {"Search YouTube", "Cerca su YouTube"},
    {"Clear", "Pulisci"},
    {"Type a query, then press ENTER to search", "Scrivi e premi INVIO per cercare"},
    {"Type to search...", "Scrivi per cercare..."},

    // Results screen
    {"No results yet", "Ancora nessun risultato"},
    {"Type a query on the Search page", "Scrivi una ricerca nella pagina Cerca"},

    // Queue screen
    {"Queue is empty", "La coda è vuota"},
    {"Press Q on a result to queue it", "Premi Q su un risultato per accodarlo"},
    {" track", " brano"},
    {" tracks", " brani"},
    {"  -  playing ", "  -  in riproduzione "},

    // Now playing screen
    {"no art", "copertina assente"},
    {"SPACE pause   LEFT/RIGHT seek 10s   V video   N next",
     "SPAZIO pausa   SINISTRA/DESTRA 10s   V video   N avanti"},
    {"Pick a result or a queued track", "Scegli un risultato o un brano in coda"},

    // Help popup
    {"Keyboard help", "Aiuto tastiera"},
    {"SEARCH", "CERCA"},
    {"RESULTS", "RISULTATI"},
    {"QUEUE", "CODA"},
    {"SETTINGS", "IMPOSTAZIONI"},
    {"Type a query", "Scrivi una ricerca"},
    {"Delete a character", "Cancella un carattere"},
    {"Go to Results", "Vai ai risultati"},
    {"Save a screenshot", "Salva uno screenshot"},
    {"Move the selection", "Sposta la selezione"},
    {"Play the track", "Riproduci il brano"},
    {"Add to the queue", "Aggiungi alla coda"},
    {"Watch with video", "Guarda in video"},
    {"Back to search", "Torna alla ricerca"},
    {"Pause / resume", "Pausa / riprendi"},
    {"Seek 10 seconds", "Sposta di 10 secondi"},
    {"Next / previous track", "Brano successivo / precedente"},
    {"Stop playback", "Ferma la riproduzione"},
    {"Remove from the queue", "Rimuovi dalla coda"},
    {"Clear the queue", "Svuota la coda"},
    {"Back to the player", "Torna al player"},
    {"Change a value", "Cambia un valore"},
    {"Activate the row", "Attiva la riga"},
    {"Check deps / update yt-dlp", "Controlla dip. / aggiorna yt-dlp"},
    {"Switch page", "Cambia pagina"},
    {"HELP / ESC  Close    PRINTSCREEN  Screenshot",
     "AIUTO / ESC  Chiudi    STAMP  Screenshot"},

    // Shortcut toasts
    {"Saved to ", "Salvato in "},
    {"Screenshot failed: ", "Screenshot non riuscito: "},
    {"Hold ", "Tieni premuto "},
    {" to exit", " per uscire"},
};

Language g_language = Language::English;

} // namespace

Language language() {
    return g_language;
}

void set_language(Language language) {
    g_language = language;
}

const char* tr(const char* english) {
    if (!english || english[0] == '\0' || g_language == Language::English) {
        return english ? english : "";
    }
    for (const auto& entry : kEntries) {
        if (std::strcmp(entry.english, english) == 0) {
            return entry.italian;
        }
    }
    return english;
}

std::string tr(const std::string& english) {
    return tr(english.c_str());
}

const char* language_code(Language language) {
    return language == Language::Italian ? "it" : "en";
}

const char* language_name(Language language) {
    return language == Language::Italian ? "Italiano" : "English";
}

Language language_from_code(const std::string& code) {
    std::string normalized;
    normalized.reserve(code.size());
    for (const char character : code) {
        const auto value = static_cast<unsigned char>(character);
        if (!std::isspace(value)) {
            normalized.push_back(static_cast<char>(std::tolower(value)));
        }
    }
    if (normalized == "it" || normalized == "ita" || normalized == "italian" ||
        normalized == "italiano") {
        return Language::Italian;
    }
    return Language::English;
}

} // namespace i18n
