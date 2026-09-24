/**
 * @file highlight.h
 * @brief Syntax highlighting performed in C, at render time.
 *
 * The prose bodies contain HTML-escaped code inside
 * `<pre><code class="lang-c">` or `class="lang-sh"`. highlight_blocks()
 * rewrites the text of each such block into span-wrapped tokens, so the
 * published pages arrive already coloured and need no client-side
 * highlighter. Token classes are styled in templates/app.css.
 */

#ifndef SITE_HIGHLIGHT_H
#define SITE_HIGHLIGHT_H

#include <cwist/core/sstring/sstring.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/** @brief Keywords coloured as control flow and declarations. */
static const char *const HL_C_KEYWORDS[] = {
    "auto", "break", "case", "const", "continue", "default", "do", "else",
    "enum", "extern", "for", "goto", "if", "inline", "register", "restrict",
    "return", "sizeof", "static", "struct", "switch", "typedef", "union",
    "volatile", "while", "true", "false", "NULL"
};

/** @brief Built-in type names. */
static const char *const HL_C_TYPES[] = {
    "bool", "char", "double", "float", "int", "long", "short", "signed",
    "unsigned", "void", "_Bool", "size_t", "ssize_t", "FILE"
};

/** @brief Commands we expect to open a shell line. */
static const char *const HL_SH_COMMANDS[] = {
    "brew", "cd", "clang", "curl", "gcc", "git", "make", "sudo", "wasmtime",
    "wrk", "pkg-config", "export", "echo", "rm", "mkdir", "cat"
};

#define HL_COUNT(a) (sizeof(a) / sizeof((a)[0]))

static bool hl_in_list(const char *word, size_t len,
                       const char *const *list, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (strlen(list[i]) == len && strncmp(list[i], word, len) == 0) return true;
    }
    return false;
}

static bool hl_ident_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

/** @brief Append one character, escaping what HTML would otherwise eat. */
static void hl_put(cwist_sstring *out, char c) {
    switch (c) {
        case '&': cwist_sstring_append(out, "&amp;"); break;
        case '<': cwist_sstring_append(out, "&lt;"); break;
        case '>': cwist_sstring_append(out, "&gt;"); break;
        default:  cwist_sstring_append_len(out, &c, 1); break;
    }
}

/** @brief Append a run of characters inside a token span. */
static void hl_span(cwist_sstring *out, const char *cls,
                    const char *text, size_t len) {
    cwist_sstring_append(out, "<span class=\"");
    cwist_sstring_append(out, cls);
    cwist_sstring_append(out, "\">");
    for (size_t i = 0; i < len; i++) hl_put(out, text[i]);
    cwist_sstring_append(out, "</span>");
}

