#include "NmeaParser.h"

#include <string.h>
#include <stdlib.h>

namespace {

// Copies comma-separated field `index` out of a sentence. Empty fields are
// legal in NMEA and come back as an empty string.
bool field(const char* sentence, int index, char* out, size_t outLen) {
    if (out == nullptr || outLen == 0) return false;
    out[0] = '\0';

    const char* p = sentence;
    if (*p == '$') ++p;

    int current = 0;
    const char* start = p;
    while (true) {
        if (*p == ',' || *p == '*' || *p == '\0' || *p == '\r' || *p == '\n') {
            if (current == index) {
                size_t n = static_cast<size_t>(p - start);
                if (n >= outLen) n = outLen - 1;
                memcpy(out, start, n);
                out[n] = '\0';
                return true;
            }
            if (*p != ',') return false;   // ran out of fields
            ++current;
            start = p + 1;
        }
        ++p;
    }
}

int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Talker-agnostic: matches GPGGA, GNGGA, GLGGA and friends.
bool isSentence(const char* s, const char* type) {
    if (*s == '$') ++s;
    if (strlen(s) < 5) return false;
    return strncmp(s + 2, type, 3) == 0;
}

}  // namespace

namespace NmeaParser {

GnssFix makeEmptyFix() {
    GnssFix f;
    f.valid       = false;
    f.lat         = 0.0;
    f.lon         = 0.0;
    f.speed_kmh   = 0.0f;
    f.hdop        = 0.0f;
    f.satellites  = 0;
    f.fix_quality = 0;
    f.time_valid  = false;
    f.epoch       = 0;
    f.sats_in_view = 0;
    f.sats_tracked = 0;
    f.best_cnr     = 0;
    return f;
}

bool checksumOk(const char* sentence) {
    if (sentence == nullptr) return false;
    const char* p = sentence;
    if (*p == '$') ++p;

    uint8_t sum = 0;
    while (*p != '\0' && *p != '*') {
        if (*p == '\r' || *p == '\n') return false;   // truncated before '*'
        sum ^= static_cast<uint8_t>(*p);
        ++p;
    }
    if (*p != '*') return false;

    const int hi = hexVal(p[1]);
    const int lo = hexVal(p[2]);
    if (hi < 0 || lo < 0) return false;

    return sum == static_cast<uint8_t>((hi << 4) | lo);
}

bool parseCoordinate(const char* value, char hemisphere, double* out) {
    if (value == nullptr || out == nullptr || value[0] == '\0') return false;

    // ddmm.mmmm (latitude) or dddmm.mmmm (longitude): the minutes are always
    // the last two digits before the decimal point.
    const char* dot = strchr(value, '.');
    const size_t intLen = (dot != nullptr) ? static_cast<size_t>(dot - value)
                                           : strlen(value);
    if (intLen < 3) return false;

    char degBuf[6];
    const size_t degLen = intLen - 2;
    if (degLen >= sizeof(degBuf)) return false;
    memcpy(degBuf, value, degLen);
    degBuf[degLen] = '\0';

    char* end = nullptr;
    const double degrees = strtod(degBuf, &end);
    if (end == degBuf) return false;

    const double minutes = strtod(value + degLen, &end);
    if (end == value + degLen) return false;
    if (minutes < 0.0 || minutes >= 60.0) return false;

    double result = degrees + minutes / 60.0;
    if (hemisphere == 'S' || hemisphere == 'W') result = -result;

    if (result < -180.0 || result > 180.0) return false;
    *out = result;
    return true;
}

uint64_t toEpoch(int year, int month, int day, int hour, int minute, int second) {
    if (month < 1 || month > 12 || day < 1 || day > 31) return 0;

    // Days-from-civil, Howard Hinnant's algorithm. Shifting the year so that
    // March is month 1 makes the leap day the last day of the "year".
    int y = year;
    if (month <= 2) --y;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);              // 0..399
    const unsigned mp  = static_cast<unsigned>((month + 9) % 12);           // Mar=0
    const unsigned doy = (153u * mp + 2u) / 5u + static_cast<unsigned>(day) - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;          // 0..146096
    const long long days = static_cast<long long>(era) * 146097LL
                         + static_cast<long long>(doe) - 719468LL;

    const long long secs = days * 86400LL
                         + static_cast<long long>(hour) * 3600LL
                         + static_cast<long long>(minute) * 60LL
                         + static_cast<long long>(second);
    if (secs < 0) return 0;
    return static_cast<uint64_t>(secs);
}

// Accumulators for the multi-sentence GSV set — the only state in an otherwise
// stateless parser. Sentence 1 of each set clears them, so a dropped sentence
// costs one cycle and nothing carries over.
static uint8_t s_gsv_tracked = 0;
static uint8_t s_gsv_best    = 0;

