#include "admin_form.h"

#include <stdint.h>
#include <string.h>

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void admin_urldecode(char *s)
{
    char *w = s;
    for (const char *r = s; *r; r++) {
        if (*r == '+') {
            *w++ = ' ';
        } else if (*r == '%' && hexval(r[1]) >= 0 && hexval(r[2]) >= 0) {
            *w++ = (char)(hexval(r[1]) << 4 | hexval(r[2]));
            r += 2;
        } else {
            *w++ = *r;
        }
    }
    *w = '\0';
}

int admin_form_field(const char *body, const char *key, char *out, size_t len)
{
    size_t klen = strlen(key);
    const char *p = body;
    while (p && *p) {
        const char *amp = strchr(p, '&');
        const char *end = amp ? amp : p + strlen(p);
        if ((size_t)(end - p) > klen && strncmp(p, key, klen) == 0 && p[klen] == '=') {
            /* Decode while copying so the limit applies to the decoded size. */
            size_t w = 0;
            for (const char *r = p + klen + 1; r < end; r++) {
                char c = *r;
                if (c == '+') {
                    c = ' ';
                } else if (c == '%' && r + 2 < end && hexval(r[1]) >= 0 && hexval(r[2]) >= 0) {
                    c = (char)(hexval(r[1]) << 4 | hexval(r[2]));
                    r += 2;
                }
                if (w + 1 >= len) {
                    out[0] = '\0';
                    return -1;
                }
                out[w++] = c;
            }
            out[w] = '\0';
            return 1;
        }
        p = amp ? amp + 1 : NULL;
    }
    return 0;
}

static void b64_encode(const uint8_t *in, size_t n, char *out)
{
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i = 0;
    for (; i + 2 < n; i += 3) {
        uint32_t v = (uint32_t)in[i] << 16 | (uint32_t)in[i + 1] << 8 | in[i + 2];
        *out++ = T[v >> 18]; *out++ = T[(v >> 12) & 63]; *out++ = T[(v >> 6) & 63]; *out++ = T[v & 63];
    }
    if (n - i == 1) {
        uint32_t v = (uint32_t)in[i] << 16;
        *out++ = T[v >> 18]; *out++ = T[(v >> 12) & 63]; *out++ = '='; *out++ = '=';
    } else if (n - i == 2) {
        uint32_t v = (uint32_t)in[i] << 16 | (uint32_t)in[i + 1] << 8;
        *out++ = T[v >> 18]; *out++ = T[(v >> 12) & 63]; *out++ = T[(v >> 6) & 63]; *out++ = '=';
    }
    *out = '\0';
}

bool admin_basic_auth_ok(const char *header, const char *user, const char *pass)
{
    if (!header || strncmp(header, "Basic ", 6) != 0) return false;

    char cred[128];
    size_t ulen = strlen(user), plen = strlen(pass);
    if (ulen + 1 + plen > sizeof(cred)) return false;
    memcpy(cred, user, ulen);
    cred[ulen] = ':';
    memcpy(cred + ulen + 1, pass, plen);

    char expect[180];
    b64_encode((const uint8_t *)cred, ulen + 1 + plen, expect);

    /* Compare without early exit. */
    const char *got = header + 6;
    size_t elen = strlen(expect);
    if (strlen(got) != elen) return false;
    unsigned diff = 0;
    for (size_t i = 0; i < elen; i++) diff |= (unsigned)(got[i] ^ expect[i]);
    return diff == 0;
}
