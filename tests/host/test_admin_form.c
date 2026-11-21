#include "minitest.h"
#include "admin_form.h"

TEST(urldecode_plus_and_percent) {
    char s[] = "a+b%20c%23d%2Fe";
    admin_urldecode(s);
    ASSERT_EQ_STR(s, "a b c#d/e");
}
TEST(urldecode_utf8) {
    char s[] = "%D0%9A%D0%B8%D1%97%D0%B2";
    admin_urldecode(s);
    ASSERT_EQ_STR(s, "Київ");
}
TEST(urldecode_bad_escape_kept) {
    char s[] = "100%zz%4";
    admin_urldecode(s);
    ASSERT_EQ_STR(s, "100%zz%4");
}
TEST(field_found_and_decoded) {
    char out[32];
    ASSERT_EQ_INT(admin_form_field("ssid=my+net&wpass=qwerty12%23", "wpass", out, sizeof out), 1);
    ASSERT_EQ_STR(out, "qwerty12#");
    ASSERT_EQ_INT(admin_form_field("ssid=my+net&wpass=x", "ssid", out, sizeof out), 1);
    ASSERT_EQ_STR(out, "my net");
}
TEST(field_prefix_not_confused) {
    char out[32];
    /* "pass" must not match "wpass" */
    ASSERT_EQ_INT(admin_form_field("wpass=secret", "pass", out, sizeof out), 0);
}
TEST(field_missing) {
    char out[8] = "x";
    ASSERT_EQ_INT(admin_form_field("a=1&b=2", "c", out, sizeof out), 0);
}
TEST(field_empty_value) {
    char out[8] = "x";
    ASSERT_EQ_INT(admin_form_field("a=&b=2", "a", out, sizeof out), 1);
    ASSERT_EQ_STR(out, "");
}
TEST(field_too_long_rejected) {
    char out[4];
    ASSERT_EQ_INT(admin_form_field("a=abcdef", "a", out, sizeof out), -1);
}
TEST(field_fits_after_decoding) {
    /* 9 encoded bytes, 3 decoded: must fit a 4-byte buffer */
    char out[4];
    ASSERT_EQ_INT(admin_form_field("a=%41%42%43", "a", out, sizeof out), 1);
    ASSERT_EQ_STR(out, "ABC");
}
TEST(field_cyrillic_fits_exactly) {
    char out[5]; /* "Ки" = 4 bytes UTF-8 + NUL */
    ASSERT_EQ_INT(admin_form_field("a=%D0%9A%D0%B8", "a", out, sizeof out), 1);
    ASSERT_EQ_STR(out, "Ки");
}
TEST(basic_auth_ok) {
    /* admin:admin */
    ASSERT_TRUE(admin_basic_auth_ok("Basic YWRtaW46YWRtaW4=", "admin", "admin"));
}
TEST(basic_auth_wrong_pass) {
    ASSERT_TRUE(!admin_basic_auth_ok("Basic YWRtaW46YWRtaW4=", "admin", "other"));
}
TEST(basic_auth_missing_or_garbage) {
    ASSERT_TRUE(!admin_basic_auth_ok(NULL, "admin", "admin"));
    ASSERT_TRUE(!admin_basic_auth_ok("Bearer abc", "admin", "admin"));
    ASSERT_TRUE(!admin_basic_auth_ok("Basic", "admin", "admin"));
}
TEST(basic_auth_utf8_password) {
    /* admin:пароль1 */
    ASSERT_TRUE(admin_basic_auth_ok("Basic YWRtaW460L/QsNGA0L7Qu9GMMQ==", "admin", "пароль1"));
}

int main(void) {
    RUN(urldecode_plus_and_percent); RUN(urldecode_utf8); RUN(urldecode_bad_escape_kept);
    RUN(field_found_and_decoded); RUN(field_prefix_not_confused); RUN(field_missing);
    RUN(field_empty_value); RUN(field_too_long_rejected); RUN(field_fits_after_decoding); RUN(field_cyrillic_fits_exactly);
    RUN(basic_auth_ok); RUN(basic_auth_wrong_pass); RUN(basic_auth_missing_or_garbage);
    RUN(basic_auth_utf8_password);
    return MT_RESULT();
}