/** @brief Highlight C source into @p out. Input is plain text, not escaped. */
static void hl_c(const char *src, size_t len, cwist_sstring *out) {
    size_t i = 0;
    bool line_start = true;
    bool pp_line = false;

    while (i < len) {
        char c = src[i];

        if (c == '\n') { hl_put(out, c); i++; line_start = true; pp_line = false; continue; }
        if (line_start && (c == ' ' || c == '\t')) { hl_put(out, c); i++; continue; }

        /* Preprocessor directive: colour the directive, then keep scanning so
         * that an include target still renders as a string. */
        if (line_start && c == '#') {
            size_t j = i + 1;
            while (j < len && hl_ident_char(src[j])) j++;
            hl_span(out, "tok-p", src + i, j - i);
            i = j;
            line_start = false;
            pp_line = true;
            continue;
        }
        line_start = false;

        if (c == '/' && i + 1 < len && src[i + 1] == '/') {
            size_t j = i;
            while (j < len && src[j] != '\n') j++;
            hl_span(out, "tok-c", src + i, j - i);
            i = j;
            continue;
        }
        if (c == '/' && i + 1 < len && src[i + 1] == '*') {
            size_t j = i + 2;
            while (j + 1 < len && !(src[j] == '*' && src[j + 1] == '/')) j++;
            j = (j + 1 < len) ? j + 2 : len;
            hl_span(out, "tok-c", src + i, j - i);
            i = j;
            continue;
        }
        if (c == '"' || c == '\'') {
            char quote = c;
            size_t j = i + 1;
            while (j < len && src[j] != quote) {
                if (src[j] == '\\' && j + 1 < len) j++;
                j++;
            }
            j = (j < len) ? j + 1 : len;
            hl_span(out, "tok-s", src + i, j - i);
            i = j;
            continue;
        }
        /* On a directive line, <cwist/app.h> is an include target, not a
         * comparison, so it reads as a string. */
        if (c == '<' && pp_line) {
            size_t j = i + 1;
            while (j < len && src[j] != '>' && src[j] != '\n') j++;
            if (j < len && src[j] == '>') {
                hl_span(out, "tok-s", src + i, j - i + 1);
                i = j + 1;
                continue;
            }
        }
        if (c >= '0' && c <= '9') {
            size_t j = i;
            while (j < len && (hl_ident_char(src[j]) || src[j] == '.')) j++;
            hl_span(out, "tok-n", src + i, j - i);
            i = j;
            continue;
        }
        if (hl_ident_char(c)) {
            size_t j = i;
            while (j < len && hl_ident_char(src[j])) j++;
            size_t wlen = j - i;
            const char *w = src + i;

            size_t k = j;
            while (k < len && (src[k] == ' ' || src[k] == '\t')) k++;
            bool call = (k < len && src[k] == '(');

            if (hl_in_list(w, wlen, HL_C_KEYWORDS, HL_COUNT(HL_C_KEYWORDS))) {
                hl_span(out, "tok-k", w, wlen);
            } else if (hl_in_list(w, wlen, HL_C_TYPES, HL_COUNT(HL_C_TYPES))) {
                hl_span(out, "tok-t", w, wlen);
            } else if (call) {
                hl_span(out, "tok-f", w, wlen);
            } else if ((wlen > 2 && strncmp(w + wlen - 2, "_t", 2) == 0) ||
                       (wlen > 6 && strncmp(w, "cwist_", 6) == 0) ||
                       (wlen > 5 && strncmp(w, "ttak_", 5) == 0) ||
                       (wlen > 5 && strncmp(w, "cJSON", 5) == 0)) {
                hl_span(out, "tok-t", w, wlen);
            } else {
                for (size_t x = 0; x < wlen; x++) hl_put(out, w[x]);
            }
            i = j;
            continue;
        }
        hl_put(out, c);
        i++;
    }
}

/** @brief Highlight a shell transcript into @p out. */
static void hl_sh(const char *src, size_t len, cwist_sstring *out) {
    size_t i = 0;
    bool first_word = true;

    while (i < len) {
        char c = src[i];

        if (c == '\n') { hl_put(out, c); i++; first_word = true; continue; }
        if (c == ' ' || c == '\t') { hl_put(out, c); i++; continue; }

        if (c == '#') {
            size_t j = i;
            while (j < len && src[j] != '\n') j++;
            hl_span(out, "tok-c", src + i, j - i);
            i = j;
            continue;
        }
        if (c == '"' || c == '\'') {
            char quote = c;
            size_t j = i + 1;
            while (j < len && src[j] != quote) {
                if (src[j] == '\\' && j + 1 < len) j++;
                j++;
            }
            j = (j < len) ? j + 1 : len;
            hl_span(out, "tok-s", src + i, j - i);
            i = j;
            first_word = false;
            continue;
        }
        if (c == '$') {
            size_t j = i + 1;
            if (j < len && src[j] == '(') {
                hl_span(out, "tok-v", src + i, 2);
                i = j + 1;
                continue;
            }
            while (j < len && hl_ident_char(src[j])) j++;
            hl_span(out, "tok-v", src + i, j - i);
            i = j;
            first_word = false;
            continue;
        }
        if (c == '-' && i + 1 < len &&
            (hl_ident_char(src[i + 1]) || src[i + 1] == '-')) {
            size_t j = i;
            while (j < len && src[j] != ' ' && src[j] != '\t' && src[j] != '\n' &&
                   src[j] != '=') j++;
            hl_span(out, "tok-k", src + i, j - i);
            i = j;
            first_word = false;
            continue;
        }
        if (hl_ident_char(c) || c == '.' || c == '/') {
            size_t j = i;
            while (j < len && (hl_ident_char(src[j]) || src[j] == '.' ||
                               src[j] == '/' || src[j] == '-')) j++;
            size_t wlen = j - i;
            if (first_word &&
                (hl_in_list(src + i, wlen, HL_SH_COMMANDS, HL_COUNT(HL_SH_COMMANDS)) ||
                 strncmp(src + i, "./", 2) == 0)) {
                hl_span(out, "tok-f", src + i, wlen);
            } else {
                for (size_t x = 0; x < wlen; x++) hl_put(out, src[i + x]);
            }
            i = j;
            first_word = false;
            continue;
        }
        if (c == '|' || c == ';' || c == '&') first_word = true;
        hl_put(out, c);
        i++;
    }
}

