#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "minitest.h"
#include "volume.h"

/* Local Kyiv time -> time_t. */
static time_t kyiv(int y, int mo, int d, int h, int mi, int s)
{
    struct tm t = { .tm_year = y - 1900, .tm_mon = mo - 1, .tm_mday = d,
                    .tm_hour = h, .tm_min = mi, .tm_sec = s, .tm_isdst = -1 };
    return mktime(&t);
}

static volume_sched_t one_rule(uint8_t def, uint8_t days, uint16_t from, uint16_t to, uint8_t vol)
{
    volume_sched_t s = { .def = def, .n = 1 };
    s.r[0] = (volume_rule_t){ days, from, to, vol };
    return s;
}

TEST(empty_gives_default) {
    volume_sched_t s = { .def = 20, .n = 0 };
    ASSERT_EQ_INT(volume_pick(&s, 0, 600), 20);
}
TEST(match_inside) {
    volume_sched_t s = one_rule(20, 0x1F, 780, 900, 5);
    ASSERT_EQ_INT(volume_pick(&s, 0, 840), 5);   /* Mon 14:00 */
    ASSERT_EQ_INT(volume_pick(&s, 5, 840), 20);  /* Sat 14:00 */
}
TEST(to_is_inclusive) {
    volume_sched_t a = one_rule(20, 0x7F, 0, 1439, 3);
    ASSERT_EQ_INT(volume_pick(&a, 2, 1439), 3);
    volume_sched_t b = one_rule(20, 0x7F, 780, 900, 5);
    ASSERT_EQ_INT(volume_pick(&b, 2, 900), 5);
    ASSERT_EQ_INT(volume_pick(&b, 2, 901), 20);
}
TEST(quietest_wins) {
    volume_sched_t s = { .def = 20, .n = 2 };
    s.r[0] = (volume_rule_t){ 0x7F, 600, 700, 10 };
    s.r[1] = (volume_rule_t){ 0x7F, 650, 750, 4 };
    ASSERT_EQ_INT(volume_pick(&s, 1, 660), 4);
}
TEST(sunday_maps_to_bit6) {
    volume_sched_t s = one_rule(20, 0x40, 0, 1439, 2);
    ASSERT_EQ_INT(volume_at(&s, kyiv(2026, 11, 22, 10, 0, 0)), 2);   /* Sunday */
    ASSERT_EQ_INT(volume_at(&s, kyiv(2026, 11, 23, 10, 0, 0)), 20);  /* Monday */
}
TEST(unsynced_gives_default) {
    volume_sched_t s = one_rule(17, 0x7F, 0, 1439, 2);
    ASSERT_EQ_INT(volume_at(&s, 0), 17);
}

TEST(rule_valid_ok) {
    volume_rule_t r = { 0x7F, 0, 1439, 30 };
    ASSERT_TRUE(volume_rule_valid(&r));
}
TEST(rule_valid_days_zero) {
    volume_rule_t r = { 0, 0, 10, 5 };
    ASSERT_TRUE(!volume_rule_valid(&r));
}
TEST(rule_valid_days_high_bit) {
    volume_rule_t r = { 0x80, 0, 10, 5 };
    ASSERT_TRUE(!volume_rule_valid(&r));
}
TEST(rule_valid_from_after_to) {
    volume_rule_t r = { 0x01, 900, 780, 5 };
    ASSERT_TRUE(!volume_rule_valid(&r));
}
TEST(rule_valid_to_1440) {
    volume_rule_t r = { 0x01, 0, 1440, 5 };
    ASSERT_TRUE(!volume_rule_valid(&r));
}
TEST(rule_valid_vol_31) {
    volume_rule_t r = { 0x01, 0, 10, 31 };
    ASSERT_TRUE(!volume_rule_valid(&r));
}

TEST(sched_valid_ok) {
    volume_sched_t s = one_rule(30, 0x01, 0, 10, 0);
    ASSERT_TRUE(volume_sched_valid(&s));
}
TEST(sched_valid_def_31) {
    volume_sched_t s = { .def = 31, .n = 0 };
    ASSERT_TRUE(!volume_sched_valid(&s));
}
TEST(sched_valid_n_65) {
    volume_sched_t s = { .def = 10, .n = 65 };
    ASSERT_TRUE(!volume_sched_valid(&s));
}
TEST(sched_valid_bad_rule) {
    volume_sched_t s = one_rule(10, 0, 0, 10, 5);
    ASSERT_TRUE(!volume_sched_valid(&s));
}

