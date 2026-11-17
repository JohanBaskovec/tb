// Tiny test harness: CHECK* macros count failures, main returns non-zero if any failed.
#pragma once

#include <math.h>
#include <stdio.h>

static int g_failures = 0;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);               \
            g_failures++;                                                                          \
        }                                                                                          \
    } while (0)

#define CHECK_EQ_U64(a, b)                                                                         \
    do {                                                                                           \
        unsigned long long va_ = (unsigned long long)(a), vb_ = (unsigned long long)(b);           \
        if (va_ != vb_) {                                                                          \
            fprintf(stderr, "%s:%d: CHECK_EQ failed: %s == %llu, %s == %llu\n", __FILE__,          \
                    __LINE__, #a, va_, #b, vb_);                                                   \
            g_failures++;                                                                          \
        }                                                                                          \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                                      \
    do {                                                                                           \
        double va_ = (double)(a), vb_ = (double)(b);                                               \
        if (fabs(va_ - vb_) > (double)(eps)) {                                                     \
            fprintf(stderr, "%s:%d: CHECK_NEAR failed: %s == %g, %s == %g\n", __FILE__, __LINE__,  \
                    #a, va_, #b, vb_);                                                             \
            g_failures++;                                                                          \
        }                                                                                          \
    } while (0)

#define RUN_TEST(fn)                                                                               \
    do {                                                                                           \
        int before_ = g_failures;                                                                  \
        fn();                                                                                      \
        printf("%s %s\n", g_failures == before_ ? "[ ok ]" : "[FAIL]", #fn);                       \
    } while (0)

#define TEST_MAIN_END() return g_failures == 0 ? 0 : 1
