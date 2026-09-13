#include "NotesStore.h"

#include <LittleFS.h>
#include <time.h>
#include "Logger.h"

namespace {

const char* TAG = "NOTES";
const char* IDS_PATH     = NOTES_DIR "/ids.tsv";
const char* IDS_TMP_PATH = NOTES_DIR "/ids.tmp";
const char* JOURNAL_PATH = NOTES_DIR "/journal.log";
const char* JOURNAL_OLD  = NOTES_DIR "/journal.old.log";

// Past this the journal rotates once. Notes are typed by hand; reaching it
// means years of runs, or something writing in a loop.
constexpr size_t JOURNAL_ROTATE_BYTES = 256 * 1024;

struct Note {
    uint32_t id;
    bool     extended;
    char     text[NOTE_TEXT_MAX + 1];
};

Note   s_notes[NOTES_MAX];
size_t s_count = 0;
bool   s_fs    = false;

// Trim, drop control characters (a tab or newline would break both files), and
// cut to size without ever leaving half of a multi-byte UTF-8 character.
void sanitize(const char* in, char* out, size_t outLen) {
    if (outLen == 0) return;
    const char* p = in ? in : "";
    while (*p == ' ') ++p;
    size_t n = 0;
    bool   cut = false;
    for (; *p; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (c < 0x20 || c == 0x7F) continue;
        if (n + 1 >= outLen) { cut = true; break; }
        out[n++] = static_cast<char>(c);
    }
    if (cut) {
        // The cap may have landed inside a character. Walk back over its
        // continuation bytes to the lead byte, and drop the character if fewer
        // continuation bytes fitted than the lead byte promises.
        size_t i = n, cont = 0;
        while (i > 0 && (static_cast<unsigned char>(out[i - 1]) & 0xC0) == 0x80) { --i; ++cont; }
        if (i > 0) {
            const unsigned char lead = static_cast<unsigned char>(out[i - 1]);
            const size_t need = (lead >= 0xF0) ? 3 : (lead >= 0xE0) ? 2
                              : (lead >= 0xC0) ? 1 : 0;
            if (cont < need) n = i - 1;
        }
    }
    while (n > 0 && out[n - 1] == ' ') --n;
    out[n] = '\0';
}

int find(uint32_t id, bool extended) {
    for (size_t i = 0; i < s_count; ++i) {
        if (s_notes[i].id == id && s_notes[i].extended == extended) return static_cast<int>(i);
    }
    return -1;
}

bool persist() {
    if (!s_fs) return false;
    File f = LittleFS.open(IDS_TMP_PATH, "w");
    if (!f) return false;
    for (size_t i = 0; i < s_count; ++i) {
        f.printf("%c%lX\t%s\n", s_notes[i].extended ? 'X' : 'S',
                 (unsigned long)s_notes[i].id, s_notes[i].text);
    }
    f.close();
    // Write-then-rename, so a power cut mid-save leaves the previous file whole.
    LittleFS.remove(IDS_PATH);
    return LittleFS.rename(IDS_TMP_PATH, IDS_PATH);
}

void journal(uint32_t id, bool extended, const char* text) {
    if (!s_fs) return;
    File probe = LittleFS.open(JOURNAL_PATH, "r");
    if (probe) {
        const size_t size = probe.size();
        probe.close();
        if (size > JOURNAL_ROTATE_BYTES) {
            LittleFS.remove(JOURNAL_OLD);
            LittleFS.rename(JOURNAL_PATH, JOURNAL_OLD);
        }
    }
    File f = LittleFS.open(JOURNAL_PATH, "a");
    if (!f) return;
    const time_t now = time(nullptr);
    f.printf("%lu\t%lld\t%c%lX\t%s\n", (unsigned long)millis(),
             (long long)(now > 1600000000 ? now : 0), extended ? 'X' : 'S',
             (unsigned long)id, text);
    f.close();
}

}  // namespace

namespace NotesStore {

void begin(bool filesystemMounted) {
    s_count = 0;
    s_fs = filesystemMounted;
    if (!s_fs) {
        LOG_W(TAG, "Filesystem not mounted — notes will not be kept");
        return;
    }
    if (!LittleFS.exists(NOTES_DIR)) LittleFS.mkdir(NOTES_DIR);

    File f = LittleFS.open(IDS_PATH, "r");
    if (!f) {
        LOG_I(TAG, "No notes yet");
        return;
    }
    char line[NOTE_TEXT_MAX + 24];
    while (f.available() && s_count < NOTES_MAX) {
        const size_t n = f.readBytesUntil('\n', line, sizeof(line) - 1);
        line[n] = '\0';
        const char* tab = strchr(line, '\t');
        if (tab == nullptr || (line[0] != 'S' && line[0] != 'X')) continue;
        Note& note = s_notes[s_count];
        note.extended = line[0] == 'X';
        note.id = strtoul(line + 1, nullptr, 16);
        sanitize(tab + 1, note.text, sizeof(note.text));
        if (note.text[0] != '\0') ++s_count;
    }
    f.close();
    LOG_I(TAG, "Loaded %u note(s)", (unsigned)s_count);
}

bool set(uint32_t id, bool extended, const char* text, char* stored, size_t storedLen) {
    char clean[NOTE_TEXT_MAX + 1];
    sanitize(text, clean, sizeof(clean));
    if (stored != nullptr && storedLen > 0) strlcpy(stored, clean, storedLen);

    const int at = find(id, extended);
    if (clean[0] == '\0') {                       // clear
        if (at < 0) return true;
        for (size_t i = static_cast<size_t>(at); i + 1 < s_count; ++i) s_notes[i] = s_notes[i + 1];
        --s_count;
    } else if (at >= 0) {                         // replace
        if (strcmp(s_notes[at].text, clean) == 0) return true;
        strlcpy(s_notes[at].text, clean, sizeof(s_notes[at].text));
    } else {                                      // add
        if (s_count >= NOTES_MAX) return false;
        s_notes[s_count].id = id;
        s_notes[s_count].extended = extended;
        strlcpy(s_notes[s_count].text, clean, sizeof(s_notes[s_count].text));
        ++s_count;
    }
    journal(id, extended, clean);
    return persist();
}

void writeJson(JsonWriter& j) {
    j.add("\"notes\":[");
    for (size_t i = 0; i < s_count && !j.overflow(); ++i) {
        j.add("%s{\"id\":%lu,\"x\":%u,\"t\":", i ? "," : "",
              (unsigned long)s_notes[i].id, s_notes[i].extended ? 1u : 0u);
        j.addString(s_notes[i].text);
        j.add("}");
    }
    j.add("]");
}

size_t count()     { return s_count; }
bool   available() { return s_fs; }

}  // namespace NotesStore