TEST(json_roundtrip) {
    volume_sched_t a = { .def = 12, .n = 2 };
    a.r[0] = (volume_rule_t){ 0x1F, 545, 900, 5 };   /* 09:05 */
    a.r[1] = (volume_rule_t){ 0x60, 0, 1439, 30 };
    char *js = volume_sched_to_json(&a);
    ASSERT_TRUE(js != NULL);
    ASSERT_TRUE(strstr(js, "\"from\":\"09:05\"") != NULL);
    volume_sched_t b;
    memset(&b, 0, sizeof b);
    char err[96] = "";
    bool ok = volume_sched_from_json(js, &b, err, sizeof err);
    free(js);
    ASSERT_TRUE(ok);
    ASSERT_EQ_INT(b.def, a.def);
    ASSERT_EQ_INT(b.n, a.n);
    for (int i = 0; i < a.n; i++) {
        ASSERT_EQ_INT(b.r[i].days, a.r[i].days);
        ASSERT_EQ_INT(b.r[i].from, a.r[i].from);
        ASSERT_EQ_INT(b.r[i].to, a.r[i].to);
        ASSERT_EQ_INT(b.r[i].vol, a.r[i].vol);
    }
}

/* Expect from_json to fail with a message and leave the marker untouched. */
static int rejects(const char *txt, char *err, size_t n)
{
    volume_sched_t out;
    memset(&out, 0, sizeof out);
    out.def = 7;
    err[0] = '\0';
    bool ok = volume_sched_from_json(txt, &out, err, n);
    return !ok && err[0] != '\0' && out.def == 7 && out.n == 0;
}

#define REJECT(txt) do { char e[128]; ASSERT_TRUE(rejects((txt), e, sizeof e)); } while (0)

TEST(rejects_hour_25) {
    REJECT("{\"default\":10,\"rules\":[{\"days\":1,\"from\":\"25:00\",\"to\":\"26:00\",\"vol\":5}]}");
}
TEST(rejects_malformed_time) {
    REJECT("{\"default\":10,\"rules\":[{\"days\":1,\"from\":\"9:5x\",\"to\":\"10:00\",\"vol\":5}]}");
}
TEST(rejects_from_after_to) {
    REJECT("{\"default\":10,\"rules\":[{\"days\":1,\"from\":\"15:00\",\"to\":\"13:00\",\"vol\":5}]}");
}
TEST(rejects_vol_31) {
    REJECT("{\"default\":10,\"rules\":[{\"days\":1,\"from\":\"10:00\",\"to\":\"11:00\",\"vol\":31}]}");
}
TEST(rejects_days_zero) {
    REJECT("{\"default\":10,\"rules\":[{\"days\":0,\"from\":\"10:00\",\"to\":\"11:00\",\"vol\":5}]}");
}
TEST(rejects_65_rules) {
    char *buf = malloc(65 * 64 + 64);
    ASSERT_TRUE(buf != NULL);
    strcpy(buf, "{\"default\":10,\"rules\":[");
    for (int i = 0; i < 65; i++) {
        if (i) strcat(buf, ",");
        strcat(buf, "{\"days\":1,\"from\":\"10:00\",\"to\":\"11:00\",\"vol\":5}");
    }
    strcat(buf, "]}");
    char e[128];
    int r = rejects(buf, e, sizeof e);
    free(buf);
    ASSERT_TRUE(r);
}
TEST(rejects_not_json) { REJECT("not json"); }
TEST(rejects_missing_default) {
    REJECT("{\"rules\":[]}");
}
TEST(error_has_line_number) {
    char e[128];
    ASSERT_TRUE(rejects("{\"default\":10,\"rules\":[{\"days\":1,\"from\":\"10:00\",\"to\":\"11:00\",\"vol\":5},"
                        "{\"days\":1,\"from\":\"15:00\",\"to\":\"13:00\",\"vol\":5}]}", e, sizeof e));
    ASSERT_TRUE(strstr(e, "2") != NULL);
}

TEST(max_sched_json_fits) {
    volume_sched_t s = { .def = 30, .n = VOLUME_RULES_MAX };
    for (int i = 0; i < VOLUME_RULES_MAX; i++)
        s.r[i] = (volume_rule_t){ 0x7F, 0, 1439, 30 };
    char *js = volume_sched_to_json(&s);
    ASSERT_TRUE(js != NULL);
    size_t len = strlen(js);
    free(js);
    ASSERT_TRUE(len < VOLUME_JSON_MAX);
}

int main(void) {
    setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
    tzset();
    RUN(empty_gives_default); RUN(match_inside); RUN(to_is_inclusive);
    RUN(quietest_wins); RUN(sunday_maps_to_bit6); RUN(unsynced_gives_default);
    RUN(rule_valid_ok); RUN(rule_valid_days_zero); RUN(rule_valid_days_high_bit);
    RUN(rule_valid_from_after_to); RUN(rule_valid_to_1440); RUN(rule_valid_vol_31);
    RUN(sched_valid_ok); RUN(sched_valid_def_31); RUN(sched_valid_n_65); RUN(sched_valid_bad_rule);
    RUN(json_roundtrip);
    RUN(rejects_hour_25); RUN(rejects_malformed_time); RUN(rejects_from_after_to);
    RUN(rejects_vol_31); RUN(rejects_days_zero); RUN(rejects_65_rules);
    RUN(rejects_not_json); RUN(rejects_missing_default); RUN(error_has_line_number);
    RUN(max_sched_json_fits);
    return MT_RESULT();
}
