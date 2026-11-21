#pragma once
#include <stdbool.h>
#include <stddef.h>

/* In-place URL decode: '+' -> space, %XX -> byte; malformed escapes kept as-is. */
void admin_urldecode(char *s);

/* Find key in an x-www-form-urlencoded body and decode its value into out.
 * Returns 1 if found, 0 if absent, -1 if the decoded value does not fit
 * (out is then an empty string). */
int admin_form_field(const char *body, const char *key, char *out, size_t len);

/* Check an "Authorization: Basic ..." header value against user:pass. */
bool admin_basic_auth_ok(const char *header, const char *user, const char *pass);
