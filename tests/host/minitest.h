/* Minimal host-side unit test harness. */
#pragma once
#include <stdio.h>
#include <string.h>

static int mt_failures;
static int mt_current_failed;

#define TEST(name) static void name(void)

#define MT_FAIL(fmt, ...)                                                    \
    do {                                                                     \
        printf("  %s:%d: " fmt "\n", __FILE__, __LINE__, __VA_ARGS__);       \
        mt_current_failed = 1;                                               \
        return;                                                              \
    } while (0)

#define ASSERT_TRUE(x)                                                       \
    do { if (!(x)) MT_FAIL("ASSERT_TRUE(%s)", #x); } while (0)

#define ASSERT_EQ_INT(a, b)                                                  \
    do {                                                                     \
        long long mt_a = (long long)(a), mt_b = (long long)(b);              \
        if (mt_a != mt_b) MT_FAIL("%s == %lld, expected %lld", #a, mt_a, mt_b); \
    } while (0)

#define ASSERT_EQ_STR(a, b)                                                  \
    do {                                                                     \
        const char *mt_a = (a), *mt_b = (b);                                 \
        if (strcmp(mt_a, mt_b) != 0) MT_FAIL("%s == \"%s\", expected \"%s\"", #a, mt_a, mt_b); \
    } while (0)

#define RUN(name)                                                            \
    do {                                                                     \
        mt_current_failed = 0;                                               \
        name();                                                              \
        printf("%s %s\n", mt_current_failed ? "FAIL" : "PASS", #name);       \
        mt_failures += mt_current_failed;                                    \
    } while (0)

#define MT_RESULT() (printf("%d failure(s)\n", mt_failures), mt_failures != 0)