NmeaResult apply(const char* sentence, GnssFix& fix) {
    if (sentence == nullptr || sentence[0] == '\0') return NmeaResult::Malformed;
    if (!isSentence(sentence, "GGA") && !isSentence(sentence, "RMC") &&
        !isSentence(sentence, "GSV")) {
        return NmeaResult::Ignored;
    }
    if (!checksumOk(sentence)) return NmeaResult::BadChecksum;

    char buf[24], hemi[8];

    // $xxGSV,totalSentences,thisSentence,satsInView,
    //        prn,elev,azim,cnr, prn,elev,azim,cnr, ... (up to four per sentence)
    if (isSentence(sentence, "GSV")) {
        if (!field(sentence, 1, buf, sizeof(buf))) return NmeaResult::Malformed;
        const int total = atoi(buf);
        if (!field(sentence, 2, buf, sizeof(buf))) return NmeaResult::Malformed;
        const int num = atoi(buf);
        if (!field(sentence, 3, buf, sizeof(buf))) return NmeaResult::Malformed;
        const int in_view = atoi(buf);

        if (num <= 1) { s_gsv_tracked = 0; s_gsv_best = 0; }

        for (int i = 0; i < 4; ++i) {
            const uint8_t cnr_field = static_cast<uint8_t>(7 + i * 4);
            if (!field(sentence, cnr_field, buf, sizeof(buf))) break;
            if (buf[0] == '\0') continue;          // blank: known of, not heard
            const int cnr = atoi(buf);
            if (cnr <= 0) continue;
            ++s_gsv_tracked;
            if (cnr > s_gsv_best) s_gsv_best = static_cast<uint8_t>(cnr);
        }

        if (total > 0 && num >= total) {
            fix.sats_in_view = static_cast<uint8_t>(in_view < 0 ? 0 : in_view);
            fix.sats_tracked = s_gsv_tracked;
            fix.best_cnr     = s_gsv_best;
        }
        return NmeaResult::Gsv;
    }

    if (isSentence(sentence, "GGA")) {
        // $xxGGA,time,lat,N,lon,E,quality,sats,hdop,alt,M,...
        if (!field(sentence, 6, buf, sizeof(buf))) return NmeaResult::Malformed;
        const int quality = atoi(buf);
        fix.fix_quality = static_cast<uint8_t>(quality < 0 ? 0 : quality);

        if (field(sentence, 7, buf, sizeof(buf)) && buf[0] != '\0') {
            fix.satellites = static_cast<uint8_t>(atoi(buf));
        }
        if (field(sentence, 8, buf, sizeof(buf)) && buf[0] != '\0') {
            fix.hdop = static_cast<float>(atof(buf));
        }

        if (quality == 0) {
            // No fix. Drop the position rather than keeping a stale one.
            fix.valid = false;
            return NmeaResult::Gga;
        }

        double lat = 0.0, lon = 0.0;
        if (!field(sentence, 2, buf,  sizeof(buf)))  return NmeaResult::Malformed;
        if (!field(sentence, 3, hemi, sizeof(hemi))) return NmeaResult::Malformed;
        if (!parseCoordinate(buf, hemi[0], &lat))    { fix.valid = false; return NmeaResult::Gga; }

        if (!field(sentence, 4, buf,  sizeof(buf)))  return NmeaResult::Malformed;
        if (!field(sentence, 5, hemi, sizeof(hemi))) return NmeaResult::Malformed;
        if (!parseCoordinate(buf, hemi[0], &lon))    { fix.valid = false; return NmeaResult::Gga; }

        fix.lat   = lat;
        fix.lon   = lon;
        fix.valid = true;
        return NmeaResult::Gga;
    }

    // $xxRMC,time,status,lat,N,lon,E,sog,cog,date,...
    if (!field(sentence, 2, buf, sizeof(buf))) return NmeaResult::Malformed;
    const bool rmc_active = (buf[0] == 'A');

    if (field(sentence, 7, buf, sizeof(buf)) && buf[0] != '\0') {
        // Speed over ground is in knots.
        fix.speed_kmh = static_cast<float>(atof(buf) * 1.852);
    }

    char timeBuf[16], dateBuf[16];
    if (field(sentence, 1, timeBuf, sizeof(timeBuf)) &&
        field(sentence, 9, dateBuf, sizeof(dateBuf)) &&
        strlen(timeBuf) >= 6 && strlen(dateBuf) == 6 && rmc_active) {

        const int hh = (timeBuf[0]-'0')*10 + (timeBuf[1]-'0');
        const int mm = (timeBuf[2]-'0')*10 + (timeBuf[3]-'0');
        const int ss = (timeBuf[4]-'0')*10 + (timeBuf[5]-'0');
        const int dd = (dateBuf[0]-'0')*10 + (dateBuf[1]-'0');
        const int mo = (dateBuf[2]-'0')*10 + (dateBuf[3]-'0');
        const int yy = (dateBuf[4]-'0')*10 + (dateBuf[5]-'0');

        if (hh <= 23 && mm <= 59 && ss <= 60 && dd >= 1 && dd <= 31 &&
            mo >= 1 && mo <= 12) {
            const uint64_t epoch = toEpoch(2000 + yy, mo, dd, hh, mm, ss);
            if (epoch != 0) {
                fix.epoch      = epoch;
                fix.time_valid = true;
            }
        }
    }

    if (!rmc_active) {
        fix.valid = false;   // 'V' = navigation receiver warning
    }
    return NmeaResult::Rmc;
}

}  // namespace NmeaParser
