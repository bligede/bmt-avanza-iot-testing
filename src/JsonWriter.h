// =============================================================================
//  JsonWriter.h — bounded JSON writer over a caller-owned buffer.
//
//  Hand-rolled with vsnprintf so the project keeps zero third-party libraries.
//  It never writes past the buffer. Once anything fails to fit, overflow() stays
//  true and every later call is a no-op, so a caller checks once at the end
//  instead of after every append — and never serves a half-written document.
// =============================================================================
#pragma once

#include <Arduino.h>
#include <stdarg.h>

class JsonWriter {
public:
    JsonWriter(char* buf, size_t cap) : buf_(buf), cap_(cap) {
        if (cap_ > 0) buf_[0] = '\0';
    }

    // printf-style raw append. The caller owns the JSON punctuation.
    void add(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
        if (overflow_ || len_ + 1 >= cap_) { overflow_ = true; return; }
        va_list a;
        va_start(a, fmt);
        const int n = vsnprintf(buf_ + len_, cap_ - len_, fmt, a);
        va_end(a);
        if (n < 0 || static_cast<size_t>(n) >= cap_ - len_) {
            overflow_ = true;
            buf_[len_] = '\0';
            return;
        }
        len_ += static_cast<size_t>(n);
    }

    // A JSON string literal, quotes included. Escapes the quote, the backslash
    // and every control character; UTF-8 bytes above 0x7F pass through, which is
    // valid JSON. Anything a person typed into the page goes through here.
    void addString(const char* s) {
        put('"');
        for (const char* p = (s ? s : ""); *p && !overflow_; ++p) {
            const unsigned char c = static_cast<unsigned char>(*p);
            if (c == '"' || c == '\\') { put('\\'); put(static_cast<char>(c)); }
            else if (c < 0x20)         { add("\\u%04X", c); }
            else                        { put(static_cast<char>(c)); }
        }
        put('"');
    }

    bool        overflow() const { return overflow_; }
    size_t      length()   const { return len_; }
    const char* c_str()    const { return buf_; }

private:
    void put(char c) {
        if (overflow_ || len_ + 1 >= cap_) { overflow_ = true; return; }
        buf_[len_++] = c;
        buf_[len_]   = '\0';
    }

    char*  buf_;
    size_t cap_;
    size_t len_      = 0;
    bool   overflow_ = false;
};
