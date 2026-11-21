#pragma once
#include <stdbool.h>
#include <stddef.h>

/* In-place URL decode: '+' -> space, %XX -> byte; malformed escapes kept as-is. */
void admin_urldecode(char *s);

/* Find key in x-www-form-urlencoded body, copy its decoded value (truncated to len-1).
 * Returns false if the key is absent. */
bool admin_form_field(const char *body, const char *key, char *out, size_t len);

/* Check an "Authorization: Basic ..." header value against user:pass. */
bool admin_basic_auth_ok(const char *header, const char *user, const char *pass);
