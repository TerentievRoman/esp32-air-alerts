#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include "esp_err.h"

#define VOLUME_MAX        30
#define VOLUME_RULES_MAX  64
#define VOLUME_JSON_MAX   6144   /* max body of POST /volume */

/* days: bit0=Mon..bit6=Sun; from/to: minutes since midnight, both inclusive. */
typedef struct { uint8_t days; uint16_t from; uint16_t to; uint8_t vol; } volume_rule_t;
typedef struct { uint8_t def; uint8_t n; volume_rule_t r[VOLUME_RULES_MAX]; } volume_sched_t;

/* Pure, volume_calc.c */
bool volume_rule_valid(const volume_rule_t *r);
bool volume_sched_valid(const volume_sched_t *s);           /* def, n, every rule */
int  volume_pick(const volume_sched_t *s, int wday_mon0, int minute); /* wday_mon0 0=Mon..6=Sun */
int  volume_at(const volume_sched_t *s, time_t now);        /* now==0 -> s->def; localtime_r */

/* Parses {"default":N,"rules":[{"days":M,"from":"HH:MM","to":"HH:MM","vol":V}]}.
 * On failure returns false, writes a Ukrainian message to err, leaves *out untouched. */
bool volume_sched_from_json(const char *txt, volume_sched_t *out, char *err, size_t errlen);

/* {"default":N,"max":64,"rules":[...]} with "HH:MM" strings; malloc'ed, caller frees. */
char *volume_sched_to_json(const volume_sched_t *s);

/* Storage and live volume, volume.c */
esp_err_t volume_init(void);                   /* load NVS "aa"/"vsched"; bad/missing -> {def=CONFIG_AA_DF_VOLUME, n=0} */
int       volume_now(void);                    /* volume_at(current, clock_now()) under mutex */
void      volume_get(volume_sched_t *out);     /* copy under mutex */
esp_err_t volume_set(const volume_sched_t *s); /* invalid -> ESP_ERR_INVALID_ARG; NVS then RAM */