/** @brief Decode the entities our content uses, so the tokeniser sees real text. */
static size_t hl_unescape(const char *src, size_t len, char *dst, size_t cap) {
    size_t o = 0;
    for (size_t i = 0; i < len && o + 1 < cap; ) {
        if (src[i] == '&') {
            if (strncmp(src + i, "&lt;", 4) == 0)        { dst[o++] = '<'; i += 4; continue; }
            if (strncmp(src + i, "&gt;", 4) == 0)        { dst[o++] = '>'; i += 4; continue; }
            if (strncmp(src + i, "&amp;", 5) == 0)       { dst[o++] = '&'; i += 5; continue; }
            if (strncmp(src + i, "&quot;", 6) == 0)      { dst[o++] = '"'; i += 6; continue; }
            if (strncmp(src + i, "&#39;", 5) == 0)       { dst[o++] = '\''; i += 5; continue; }
        }
        dst[o++] = src[i++];
    }
    dst[o] = '\0';
    return o;
}

/**
 * @brief Highlight one standalone snippet that is already HTML-escaped.
 * @return A new string of span-wrapped tokens; the caller owns it.
 */
static cwist_sstring *highlight_snippet(const char *escaped, bool is_c) {
    cwist_sstring *out = cwist_sstring_create();
    char plain[16384];
    size_t n = hl_unescape(escaped, strlen(escaped), plain, sizeof(plain));
    if (is_c) hl_c(plain, n, out);
    else      hl_sh(plain, n, out);
    return out;
}

/**
 * @brief Rewrite every tagged code block in @p html with highlighted tokens.
 * @return A new string; the caller owns it.
 */
static cwist_sstring *highlight_blocks(const char *html) {
    static const char *const OPEN_C  = "<pre><code class=\"lang-c\">";
    static const char *const OPEN_SH = "<pre><code class=\"lang-sh\">";
    static const char *const CLOSE   = "</code></pre>";

    cwist_sstring *out = cwist_sstring_create();
    const char *p = html;

    while (*p) {
        const char *hit_c = strstr(p, OPEN_C);
        const char *hit_s = strstr(p, OPEN_SH);
        const char *hit;
        bool is_c;

        if (hit_c && (!hit_s || hit_c < hit_s)) { hit = hit_c; is_c = true; }
        else if (hit_s)                          { hit = hit_s; is_c = false; }
        else break;

        const char *open_end = hit + strlen(is_c ? OPEN_C : OPEN_SH);
        const char *close = strstr(open_end, CLOSE);
        if (!close) break;

        cwist_sstring_append_len(out, p, (size_t)(hit - p));
        cwist_sstring_append(out, is_c ? OPEN_C : OPEN_SH);

        size_t raw_len = (size_t)(close - open_end);
        char plain[16384];
        size_t plain_len = hl_unescape(open_end, raw_len, plain, sizeof(plain));
        if (is_c) hl_c(plain, plain_len, out);
        else      hl_sh(plain, plain_len, out);

        cwist_sstring_append(out, CLOSE);
        p = close + strlen(CLOSE);
    }
    cwist_sstring_append(out, p);
    return out;
}

#endif /* SITE_HIGHLIGHT_H */
