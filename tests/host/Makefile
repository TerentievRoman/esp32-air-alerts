# Host unit tests: make -C tests/host test
# Each test_<name>.c is linked with the sources listed in SRCS_<name>.
CC      ?= cc
MAIN    := ../../main
CJSON   := ../../managed_components/espressif__cjson/cJSON
CFLAGS  := -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -g -I$(MAIN) -Istubs -I$(CJSON)
BUILD   := build

TESTS   := $(patsubst %.c,%,$(wildcard test_*.c))

# Per-test production sources (added by later tasks), e.g.
# SRCS_test_alert_state := $(MAIN)/alert_state.c $(CJSON)/cJSON.c
SRCS_test_alert_state := $(MAIN)/alert_state.c $(CJSON)/cJSON.c
SRCS_test_admin_form := $(MAIN)/admin_form.c
SRCS_test_clock := $(MAIN)/clock_calc.c
SRCS_test_notify_rules := $(MAIN)/notify_rules.c

.PHONY: test clean
test: $(addprefix $(BUILD)/,$(TESTS))
	@rc=0; for t in $^; do echo "== $$t"; ./$$t || rc=1; done; \
	if [ -z "$^" ]; then echo "no tests"; fi; exit $$rc

$(BUILD)/%: %.c minitest.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ $< $(SRCS_$*)

clean:
	rm -rf $(BUILD)
