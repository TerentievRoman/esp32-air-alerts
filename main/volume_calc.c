/* Pure volume schedule logic (no ESP headers) so it can be unit-tested on the host. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "volume.h"

bool volume_rule_valid(const volume_rule_t *r)
{
    return r->days != 0 && (r->days & 0x80) == 0
        && r->to < 1440 && r->from <= r->to
        && r->vol <= VOLUME_MAX;
}

bool volume_sched_valid(const volume_sched_t *s)
{
    if (s->def > VOLUME_MAX || s->n > VOLUME_RULES_MAX) {
        return false;
    }
    for (int i = 0; i < s->n; i++) {
        if (!volume_rule_valid(&s->r[i])) {
            return false;
        }
    }
    return true;
}

int volume_pick(const volume_sched_t *s, int wday_mon0, int minute)
{
    int best = -1;
    for (int i = 0; i < s->n; i++) {
        const volume_rule_t *r = &s->r[i];
        if ((r->days & (1 << wday_mon0)) && minute >= r->from && minute <= r->to) {
            if (best < 0 || r->vol < best) {
                best = r->vol;
            }
        }
    }
    return best < 0 ? s->def : best;
}

int volume_at(const volume_sched_t *s, time_t now)
{
    if (now == 0) {
        return s->def;
    }
    struct tm t;
    localtime_r(&now, &t);
    return volume_pick(s, (t.tm_wday + 6) % 7, t.tm_hour * 60 + t.tm_min);
}

/* Strict "HH:MM": exactly 5 chars, HH 00..23, MM 00..59. Returns minutes or -1. */
static int parse_hhmm(const char *s)
{
    if (strlen(s) != 5 || s[2] != ':') {
        return -1;
    }
    const int pos[4] = { 0, 1, 3, 4 };
    for (int i = 0; i < 4; i++) {
        if (s[pos[i]] < '0' || s[pos[i]] > '9') {
            return -1;
        }
    }
    int h = (s[0] - '0') * 10 + (s[1] - '0');
    int m = (s[3] - '0') * 10 + (s[4] - '0');
    if (h > 23 || m > 59) {
        return -1;
    }
    return h * 60 + m;
}

/* Integer JSON number in [lo, hi] -> true and *out, else false. */
static bool get_int(const cJSON *item, int lo, int hi, int *out)
{
    if (!cJSON_IsNumber(item)) {
        return false;
    }
    double d = item->valuedouble;
    if (d < lo || d > hi || d != (double)(int)d) {
        return false;
    }
    *out = (int)d;
    return true;
}

static bool get_time(const cJSON *item, int *out)
{
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return false;
    }
    int m = parse_hhmm(item->valuestring);
    if (m < 0) {
        return false;
    }
    *out = m;
    return true;
}

bool volume_sched_from_json(const char *txt, volume_sched_t *out, char *err, size_t errlen)
{
    cJSON *root = cJSON_Parse(txt);
    if (root == NULL) {
        snprintf(err, errlen, "Некоректний JSON");
        return false;
    }

    volume_sched_t s;
    memset(&s, 0, sizeof s);
    int v;
    bool ok = false;

    if (!get_int(cJSON_GetObjectItemCaseSensitive(root, "default"), 0, VOLUME_MAX, &v)) {
        snprintf(err, errlen, "Некоректна гучність за замовчуванням (0..%d)", VOLUME_MAX);
        goto done;
    }
    s.def = (uint8_t)v;

    const cJSON *rules = cJSON_GetObjectItemCaseSensitive(root, "rules");
    if (!cJSON_IsArray(rules)) {
        snprintf(err, errlen, "Відсутній список правил");
        goto done;
    }
    int count = cJSON_GetArraySize(rules);
    if (count > VOLUME_RULES_MAX) {
        snprintf(err, errlen, "Забагато правил (максимум %d)", VOLUME_RULES_MAX);
        goto done;
    }

    int line = 0;
    const cJSON *it;
    cJSON_ArrayForEach(it, rules) {
        line++;
        volume_rule_t *r = &s.r[s.n];
        int days, from, to, vol;
        if (!get_int(cJSON_GetObjectItemCaseSensitive(it, "days"), 1, 0x7F, &days)) {
            snprintf(err, errlen, "Рядок %d: некоректні дні", line);
            goto done;
        }
        if (!get_time(cJSON_GetObjectItemCaseSensitive(it, "from"), &from)) {
            snprintf(err, errlen, "Рядок %d: некоректний час «від» (потрібно ГГ:ХХ)", line);
            goto done;
        }
        if (!get_time(cJSON_GetObjectItemCaseSensitive(it, "to"), &to)) {
            snprintf(err, errlen, "Рядок %d: некоректний час «до» (потрібно ГГ:ХХ)", line);
            goto done;
        }
        if (!get_int(cJSON_GetObjectItemCaseSensitive(it, "vol"), 0, VOLUME_MAX, &vol)) {
            snprintf(err, errlen, "Рядок %d: гучність має бути 0..%d", line, VOLUME_MAX);
            goto done;
        }
        *r = (volume_rule_t){ (uint8_t)days, (uint16_t)from, (uint16_t)to, (uint8_t)vol };
        if (!volume_rule_valid(r)) {
            snprintf(err, errlen, "Рядок %d: «від» пізніше за «до»", line);
            goto done;
        }
        s.n++;
    }

    *out = s;
    ok = true;
done:
    cJSON_Delete(root);
    return ok;
}

static void fmt_hhmm(char *buf, unsigned minutes)
{
    snprintf(buf, 6, "%02u:%02u", (minutes / 60) % 100, minutes % 60);
}

char *volume_sched_to_json(const volume_sched_t *s)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *rules = cJSON_CreateArray();
    if (root == NULL || rules == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(rules);
        return NULL;
    }
    cJSON_AddNumberToObject(root, "default", s->def);
    cJSON_AddNumberToObject(root, "max", VOLUME_RULES_MAX);
    cJSON_AddItemToObject(root, "rules", rules);
    for (int i = 0; i < s->n; i++) {
        char from[6], to[6];
        fmt_hhmm(from, s->r[i].from);
        fmt_hhmm(to, s->r[i].to);
        cJSON *o = cJSON_CreateObject();
        if (o == NULL) {
            cJSON_Delete(root);
            return NULL;
        }
        cJSON_AddNumberToObject(o, "days", s->r[i].days);
        cJSON_AddStringToObject(o, "from", from);
        cJSON_AddStringToObject(o, "to", to);
        cJSON_AddNumberToObject(o, "vol", s->r[i].vol);
        cJSON_AddItemToArray(rules, o);
    }
    char *txt = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return txt;
}
